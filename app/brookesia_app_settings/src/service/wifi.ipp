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
    const auto visible_slot_count = std::min(visible_count, slot_limit);
#if BROOKESIA_APP_SETTINGS_ENABLE_MEMORY_TRACE
    BROOKESIA_LOGI(
        "[HeapTrace][settings.sync_wifi] parent(%1%) prefix(%2%) slot_count(%3%) visible_count(%4%) "
        "networks(%5%) page_start(%6%) page_size(%7%) slots_to_create(%8%)",
        std::string(parent), std::string(id_prefix), slot_count, visible_slot_count, networks.size(),
        page_start, slot_limit, (visible_slot_count > slot_count ? visible_slot_count - slot_count : 0)
    );
#endif
    for (size_t i = slot_count; i < visible_slot_count; ++i) {
        const auto heap = ::esp_brookesia::lib_utils::MemoryProfiler::take_raw_heap_snapshot();
        if (heap.external_free != 0 && heap.external_free < WIFI_SLOT_MIN_FREE_PSRAM_BYTES) {
            BROOKESIA_LOGW(
                "[HeapTrace][settings.wifi_guard] stop creating rows: low PSRAM "
                "(free=%1% largest=%2% reserve=%3% created=%4% requested=%5%)",
                heap.external_free, heap.external_largest,
                static_cast<size_t>(WIFI_SLOT_MIN_FREE_PSRAM_BYTES), slot_count, visible_slot_count
            );
            break;
        }

        auto slot_id = std::string(id_prefix);
        slot_id += "_";
        slot_id += std::to_string(i);
        auto created = context.gui().create_view(WIFI_TEMPLATE_ID, parent, slot_id);
        if (!created) {
            return std::unexpected(created.error());
        }

        dynamic_wifi_paths_.push_back(join_path(parent, slot_id));
        ++slot_count;
    }

    std::vector<gui::BindingValueUpdate> updates;
    for (size_t i = 0; i < slot_count; ++i) {
        auto slot_id = std::string(id_prefix);
        slot_id += "_";
        slot_id += std::to_string(i);
        const auto instance_path = join_path(parent, slot_id);

        const auto network_index = page_start + i;
        if (i >= visible_slot_count || network_index >= networks.size()) {
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
                           "Failed to refresh Wi-Fi network bindings" :
                           command_result.error_message
                       );
            }
        }
        return std::unexpected("Failed to refresh Wi-Fi network bindings");
    }
    return {};
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
    const auto generation = wifi_operation_generation_;
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

    auto scan_start_handler = [this, generation](service::FunctionResult && result) {
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
    auto set_params_handler = [this, scan_start_handler, generation](service::FunctionResult && result) {
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

        if (!WifiHelper::call_function_async(WifiHelper::FunctionId::TriggerScanStart, scan_start_handler)) {
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
                set_params_handler
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
    pending_wifi_scan_retry_generation_ = wifi_operation_generation_;
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

    auto scan_stop_handler = [](service::FunctionResult && result) {
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
        return std::unexpected(localized_text(current_locale_, "wifi_service_unavailable"));
    }
    if (network.ssid.empty()) {
        return std::unexpected("Wi-Fi SSID is empty");
    }

    const auto generation = ++wifi_operation_generation_;
    cancel_wifi_scan_retry_timer(context);
    wifi_scan_retry_remaining_ = 0;
    const auto save_handler = [this, generation](service::FunctionResult && result) {
        if (generation != wifi_operation_generation_) {
            return;
        }
        if (result.success) {
            return;
        }
        BROOKESIA_LOGW("Failed to save Settings Wi-Fi enabled preference: %1%", result.error_message);
    };
    auto save_result = submit_wifi_enabled_preference_save(true, save_handler);
    if (!save_result) {
        return save_result;
    }
    wifi_ui_state_.connecting = true;
    wifi_ui_state_.enabled = true;
    wifi_scan_stop_after_first_result_ = false;
    wifi_ui_state_.connected_ssid = network.ssid;
    cancel_wifi_connected_hide_timer(context);
    show_wifi_connected_card(WifiConnectedCardState::Connecting, network.ssid);
    auto refresh_result = refresh_wifi_page_state(context);
    if (!refresh_result) {
        return refresh_result;
    }
    refresh_result = refresh_wifi_summary_state(context);
    if (!refresh_result) {
        return refresh_result;
    }

    const auto connect_handler = [this, generation](service::FunctionResult && result) {
        if (context_ == nullptr || generation != wifi_operation_generation_ || !wifi_ui_state_.enabled) {
            return;
        }
        if (result.success) {
            return;
        }
        BROOKESIA_LOGW("Failed to connect Wi-Fi: %1%", result.error_message);
        wifi_ui_state_.connecting = false;
        hide_wifi_connected_card();
        if (auto state_result = refresh_wifi_service_state(*context_); !state_result) {
            BROOKESIA_LOGW("Failed to refresh Wi-Fi state after failed connect: %1%", state_result.error());
        }
    };
    const auto set_target_handler = [this, connect_handler, generation](service::FunctionResult && result) {
        if (context_ == nullptr || generation != wifi_operation_generation_ || !wifi_ui_state_.enabled) {
            return;
        }
        if (!result.success) {
            BROOKESIA_LOGW("Failed to set Wi-Fi target AP: %1%", result.error_message);
            wifi_ui_state_.connecting = false;
            hide_wifi_connected_card();
            if (auto state_result = refresh_wifi_service_state(*context_); !state_result) {
                BROOKESIA_LOGW(
                    "Failed to refresh Wi-Fi state after failed target AP update: %1%",
                    state_result.error()
                );
            }
            return;
        }

        if (!WifiHelper::call_function_async(
                    WifiHelper::FunctionId::TriggerGeneralAction,
                    BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralAction::Connect),
                    connect_handler
                )) {
            wifi_ui_state_.connecting = false;
            hide_wifi_connected_card();
            BROOKESIA_LOGW("Failed to submit Wi-Fi connect request");
            if (auto state_result = refresh_wifi_service_state(*context_); !state_result) {
                BROOKESIA_LOGW("Failed to refresh Wi-Fi state after failed connect submit: %1%", state_result.error());
            }
        }
    };
    if (!WifiHelper::call_function_async(
                WifiHelper::FunctionId::SetConnectAp,
                network.ssid,
                std::string(password),
                set_target_handler
            )) {
        wifi_ui_state_.connecting = false;
        hide_wifi_connected_card();
        if (auto state_result = refresh_wifi_service_state(context); !state_result) {
            BROOKESIA_LOGW("Failed to refresh Wi-Fi state after failed connect: %1%", state_result.error());
        }
        return std::unexpected("Failed to submit Wi-Fi target AP update request");
    }
    return {};
}

std::expected<void, std::string> SettingsApp::disconnect_wifi(system::core::AppContext &context)
{
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
    auto save_result = submit_wifi_enabled_preference_save(false, save_handler);
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
                stop_handler
            )) {
        BROOKESIA_LOGW("Failed to submit Wi-Fi stop request");
    }
    return {};
}

