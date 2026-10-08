/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

void SettingsApp::clamp_wifi_list_pages()
{
    const auto clamp_page = [](size_t page, size_t total_count) {
        const auto page_count = get_page_count(total_count);
        if (page >= page_count) {
            return page_count - 1;
        }
        return page;
    };
    saved_wifi_page_ = clamp_page(saved_wifi_page_, saved_wifi_networks_.size());
    available_wifi_page_ = clamp_page(available_wifi_page_, available_wifi_networks_.size());
}

std::expected<void, std::string> SettingsApp::populate_wifi_networks(system::core::AppContext &context)
{
    clamp_wifi_list_pages();
    const auto saved_page_start = get_page_start(saved_wifi_page_);
    const auto saved_visible_count = get_page_visible_count(saved_wifi_networks_.size(), saved_wifi_page_);
    auto result = sync_wifi_network_section(
                      context,
                      SAVED_NETWORK_PARENT,
                      "saved",
                      saved_wifi_networks_,
                      saved_page_start,
                      saved_visible_count,
                      saved_wifi_slot_count_
                  );
    if (!result) {
        return result;
    }

    const bool hide_available_rows = available_wifi_rows_hidden_for_scan_ &&
                                     (wifi_ui_state_.scanning || !available_wifi_scan_results_ready_);
    const auto available_page_start = hide_available_rows ? 0 : get_page_start(available_wifi_page_);
    const auto available_count = hide_available_rows ?
                                 0 :
                                 get_page_visible_count(available_wifi_networks_.size(), available_wifi_page_);
    return sync_wifi_network_section(
               context,
               AVAILABLE_NETWORK_PARENT,
               "available",
               available_wifi_networks_,
               available_page_start,
               available_count,
               available_wifi_slot_count_
           );
}

std::expected<void, std::string> SettingsApp::sync_wifi_network_section(
    system::core::AppContext &context,
    std::string_view parent,
    std::string_view id_prefix,
    const std::vector<WifiNetworkState> &networks,
    size_t page_start,
    size_t visible_count,
    size_t &slot_count
)
{
    const auto slot_limit = get_wifi_list_page_size();
    const auto target_slot_count = std::min(visible_count, slot_limit);
#if BROOKESIA_APP_SETTINGS_ENABLE_MEMORY_TRACE
    BROOKESIA_LOGI(
        "[HeapTrace][settings.sync_wifi] parent(%1%) prefix(%2%) slot_count(%3%) visible_count(%4%) "
        "networks(%5%) page_start(%6%) page_size(%7%) slots_to_create(%8%)",
        std::string(parent), std::string(id_prefix), slot_count, target_slot_count, networks.size(),
        page_start, slot_limit, (target_slot_count > slot_count ? target_slot_count - slot_count : 0)
    );
#endif
    const auto make_slot_id = [&id_prefix](size_t index) {
        auto slot_id = std::string(id_prefix);
        slot_id += "_";
        slot_id += std::to_string(index);
        return slot_id;
    };

    // Destroy rows that are no longer needed instead of leaving them hidden. Keeping
    // slot_count aligned with the live GUI tree ensures a dynamic Wi-Fi screen rebuild
    // can never leave stale rows behind that later trigger "binding update target not
    // found" warnings when we try to hide them.
    for (size_t i = target_slot_count; i < slot_count; ++i) {
        const auto slot_id = make_slot_id(i);
        const auto instance_path = join_path(parent, slot_id);
        (void)context.gui().destroy_view(instance_path);
        wifi_instance_to_network_.erase(slot_id);
        std::erase(dynamic_wifi_paths_, instance_path);
    }
    if (slot_count > target_slot_count) {
        slot_count = target_slot_count;
    }
    const auto retained_slot_count = slot_count;

    // Ensure every row we are about to populate exists. create_view() is idempotent: an
    // existing instance simply reports "View already exists" and is kept in place, while a
    // row that was dropped by a screen rebuild is recreated. This self-heals any drift
    // between the app-side bookkeeping and the live GUI tree.
    slot_count = 0;
    for (size_t i = 0; i < target_slot_count; ++i) {
        if (i >= retained_slot_count) {
            const auto heap = ::esp_brookesia::lib_utils::MemoryProfiler::take_raw_heap_snapshot();
            if (heap.external_free != 0 && heap.external_free < WIFI_SLOT_MIN_FREE_PSRAM_BYTES) {
                BROOKESIA_LOGW(
                    "[HeapTrace][settings.wifi_guard] stop creating rows: low PSRAM "
                    "(free=%1% largest=%2% reserve=%3% created=%4% requested=%5%)",
                    heap.external_free, heap.external_largest,
                    static_cast<size_t>(WIFI_SLOT_MIN_FREE_PSRAM_BYTES), slot_count, target_slot_count
                );
                break;
            }
        }

        const auto slot_id = make_slot_id(i);
        const auto instance_path = join_path(parent, slot_id);
        auto created = context.gui().create_view(WIFI_TEMPLATE_ID, parent, slot_id);
        if (!created) {
            if (!created.error().starts_with("View already exists:")) {
                return std::unexpected(created.error());
            }
        } else if (std::find(dynamic_wifi_paths_.begin(), dynamic_wifi_paths_.end(), instance_path) ==
                   dynamic_wifi_paths_.end()) {
            dynamic_wifi_paths_.push_back(instance_path);
        }
        slot_count = i + 1;
    }

    std::vector<gui::BindingValueUpdate> updates;
    for (size_t i = 0; i < slot_count; ++i) {
        const auto slot_id = make_slot_id(i);
        const auto instance_path = join_path(parent, slot_id);

        const auto network_index = page_start + i;
        if (network_index >= networks.size()) {
            wifi_instance_to_network_.erase(slot_id);
            add_binding_update(updates, instance_path, "commonProps.hidden", "true");
            continue;
        }

        auto network = networks[network_index];
        network.parent = std::string(parent);
        network.id = slot_id;
        wifi_instance_to_network_[slot_id] = network;

        add_binding_update(updates, instance_path, "commonProps.hidden", "false");
        add_binding_update(updates, instance_path + "/text/ssid", "ssid", network.ssid);
        add_binding_update(updates, instance_path + "/text/detail", "detail", network.detail);
    }

    if (updates.empty()) {
        return {};
    }

    return context.gui().set_binding_values(updates);
}

void SettingsApp::clear_wifi_networks(system::core::AppContext &context)
{
    for (auto it = dynamic_wifi_paths_.rbegin(); it != dynamic_wifi_paths_.rend(); ++it) {
        (void)context.gui().destroy_view(*it);
    }
    dynamic_wifi_paths_.clear();
    saved_wifi_slot_count_ = 0;
    available_wifi_slot_count_ = 0;
    saved_wifi_page_ = 0;
    available_wifi_page_ = 0;
    wifi_instance_to_network_.clear();
    reset_available_wifi_scan_visibility();
}

void SettingsApp::update_wifi_refresh_animation(system::core::AppContext &context)
{
    if (!wifi_ui_state_.scanning || current_page_ != PAGE_WIFI) {
        if (wifi_refresh_animation_id_ != 0) {
            (void)context.gui().stop_animation(wifi_refresh_animation_id_);
            wifi_refresh_animation_id_ = 0;
        }
        (void)context.gui().set_binding_value(WIFI_SCAN_ICON_PATH, "angle", "0");
        return;
    }

    if (wifi_refresh_animation_id_ != 0) {
        return;
    }

    const gui::Animation animation{
        .id = {},
        .property = gui::AnimationProperty::Angle,
        .from_mode = gui::AnimationValueMode::Absolute,
        .from = 0,
        .to_mode = gui::AnimationValueMode::Absolute,
        .to = 360,
        .duration = 900,
        .easing = gui::AnimationEasing::Linear,
        .repeat = 10000,
    };
    auto result = context.gui().start_view_animation_with_result(WIFI_SCAN_ICON_PATH, animation);
    if (!result) {
        BROOKESIA_LOGW("Failed to start Wi-Fi refresh animation: %1%", result.error());
        return;
    }
    wifi_refresh_animation_id_ = result->subscription_id;
}

