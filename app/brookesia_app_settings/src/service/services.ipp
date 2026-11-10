/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

void SettingsApp::bind_wifi_service()
{
    if (wifi_service_binding_.is_valid() || !WifiHelper::is_available()) {
        return;
    }

    auto binding = service::ServiceManager::get_instance().bind(WifiHelper::get_name().data());
    if (!binding.is_valid()) {
        BROOKESIA_LOGW("Wi-Fi service is available but failed to bind; Wi-Fi page will stay disabled");
        return;
    }
    wifi_service_binding_ = std::move(binding);
}

void SettingsApp::release_wifi_service()
{
    ++wifi_operation_generation_;
    wifi_service_binding_.release();
}

void SettingsApp::bind_sntp_service()
{
    if (sntp_service_binding_.is_valid()) {
        return;
    }

    if (!SNTPHelper::is_available()) {
        BROOKESIA_LOGW("SNTP service is not registered; Time zone page will stay unavailable");
        return;
    }

    auto binding = service::ServiceManager::get_instance().bind(SNTPHelper::get_name().data());
    if (!binding.is_valid()) {
        BROOKESIA_LOGW("SNTP service is available but failed to bind; Time zone page will stay unavailable");
        return;
    }
    sntp_service_binding_ = std::move(binding);
    if (!SNTPHelper::is_running()) {
        BROOKESIA_LOGW("SNTP service binding was created but service is not running");
        sntp_service_binding_.release();
    }
}

void SettingsApp::release_sntp_service()
{
    sntp_service_binding_.release();
}

void SettingsApp::subscribe_sntp_events()
{
    disconnect_sntp_events();
    if (!time_zone_ui_state_.service_ready) {
        return;
    }

    const std::weak_ptr<WifiUiMailbox> weak_mailbox = wifi_ui_mailbox_;
    const service::EventRegistry::SignalSlot state_callback =
        [weak_mailbox](const std::string &, const service::EventItemMap &items) {
        const auto *state = std::get_if<std::string>(find_event_item(items, "State"));
        if (!state) {
            return;
        }
        if (auto mailbox = weak_mailbox.lock()) {
            mailbox->enqueue([state = *state](SettingsApp &app) {
                app.time_zone_ui_state_.state = state;
                if (app.context_ == nullptr) {
                    return;
                }
                if (auto result = app.refresh_time_zone_state(*app.context_); !result) {
                    BROOKESIA_LOGW("Failed to refresh Time zone state after SNTP event: %1%", result.error());
                }
            });
        }
    };
    auto state_connection = SNTPHelper::subscribe_event(SNTPHelper::EventId::StateChanged, state_callback);
    if (state_connection.connected()) {
        sntp_event_connections_.push_back(std::move(state_connection));
    } else {
        BROOKESIA_LOGW("Failed to subscribe SNTP state event");
    }
}

void SettingsApp::disconnect_sntp_events()
{
    sntp_event_connections_.clear();
}

std::expected<void, std::string> SettingsApp::refresh_time_zone_state(system::core::AppContext &context)
{
    bind_sntp_service();
    time_zone_ui_state_ = TimeZoneUiState{};
    time_zone_ui_state_.service_ready = sntp_service_binding_.is_valid() && SNTPHelper::is_running();

    if (!time_zone_ui_state_.service_ready) {
        return refresh_time_zone_page_state(context);
    }

    auto timezone = SNTPHelper::call_function_sync<std::string>(
                        SNTPHelper::FunctionId::GetTimezone,
                        service::helper::Timeout(SNTP_SERVICE_TIMEOUT_MS)
                    );
    if (timezone) {
        time_zone_ui_state_.timezone = *timezone;
    } else {
        BROOKESIA_LOGW("Failed to get SNTP timezone: %1%", timezone.error());
    }

    auto state = SNTPHelper::call_function_sync<std::string>(
                     SNTPHelper::FunctionId::GetState,
                     service::helper::Timeout(SNTP_SERVICE_TIMEOUT_MS)
                 );
    if (state) {
        time_zone_ui_state_.state = *state;
    } else {
        BROOKESIA_LOGW("Failed to get SNTP state: %1%", state.error());
    }

    return refresh_time_zone_page_state(context);
}

