/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

std::expected<void, std::string> SettingsApp::open_wifi_page(system::core::AppContext &context)
{
    close_wifi_page(context);
    wifi_ui_state_ = WifiUiState{};
    wifi_page_.mailbox = std::make_shared<WifiPageMailbox>();
    bind_wifi_service();
    wifi_ui_state_.service_ready = wifi_service_binding_.is_valid() && WifiHelper::is_running();
    if (wifi_ui_state_.service_ready) {
        subscribe_wifi_page_events();
        if (auto result = read_wifi_page_state(); !result) {
            wifi_page_.error = localized_text(current_locale_, "wifi.unavailable_retry");
        }
    }
    auto timer = context.timer().start_periodic(WIFI_PAGE_TIMER_NAME, WIFI_PAGE_POLL_INTERVAL_MS);
    if (!timer) {
        close_wifi_page(context);
        return std::unexpected(timer.error());
    }
    wifi_page_.timer_id = *timer;
    // The saved switch preference may still be starting Wi-Fi when the page opens.
    wifi_page_.scan_requested = true;
    auto rendered = render_wifi_page(context);
    return rendered ? sync_wifi_page(context) : rendered;
}

void SettingsApp::close_wifi_page(system::core::AppContext &context)
{
    invalidate_wifi_connect_operation(context);
    cancel_wifi_page_selection();
    cancel_wifi_keyboard_if_active(context);
    std::fill(wifi_connect_password_.begin(), wifi_connect_password_.end(), '\0');
    wifi_connect_password_.clear();
    selected_wifi_network_.reset();
    if (!wifi_page_.mailbox) {
        return;
    }
    {
        boost::lock_guard lock(wifi_page_.mailbox->mutex);
        wifi_page_.mailbox->open = false;
    }
    disconnect_wifi_events();
    if (wifi_page_.timer_id != system::core::INVALID_TIMER_ID) {
        (void)context.timer().stop(wifi_page_.timer_id);
    }
    stop_wifi_page_scan();
    wifi_page_ = WifiPageState{};
    wifi_ui_state_.scanning = false;
    std::vector<WifiNetworkState>().swap(available_wifi_networks_);
    std::vector<WifiHelper::ScanApInfo>().swap(last_wifi_scan_ap_infos_);
    available_wifi_page_ = 0;
}

void SettingsApp::subscribe_wifi_page_events()
{
    disconnect_wifi_events();
    const auto mailbox = wifi_page_.mailbox;
    const service::EventRegistry::SignalSlot state_callback =
    [mailbox](const std::string &, const service::EventItemMap &) {
        boost::lock_guard lock(mailbox->mutex);
        if (mailbox->open) {
            mailbox->state_changed = true;
        }
    };
    const service::EventRegistry::SignalSlot scan_callback =
    [mailbox](const std::string &, const service::EventItemMap & items) {
        const auto *running = std::get_if<bool>(find_event_item(items, "IsRunning"));
        if (!running) {
            return;
        }
        boost::lock_guard lock(mailbox->mutex);
        if (!mailbox->open) {
            return;
        }
        if (*running) {
            mailbox->scan_started = true;
            mailbox->scan_stopped = false;
        } else {
            mailbox->scan_stopped = true;
        }
    };
    const service::EventRegistry::SignalSlot results_callback =
    [mailbox](const std::string &, const service::EventItemMap & items) {
        const auto *results = std::get_if<boost::json::array>(find_event_item(items, "ApInfos"));
        if (!results) {
            return;
        }
        boost::lock_guard lock(mailbox->mutex);
        if (mailbox->open && mailbox->scan_started) {
            // Coalesce results so a busy App cannot accumulate service payloads.
            mailbox->scan_results = *results;
        }
    };
    wifi_event_connections_.push_back(WifiHelper::subscribe_event(WifiHelper::EventId::GeneralEventHappened,
                                      state_callback));
    wifi_event_connections_.push_back(WifiHelper::subscribe_event(
                                          WifiHelper::EventId::ScanStateChanged, scan_callback
                                      ));
    wifi_event_connections_.push_back(WifiHelper::subscribe_event(WifiHelper::EventId::ScanApInfosUpdated,
                                      results_callback));
    const auto disconnected = [](const auto & connection) {
        return !connection.connected();
    };
    if (std::any_of(wifi_event_connections_.begin(), wifi_event_connections_.end(), disconnected)) {
        wifi_ui_state_.service_ready = false;
        wifi_page_.error = localized_text(current_locale_, "wifi.unavailable_retry");
        disconnect_wifi_events();
    }
}