std::expected<void, std::string> SettingsApp::refresh_wifi_page_state(system::core::AppContext &context)
{
    const bool service_ready = wifi_ui_state_.service_ready;
    const bool wifi_content_hidden = !service_ready || !wifi_ui_state_.enabled;
    const bool connected_card_visible = is_wifi_connected_card_visible();
    auto connected_card_detail = localized_text(current_locale_, "wifi_not_connected");
    switch (wifi_ui_state_.connected_card_state) {
    case WifiConnectedCardState::Connecting:
        connected_card_detail = localized_text(current_locale_, "wifi_connecting");
        break;
    case WifiConnectedCardState::Connected:
        connected_card_detail = localized_text(current_locale_, "wifi_connected_detail");
        break;
    case WifiConnectedCardState::DisconnectedGrace:
        connected_card_detail = localized_text(current_locale_, "wifi_disconnected");
        break;
    case WifiConnectedCardState::Hidden:
        break;
    }

    std::vector<gui::BindingValueUpdate> updates;
    add_binding_update(
        updates,
        WIFI_STATUS_CARD_PATH,
        "opacity",
        std::to_string(service_ready ? ENABLED_OPACITY : DISABLED_OPACITY)
    );
    add_binding_update(updates, WIFI_SWITCH_PATH, "checked", wifi_ui_state_.enabled ? "true" : "false");
    add_binding_update(updates, WIFI_SWITCH_PATH, "disabled", service_ready ? "false" : "true");
    add_binding_update(updates, WIFI_CONTENT_PATH, "commonProps.hidden", wifi_content_hidden ? "true" : "false");
    const bool scan_enabled = service_ready && wifi_ui_state_.enabled && !wifi_ui_state_.scanning;
    add_binding_update(updates, WIFI_SCAN_BUTTON_PATH, "disabled", scan_enabled ? "false" : "true");
    add_binding_update(
        updates,
        WIFI_SCAN_BUTTON_PATH,
        "opacity",
        std::to_string(scan_enabled ? ENABLED_OPACITY : DISABLED_OPACITY)
    );
    add_binding_update(
        updates,
        WIFI_CONNECTED_CARD_PATH,
        "commonProps.hidden",
        connected_card_visible ? "false" : "true"
    );
    add_binding_update(
        updates,
        WIFI_CONNECTED_SSID_PATH,
        "labelProps.text",
        connected_card_visible ? wifi_ui_state_.connected_card_ssid : ""
    );
    add_binding_update(
        updates,
        WIFI_CONNECTED_DETAIL_PATH,
        "labelProps.text",
        connected_card_visible ? connected_card_detail : localized_text(current_locale_, "wifi_not_connected")
    );
    system::core::GuiBatchCommand command;
    command.type = system::core::GuiBatchCommand::Type::SetBindings;
    command.binding_updates = std::move(updates);
    auto batch_result = context.gui().execute_batch({command});
    if (!batch_result) {
        return std::unexpected(batch_result.error());
    }
    if (!batch_result->success) {
        for (const auto &command_result : batch_result->results) {
            if (!command_result.success) {
                return std::unexpected(
                           command_result.error_message.empty() ?
                           "Failed to refresh Wi-Fi page bindings" :
                           command_result.error_message
                       );
            }
        }
        return std::unexpected("Failed to refresh Wi-Fi page bindings");
    }
    update_wifi_refresh_animation(context);
    return {};
}

std::expected<void, std::string> SettingsApp::refresh_wifi_lists(system::core::AppContext &context)
{
    if (current_page_ != PAGE_WIFI) {
        return {};
    }

    const bool wifi_content_hidden = !wifi_ui_state_.service_ready || !wifi_ui_state_.enabled;
    const bool hide_available_rows_for_scan = available_wifi_rows_hidden_for_scan_ &&
            (wifi_ui_state_.scanning || !available_wifi_scan_results_ready_);
    if (wifi_content_hidden) {
        auto result = sync_wifi_network_section(
                          context,
                          SAVED_NETWORK_PARENT,
                          "saved",
                          saved_wifi_networks_,
                          0,
                          0,
                          saved_wifi_slot_count_
                      );
        if (!result) {
            return result;
        }
        result = sync_wifi_network_section(
                     context,
                     AVAILABLE_NETWORK_PARENT,
                     "available",
                     available_wifi_networks_,
                     0,
                     0,
                     available_wifi_slot_count_
                 );
        if (!result) {
            return result;
        }

        std::vector<gui::BindingValueUpdate> updates;
        add_binding_update(updates, SAVED_NETWORK_EMPTY_PATH, "commonProps.hidden", "true");
        add_binding_update(updates, AVAILABLE_NETWORK_EMPTY_PATH, "commonProps.hidden", "true");
        add_binding_update(updates, SAVED_NETWORK_PAGER_PATH, "commonProps.hidden", "true");
        add_binding_update(updates, AVAILABLE_NETWORK_PAGER_PATH, "commonProps.hidden", "true");
        return context.gui().set_binding_values(updates);
    }

    clamp_wifi_list_pages();
    auto result = populate_wifi_networks(context);
    if (!result) {
        return result;
    }

    std::vector<gui::BindingValueUpdate> updates;
    const auto add_pager_updates = [&updates](
                                       std::string_view pager_path,
                                       std::string_view prev_path,
                                       std::string_view label_path,
                                       std::string_view next_path,
                                       size_t page,
                                       size_t total_count,
                                       bool force_hide = false
    ) {
        const auto page_count = get_page_count(total_count);
        const auto show_pager = !force_hide && total_count > get_wifi_list_page_size();
        const auto can_prev = show_pager && page > 0;
        const auto can_next = show_pager && page + 1 < page_count;
        add_binding_update(
            updates,
            std::string(pager_path),
            "commonProps.hidden",
            show_pager ? "false" : "true"
        );
        add_binding_update(
            updates,
            std::string(prev_path),
            "commonProps.disabled",
            can_prev ? "false" : "true"
        );
        add_binding_update(
            updates,
            std::string(prev_path),
            "style.opacity",
            std::to_string(can_prev ? ENABLED_OPACITY : DISABLED_OPACITY)
        );
        add_binding_update(
            updates,
            std::string(label_path),
            "labelProps.text",
            make_page_text(page, page_count)
        );
        add_binding_update(
            updates,
            std::string(next_path),
            "commonProps.disabled",
            can_next ? "false" : "true"
        );
        add_binding_update(
            updates,
            std::string(next_path),
            "style.opacity",
            std::to_string(can_next ? ENABLED_OPACITY : DISABLED_OPACITY)
        );
    };
    add_binding_update(
        updates,
        SAVED_NETWORK_EMPTY_PATH,
        "commonProps.hidden",
        saved_wifi_networks_.empty() ? "false" : "true"
    );
    add_binding_update(
        updates,
        SAVED_NETWORK_EMPTY_PATH,
        "labelProps.text",
        localized_text(current_locale_, "wifi_no_saved_networks")
    );
    add_binding_update(
        updates,
        AVAILABLE_NETWORK_EMPTY_PATH,
        "commonProps.hidden",
        (hide_available_rows_for_scan || available_wifi_networks_.empty()) ? "false" : "true"
    );
    add_binding_update(
        updates,
        AVAILABLE_NETWORK_EMPTY_PATH,
        "labelProps.text",
        (hide_available_rows_for_scan || wifi_ui_state_.scanning) ? localized_text(current_locale_, "wifi_scanning") :
        localized_text(current_locale_, "wifi_no_available_networks")
    );
    add_pager_updates(
        SAVED_NETWORK_PAGER_PATH,
        SAVED_NETWORK_PAGER_PREV_PATH,
        SAVED_NETWORK_PAGER_LABEL_PATH,
        SAVED_NETWORK_PAGER_NEXT_PATH,
        saved_wifi_page_,
        saved_wifi_networks_.size()
    );
    add_pager_updates(
        AVAILABLE_NETWORK_PAGER_PATH,
        AVAILABLE_NETWORK_PAGER_PREV_PATH,
        AVAILABLE_NETWORK_PAGER_LABEL_PATH,
        AVAILABLE_NETWORK_PAGER_NEXT_PATH,
        available_wifi_page_,
        available_wifi_networks_.size(),
        hide_available_rows_for_scan
    );
    return context.gui().set_binding_values(updates);
}