std::expected<void, std::string> SettingsApp::refresh_time_zone_page_state(system::core::AppContext &context)
{
    const auto unavailable = make_localized_unavailable_text(current_locale_);
    const auto timezone_text = time_zone_ui_state_.timezone.empty() ? unavailable : time_zone_ui_state_.timezone;
    const auto state_text = time_zone_ui_state_.state.empty() ? unavailable : time_zone_ui_state_.state;

    if (settings_uses_gsp_backend()) {
        if (current_page_ != PAGE_TIME_ZONE) {
            return {};
        }
        TimeZonePageView view;
        view.current_timezone = time_zone_ui_state_.timezone.empty() ?
                                unavailable :
                                (get_time_zone_summary_name(current_locale_, time_zone_ui_state_.timezone) +
                                 " (" + time_zone_ui_state_.timezone + ")");
        view.sntp_state = state_text;
        static constexpr std::pair<std::string_view, std::string_view> SNTP_STATE_LABELS[] = {
            {"Stopped", "gsp.sntp_stopped"},
            {"CheckingNetwork", "gsp.sntp_checking_network"},
            {"Syncing", "gsp.sntp_syncing"},
            {"Synced", "gsp.sntp_synced"},
        };
        for (const auto &[state, label] : SNTP_STATE_LABELS) {
            if (state_text == state) {
                view.sntp_state = localized_text(current_locale_, label);
                break;
            }
        }
        view.status = localized_text(current_locale_, "gsp.time_zone_hint");
        for (size_t i = 0; i < TIME_ZONE_OPTIONS.size() && i < view.labels.size(); ++i) {
            const auto &option = TIME_ZONE_OPTIONS[i];
            view.labels[i] = get_time_zone_summary_name(current_locale_, option.timezone);
            view.values[i] = (option.timezone == time_zone_ui_state_.timezone) ?
                             (localized_text(current_locale_, "current") + " | " + std::string(option.timezone)) :
                             std::string(option.timezone);
        }
        return surface_->set_time_zone(context, view);
    }

    std::vector<gui::BindingValueUpdate> updates;
    add_binding_update(
        updates,
        MORE_TIME_ZONE_VALUE_PATH,
        "labelProps.text",
        time_zone_ui_state_.timezone.empty() ? unavailable :
        get_time_zone_summary_name(current_locale_, time_zone_ui_state_.timezone)
    );
    add_binding_update(updates, TIME_ZONE_CURRENT_VALUE_PATH, "labelProps.text", timezone_text);
    add_binding_update(updates, TIME_ZONE_STATE_VALUE_PATH, "labelProps.text", state_text);

    for (const auto &option : TIME_ZONE_OPTIONS) {
        add_binding_update(
            updates,
            std::string(option.value_path),
            "labelProps.text",
            option.timezone == time_zone_ui_state_.timezone ? localized_text(current_locale_, "current") : ""
        );
    }
    return context.gui().set_binding_values(updates);
}

std::expected<void, std::string> SettingsApp::set_time_zone(
    system::core::AppContext &context,
    std::string_view timezone
)
{
    bind_sntp_service();
    if (!sntp_service_binding_.is_valid() || !SNTPHelper::is_running()) {
        ensure_message_dialog(
            context,
            make_localized_unavailable_text(current_locale_),
            system::core::MessageDialogIcon::Warning,
            MESSAGE_DIALOG_FAILED_AUTO_CLOSE_MS
        );
        return refresh_time_zone_state(context);
    }

    auto result = SNTPHelper::call_function_sync(
                      SNTPHelper::FunctionId::SetTimezone,
                      std::string(timezone),
                      service::helper::Timeout(SNTP_SERVICE_TIMEOUT_MS)
                  );
    if (!result) {
        BROOKESIA_LOGW("Failed to set SNTP timezone '%1%': %2%", timezone, result.error());
        ensure_message_dialog(
            context,
            localized_text(current_locale_, "time_zone_set_failed"),
            system::core::MessageDialogIcon::Warning,
            MESSAGE_DIALOG_FAILED_AUTO_CLOSE_MS
        );
        return refresh_time_zone_state(context);
    }

    time_zone_ui_state_.timezone = timezone;
    auto refresh_result = refresh_time_zone_state(context);
    if (!refresh_result) {
        return refresh_result;
    }
    ensure_message_dialog(
        context,
        localized_text(current_locale_, "time_zone_set"),
        system::core::MessageDialogIcon::Information,
        MESSAGE_DIALOG_SUCCESS_AUTO_CLOSE_MS
    );
    return {};
}