std::expected<void, std::string> SettingsApp::read_wifi_page_state()
{
    if (!wifi_service_binding_.is_valid() || !WifiHelper::is_running() || wifi_event_connections_.size() != 3) {
        wifi_ui_state_.service_ready = false;
        return std::unexpected("Wi-Fi service unavailable");
    }
    auto state = WifiHelper::call_function_sync<std::string>(
                     WifiHelper::FunctionId::GetGeneralState, service::helper::Timeout(WIFI_SERVICE_TIMEOUT_MS)
                 );
    if (!state) {
        wifi_ui_state_.service_ready = false;
        return std::unexpected(state.error());
    }
    wifi_ui_state_.service_ready = true;
    wifi_ui_state_.general_state = *state;
    wifi_ui_state_.enabled = is_wifi_started_state(*state);
    wifi_ui_state_.connecting = is_wifi_connecting_state(*state);
    wifi_ui_state_.connected_ssid.clear();
    if (*state == "Connected") {
        auto actual = WifiHelper::call_function_sync<std::string>(
                          WifiHelper::FunctionId::GetConnectedSsid, service::helper::Timeout(WIFI_SERVICE_TIMEOUT_MS));
        if (!actual) {
            wifi_ui_state_.service_ready = false;
            return std::unexpected(actual.error());
        }
        wifi_ui_state_.connected_ssid = *actual;
    } else if (wifi_ui_state_.connecting) {
        auto target = WifiHelper::call_function_sync<boost::json::object>(
                          WifiHelper::FunctionId::GetConnectAp, service::helper::Timeout(WIFI_SERVICE_TIMEOUT_MS)
                      );
        if (!target) {
            wifi_ui_state_.service_ready = false;
            return std::unexpected(target.error());
        }
        // Retain only the SSID; credentials are not part of the page model.
        auto ssid = target->if_contains("ssid");
        if (ssid && ssid->is_string()) {
            wifi_ui_state_.connected_ssid = std::string(ssid->as_string());
        }
    }
    return {};
}

void SettingsApp::stop_wifi_page_scan()
{
    if (!wifi_page_.scan_owned) {
        return;
    }
    wifi_page_.scan_owned = false;
    auto result = WifiHelper::call_function_sync(
                      WifiHelper::FunctionId::TriggerScanStop, service::helper::Timeout(WIFI_SERVICE_TIMEOUT_MS)
                  );
    if (!result) {
        BROOKESIA_LOGW("Failed to stop Settings scan: %1%", result.error());
    }
}

std::expected<void, std::string> SettingsApp::scan_wifi_page(system::core::AppContext &context)
{
    if (!wifi_page_.mailbox || !wifi_ui_state_.service_ready || !wifi_ui_state_.enabled ||
            wifi_ui_state_.connecting ||
            wifi_ui_state_.scanning || wifi_page_.toggle_target || wifi_page_.selection) {
        return render_wifi_page(context);
    }
    stop_wifi_page_scan();
    {
        boost::lock_guard lock(wifi_page_.mailbox->mutex);
        wifi_page_.mailbox->scan_started = false;
        wifi_page_.mailbox->scan_stopped = false;
        wifi_page_.mailbox->scan_results.reset();
    }
    wifi_page_.error.clear();
    wifi_page_.scan_received = false;
    wifi_page_.scan_requested = false;
    available_wifi_networks_.clear();
    last_wifi_scan_ap_infos_.clear();
    available_wifi_page_ = 0;
    WifiHelper::ScanParams params;
    params.ap_count = WIFI_SCAN_AP_LIMIT;
    params.interval_ms = WIFI_SCAN_WINDOW_MS;
    params.timeout_ms = WIFI_SCAN_WINDOW_MS;
    auto result = WifiHelper::call_function_sync(
                      WifiHelper::FunctionId::SetScanParams, BROOKESIA_DESCRIBE_TO_JSON(params).as_object(),
                      service::helper::Timeout(WIFI_SERVICE_TIMEOUT_MS)
                  );
    if (result) {
        wifi_page_.scan_owned = true;
        wifi_ui_state_.scanning = true;
        wifi_page_.deadline = SteadyClock::now() +
                              std::chrono::milliseconds(WIFI_SCAN_WINDOW_MS + WIFI_SCAN_SETTLE_TIMEOUT_MS);
        result = WifiHelper::call_function_sync(
                     WifiHelper::FunctionId::TriggerScanStart, service::helper::Timeout(WIFI_SERVICE_TIMEOUT_MS)
                 );
    }
    if (!result) {
        stop_wifi_page_scan();
        wifi_ui_state_.scanning = false;
        wifi_page_.error = localized_text(current_locale_, "wifi.scan_failed");
        BROOKESIA_LOGW("Settings scan failed: %1%", result.error());
    }
    return render_wifi_page(context);
}