std::expected<void, std::string> SettingsApp::start_wifi_scan(system::core::AppContext &context)
{
    return start_wifi_scan(context, true);
}

std::expected<void, std::string> SettingsApp::start_wifi_scan(
    system::core::AppContext &context,
    bool allow_interrupted_retry
)
{
    if (!wifi_ui_state_.service_ready) {
        return refresh_wifi_page_state(context);
    }
    if (!wifi_ui_state_.enabled) {
        return refresh_wifi_page_state(context);
    }
    if (wifi_ui_state_.scanning) {
        return refresh_wifi_page_state(context);
    }
    if (allow_interrupted_retry) {
        cancel_wifi_scan_retry_timer(context);
        wifi_scan_retry_remaining_ = WIFI_SCAN_INTERRUPTED_RETRY_COUNT;
    }
    const auto generation = wifi_operation_generation_.load();
    if (!saved_wifi_networks_loaded_) {
        request_saved_wifi_networks_refresh();
    }

    WifiHelper::ScanParams params;
    params.ap_count = WIFI_SCAN_AP_LIMIT;
    params.interval_ms = WIFI_SCAN_WINDOW_MS;
    params.timeout_ms = WIFI_SCAN_WINDOW_MS;

    wifi_ui_state_.scanning = true;
    wifi_scan_stop_after_first_result_ = true;
    available_wifi_rows_hidden_for_scan_ = true;
    available_wifi_scan_results_ready_ = false;
    auto refresh_result = refresh_wifi_page_state(context);
    if (!refresh_result) {
        wifi_ui_state_.scanning = false;
        wifi_scan_stop_after_first_result_ = false;
        wifi_scan_retry_remaining_ = 0;
        reset_available_wifi_scan_visibility();
        return refresh_result;
    }
    refresh_result = refresh_wifi_lists(context);
    if (!refresh_result) {
        wifi_ui_state_.scanning = false;
        wifi_scan_stop_after_first_result_ = false;
        wifi_scan_retry_remaining_ = 0;
        reset_available_wifi_scan_visibility();
        return refresh_result;
    }

    service::ServiceBase::FunctionResultHandler scan_start_handler = [this, generation](service::FunctionResult && result) {
        if (context_ == nullptr || generation != wifi_operation_generation_ || !wifi_ui_state_.enabled) {
            return;
        }
        if (result.success) {
            return;
        }
        BROOKESIA_LOGW("Failed to start Wi-Fi scan: %1%", result.error_message);
        wifi_ui_state_.scanning = false;
        wifi_scan_stop_after_first_result_ = false;
        wifi_scan_retry_remaining_ = 0;
        reset_available_wifi_scan_visibility();
        if (auto page_result = refresh_wifi_page_state(*context_); !page_result) {
            BROOKESIA_LOGW("Failed to refresh Wi-Fi page after scan start failure: %1%", page_result.error());
        }
        if (auto list_result = refresh_wifi_lists(*context_); !list_result) {
            BROOKESIA_LOGW("Failed to refresh Wi-Fi lists after scan start failure: %1%", list_result.error());
        }
    };
    service::ServiceBase::FunctionResultHandler set_params_handler = [this, scan_start_handler, generation](
        service::FunctionResult && result
    ) {
        if (context_ == nullptr || generation != wifi_operation_generation_ || !wifi_ui_state_.enabled) {
            return;
        }
        if (!result.success) {
            BROOKESIA_LOGW("Failed to set Wi-Fi scan params: %1%", result.error_message);
            wifi_ui_state_.scanning = false;
            wifi_scan_stop_after_first_result_ = false;
            wifi_scan_retry_remaining_ = 0;
            reset_available_wifi_scan_visibility();
            if (auto page_result = refresh_wifi_page_state(*context_); !page_result) {
                BROOKESIA_LOGW("Failed to refresh Wi-Fi page after scan params failure: %1%", page_result.error());
            }
            if (auto list_result = refresh_wifi_lists(*context_); !list_result) {
                BROOKESIA_LOGW("Failed to refresh Wi-Fi lists after scan params failure: %1%", list_result.error());
            }
            return;
        }

        if (!WifiHelper::call_function_async(
                    WifiHelper::FunctionId::TriggerScanStart,
                    make_wifi_ui_result_handler(wifi_ui_mailbox_, scan_start_handler))) {
            BROOKESIA_LOGW("Failed to submit Wi-Fi scan start request");
            if (context_ == nullptr) {
                return;
            }
            wifi_ui_state_.scanning = false;
            wifi_scan_stop_after_first_result_ = false;
            wifi_scan_retry_remaining_ = 0;
            reset_available_wifi_scan_visibility();
            if (auto page_result = refresh_wifi_page_state(*context_); !page_result) {
                BROOKESIA_LOGW("Failed to refresh Wi-Fi page after scan submit failure: %1%", page_result.error());
            }
            if (auto list_result = refresh_wifi_lists(*context_); !list_result) {
                BROOKESIA_LOGW("Failed to refresh Wi-Fi lists after scan submit failure: %1%", list_result.error());
            }
        }
    };
    if (!WifiHelper::call_function_async(
                WifiHelper::FunctionId::SetScanParams,
                BROOKESIA_DESCRIBE_TO_JSON(params).as_object(),
                make_wifi_ui_result_handler(wifi_ui_mailbox_, set_params_handler)
            )) {
        wifi_ui_state_.scanning = false;
        wifi_scan_stop_after_first_result_ = false;
        wifi_scan_retry_remaining_ = 0;
        reset_available_wifi_scan_visibility();
        auto rollback_result = refresh_wifi_page_state(context);
        if (!rollback_result) {
            return rollback_result;
        }
        rollback_result = refresh_wifi_lists(context);
        if (!rollback_result) {
            return rollback_result;
        }
        return std::unexpected("Failed to submit Wi-Fi scan params request");
    }

    return {};
}

void SettingsApp::cancel_wifi_scan_retry_timer(system::core::AppContext &context)
{
    if (pending_wifi_scan_retry_timer_id_ == system::core::INVALID_TIMER_ID) {
        pending_wifi_scan_retry_generation_ = 0;
        return;
    }
    if (!context.timer().stop(pending_wifi_scan_retry_timer_id_)) {
        BROOKESIA_LOGW(
            "Failed to stop pending Settings Wi-Fi scan retry timer: %1%",
            pending_wifi_scan_retry_timer_id_
        );
    }
    pending_wifi_scan_retry_timer_id_ = system::core::INVALID_TIMER_ID;
    pending_wifi_scan_retry_generation_ = 0;
}