bool SettingsApp::load_wifi_enabled_preference()
{
    bind_storage_service();
    if (!storage_service_binding_.is_valid()) {
        BROOKESIA_LOGW("Storage service is unavailable; default Settings Wi-Fi switch to off");
        return false;
    }

    auto kv_name = make_settings_kv_name(SETTINGS_STORAGE_NAMESPACE, WIFI_ENABLED_STORAGE_KEY);
    if (!kv_name) {
        BROOKESIA_LOGW("Failed to resolve Settings Wi-Fi switch preference key: %1%", kv_name.error());
        return false;
    }

    auto result = StorageHelper::get_key_value<bool>(
                      kv_name->nspace,
                      kv_name->key,
                      SETTINGS_STORAGE_TIMEOUT_MS
                  );
    if (!result) {
        BROOKESIA_LOGD(
            "Settings Wi-Fi switch preference is not stored yet; default to off: %1%",
            result.error()
        );
        return false;
    }
    return *result;
}

std::expected<void, std::string> SettingsApp::submit_wifi_enabled_preference_save(
    bool enabled,
    service::ServiceBase::FunctionResultHandler handler
)
{
    bind_storage_service();
    if (!storage_service_binding_.is_valid()) {
        return std::unexpected("Storage service is unavailable");
    }

    auto kv_name = make_settings_kv_name(SETTINGS_STORAGE_NAMESPACE, WIFI_ENABLED_STORAGE_KEY);
    if (!kv_name) {
        return std::unexpected("Failed to resolve Settings Wi-Fi switch preference key: " + kv_name.error());
    }

    if (!StorageHelper::save_key_value_async(
                kv_name->nspace,
                kv_name->key,
                enabled,
                std::move(handler)
            )) {
        return std::unexpected("Failed to submit Settings Wi-Fi preference save request");
    }
    return {};
}

void SettingsApp::apply_wifi_enabled_preference_to_service()
{
    bind_wifi_service();
    if (!wifi_service_binding_.is_valid() || !WifiHelper::is_running()) {
        return;
    }

    if (settings_uses_gsp_backend()) {
        auto state = WifiHelper::call_function_sync<std::string>(
                         WifiHelper::FunctionId::GetGeneralState, service::helper::Timeout(WIFI_SERVICE_TIMEOUT_MS)
                     );
        if (state && is_wifi_started_state(*state) != wifi_ui_state_.enabled) {
            auto result = WifiHelper::call_function_sync(
                              WifiHelper::FunctionId::TriggerGeneralAction,
                              wifi_ui_state_.enabled ? "Start" : "Stop",
                              service::helper::Timeout(WIFI_SERVICE_TIMEOUT_MS)
                          );
            if (!result) {
                BROOKESIA_LOGW("Failed to restore Wi-Fi preference: %1%", result.error());
            }
        }
        return;
    }

    const auto preferred_enabled = wifi_ui_state_.enabled;
    const auto generation_token = impl_->wifi_operation_generation;
    const auto generation = generation_token->load();
    const std::weak_ptr<WifiUiMailbox> weak_mailbox = wifi_ui_mailbox_;
    const auto state_handler = [preferred_enabled, generation_token, generation, weak_mailbox](service::FunctionResult && result) {
        if (generation != generation_token->load()) {
            return;
        }
        if (!result.success) {
            BROOKESIA_LOGW("Failed to get Wi-Fi state before applying Settings preference: %1%", result.error_message);
            return;
        }
        if (!result.has_data()) {
            BROOKESIA_LOGW("Failed to apply Settings Wi-Fi preference: GetGeneralState returned no data");
            return;
        }

        const auto &state = result.get_data<std::string>();
        const bool service_started = is_wifi_started_state(state);
        if (preferred_enabled == service_started) {
            return;
        }

        const auto action = preferred_enabled ? WifiHelper::GeneralAction::Start : WifiHelper::GeneralAction::Stop;
        const auto action_name = BROOKESIA_DESCRIBE_TO_STR(action);
        const auto action_handler = [action_name, generation_token, generation](service::FunctionResult && action_result) {
            if (generation != generation_token->load()) {
                return;
            }
            if (action_result.success) {
                return;
            }
            BROOKESIA_LOGW(
                "Failed to apply Settings Wi-Fi preference action '%1%': %2%",
                action_name,
                action_result.error_message
            );
        };
        if (!WifiHelper::call_function_async(
                    WifiHelper::FunctionId::TriggerGeneralAction,
                    action_name,
                    make_wifi_ui_result_handler(weak_mailbox.lock(), action_handler)
                )) {
            BROOKESIA_LOGW("Failed to submit Settings Wi-Fi preference action '%1%'", action_name);
        }
    };
    if (!WifiHelper::call_function_async(WifiHelper::FunctionId::GetGeneralState,
                                         make_wifi_ui_result_handler(wifi_ui_mailbox_, state_handler))) {
        BROOKESIA_LOGW("Failed to submit Settings Wi-Fi state request before applying preference");
    }
}