std::expected<void, std::string> SettingsApp::toggle_wifi_page(system::core::AppContext &context)
{
    if (!wifi_page_.mailbox || !wifi_ui_state_.service_ready || wifi_page_.toggle_target) {
        return {};
    }
    const bool enabled = !wifi_ui_state_.enabled;
    if (!enabled) {
        invalidate_wifi_connect_operation(context);
        cancel_wifi_page_selection();
        cancel_wifi_keyboard_if_active(context);
        std::fill(wifi_connect_password_.begin(), wifi_connect_password_.end(), '\0');
        wifi_connect_password_.clear();
        selected_wifi_network_.reset();
    }
    auto kv_name = make_settings_kv_name(SETTINGS_STORAGE_NAMESPACE, WIFI_ENABLED_STORAGE_KEY);
    auto saved = kv_name ? StorageHelper::save_key_value(
                     kv_name->nspace, kv_name->key, enabled, SETTINGS_STORAGE_TIMEOUT_MS
                 ) : std::expected<void, std::string>(std::unexpected(kv_name.error()));
    if (!saved) {
        wifi_page_.error = localized_text(current_locale_, "wifi.save_failed");
        return render_wifi_page(context);
    }
    stop_wifi_page_scan();
    wifi_ui_state_.scanning = false;
    wifi_page_.scan_requested = false;
    wifi_page_.error.clear();
    auto result = WifiHelper::call_function_sync(
                      WifiHelper::FunctionId::TriggerGeneralAction, enabled ? "Start" : "Stop",
                      service::helper::Timeout(WIFI_SERVICE_TIMEOUT_MS)
                  );
    if (!result) {
        wifi_page_.error = localized_text(current_locale_, "wifi.toggle_failed");
        auto restored = StorageHelper::save_key_value(
                            kv_name->nspace, kv_name->key, !enabled, SETTINGS_STORAGE_TIMEOUT_MS
                        );
        if (!restored) {
            BROOKESIA_LOGW("Failed to restore Wi-Fi preference: %1%", restored.error());
        }
    } else {
        wifi_page_.toggle_target = enabled;
        wifi_page_.deadline = SteadyClock::now() + std::chrono::milliseconds(WIFI_TOGGLE_TIMEOUT_MS);
    }
    wifi_page_.next_state_read = {};
    auto rendered = render_wifi_page(context);
    return rendered ? sync_wifi_page(context) : rendered;
}