void SettingsApp::schedule_wifi_scan_retry(system::core::AppContext &context)
{
    if (wifi_scan_retry_remaining_ == 0 || pending_wifi_scan_retry_timer_id_ != system::core::INVALID_TIMER_ID) {
        return;
    }

    --wifi_scan_retry_remaining_;
    pending_wifi_scan_retry_generation_ = wifi_operation_generation_.load();
    auto timer = context.timer().start_delayed(WIFI_SCAN_RETRY_TIMER_NAME, WIFI_SCAN_RETRY_DELAY_MS);
    if (!timer) {
        pending_wifi_scan_retry_generation_ = 0;
        BROOKESIA_LOGW("Failed to schedule Settings Wi-Fi scan retry: %1%", timer.error());
        return;
    }
    pending_wifi_scan_retry_timer_id_ = *timer;
}

void SettingsApp::stop_wifi_scan_after_first_result()
{
    if (!wifi_scan_stop_after_first_result_) {
        return;
    }
    wifi_scan_stop_after_first_result_ = false;

    service::ServiceBase::FunctionResultHandler scan_stop_handler = [](service::FunctionResult && result) {
        if (result.success) {
            return;
        }
        BROOKESIA_LOGW("Failed to stop one-shot Wi-Fi scan: %1%", result.error_message);
    };
    if (!WifiHelper::call_function_async(WifiHelper::FunctionId::TriggerScanStop, scan_stop_handler)) {
        BROOKESIA_LOGW("Failed to submit one-shot Wi-Fi scan stop request");
    }
}

std::expected<void, std::string> SettingsApp::connect_wifi_network(
    system::core::AppContext &context,
    const WifiNetworkState &network,
    std::string_view password
)
{
    if (!wifi_ui_state_.service_ready) {
        return std::unexpected(settings_uses_gsp_backend() ?
                               localized_text(current_locale_, "wifi.unavailable_retry") :
                               localized_text(current_locale_, "wifi_service_unavailable"));
    }
    if (network.ssid.empty()) {
        return std::unexpected("Wi-Fi SSID is empty");
    }
    if (settings_uses_gsp_backend() && network.enterprise) {
        return std::unexpected(localized_text(current_locale_, "wifi.enterprise_error"));
    }

    invalidate_wifi_connect_operation(context);
    if (!settings_uses_gsp_backend()) {
        auto timer = context.timer().start_periodic(WIFI_CONNECT_TIMER_NAME, WIFI_CONNECT_POLL_INTERVAL_MS);
        if (!timer) {
            return std::unexpected(timer.error());
        }
        wifi_connect_timer_id_ = *timer;
    }
    auto operation = std::make_shared<WifiConnectOperation>();
    operation->ssid = network.ssid;
    operation->password = password;
    operation->deadline = SteadyClock::now() + std::chrono::milliseconds(WIFI_CONNECT_TIMEOUT_MS);
    operation->phase = WifiConnectOperation::Phase::Saving;
    wifi_connect_operation_ = operation;
    const std::weak_ptr<WifiConnectOperation> weak_operation = operation;
    const service::EventRegistry::SignalSlot operation_event_handler =
    [weak_operation](const std::string &, const service::EventItemMap & items) {
        const auto *event = std::get_if<std::string>(find_event_item(items, "Event"));
        if (!event) {
            return;
        }
        if (auto active = weak_operation.lock()) {
            boost::lock_guard lock(active->mutex);
            if (*event == BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralEvent::Stopped) ||
                    *event == BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralEvent::Deinited)) {
                active->valid = false;
                std::fill(active->password.begin(), active->password.end(), '\0');
                active->password.clear();
            } else if (active->valid &&
                       *event == BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralEvent::Connected)) {
                ++active->connected_event_epoch;
            }
        }
    };
    operation->connected_event_connection = WifiHelper::subscribe_event(
                                            WifiHelper::EventId::GeneralEventHappened, operation_event_handler);
    if (!operation->connected_event_connection.connected()) {
        boost::lock_guard lock(operation->mutex);
        std::fill(operation->password.begin(), operation->password.end(), '\0');
        operation->password.clear();
        operation->phase = WifiConnectOperation::Phase::Failed;
        operation->error = "wifi.observe_failed";
    }
    wifi_page_.connect_pending = true;
    wifi_page_.connect_ssid = network.ssid;
    wifi_page_.error.clear();
    ++wifi_operation_generation_;
    cancel_wifi_scan_retry_timer(context);
    wifi_scan_retry_remaining_ = 0;
    wifi_ui_state_.connecting = true;
    wifi_ui_state_.enabled = true;
    wifi_scan_stop_after_first_result_ = false;
    cancel_wifi_connected_hide_timer(context);
    show_wifi_connected_card(WifiConnectedCardState::Connecting, network.ssid);

    const auto set_target_handler = [operation](service::FunctionResult && target) {
        boost::lock_guard lock(operation->mutex);
        if (!operation->valid) {
            return;
        }
        if (!target.success) {
            operation->phase = WifiConnectOperation::Phase::Failed;
            operation->error = "wifi.set_target_failed";
            return;
        }
        operation->phase = WifiConnectOperation::Phase::SendingConnect;
        const auto connect_handler = [operation](service::FunctionResult && connect) {
            boost::lock_guard connect_lock(operation->mutex);
            if (!operation->valid) {
                return;
            }
            operation->phase = connect.success ? WifiConnectOperation::Phase::WaitingForConnection :
                               WifiConnectOperation::Phase::Failed;
            if (!connect.success) {
                operation->error = "wifi.start_failed";
            }
        };
        // Keep the validity lock through submission so cancellation blocks later Connect submissions.
        operation->connected_event_armed = true;
        operation->connect_start_epoch = operation->connected_event_epoch;
        if (!WifiHelper::call_function_async(
                    WifiHelper::FunctionId::TriggerGeneralAction,
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralAction::Connect), connect_handler)) {
            operation->phase = WifiConnectOperation::Phase::Failed;
            operation->error = "wifi.submit_connect_failed";
        }
    };
    const auto save_handler = [operation, set_target_handler](service::FunctionResult && result) {
        boost::lock_guard lock(operation->mutex);
        if (!operation->valid) {
            return;
        }
        if (!result.success) {
            std::fill(operation->password.begin(), operation->password.end(), '\0');
            operation->password.clear();
            operation->phase = WifiConnectOperation::Phase::Failed;
            operation->error = "wifi.preference_save_failed";
            return;
        }
        operation->phase = WifiConnectOperation::Phase::SettingTarget;
        auto password_copy = operation->password;
        std::fill(operation->password.begin(), operation->password.end(), '\0');
        operation->password.clear();
        // Keep the validity lock through submission so cancellation blocks later SetConnectAp submissions.
        const bool submitted = WifiHelper::call_function_async(
                                   WifiHelper::FunctionId::SetConnectAp,
                                   operation->ssid, password_copy, set_target_handler);
        std::fill(password_copy.begin(), password_copy.end(), '\0');
        password_copy.clear();
        if (!submitted) {
            operation->phase = WifiConnectOperation::Phase::Failed;
            operation->error = "wifi.submit_target_failed";
        }
    };
    auto save_result = operation->connected_event_connection.connected() ?
                       submit_wifi_enabled_preference_save(true, save_handler) :
                       std::expected<void, std::string>(std::unexpected("Wi-Fi event subscription failed"));
    if (!save_result && operation->connected_event_connection.connected()) {
        {
            boost::lock_guard lock(operation->mutex);
            std::fill(operation->password.begin(), operation->password.end(), '\0');
            operation->password.clear();
            operation->phase = WifiConnectOperation::Phase::Failed;
            operation->error = "wifi.preference_save_failed";
        }
        // The page/timer consumes this failure just like an asynchronous result.
    }
    if (settings_uses_gsp_backend()) {
        return current_page_ == PAGE_WIFI ? render_wifi_page(context) : std::expected<void, std::string>{};
    }
    auto refresh_result = refresh_wifi_page_state(context);
    if (!refresh_result) {
        return refresh_result;
    }
    return refresh_wifi_summary_state(context);
}