void SettingsApp::subscribe_wifi_events()
{
    disconnect_wifi_events();
    if (!wifi_ui_state_.service_ready || !wifi_ui_mailbox_) {
        return;
    }
    const std::weak_ptr<WifiUiMailbox> weak_mailbox = wifi_ui_mailbox_;

    const service::EventRegistry::SignalSlot general_callback =
        [weak_mailbox](const std::string &, const service::EventItemMap &items) {
        const auto *event = std::get_if<std::string>(find_event_item(items, "Event"));
        const auto *unexpected = std::get_if<bool>(find_event_item(items, "IsUnexpected"));
        if (!event || !unexpected) {
            return;
        }
        if (auto mailbox = weak_mailbox.lock()) {
            mailbox->enqueue([event = *event, unexpected = *unexpected](SettingsApp &app) {
                app.handle_wifi_general_event(event, unexpected);
            });
        }
    };
    auto general_connection = WifiHelper::subscribe_event(
                                  WifiHelper::EventId::GeneralEventHappened,
                                  general_callback
                              );
    if (general_connection.connected()) {
        wifi_event_connections_.push_back(std::move(general_connection));
    } else {
        BROOKESIA_LOGW("Failed to subscribe Wi-Fi general event");
    }

    const service::EventRegistry::SignalSlot scan_state_callback =
        [weak_mailbox](const std::string &, const service::EventItemMap &items) {
        const auto *running = std::get_if<bool>(find_event_item(items, "IsRunning"));
        if (!running) {
            return;
        }
        if (auto mailbox = weak_mailbox.lock()) {
            mailbox->enqueue([running = *running](SettingsApp &app) {
                app.handle_wifi_scan_state_changed(running);
            });
        }
    };
    auto scan_state_connection = WifiHelper::subscribe_event(
                                     WifiHelper::EventId::ScanStateChanged,
                                     scan_state_callback
                                 );
    if (scan_state_connection.connected()) {
        wifi_event_connections_.push_back(std::move(scan_state_connection));
    } else {
        BROOKESIA_LOGW("Failed to subscribe Wi-Fi scan state event");
    }

    const service::EventRegistry::SignalSlot scan_infos_callback =
        [weak_mailbox](const std::string &, const service::EventItemMap &items) {
        const auto *ap_infos_json = std::get_if<boost::json::array>(find_event_item(items, "ApInfos"));
        if (!ap_infos_json) {
            return;
        }
        if (auto mailbox = weak_mailbox.lock()) {
            mailbox->enqueue([ap_infos = *ap_infos_json](SettingsApp &app) {
                app.handle_wifi_scan_ap_infos_updated(ap_infos);
            });
        }
    };
    auto scan_infos_connection = WifiHelper::subscribe_event(
                                     WifiHelper::EventId::ScanApInfosUpdated,
                                     scan_infos_callback
                                 );
    if (scan_infos_connection.connected()) {
        wifi_event_connections_.push_back(std::move(scan_infos_connection));
    } else {
        BROOKESIA_LOGW("Failed to subscribe Wi-Fi scan AP event");
    }
}

void SettingsApp::disconnect_wifi_events()
{
    wifi_event_connections_.clear();
}

bool SettingsApp::is_saved_wifi_ssid(std::string_view ssid) const
{
    return std::any_of(saved_wifi_networks_.begin(), saved_wifi_networks_.end(), [ssid](const auto & network) {
        return std::string_view(network.ssid) == ssid;
    });
}

void SettingsApp::remove_unavailable_wifi_networks_from_available_list()
{
    rebuild_available_wifi_networks();
}