std::expected<void, std::string> SettingsApp::sync_wifi_page(system::core::AppContext &context)
{
    auto mailbox = wifi_page_.mailbox;
    if (!mailbox || (current_page_ != PAGE_WIFI && current_page_ != PAGE_WIFI_CONNECT)) {
        return {};
    }
    bool changed = false;
    bool stopped = false;
    std::optional<boost::json::array> results;
    {
        boost::lock_guard lock(mailbox->mutex);
        changed = std::exchange(mailbox->state_changed, false);
        stopped = std::exchange(mailbox->scan_stopped, false);
        results = std::move(mailbox->scan_results);
        mailbox->scan_results.reset();
    }
    const auto now = SteadyClock::now();
    if (changed || now >= wifi_page_.next_state_read) {
        const auto previous_state = wifi_ui_state_.general_state;
        const auto previous_ssid = wifi_ui_state_.connected_ssid;
        const auto previous_ready = wifi_ui_state_.service_ready;
        if (auto result = read_wifi_page_state(); !result) {
            wifi_page_.error = localized_text(current_locale_, "wifi.unavailable_retry");
        } else if (!previous_ready) {
            wifi_page_.error.clear();
        }
        changed = changed || previous_state != wifi_ui_state_.general_state ||
                  previous_ssid != wifi_ui_state_.connected_ssid || previous_ready != wifi_ui_state_.service_ready;
        wifi_page_.next_state_read = SteadyClock::now() + std::chrono::milliseconds(WIFI_STATE_POLL_INTERVAL_MS);
    }
    if (settings_uses_gsp_backend() && current_page_ == PAGE_WIFI_CONNECT &&
            (!wifi_ui_state_.service_ready || !wifi_ui_state_.enabled)) {
        // An external Wi-Fi stop or service loss can happen while the password keyboard is open.
        // Hide it before switching pages because its close path restores the connection page.
        invalidate_wifi_connect_operation(context);
        cancel_wifi_page_selection();
        cancel_wifi_keyboard_if_active(context);
        std::fill(wifi_connect_password_.begin(), wifi_connect_password_.end(), '\0');
        wifi_connect_password_.clear();
        selected_wifi_network_.reset();
        wifi_page_.toggle_target.reset();
        wifi_page_.scan_requested = false;
        stop_wifi_page_scan();
        wifi_ui_state_.scanning = false;
        available_wifi_networks_.clear();
        last_wifi_scan_ap_infos_.clear();
        if (wifi_ui_state_.service_ready) {
            wifi_page_.error.clear();
        }
        auto shown = surface_->show_page(context, PAGE_WIFI);
        if (!shown) {
            return shown;
        }
        current_page_ = PAGE_WIFI;
        return render_wifi_page(context);
    }
    if (auto selection = wifi_page_.selection) {
        bool done = false;
        bool success = false;
        bool saved = false;
        std::string ssid;
        std::string password;
        {
            boost::lock_guard lock(selection->mutex);
            done = selection->done;
            if (done && selection->valid) {
                selection->valid = false;
                success = selection->success;
                saved = selection->saved;
                ssid = selection->ssid;
                password = selection->password;
                std::fill(selection->password.begin(), selection->password.end(), '\0');
                selection->password.clear();
            }
        }
        if (done) {
            wifi_page_.selection.reset();
            if (!success) {
                wifi_page_.error = localized_text(current_locale_, "wifi.saved_failed");
                changed = true;
            } else if (current_page_ == PAGE_WIFI && wifi_ui_state_.service_ready && wifi_ui_state_.enabled &&
                       !wifi_ui_state_.scanning && !wifi_page_.toggle_target) {
                auto it = std::find_if(available_wifi_networks_.begin(), available_wifi_networks_.end(),
                [&ssid](const WifiNetworkState &network) { return network.ssid == ssid; });
                if (it != available_wifi_networks_.end()) {
                    auto network = *it;
                    network.saved = saved;
                    network.password = password;
                    std::fill(password.begin(), password.end(), '\0');
                    if (network.saved) {
                        auto connected = connect_wifi_network(context, network, network.password);
                        std::fill(network.password.begin(), network.password.end(), '\0');
                        if (!connected) {
                            wifi_page_.error = localized_text(current_locale_, "wifi.start_failed");
                            changed = true;
                        } else {
                            return connected;
                        }
                    } else {
                        return open_wifi_connect(context, std::move(network));
                    }
                }
            }
            std::fill(password.begin(), password.end(), '\0');
        }
    }
    changed = sync_wifi_connect_operation(context) || changed;
    if (!wifi_ui_state_.service_ready) {
        if (wifi_page_.selection) {
            cancel_wifi_page_selection();
            changed = true;
        }
        stop_wifi_page_scan();
        wifi_ui_state_.scanning = false;
        wifi_page_.toggle_target.reset();
        wifi_page_.scan_requested = false;
    } else if (wifi_page_.toggle_target) {
        const bool target = *wifi_page_.toggle_target;
        const bool settled = target ? wifi_ui_state_.enabled :
                             (wifi_ui_state_.general_state == "Inited" || wifi_ui_state_.general_state == "Idle");
        if (settled || now >= wifi_page_.deadline) {
            wifi_page_.toggle_target.reset();
            wifi_page_.scan_requested = settled && target;
            if (!settled) {
                wifi_page_.error = localized_text(current_locale_, "wifi.toggle_timeout");
            }
            changed = true;
        }
    }
    if (wifi_ui_state_.scanning) {
        if (results) {
            std::vector<WifiHelper::ScanApInfo> aps;
            if (BROOKESIA_DESCRIBE_FROM_JSON(*results, aps)) {
                if (aps.size() > WIFI_SCAN_AP_LIMIT) {
                    aps.resize(WIFI_SCAN_AP_LIMIT);
                }
                last_wifi_scan_ap_infos_ = std::move(aps);
                rebuild_available_wifi_networks();
                wifi_page_.scan_received = true;
                BROOKESIA_LOGI("Settings Wi-Fi scan: %1% networks", available_wifi_networks_.size());
            } else {
                wifi_page_.error = localized_text(current_locale_, "wifi.scan_failed");
            }
            stop_wifi_page_scan();
            wifi_ui_state_.scanning = false;
            changed = true;
        } else if (stopped || now >= wifi_page_.deadline || !wifi_ui_state_.enabled) {
            // Successful scans publish a result, including an empty array when no AP is found.
            stop_wifi_page_scan();
            wifi_ui_state_.scanning = false;
            wifi_page_.error = localized_text(current_locale_, "wifi.scan_failed");
            changed = true;
        }
    }
    if (!wifi_ui_state_.enabled && !wifi_page_.toggle_target) {
        available_wifi_networks_.clear();
        last_wifi_scan_ap_infos_.clear();
    }
    if (wifi_page_.scan_requested && current_page_ == PAGE_WIFI && wifi_ui_state_.enabled &&
            !wifi_page_.toggle_target) {
        return scan_wifi_page(context);
    }
    if (!changed) {
        return {};
    }
    return current_page_ == PAGE_WIFI ? render_wifi_page(context) : refresh_wifi_connect_state(context);
}