void SettingsApp::invalidate_wifi_connect_operation(system::core::AppContext &context)
{
    auto operation = std::move(wifi_connect_operation_);
    if (operation) {
        {
            boost::lock_guard lock(operation->mutex);
            operation->valid = false;
            std::fill(operation->password.begin(), operation->password.end(), '\0');
            operation->password.clear();
        }
        operation->connected_event_connection.disconnect();
    }
    if (wifi_connect_timer_id_ != system::core::INVALID_TIMER_ID) {
        (void)context.timer().stop(wifi_connect_timer_id_);
        wifi_connect_timer_id_ = system::core::INVALID_TIMER_ID;
    }
    wifi_page_.connect_pending = false;
    wifi_page_.connect_ssid.clear();
}

bool SettingsApp::sync_wifi_connect_operation(system::core::AppContext &context)
{
    auto operation = wifi_connect_operation_;
    if (!operation) {
        return false;
    }
    WifiConnectOperation::Phase phase;
    std::string target_ssid;
    std::string error;
    bool valid = false;
    bool fresh_connected_event = false;
    SteadyTimePoint deadline;
    {
        boost::lock_guard lock(operation->mutex);
        valid = operation->valid;
        phase = operation->phase;
        target_ssid = operation->ssid;
        error = operation->error;
        fresh_connected_event = operation->connected_event_armed &&
                                operation->connected_event_epoch > operation->connect_start_epoch;
        deadline = operation->deadline;
    }
    if (!valid) {
        // Service callbacks invalidate the operation; the App task releases its UI ownership.
        invalidate_wifi_connect_operation(context);
        return true;
    }
    const bool connected = phase == WifiConnectOperation::Phase::WaitingForConnection && fresh_connected_event &&
                           wifi_ui_state_.general_state == "Connected" &&
                           wifi_ui_state_.connected_ssid == target_ssid;
    if (connected) {
        invalidate_wifi_connect_operation(context);
        wifi_page_.error.clear();
        wifi_ui_state_.connecting = false;
        std::fill(wifi_connect_password_.begin(), wifi_connect_password_.end(), '\0');
        wifi_connect_password_.clear();
        if (!settings_uses_gsp_backend()) {
            selected_wifi_network_.reset();
            request_saved_wifi_networks_refresh();
        }
        return true;
    }
    if (phase != WifiConnectOperation::Phase::Failed && SteadyClock::now() < deadline) {
        return false;
    }
    invalidate_wifi_connect_operation(context);
    wifi_ui_state_.connecting = false;
    hide_wifi_connected_card();
    wifi_page_.error = localized_text(current_locale_, error.empty() ? "wifi.timeout" : error);
    return true;
}

std::expected<void, std::string> SettingsApp::disconnect_wifi(system::core::AppContext &context)
{
    invalidate_wifi_connect_operation(context);
    cancel_wifi_keyboard_if_active(context);
    std::fill(wifi_connect_password_.begin(), wifi_connect_password_.end(), '\0');
    wifi_connect_password_.clear();
    selected_wifi_network_.reset();
    const auto generation = ++wifi_operation_generation_;
    cancel_wifi_scan_retry_timer(context);
    wifi_scan_retry_remaining_ = 0;
    if (!wifi_ui_state_.service_ready) {
        return refresh_wifi_page_state(context);
    }

    const auto save_handler = [this, generation](service::FunctionResult && result) {
        if (context_ == nullptr || generation != wifi_operation_generation_) {
            return;
        }
        if (result.success) {
            return;
        }
        BROOKESIA_LOGW("Failed to save Settings Wi-Fi disabled preference: %1%", result.error_message);
        ++wifi_operation_generation_;
        wifi_ui_state_.enabled = true;
        if (auto page_result = refresh_wifi_page_state(*context_); !page_result) {
            BROOKESIA_LOGW("Failed to restore Wi-Fi page after preference save failure: %1%", page_result.error());
        }
        if (auto list_result = refresh_wifi_lists(*context_); !list_result) {
            BROOKESIA_LOGW("Failed to restore Wi-Fi lists after preference save failure: %1%", list_result.error());
        }
        if (auto summary_result = refresh_wifi_summary_state(*context_); !summary_result) {
            BROOKESIA_LOGW(
                "Failed to restore Wi-Fi summary after preference save failure: %1%",
                summary_result.error()
            );
        }
        apply_wifi_enabled_preference_to_service();
    };
    auto save_result = submit_wifi_enabled_preference_save(
                           false, make_wifi_ui_result_handler(wifi_ui_mailbox_, save_handler));
    if (!save_result) {
        if (auto refresh_result = refresh_wifi_page_state(context); !refresh_result) {
            BROOKESIA_LOGW(
                "Failed to restore Wi-Fi page after preference save failure: %1%",
                refresh_result.error()
            );
        }
        return save_result;
    }
    available_wifi_networks_.clear();
    last_wifi_scan_ap_infos_.clear();
    reset_available_wifi_scan_visibility();
    wifi_scan_stop_after_first_result_ = false;
    wifi_ui_state_.enabled = false;
    wifi_ui_state_.scanning = false;
    wifi_ui_state_.connecting = false;
    wifi_ui_state_.general_state.clear();
    wifi_ui_state_.connected_ssid.clear();
    cancel_wifi_connected_hide_timer(context);
    hide_wifi_connected_card();
    auto result = refresh_wifi_page_state(context);
    if (!result) {
        return result;
    }
    result = refresh_wifi_lists(context);
    if (!result) {
        return result;
    }
    result = refresh_wifi_summary_state(context);
    if (!result) {
        return result;
    }

    auto scan_stop_handler = [](service::FunctionResult && result) {
        if (result.success) {
            return;
        }
        BROOKESIA_LOGW("Failed to stop Wi-Fi scan: %1%", result.error_message);
    };
    if (!WifiHelper::call_function_async(WifiHelper::FunctionId::TriggerScanStop, scan_stop_handler)) {
        BROOKESIA_LOGW("Failed to submit Wi-Fi scan stop request");
    }

    auto stop_handler = [this, generation](service::FunctionResult && result) {
        if (generation != wifi_operation_generation_) {
            return;
        }
        if (result.success) {
            return;
        }
        BROOKESIA_LOGW("Failed to stop Wi-Fi: %1%", result.error_message);
    };
    if (!WifiHelper::call_function_async(
                WifiHelper::FunctionId::TriggerGeneralAction,
                BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralAction::Stop),
                make_wifi_ui_result_handler(wifi_ui_mailbox_, stop_handler)
            )) {
        BROOKESIA_LOGW("Failed to submit Wi-Fi stop request");
    }
    return {};
}