void SettingsApp::rebuild_available_wifi_networks()
{
    available_wifi_networks_.erase(
        std::remove_if(available_wifi_networks_.begin(), available_wifi_networks_.end(), [](const auto &network) {
            return network.ssid.empty();
        }),
        available_wifi_networks_.end()
    );

    const auto connected_state = BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralState::Connected);
    const bool should_filter_target_ssid = !settings_uses_gsp_backend() &&
                                          (wifi_ui_state_.general_state == connected_state || wifi_ui_state_.connecting);
    std::vector<WifiHelper::ScanApInfo> best_ap_infos;
    for (const auto &ap : last_wifi_scan_ap_infos_) {
        if (ap.ssid.empty()) {
            continue;
        }
        if (should_filter_target_ssid && !wifi_ui_state_.connected_ssid.empty() &&
                ap.ssid == wifi_ui_state_.connected_ssid) {
            continue;
        }
        if (!settings_uses_gsp_backend() && is_saved_wifi_ssid(ap.ssid)) {
            continue;
        }

        auto existing = std::find_if(best_ap_infos.begin(), best_ap_infos.end(), [&ap](const auto &candidate) {
            return candidate.ssid == ap.ssid;
        });
        if (existing == best_ap_infos.end()) {
            best_ap_infos.push_back(ap);
            continue;
        }

        const auto existing_signal_level = BROOKESIA_DESCRIBE_ENUM_TO_NUM(existing->signal_level);
        const auto signal_level = BROOKESIA_DESCRIBE_ENUM_TO_NUM(ap.signal_level);
        if (signal_level > existing_signal_level ||
                (signal_level == existing_signal_level && ap.rssi > existing->rssi)) {
            *existing = ap;
        }
    }

    available_wifi_networks_.clear();
    available_wifi_page_ = 0;
    for (size_t i = 0; i < best_ap_infos.size(); ++i) {
        const auto &ap = best_ap_infos[i];
        const auto signal_level = BROOKESIA_DESCRIBE_ENUM_TO_NUM(ap.signal_level);
        const auto signal_text = get_wifi_signal_text(signal_level);
        const auto security_text = ap.is_locked ? localized_text(current_locale_, "wifi_secured_network") :
                                   localized_text(current_locale_, "wifi_open_network");
        const auto band_text = get_wifi_band_text(ap.channel);
        std::string detail = security_text;
        if (!signal_text.empty()) {
            detail += " / ";
            detail += signal_text;
        }
        if (!band_text.empty()) {
            detail += " / ";
            detail += band_text;
        }
        available_wifi_networks_.push_back(WifiNetworkState{
            .parent = AVAILABLE_NETWORK_PARENT,
            .id = make_wifi_instance_id("available", ap.ssid, i),
            .ssid = ap.ssid,
            .detail = detail,
            .band = band_text,
            .password = "",
            .saved = false,
            .locked = ap.is_locked,
            .enterprise = ap.is_enterprise,
            .signal_level = signal_level,
            .channel = ap.channel,
            .rssi = ap.rssi,
        });
    }
}