std::expected<void, std::string> SettingsApp::render_wifi_page(system::core::AppContext &context)
{
    WifiPageView view;
    view.enabled = wifi_ui_state_.enabled;
    view.toggle_enabled = wifi_ui_state_.service_ready && !wifi_page_.toggle_target &&
                          wifi_ui_state_.general_state != "Starting" && wifi_ui_state_.general_state != "Stopping";
    view.refresh_enabled = view.toggle_enabled && view.enabled && !wifi_ui_state_.connecting &&
                           !wifi_ui_state_.scanning &&
                           !wifi_page_.selection;
    view.disconnect_enabled = view.enabled && wifi_ui_state_.general_state == "Connected";
    view.connection = localized_text(current_locale_, "wifi.not_connected");
    if (wifi_page_.connect_pending) {
        view.connection = localized_text(current_locale_, "wifi.connecting_prefix") + wifi_page_.connect_ssid;
    } else if (wifi_ui_state_.general_state == "Connected") {
        view.connection = localized_text(current_locale_, "wifi.connected_prefix") + wifi_ui_state_.connected_ssid;
    } else if (wifi_ui_state_.connecting) {
        view.connection = localized_text(current_locale_, "wifi.connecting_prefix") + wifi_ui_state_.connected_ssid;
    }
    if (!wifi_ui_state_.service_ready) {
        view.status = localized_text(current_locale_, "wifi.unavailable_retry");
        view.connection = localized_text(current_locale_, "wifi.status_unavailable");
    } else if (wifi_page_.toggle_target) {
        view.status = localized_text(current_locale_, *wifi_page_.toggle_target ?
                                     "wifi.turning_on" : "wifi.turning_off");
    } else if (wifi_ui_state_.general_state == "Starting" || wifi_ui_state_.general_state == "Stopping") {
        view.status = localized_text(current_locale_, wifi_ui_state_.general_state == "Starting" ?
                                     "wifi.turning_on" : "wifi.turning_off");
    } else if (!wifi_page_.error.empty()) {
        view.status = wifi_page_.error;
    } else if (wifi_page_.selection) {
        view.status = localized_text(current_locale_, "wifi.checking_saved");
    } else if (wifi_page_.connect_pending || wifi_ui_state_.connecting) {
        view.status = localized_text(current_locale_, "wifi.connecting_network");
    } else if (!view.enabled) {
        view.status = localized_text(current_locale_, "wifi.off_hint");
    } else if (wifi_ui_state_.scanning) {
        view.status = localized_text(current_locale_, "wifi.scanning_networks");
    } else if (wifi_page_.scan_received && available_wifi_networks_.empty()) {
        view.status = localized_text(current_locale_, "wifi.empty_hint");
    } else {
        view.status = localized_text(current_locale_, "wifi.nearby");
    }
    const auto count = (wifi_ui_state_.service_ready && view.enabled && !wifi_ui_state_.scanning) ?
                       available_wifi_networks_.size() : 0;
    const auto pages = std::max<size_t>(1, (count + WIFI_PAGE_ROW_COUNT - 1) / WIFI_PAGE_ROW_COUNT);
    available_wifi_page_ = std::min(available_wifi_page_, pages - 1);
    view.previous_enabled = available_wifi_page_ > 0;
    view.next_enabled = available_wifi_page_ + 1 < pages;
    view.pagination = std::to_string(count) + localized_text(current_locale_, "gsp.networks") +
                      "   " +
                      std::to_string(available_wifi_page_ + 1) +
                      " / " + std::to_string(pages);
    for (size_t row = 0; row < WIFI_PAGE_ROW_COUNT; ++row) {
        const auto index = available_wifi_page_ * WIFI_PAGE_ROW_COUNT + row;
        if (index >= count) {
            break;
        }
        const auto &network = available_wifi_networks_[index];
        view.rows[row].enabled = !wifi_ui_state_.scanning && !wifi_page_.selection && !network.ssid.empty();
        view.rows[row].ssid = network.ssid;
        view.rows[row].detail = localized_text(current_locale_, network.enterprise ? "wifi.enterprise" :
                                network.locked ? "wifi.secured" : "wifi.open") +
                                "   " + localized_text(current_locale_, "wifi.signal") + " " +
                                std::to_string(network.signal_level) + "/4   " +
                                std::to_string(network.rssi) + " dBm   " + network.band;
        if (wifi_ui_state_.general_state == "Connected" && network.ssid == wifi_ui_state_.connected_ssid) {
            view.rows[row].detail += "   " + localized_text(current_locale_, "wifi.connected");
        }
    }
    return surface_->set_wifi(context, view);
}