std::expected<void, std::string> SettingsApp::disconnect_current_wifi(system::core::AppContext &context)
{
    invalidate_wifi_connect_operation(context);
    if (!wifi_ui_state_.service_ready || !wifi_ui_state_.enabled) {
        return refresh_wifi_page_state(context);
    }

    if (settings_uses_gsp_backend()) {
        auto handler = [](service::FunctionResult && result) {
            if (!result.success) {
                BROOKESIA_LOGW("Failed to disconnect Wi-Fi: %1%", result.error_message);
            }
        };
        if (!WifiHelper::call_function_async(
                    WifiHelper::FunctionId::TriggerGeneralAction,
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralAction::Disconnect), handler)) {
            return std::unexpected("Failed to submit Wi-Fi disconnect request");
        }
        wifi_page_.next_state_read = {};
        return {};
    }

    const auto disconnected_handler = [this](service::FunctionResult && result) {
        if (!result.success) {
            BROOKESIA_LOGW("Failed to disconnect Wi-Fi: %1%", result.error_message);
            if (context_ == nullptr) {
                return;
            }
            if (auto state_result = refresh_wifi_service_state(*context_); !state_result) {
                BROOKESIA_LOGW("Failed to refresh Wi-Fi state after failed disconnect: %1%", state_result.error());
            }
            return;
        }

        const auto ssid = !wifi_ui_state_.connected_card_ssid.empty() ?
                          wifi_ui_state_.connected_card_ssid :
                          wifi_ui_state_.connected_ssid;
        wifi_ui_state_.connecting = false;
        wifi_ui_state_.connected_ssid.clear();
        if (!ssid.empty()) {
            show_wifi_connected_card(WifiConnectedCardState::DisconnectedGrace, ssid);
        } else {
            hide_wifi_connected_card();
        }
        if (context_ == nullptr) {
            return;
        }
        if (!ssid.empty()) {
            schedule_wifi_connected_hide_timer(*context_);
        }
        if (auto page_result = refresh_wifi_page_state(*context_); !page_result) {
            BROOKESIA_LOGW("Failed to refresh Wi-Fi page after disconnect: %1%", page_result.error());
        }
        if (auto list_result = refresh_wifi_lists(*context_); !list_result) {
            BROOKESIA_LOGW("Failed to refresh Wi-Fi lists after disconnect: %1%", list_result.error());
        }
        if (auto summary_result = refresh_wifi_summary_state(*context_); !summary_result) {
            BROOKESIA_LOGW("Failed to refresh Wi-Fi summary after disconnect: %1%", summary_result.error());
        }
    };

    if (!WifiHelper::call_function_async(
                WifiHelper::FunctionId::TriggerGeneralAction,
                BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralAction::Disconnect),
                make_wifi_ui_result_handler(wifi_ui_mailbox_, disconnected_handler)
            )) {
        return std::unexpected("Failed to submit Wi-Fi disconnect request");
    }
    return {};
}

void SettingsApp::handle_wifi_select_event(const gui::Event &event)
{
    if (context_ == nullptr) {
        return;
    }

    std::string instance_id;
    if (event.path.starts_with(SAVED_NETWORK_PARENT)) {
        instance_id = extract_child_instance_id(SAVED_NETWORK_PARENT, event.path);
    } else if (event.path.starts_with(AVAILABLE_NETWORK_PARENT)) {
        instance_id = extract_child_instance_id(AVAILABLE_NETWORK_PARENT, event.path);
    } else {
        BROOKESIA_LOGW("Ignore Wi-Fi network action from unexpected path: %1%", event.path);
        return;
    }

    auto it = wifi_instance_to_network_.find(instance_id);
    if (it == wifi_instance_to_network_.end()) {
        BROOKESIA_LOGW("Ignore Wi-Fi network action from unknown item: %1%", event.path);
        return;
    }
    if (instance_id == wifi_forget_click_suppression_id_ &&
            it->second.ssid == wifi_forget_click_suppression_ssid_) {
        wifi_forget_click_suppression_id_.clear();
        wifi_forget_click_suppression_ssid_.clear();
        return;
    }
    if (instance_id == wifi_forget_click_suppression_id_) {
        wifi_forget_click_suppression_id_.clear();
        wifi_forget_click_suppression_ssid_.clear();
    }
    if (it->second.saved &&
            wifi_ui_state_.general_state == BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralState::Connected) &&
            it->second.ssid == wifi_ui_state_.connected_ssid) {
        BROOKESIA_LOGD("Ignore saved Wi-Fi AP click because it is already connected: %1%", it->second.ssid);
        return;
    }

    std::expected<void, std::string> result = {};
    if (it->second.saved || !it->second.locked) {
        result = connect_wifi_network(*context_, it->second, it->second.password);
    } else {
        result = open_wifi_connect(*context_, it->second);
    }
    if (!result) {
        BROOKESIA_LOGW("Failed to handle Wi-Fi network '%1%': %2%", it->second.ssid, result.error());
    }
}

void SettingsApp::handle_wifi_forget_event(const gui::Event &event)
{
    if (context_ == nullptr) {
        return;
    }
    if (!event.path.starts_with(SAVED_NETWORK_PARENT)) {
        return;
    }

    auto instance_id = extract_child_instance_id(SAVED_NETWORK_PARENT, event.path);
    auto it = wifi_instance_to_network_.find(instance_id);
    if (it == wifi_instance_to_network_.end()) {
        BROOKESIA_LOGW("Ignore saved Wi-Fi forget action from unknown item: %1%", event.path);
        return;
    }
    if (!it->second.saved) {
        return;
    }

    auto network = it->second;
    wifi_forget_click_suppression_id_ = instance_id;
    wifi_forget_click_suppression_ssid_ = network.ssid;
    const auto forget_handler = [this, ssid = network.ssid](service::FunctionResult && result) {
        if (!result.success) {
            BROOKESIA_LOGW("Failed to forget saved Wi-Fi AP '%1%': %2%", ssid, result.error_message);
            return;
        }
        if (context_ == nullptr) {
            return;
        }

        saved_wifi_networks_.erase(
            std::remove_if(
                saved_wifi_networks_.begin(),
                saved_wifi_networks_.end(),
        [&ssid](const auto & saved_network) {
            return saved_network.ssid == ssid;
        }
            ),
        saved_wifi_networks_.end()
        );
        for (auto item = wifi_instance_to_network_.begin(); item != wifi_instance_to_network_.end();) {
            if (item->second.saved && item->second.ssid == ssid) {
                item = wifi_instance_to_network_.erase(item);
            } else {
                ++item;
            }
        }
        rebuild_available_wifi_networks();
        if (auto list_result = refresh_wifi_lists(*context_); !list_result) {
            BROOKESIA_LOGW("Failed to refresh Wi-Fi lists after forgetting AP: %1%", list_result.error());
        }
    };

    if (!WifiHelper::call_function_async(
                WifiHelper::FunctionId::RemoveConnectedAp,
                network.ssid,
                make_wifi_ui_result_handler(wifi_ui_mailbox_, forget_handler)
            )) {
        wifi_forget_click_suppression_id_.clear();
        wifi_forget_click_suppression_ssid_.clear();
        BROOKESIA_LOGW("Failed to submit saved Wi-Fi AP forget request: %1%", network.ssid);
    }
}

std::expected<void, std::string> SettingsApp::open_wifi_connect(
    system::core::AppContext &context,
    WifiNetworkState network
)
{
    selected_wifi_network_ = std::move(network);
    std::fill(wifi_connect_password_.begin(), wifi_connect_password_.end(), '\0');
    wifi_connect_password_.clear();
    wifi_page_.error.clear();
    if (settings_uses_gsp_backend()) {
        auto shown = surface_->show_page(context, PAGE_WIFI_CONNECT);
        if (!shown) {
            selected_wifi_network_.reset();
            return shown;
        }
        current_page_ = PAGE_WIFI_CONNECT;
        return refresh_wifi_connect_state(context);
    }
    auto result = surface_->show_page(context, PAGE_WIFI_CONNECT);
    if (!result) {
        return result;
    }
    current_page_ = PAGE_WIFI_CONNECT;
    clear_wifi_networks(context);
    result = refresh_header(context);
    if (!result) {
        return result;
    }
    return refresh_wifi_connect_state(context);
}