std::expected<void, std::string> SettingsApp::disconnect_current_wifi(system::core::AppContext &context)
{
    if (!wifi_ui_state_.service_ready || !wifi_ui_state_.enabled) {
        return refresh_wifi_page_state(context);
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
                disconnected_handler
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
        if (auto list_result = refresh_wifi_lists(*context_); !list_result) {
            BROOKESIA_LOGW("Failed to refresh Wi-Fi lists after forgetting AP: %1%", list_result.error());
        }
    };

    if (!WifiHelper::call_function_async(
                WifiHelper::FunctionId::RemoveConnectedAp,
                network.ssid,
                forget_handler
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
    wifi_connect_password_.clear();
    auto result = context.gui().trigger_screen_flow(CONTENT_FLOW_ID, ACTION_OPEN_WIFI_CONNECT);
    if (!result) {
        return result;
    }
    current_page_ = PAGE_WIFI_CONNECT;
    result = refresh_header(context);
    if (!result) {
        return result;
    }
    return refresh_wifi_connect_state(context);
}

std::expected<void, std::string> SettingsApp::refresh_wifi_connect_state(system::core::AppContext &context)
{
    const auto password_empty_text = localized_text(current_locale_, "wifi_password_placeholder");
    const auto status_text = wifi_connect_password_.empty() ?
                             localized_text(current_locale_, "wifi_password_required") :
                             localized_text(current_locale_, "wifi_ready_to_connect");
    const bool connect_enabled = selected_wifi_network_.has_value() && !wifi_connect_password_.empty();

    std::vector<gui::BindingValueUpdate> updates;
    add_binding_update(
        updates,
        WIFI_CONNECT_SSID_PATH,
        "ssid",
        selected_wifi_network_.has_value() ? selected_wifi_network_->ssid : ""
    );
    add_binding_update(
        updates,
        WIFI_CONNECT_DETAIL_PATH,
        "detail",
        selected_wifi_network_.has_value() ? selected_wifi_network_->detail : ""
    );
    add_binding_update(
        updates,
        WIFI_CONNECT_PASSWORD_VALUE_PATH,
        "passwordValue",
        make_password_summary(wifi_connect_password_, password_empty_text)
    );
    add_binding_update(updates, WIFI_CONNECT_STATUS_PATH, "status", status_text);
    add_binding_update(updates, WIFI_CONNECT_BUTTON_PATH, "disabled", connect_enabled ? "false" : "true");
    add_binding_update(
        updates,
        WIFI_CONNECT_BUTTON_PATH,
        "opacity",
        std::to_string(connect_enabled ? ENABLED_OPACITY : DISABLED_OPACITY)
    );
    return context.gui().set_binding_values(updates);
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
    if (!selected_wifi_network_.has_value()) {
        return std::unexpected(localized_text(current_locale_, "wifi_no_network_selected"));
    }
    if (wifi_keyboard_request_id_ != system::core::INVALID_KEYBOARD_REQUEST_ID) {
        return {};
    }

    auto request = context.system_service().show_keyboard(
    system::core::KeyboardRequestOptions{
        .title = selected_wifi_network_->ssid,
        .placeholder = localized_text(current_locale_, "wifi_password_placeholder"),
        .initial_text = wifi_connect_password_,
        .max_length = 64,
        .password = true,
        .mode = "text",
        .allowed_modes = {"text", "upper", "number", "special"},
    },
    [this](const system::core::KeyboardResult & result) {
        handle_wifi_keyboard_result(result);
    }
                   );
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

    wifi_connect_password_ = result.text;
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
        return std::unexpected(localized_text(current_locale_, "wifi_no_network_selected"));
    }
    if (wifi_connect_password_.empty()) {
        return refresh_wifi_connect_state(context);
    }

    auto result = connect_wifi_network(context, *selected_wifi_network_, wifi_connect_password_);
    if (!result) {
        const std::vector<gui::BindingValueUpdate> updates = {
            gui::BindingValueUpdate{
                .absolute_path = WIFI_CONNECT_STATUS_PATH,
                .key = "status",
                .value = localized_text(current_locale_, "wifi_connect_failed"),
            },
        };
        if (auto status_result = context.gui().set_binding_values(updates); !status_result) {
            BROOKESIA_LOGW("Failed to refresh Wi-Fi connect failure status: %1%", status_result.error());
        }
        return result;
    }
    result = context.gui().trigger_screen_flow(CONTENT_FLOW_ID, ACTION_BACK_WIFI_CONNECT);
    if (!result) {
        return result;
    }
    current_page_ = PAGE_WIFI;
    result = refresh_header(context);
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

    available_wifi_networks_.clear();
    available_wifi_page_ = 0;
    for (size_t i = 0; i < ap_infos.size(); ++i) {
        const auto &ap = ap_infos[i];
        if (ap.ssid.empty()) {
            continue;
        }
        if (!wifi_ui_state_.connected_ssid.empty() && ap.ssid == wifi_ui_state_.connected_ssid) {
            continue;
        }
        if (is_saved_wifi_ssid(ap.ssid)) {
            continue;
        }
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
        // The dedicated band chip node was removed from wifi_network_item to cut per-row memory; the
        // band is now folded into the detail line so the information is still shown.
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
            .signal_level = signal_level,
            .channel = ap.channel,
        });
    }

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