void SettingsApp::request_saved_wifi_networks_refresh()
{
    if (context_ == nullptr || !wifi_ui_state_.service_ready || !wifi_ui_state_.enabled ||
            saved_wifi_networks_refreshing_) {
        return;
    }

    saved_wifi_networks_refreshing_ = true;
    const auto generation = wifi_operation_generation_.load();
    const auto saved_handler = [this, generation](service::FunctionResult && result) {
        saved_wifi_networks_refreshing_ = false;
        if (context_ == nullptr || generation != wifi_operation_generation_ || !wifi_ui_state_.enabled) {
            return;
        }
        if (!result.success || !result.has_data()) {
            BROOKESIA_LOGW("Failed to get saved Wi-Fi APs: %1%", result.error_message);
            return;
        }

        auto &saved_json = result.get_data<boost::json::array>();
        std::vector<WifiHelper::ConnectApInfo> saved_aps;
        std::vector<WifiNetworkState> next_saved_networks;
        if (BROOKESIA_DESCRIBE_FROM_JSON(saved_json, saved_aps)) {
            std::unordered_set<std::string> seen_ssids;
            for (size_t i = 0; i < saved_aps.size(); ++i) {
                const auto &ap = saved_aps[i];
                if (ap.ssid.empty() || seen_ssids.contains(ap.ssid)) {
                    continue;
                }
                seen_ssids.insert(ap.ssid);
                next_saved_networks.push_back(WifiNetworkState{
                    .parent = SAVED_NETWORK_PARENT,
                    .id = make_wifi_instance_id("saved", ap.ssid, next_saved_networks.size()),
                    .ssid = ap.ssid,
                    .detail = localized_text(current_locale_, "wifi_saved_secured_network"),
                    .band = "",
                    .password = ap.password,
                    .saved = true,
                    .locked = true,
                    .signal_level = 0,
                    .channel = 0,
                });
            }
        } else {
            BROOKESIA_LOGW("Failed to parse saved Wi-Fi APs");
            return;
        }

        saved_wifi_networks_ = std::move(next_saved_networks);
        saved_wifi_networks_loaded_ = true;
        remove_unavailable_wifi_networks_from_available_list();
        if (auto list_result = refresh_wifi_lists(*context_); !list_result) {
            BROOKESIA_LOGW("Failed to refresh Wi-Fi lists after saved AP update: %1%", list_result.error());
        }
    };
    if (!WifiHelper::call_function_async(WifiHelper::FunctionId::GetConnectedAps,
                                         make_wifi_ui_result_handler(wifi_ui_mailbox_, saved_handler))) {
        saved_wifi_networks_refreshing_ = false;
        BROOKESIA_LOGW("Failed to submit saved Wi-Fi AP request");
    }
}

void SettingsApp::reset_available_wifi_scan_visibility()
{
    available_wifi_rows_hidden_for_scan_ = false;
    available_wifi_scan_results_ready_ = false;
}

void SettingsApp::cancel_wifi_connected_hide_timer(system::core::AppContext &context)
{
    if (pending_wifi_connected_hide_timer_id_ == system::core::INVALID_TIMER_ID) {
        return;
    }
    if (!context.timer().stop(pending_wifi_connected_hide_timer_id_)) {
        BROOKESIA_LOGW("Failed to stop pending Settings Wi-Fi connected hide timer: %1%",
                       pending_wifi_connected_hide_timer_id_);
    }
    pending_wifi_connected_hide_timer_id_ = system::core::INVALID_TIMER_ID;
}

void SettingsApp::schedule_wifi_connected_hide_timer(system::core::AppContext &context)
{
    cancel_wifi_connected_hide_timer(context);
    auto timer_result = context.timer().start_delayed(WIFI_CONNECTED_HIDE_TIMER_NAME, WIFI_DISCONNECTED_HIDE_DELAY_MS);
    if (!timer_result) {
        BROOKESIA_LOGW("Failed to schedule Settings Wi-Fi connected hide timer: %1%", timer_result.error());
        return;
    }
    pending_wifi_connected_hide_timer_id_ = *timer_result;
}

void SettingsApp::cancel_wifi_connected_scroll_timer(system::core::AppContext &context)
{
    if (pending_wifi_connected_scroll_timer_id_ == system::core::INVALID_TIMER_ID) {
        return;
    }
    if (!context.timer().stop(pending_wifi_connected_scroll_timer_id_)) {
        BROOKESIA_LOGW(
            "Failed to stop pending Settings Wi-Fi connected scroll timer: %1%",
            pending_wifi_connected_scroll_timer_id_
        );
    }
    pending_wifi_connected_scroll_timer_id_ = system::core::INVALID_TIMER_ID;
}

void SettingsApp::schedule_wifi_connected_card_scroll(system::core::AppContext &context)
{
    cancel_wifi_connected_scroll_timer(context);
    if (current_page_ != PAGE_WIFI) {
        return;
    }

    auto timer_result = context.timer().start_delayed(
                            WIFI_CONNECTED_SCROLL_TIMER_NAME,
                            WIFI_CONNECTED_SCROLL_DELAY_MS
                        );
    if (!timer_result) {
        BROOKESIA_LOGW("Failed to schedule Settings Wi-Fi connected scroll timer: %1%", timer_result.error());
        return;
    }
    pending_wifi_connected_scroll_timer_id_ = *timer_result;
}

void SettingsApp::show_wifi_connected_card(WifiConnectedCardState state, std::string ssid)
{
    wifi_ui_state_.connected_card_state = state;
    wifi_ui_state_.connected_card_ssid = std::move(ssid);
}