std::expected<void, std::string> SettingsApp::refresh_wifi_connect_state(system::core::AppContext &context)
{
    const bool gsp_backend = settings_uses_gsp_backend();
    const auto password_empty_text = gsp_backend ? localized_text(current_locale_, "wifi.enter_password") :
                                     localized_text(current_locale_, "wifi_password_placeholder");
    const auto status_text = wifi_connect_password_.empty() ?
                             (gsp_backend ? localized_text(current_locale_, "wifi.enter_password_to_connect") :
                              localized_text(current_locale_, "wifi_password_required")) :
                             localized_text(current_locale_, "wifi_ready_to_connect");
    const bool connect_enabled = selected_wifi_network_.has_value() && !wifi_connect_password_.empty();
    WifiConnectView view;
    view.ssid = selected_wifi_network_ ? selected_wifi_network_->ssid : "";
    view.detail = selected_wifi_network_ ? selected_wifi_network_->detail : "";
    view.password_summary = make_password_summary(wifi_connect_password_, password_empty_text);
    view.status = wifi_page_.error.empty() ? status_text : wifi_page_.error;
    view.password_enabled = selected_wifi_network_.has_value();
    view.connect_enabled = connect_enabled;

    if (gsp_backend) {
        if (selected_wifi_network_) {
            const auto &network = *selected_wifi_network_;
            view.detail = localized_text(current_locale_, network.enterprise ? "wifi.enterprise" :
                                         network.locked ? "wifi.secured" : "wifi.open") +
                          "   " + localized_text(current_locale_, "wifi.signal") + " " +
                          std::to_string(network.signal_level) + "/4   " +
                          std::to_string(network.rssi) + " dBm   " + network.band;
        }
        const bool connected = selected_wifi_network_ && wifi_ui_state_.general_state == "Connected" &&
                               wifi_ui_state_.connected_ssid == selected_wifi_network_->ssid &&
                               !wifi_page_.connect_pending;
        view.status = !wifi_page_.error.empty() ? wifi_page_.error :
                      wifi_page_.connect_pending ? localized_text(current_locale_, "wifi.connecting_network") :
                      connected ? localized_text(current_locale_, "wifi.connected_network") : status_text;
        view.password_enabled = selected_wifi_network_ && !wifi_page_.connect_pending && !connected;
        view.connect_enabled = connect_enabled && view.password_enabled;
    }
    return surface_->set_wifi_connect(context, view);
}

std::expected<void, std::string> SettingsApp::refresh_wifi_summary_state(system::core::AppContext &context)
{
    const bool connected = wifi_ui_state_.service_ready && wifi_ui_state_.enabled &&
                           wifi_ui_state_.general_state ==
                           BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralState::Connected) &&
                           !wifi_ui_state_.connected_ssid.empty();
    connected_wifi_ssid_ = connected ? wifi_ui_state_.connected_ssid : "";
    const std::string summary = connected ? connected_wifi_ssid_ :
                                localized_text(current_locale_, "wifi_not_connected");
    std::vector<gui::BindingValueUpdate> updates;
    add_binding_update(updates, HOME_WIFI_VALUE_PATH, "labelProps.text", summary);
    return context.gui().set_binding_values(updates);
}

std::expected<void, std::string> SettingsApp::show_wifi_password_keyboard(system::core::AppContext &context)
{
    if (!selected_wifi_network_.has_value() || wifi_page_.connect_pending ||
            (settings_uses_gsp_backend() && wifi_ui_state_.general_state == "Connected" &&
             wifi_ui_state_.connected_ssid == selected_wifi_network_->ssid)) {
        return std::unexpected(settings_uses_gsp_backend() ? localized_text(current_locale_, "wifi.select_first") :
                               localized_text(current_locale_, "wifi_no_network_selected"));
    }
    if (wifi_keyboard_request_id_ != system::core::INVALID_KEYBOARD_REQUEST_ID) {
        return {};
    }

    auto options = system::core::KeyboardRequestOptions{
        .title = settings_uses_gsp_backend() ?
                 localized_text(current_locale_, "wifi.password_title") + ": " + selected_wifi_network_->ssid :
                 selected_wifi_network_->ssid,
        .placeholder = settings_uses_gsp_backend() ? localized_text(current_locale_, "wifi.enter_password") :
                       localized_text(current_locale_, "wifi_password_placeholder"),
        .initial_text = wifi_connect_password_,
        .max_length = 64,
        .password = true,
        .mode = "text",
        .allowed_modes = {"text", "upper", "number", "special"},
    };
    auto completion = [this](const system::core::KeyboardResult & result) {
        handle_wifi_keyboard_result(result);
    };
    auto request = context.system_service().show_keyboard(std::move(options), std::move(completion));
    if (!request) {
        return std::unexpected(request.error());
    }
    wifi_keyboard_request_id_ = *request;
    return {};
}

void SettingsApp::handle_wifi_keyboard_result(const system::core::KeyboardResult &result)
{
    if (wifi_keyboard_request_id_ == system::core::INVALID_KEYBOARD_REQUEST_ID ||
            result.request_id != wifi_keyboard_request_id_) {
        return;
    }

    wifi_keyboard_request_id_ = system::core::INVALID_KEYBOARD_REQUEST_ID;
    if (!result.confirmed) {
        return;
    }

    if (current_page_ != PAGE_WIFI_CONNECT || !selected_wifi_network_) {
        return;
    }
    std::fill(wifi_connect_password_.begin(), wifi_connect_password_.end(), '\0');
    wifi_connect_password_ = result.text;
    wifi_page_.error.clear();
    if (context_ == nullptr) {
        return;
    }
    auto refresh_result = refresh_wifi_connect_state(*context_);
    if (!refresh_result) {
        BROOKESIA_LOGW("Failed to refresh Wi-Fi password state: %1%", refresh_result.error());
    }
}

std::expected<void, std::string> SettingsApp::submit_wifi_connect(system::core::AppContext &context)
{
    if (!selected_wifi_network_.has_value()) {
        return std::unexpected(settings_uses_gsp_backend() ? localized_text(current_locale_, "wifi.select_first") :
                               localized_text(current_locale_, "wifi_no_network_selected"));
    }
    if (wifi_connect_password_.empty()) {
        return refresh_wifi_connect_state(context);
    }

    auto result = connect_wifi_network(context, *selected_wifi_network_, wifi_connect_password_);
    std::fill(wifi_connect_password_.begin(), wifi_connect_password_.end(), '\0');
    wifi_connect_password_.clear();
    if (settings_uses_gsp_backend()) {
        if (!result) {
            wifi_page_.error = localized_text(current_locale_, "wifi.start_failed");
        }
        return refresh_wifi_connect_state(context);
    }
    if (!result) {
        wifi_page_.error = localized_text(current_locale_, "wifi_connect_failed");
        if (auto status_result = refresh_wifi_connect_state(context); !status_result) {
            BROOKESIA_LOGW("Failed to refresh Wi-Fi connect failure status: %1%", status_result.error());
        }
        return result;
    }
    result = surface_->show_page(context, PAGE_WIFI);
    if (!result) {
        return result;
    }
    current_page_ = PAGE_WIFI;
    result = refresh_header(context);
    if (!result) {
        return result;
    }
    result = refresh_wifi_page_state(context);
    if (!result) {
        return result;
    }
    result = refresh_wifi_lists(context);
    if (!result) {
        return result;
    }
    result = refresh_wifi_summary_state(context);
    if (!result) {
        return result;
    }
    schedule_wifi_connected_card_scroll(context);
    return {};
}