std::expected<void, std::string> SettingsApp::select_wifi_page_network(
    system::core::AppContext &context, size_t row
)
{
    if (current_page_ != PAGE_WIFI || row >= WIFI_PAGE_ROW_COUNT || !wifi_ui_state_.service_ready ||
            !wifi_ui_state_.enabled || wifi_ui_state_.scanning || wifi_page_.toggle_target || wifi_page_.selection) {
        return {};
    }
    const size_t index = available_wifi_page_ * WIFI_PAGE_ROW_COUNT + row;
    if (index >= available_wifi_networks_.size()) {
        return {};
    }
    auto network = available_wifi_networks_[index];
    if (network.ssid.empty()) {
        return {};
    }
    if (wifi_ui_state_.general_state == "Connected" && wifi_ui_state_.connected_ssid == network.ssid) {
        wifi_page_.error.clear();
        return render_wifi_page(context);
    }
    if (network.enterprise) {
        wifi_page_.error = localized_text(current_locale_, "wifi.enterprise_error");
        return render_wifi_page(context);
    }

    if (!network.locked) {
        auto result = connect_wifi_network(context, network, "");
        if (!result) {
            wifi_page_.error = localized_text(current_locale_, "wifi.start_failed");
            return render_wifi_page(context);
        }
        return result;
    }

    auto selection = std::make_shared<WifiSelectionOperation>();
    selection->ssid = network.ssid;
    wifi_page_.selection = selection;
    wifi_page_.error.clear();
    const auto handler = [selection](service::FunctionResult && result) {
        boost::lock_guard lock(selection->mutex);
        if (!selection->valid) {
            return;
        }
        if (result.success && result.has_data()) {
            std::vector<WifiHelper::ConnectApInfo> aps;
            if (BROOKESIA_DESCRIBE_FROM_JSON(result.get_data<boost::json::array>(), aps)) {
                selection->success = true;
                for (auto &ap : aps) {
                    if (ap.ssid == selection->ssid) {
                        selection->saved = true;
                        selection->password = ap.password;
                    }
                    std::fill(ap.password.begin(), ap.password.end(), '\0');
                }
            }
        }
        selection->done = true;
    };
    if (!WifiHelper::call_function_async(WifiHelper::FunctionId::GetConnectedAps, handler)) {
        cancel_wifi_page_selection();
        wifi_page_.error = localized_text(current_locale_, "wifi.saved_failed");
    }
    return render_wifi_page(context);
}

void SettingsApp::cancel_wifi_page_selection()
{
    auto selection = std::move(wifi_page_.selection);
    if (!selection) {
        return;
    }
    boost::lock_guard lock(selection->mutex);
    selection->valid = false;
    std::fill(selection->password.begin(), selection->password.end(), '\0');
    selection->password.clear();
}

std::expected<void, std::string> SettingsApp::back_wifi_connect(system::core::AppContext &context)
{
    if (current_page_ != PAGE_WIFI_CONNECT) {
        return {};
    }
    cancel_wifi_keyboard_if_active(context);
    auto shown = surface_->show_page(context, PAGE_WIFI);
    if (!shown) {
        return shown;
    }
    invalidate_wifi_connect_operation(context);
    std::fill(wifi_connect_password_.begin(), wifi_connect_password_.end(), '\0');
    wifi_connect_password_.clear();
    selected_wifi_network_.reset();
    current_page_ = PAGE_WIFI;
    return render_wifi_page(context);
}