void SettingsApp::hide_wifi_connected_card()
{
    wifi_ui_state_.connected_card_state = WifiConnectedCardState::Hidden;
    wifi_ui_state_.connected_card_ssid.clear();
}

bool SettingsApp::is_wifi_connected_card_visible() const
{
    return wifi_ui_state_.service_ready && wifi_ui_state_.enabled &&
           wifi_ui_state_.connected_card_state != WifiConnectedCardState::Hidden &&
           !wifi_ui_state_.connected_card_ssid.empty();
}

void SettingsApp::sync_wifi_connected_card_from_service_state()
{
    if (!wifi_ui_state_.service_ready || !wifi_ui_state_.enabled) {
        hide_wifi_connected_card();
        return;
    }

    const auto connected_state = BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralState::Connected);
    if (wifi_ui_state_.general_state == connected_state) {
        const auto ssid = !wifi_ui_state_.connected_ssid.empty() ?
                          wifi_ui_state_.connected_ssid :
                          wifi_ui_state_.connected_card_ssid;
        if (!ssid.empty()) {
            show_wifi_connected_card(WifiConnectedCardState::Connected, ssid);
            return;
        }
    }

    if (wifi_ui_state_.connecting) {
        const auto ssid = !wifi_ui_state_.connected_ssid.empty() ?
                          wifi_ui_state_.connected_ssid :
                          wifi_ui_state_.connected_card_ssid;
        if (!ssid.empty()) {
            show_wifi_connected_card(WifiConnectedCardState::Connecting, ssid);
            return;
        }
    }

    if (wifi_ui_state_.connected_card_state == WifiConnectedCardState::DisconnectedGrace) {
        return;
    }
    if (wifi_ui_state_.connected_card_state == WifiConnectedCardState::Connecting &&
            !wifi_ui_state_.connected_card_ssid.empty()) {
        return;
    }
    hide_wifi_connected_card();
}

std::expected<void, std::string> SettingsApp::scroll_wifi_connected_card_to_top(system::core::AppContext &context)
{
    system::core::GuiBatchCommand command;
    command.type = system::core::GuiBatchCommand::Type::ScrollToView;
    command.path = WIFI_CONNECTED_CARD_PATH;
    command.animated = true;

    auto batch_result = context.gui().execute_batch({command});
    if (!batch_result) {
        return std::unexpected(batch_result.error());
    }
    if (batch_result->success) {
        return {};
    }
    for (const auto &command_result : batch_result->results) {
        if (!command_result.success) {
            return std::unexpected(
                       command_result.error_message.empty() ?
                       "Failed to scroll Wi-Fi connected card" :
                       command_result.error_message
                   );
        }
    }
    return std::unexpected("Failed to scroll Wi-Fi connected card");
}