void SettingsApp::handle_wifi_general_event(std::string_view event, bool unexpected)
{
    if (context_ == nullptr) {
        return;
    }
    if (unexpected) {
        BROOKESIA_LOGW("Wi-Fi service reported unexpected event: %1%", event);
    }
    const bool stopped = event == BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralEvent::Stopped) ||
                         event == BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralEvent::Deinited);
    const bool service_unavailable = !wifi_service_binding_.is_valid() || !WifiHelper::is_running();
    if (stopped || service_unavailable) {
        invalidate_wifi_connect_operation(*context_);
        ++wifi_operation_generation_;
        cancel_wifi_keyboard_if_active(*context_);
        std::fill(wifi_connect_password_.begin(), wifi_connect_password_.end(), '\0');
        wifi_connect_password_.clear();
        selected_wifi_network_.reset();
        wifi_ui_state_.enabled = false;
        wifi_ui_state_.connecting = false;
        wifi_ui_state_.connected_ssid.clear();
        if (service_unavailable) {
            wifi_ui_state_.service_ready = false;
        }
        if (current_page_ == PAGE_WIFI_CONNECT) {
            if (auto result = on_action(*context_, ACTION_BACK_WIFI_CONNECT); !result) {
                BROOKESIA_LOGW("Failed to leave Wi-Fi connect page after Wi-Fi stopped: %1%", result.error());
            }
        }
    }
    if (!wifi_ui_state_.enabled) {
        wifi_ui_state_.scanning = false;
        wifi_ui_state_.connecting = false;
        wifi_ui_state_.connected_ssid.clear();
        hide_wifi_connected_card();
        if (is_wifi_started_event(event)) {
            BROOKESIA_LOGW("Wi-Fi reported '%1%' while Settings Wi-Fi is disabled; request stop", event);
            auto stop_handler = [](service::FunctionResult && result) {
                if (result.success) {
                    return;
                }
                BROOKESIA_LOGW("Failed to stop Wi-Fi after disabled event: %1%", result.error_message);
            };
            if (!WifiHelper::call_function_async(
                        WifiHelper::FunctionId::TriggerGeneralAction,
                        BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralAction::Stop),
                        stop_handler
                    )) {
                BROOKESIA_LOGW("Failed to submit Wi-Fi stop request after disabled event");
            }
        }
        if (auto page_result = refresh_wifi_page_state(*context_); !page_result) {
            BROOKESIA_LOGW("Failed to refresh disabled Wi-Fi page after event '%1%': %2%", event, page_result.error());
        }
        if (auto list_result = refresh_wifi_lists(*context_); !list_result) {
            BROOKESIA_LOGW("Failed to refresh disabled Wi-Fi lists after event '%1%': %2%", event, list_result.error());
        }
        if (auto summary_result = refresh_wifi_summary_state(*context_); !summary_result) {
            BROOKESIA_LOGW(
                "Failed to refresh disabled Wi-Fi summary after event '%1%': %2%",
                event,
                summary_result.error()
            );
        }
        return;
    }
    if (event == BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralEvent::Connected)) {
        cancel_wifi_connected_hide_timer(*context_);
        const auto ssid = !wifi_ui_state_.connected_ssid.empty() ?
                          wifi_ui_state_.connected_ssid :
                          wifi_ui_state_.connected_card_ssid;
        if (!ssid.empty()) {
            show_wifi_connected_card(WifiConnectedCardState::Connected, ssid);
        }
    } else if (event == BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralEvent::Disconnected)) {
        const auto ssid = !wifi_ui_state_.connected_card_ssid.empty() ?
                          wifi_ui_state_.connected_card_ssid :
                          wifi_ui_state_.connected_ssid;
        wifi_ui_state_.connecting = false;
        wifi_ui_state_.connected_ssid.clear();
        if (!ssid.empty()) {
            show_wifi_connected_card(WifiConnectedCardState::DisconnectedGrace, ssid);
            schedule_wifi_connected_hide_timer(*context_);
        } else {
            hide_wifi_connected_card();
        }
    }
    auto result = refresh_wifi_service_state(*context_);
    if (!result) {
        BROOKESIA_LOGW("Failed to refresh Wi-Fi state after event '%1%': %2%", event, result.error());
        return;
    }
    if (event == BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralEvent::Connected)) {
        selected_wifi_network_.reset();
        std::fill(wifi_connect_password_.begin(), wifi_connect_password_.end(), '\0');
        wifi_connect_password_.clear();
        request_saved_wifi_networks_refresh();
    }
}

void SettingsApp::handle_wifi_scan_state_changed(bool running)
{
    if (context_ == nullptr) {
        return;
    }
    if (!wifi_ui_state_.enabled) {
        wifi_ui_state_.scanning = false;
        wifi_scan_stop_after_first_result_ = false;
        cancel_wifi_scan_retry_timer(*context_);
        wifi_scan_retry_remaining_ = 0;
        reset_available_wifi_scan_visibility();
        if (auto page_result = refresh_wifi_page_state(*context_); !page_result) {
            BROOKESIA_LOGW("Failed to refresh disabled Wi-Fi scan state: %1%", page_result.error());
        }
        if (auto list_result = refresh_wifi_lists(*context_); !list_result) {
            BROOKESIA_LOGW("Failed to refresh disabled Wi-Fi lists after scan state: %1%", list_result.error());
        }
        return;
    }
    const bool interrupted = !running && available_wifi_rows_hidden_for_scan_ && !available_wifi_scan_results_ready_;
    wifi_ui_state_.scanning = running;
    if (!running) {
        wifi_scan_stop_after_first_result_ = false;
        available_wifi_rows_hidden_for_scan_ = false;
    }
    auto result = refresh_wifi_page_state(*context_);
    if (!result) {
        BROOKESIA_LOGW("Failed to refresh Wi-Fi scan state: %1%", result.error());
    }
    if (!running || available_wifi_rows_hidden_for_scan_) {
        result = refresh_wifi_lists(*context_);
        if (!result) {
            BROOKESIA_LOGW("Failed to refresh Wi-Fi lists after scan: %1%", result.error());
        }
    }
    if (interrupted) {
        schedule_wifi_scan_retry(*context_);
    }
}

void SettingsApp::handle_wifi_scan_ap_infos_updated(const boost::json::array &ap_infos_json)
{
    if (context_ == nullptr) {
        return;
    }
    if (!wifi_ui_state_.enabled) {
        wifi_scan_stop_after_first_result_ = false;
        cancel_wifi_scan_retry_timer(*context_);
        wifi_scan_retry_remaining_ = 0;
        last_wifi_scan_ap_infos_.clear();
        available_wifi_networks_.clear();
        reset_available_wifi_scan_visibility();
        if (auto list_result = refresh_wifi_lists(*context_); !list_result) {
            BROOKESIA_LOGW("Failed to clear disabled Wi-Fi scan list: %1%", list_result.error());
        }
        return;
    }

    stop_wifi_scan_after_first_result();
    cancel_wifi_scan_retry_timer(*context_);
    wifi_scan_retry_remaining_ = 0;

    std::vector<WifiHelper::ScanApInfo> ap_infos;
    if (!BROOKESIA_DESCRIBE_FROM_JSON(ap_infos_json, ap_infos)) {
        BROOKESIA_LOGW("Failed to parse Wi-Fi scan AP infos");
        reset_available_wifi_scan_visibility();
        if (auto list_result = refresh_wifi_lists(*context_); !list_result) {
            BROOKESIA_LOGW("Failed to restore Wi-Fi lists after scan parse failure: %1%", list_result.error());
        }
        return;
    }

    last_wifi_scan_ap_infos_ = std::move(ap_infos);
    rebuild_available_wifi_networks();

    available_wifi_scan_results_ready_ = true;
    if (!wifi_ui_state_.scanning) {
        available_wifi_rows_hidden_for_scan_ = false;
    }
    auto result = refresh_wifi_lists(*context_);
    if (!result) {
        BROOKESIA_LOGW("Failed to refresh scanned Wi-Fi AP list: %1%", result.error());
    }
}

void SettingsApp::cancel_wifi_keyboard_if_active(system::core::AppContext &context)
{
    if (wifi_keyboard_request_id_ == system::core::INVALID_KEYBOARD_REQUEST_ID) {
        return;
    }

    const auto request_id = wifi_keyboard_request_id_;
    wifi_keyboard_request_id_ = system::core::INVALID_KEYBOARD_REQUEST_ID;
    auto result = context.system_service().hide_keyboard(request_id);
    if (!result) {
        BROOKESIA_LOGW("Failed to hide Settings Wi-Fi keyboard: %1%", result.error());
    }
}