std::expected<void, std::string> SettingsApp::refresh_wifi_service_state(system::core::AppContext &context)
{
    wifi_ui_state_.service_ready = wifi_service_binding_.is_valid() && WifiHelper::is_running();
    wifi_ui_state_.connecting = false;

    if (!wifi_ui_state_.service_ready) {
        wifi_ui_state_.connected_ssid.clear();
        hide_wifi_connected_card();
        available_wifi_networks_.clear();
        last_wifi_scan_ap_infos_.clear();
        reset_available_wifi_scan_visibility();
        auto result = refresh_wifi_page_state(context);
        if (!result) {
            return result;
        }
        result = refresh_wifi_lists(context);
        if (!result) {
            return result;
        }
        return refresh_wifi_summary_state(context);
    }

    const auto generation = wifi_operation_generation_.load();
    const auto apply_ssid = [this, generation](std::string ssid, bool actual) {
        if (context_ == nullptr || generation != wifi_operation_generation_ || !wifi_ui_state_.enabled) {
            return;
        }
        const bool connected_state = wifi_ui_state_.general_state ==
                                     BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralState::Connected);
        if ((actual && !connected_state) || (!actual && !wifi_ui_state_.connecting)) {
            return;
        }
        wifi_ui_state_.connected_ssid = std::move(ssid);
        if (connected_state) {
            cancel_wifi_connected_hide_timer(*context_);
        }
        sync_wifi_connected_card_from_service_state();
        remove_unavailable_wifi_networks_from_available_list();
        if (auto page_result = refresh_wifi_page_state(*context_); !page_result) {
            BROOKESIA_LOGW("Failed to refresh Wi-Fi page after SSID update: %1%", page_result.error());
        }
        if (auto list_result = refresh_wifi_lists(*context_); !list_result) {
            BROOKESIA_LOGW("Failed to refresh Wi-Fi lists after SSID update: %1%", list_result.error());
        }
        if (auto summary_result = refresh_wifi_summary_state(*context_); !summary_result) {
            BROOKESIA_LOGW("Failed to refresh Wi-Fi summary after SSID update: %1%", summary_result.error());
        }
    };
    const auto actual_ssid_handler = [apply_ssid](service::FunctionResult && result) {
        if (!result.success || !result.has_data()) {
            BROOKESIA_LOGW("Failed to get connected Wi-Fi SSID: %1%", result.error_message);
            return;
        }
        apply_ssid(result.get_data<std::string>(), true);
    };
    const auto target_handler = [apply_ssid](service::FunctionResult && result) {
        if (!result.success || !result.has_data()) {
            BROOKESIA_LOGW("Failed to get Wi-Fi target AP: %1%", result.error_message);
            return;
        }
        WifiHelper::ConnectApInfo target_ap;
        auto &target_json = result.get_data<boost::json::object>();
        apply_ssid(BROOKESIA_DESCRIBE_FROM_JSON(target_json, target_ap) && target_ap.is_valid() ?
                   target_ap.ssid : std::string{}, false);
    };
    const auto state_handler = [this, generation, actual_ssid_handler, target_handler](
                                   service::FunctionResult && result) {
        if (context_ == nullptr || generation != wifi_operation_generation_ || !wifi_ui_state_.enabled) {
            return;
        }
        if (!result.success || !result.has_data()) {
            BROOKESIA_LOGW("Failed to get Wi-Fi general state: %1%", result.error_message);
            return;
        }
        auto &state = result.get_data<std::string>();
        const auto previous_state = wifi_ui_state_.general_state;
        wifi_ui_state_.general_state = state;
        wifi_ui_state_.connecting = is_wifi_connecting_state(state);
        if (state == BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralState::Connected)) {
            if (previous_state != state) {
                wifi_ui_state_.connected_ssid.clear();
            }
            if (!WifiHelper::call_function_async(WifiHelper::FunctionId::GetConnectedSsid,
                                                 make_wifi_ui_result_handler(wifi_ui_mailbox_, actual_ssid_handler))) {
                BROOKESIA_LOGW("Failed to submit connected Wi-Fi SSID request");
            }
        } else if (wifi_ui_state_.connecting) {
            if (!WifiHelper::call_function_async(WifiHelper::FunctionId::GetConnectAp,
                                                 make_wifi_ui_result_handler(wifi_ui_mailbox_, target_handler))) {
                BROOKESIA_LOGW("Failed to submit Wi-Fi target AP request");
            }
        } else {
            wifi_ui_state_.connected_ssid.clear();
        }
        if (wifi_ui_state_.connecting || state == BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralState::Connected)) {
            cancel_wifi_connected_hide_timer(*context_);
        }
        sync_wifi_connected_card_from_service_state();
        if (auto page_result = refresh_wifi_page_state(*context_); !page_result) {
            BROOKESIA_LOGW("Failed to refresh Wi-Fi page after state update: %1%", page_result.error());
        }
        rebuild_available_wifi_networks();
        if (auto list_result = refresh_wifi_lists(*context_); !list_result) {
            BROOKESIA_LOGW("Failed to refresh Wi-Fi lists after state update: %1%", list_result.error());
        }
        if (auto summary_result = refresh_wifi_summary_state(*context_); !summary_result) {
            BROOKESIA_LOGW("Failed to refresh Wi-Fi summary after state update: %1%", summary_result.error());
        }
    };
    if (!WifiHelper::call_function_async(WifiHelper::FunctionId::GetGeneralState,
                                         make_wifi_ui_result_handler(wifi_ui_mailbox_, state_handler))) {
        BROOKESIA_LOGW("Failed to submit Wi-Fi general state request");
    }

    if (wifi_ui_state_.enabled && !saved_wifi_networks_loaded_) {
        request_saved_wifi_networks_refresh();
    }

    sync_wifi_connected_card_from_service_state();
    auto result = refresh_wifi_page_state(context);
    if (!result) {
        return result;
    }
    result = refresh_wifi_lists(context);
    if (!result) {
        return result;
    }
    return refresh_wifi_summary_state(context);
}
