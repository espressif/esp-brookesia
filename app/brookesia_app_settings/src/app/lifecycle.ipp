/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

system::core::AppManifest SettingsApp::get_manifest() const
{
    return {
        .id = APP_ID,
        .name = localized_manifest_text(LOCALE_EN, APP_NAME_I18N_KEY, APP_NAME),
        .localized_names = {
            {LOCALE_EN, localized_manifest_text(LOCALE_EN, APP_NAME_I18N_KEY, APP_NAME)},
            {LOCALE_ZH_CN, localized_manifest_text(LOCALE_ZH_CN, APP_NAME_I18N_KEY, APP_NAME_ZH_CN)},
        },
        .version = make_app_version(),
        .kind = system::core::AppKind::Native,
        .visible = true,
        .icon_id = APP_ICON_ID,
        .supported_systems = {},
        .icon_path = "",
        .runtime_type = runtime::BackendType::Unknown,
        .app_path = "",
        .entry = "",
        .resource_dir = BROOKESIA_APP_SETTINGS_RESOURCE_DIR,
        .arguments = {},
    };
}

system::core::AppGuiDescriptor SettingsApp::get_gui_descriptor() const
{
    return {
        .root_kind = system::core::GuiRootKind::File,
        .root = GUI_ROOT,
        .resources = {},
        .screen_flows = {
            system::core::GuiScreenFlowEntry{
                .screen_flow = CONTENT_FLOW_ID,
                .layer = system::core::GuiAppLayer::AppDefault,
            },
            system::core::GuiScreenFlowEntry{
                .screen_flow = HEADER_FLOW_ID,
                .layer = system::core::GuiAppLayer::AppTop,
            },
        },
    };
}

std::expected<void, std::string> SettingsApp::on_install(system::core::AppContext &)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    bind_storage_service();
    bind_wifi_service();
    wifi_ui_state_.enabled = load_wifi_enabled_preference();
    apply_wifi_enabled_preference_to_service();
    return {};
}

void SettingsApp::on_uninstall(system::core::AppContext &)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    disconnect_sntp_events();
    disconnect_wifi_events();
    release_sntp_service();
    release_wifi_service();
    release_device_service();
    release_storage_service();
}

std::expected<void, std::string> SettingsApp::on_start(system::core::AppContext &context)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    context_ = &context;
    const auto active_language = context.gui().get_language();
    current_locale_ = normalize_locale(active_language);
    const auto active_theme = context.gui().get_theme();
    current_theme_id_ = active_theme == THEME_DARK ? THEME_DARK : THEME_LIGHT;
    current_page_ = PAGE_HOME;
    ++wifi_operation_generation_;

    auto result = subscribe_actions(context);
    if (!result) {
        return result;
    }
    result = apply_locale(context);
    if (!result) {
        return result;
    }
    result = refresh_header(context);
    if (!result) {
        return result;
    }
    result = refresh_theme_state(context);
    if (!result) {
        return result;
    }
    result = populate_language_options(context);
    if (!result) {
        return result;
    }
    result = refresh_language_state(context);
    if (!result) {
        return result;
    }
    bind_storage_service();
    bind_wifi_service();
    bind_sntp_service();
    wifi_ui_state_.enabled = load_wifi_enabled_preference();
    apply_wifi_enabled_preference_to_service();
    result = refresh_device_state(context);
    if (!result) {
        BROOKESIA_LOGW("Failed to refresh Device service state: %1%", result.error());
    }
    wifi_ui_state_.service_ready = wifi_service_binding_.is_valid() && WifiHelper::is_running();
    if (wifi_ui_state_.enabled) {
        result = refresh_wifi_service_state(context);
        if (!result) {
            return result;
        }
    } else {
        available_wifi_networks_.clear();
        reset_available_wifi_scan_visibility();
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
    }
    subscribe_wifi_events();
    if (auto time_zone_result = refresh_time_zone_state(context); !time_zone_result) {
        BROOKESIA_LOGW("Failed to refresh Settings time zone state: %1%", time_zone_result.error());
    }
    subscribe_sntp_events();
    return {};
}

std::expected<void, std::string> SettingsApp::on_stop(system::core::AppContext &context)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    ++wifi_operation_generation_;
    if (pending_language_timer_id_ != system::core::INVALID_TIMER_ID) {
        if (!context.timer().stop(pending_language_timer_id_)) {
            BROOKESIA_LOGW("Failed to stop pending Settings language switch timer: %1%", pending_language_timer_id_);
        }
        pending_language_timer_id_ = system::core::INVALID_TIMER_ID;
    }
    if (pending_theme_timer_id_ != system::core::INVALID_TIMER_ID) {
        if (!context.timer().stop(pending_theme_timer_id_)) {
            BROOKESIA_LOGW("Failed to stop pending Settings theme switch timer: %1%", pending_theme_timer_id_);
        }
        pending_theme_timer_id_ = system::core::INVALID_TIMER_ID;
    }
    cancel_wifi_scan_retry_timer(context);
    if (pending_wifi_connected_hide_timer_id_ != system::core::INVALID_TIMER_ID) {
        if (!context.timer().stop(pending_wifi_connected_hide_timer_id_)) {
            BROOKESIA_LOGW(
                "Failed to stop pending Settings Wi-Fi connected hide timer: %1%",
                pending_wifi_connected_hide_timer_id_
            );
        }
        pending_wifi_connected_hide_timer_id_ = system::core::INVALID_TIMER_ID;
    }
    if (pending_wifi_connected_scroll_timer_id_ != system::core::INVALID_TIMER_ID) {
        if (!context.timer().stop(pending_wifi_connected_scroll_timer_id_)) {
            BROOKESIA_LOGW(
                "Failed to stop pending Settings Wi-Fi connected scroll timer: %1%",
                pending_wifi_connected_scroll_timer_id_
            );
        }
        pending_wifi_connected_scroll_timer_id_ = system::core::INVALID_TIMER_ID;
    }
    pending_language_locale_.clear();
    pending_theme_id_.clear();
    language_switch_in_progress_ = false;
    theme_switch_in_progress_ = false;
    hide_language_loading_if_visible(context);
    hide_theme_loading_if_visible(context);
    hide_message_dialog_if_visible(context);
    cancel_wifi_keyboard_if_active(context);
    if (wifi_refresh_animation_id_ != 0) {
        (void)context.gui().stop_animation(wifi_refresh_animation_id_);
        wifi_refresh_animation_id_ = 0;
    }
    language_action_connection_.disconnect();
    wifi_select_action_connection_.disconnect();
    wifi_forget_action_connection_.disconnect();
    brightness_action_connection_.disconnect();
    volume_action_connection_.disconnect();
    mute_action_connection_.disconnect();
    disconnect_device_events();
    disconnect_wifi_events();
    disconnect_sntp_events();
    clear_hardware_groups(context);
    release_device_service();
    release_display_service();
    release_audio_service();
    release_sntp_service();
    clear_language_options(context);
    clear_wifi_networks(context);
    selected_wifi_network_.reset();
    wifi_connect_password_.clear();
    wifi_forget_click_suppression_id_.clear();
    wifi_forget_click_suppression_ssid_.clear();
    saved_wifi_networks_.clear();
    available_wifi_networks_.clear();
    reset_available_wifi_scan_visibility();
    wifi_scan_stop_after_first_result_ = false;
    wifi_scan_retry_remaining_ = 0;
    saved_wifi_networks_loaded_ = false;
    saved_wifi_networks_refreshing_ = false;
    context_ = nullptr;
    current_page_ = PAGE_HOME;
    device_ui_state_ = DeviceUiState{};
    wifi_ui_state_ = WifiUiState{};
    return {};
}

std::expected<void, std::string> SettingsApp::on_timer(
    system::core::AppContext &context,
    system::core::TimerId timer_id,
    std::string_view name
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    if (name == THEME_SWITCH_TIMER_NAME) {
        if (timer_id != pending_theme_timer_id_) {
            BROOKESIA_LOGW(
                "Ignore stale Settings theme switch timer: timer_id(%1%), pending(%2%)",
                timer_id, pending_theme_timer_id_
            );
            return {};
        }

        pending_theme_timer_id_ = system::core::INVALID_TIMER_ID;
        std::string theme_id = std::move(pending_theme_id_);
        pending_theme_id_.clear();
        if (theme_id.empty()) {
            hide_theme_loading_if_visible(context);
            return {};
        }
        return commit_theme_switch(context, theme_id);
    }
    if (name == WIFI_SCAN_RETRY_TIMER_NAME) {
        if (timer_id != pending_wifi_scan_retry_timer_id_) {
            BROOKESIA_LOGW(
                "Ignore stale Settings Wi-Fi scan retry timer: timer_id(%1%), pending(%2%)",
                timer_id, pending_wifi_scan_retry_timer_id_
            );
            return {};
        }
        const auto retry_generation = pending_wifi_scan_retry_generation_;
        pending_wifi_scan_retry_timer_id_ = system::core::INVALID_TIMER_ID;
        pending_wifi_scan_retry_generation_ = 0;
        if (retry_generation != wifi_operation_generation_ || !wifi_ui_state_.service_ready ||
                !wifi_ui_state_.enabled || available_wifi_scan_results_ready_) {
            return {};
        }
        BROOKESIA_LOGI("Retry Settings Wi-Fi scan after interrupted scan state");
        return start_wifi_scan(context, false);
    }
    if (name == WIFI_CONNECTED_HIDE_TIMER_NAME) {
        if (timer_id != pending_wifi_connected_hide_timer_id_) {
            BROOKESIA_LOGW(
                "Ignore stale Settings Wi-Fi connected hide timer: timer_id(%1%), pending(%2%)",
                timer_id, pending_wifi_connected_hide_timer_id_
            );
            return {};
        }
        pending_wifi_connected_hide_timer_id_ = system::core::INVALID_TIMER_ID;
        if (wifi_ui_state_.connected_card_state == WifiConnectedCardState::DisconnectedGrace) {
            hide_wifi_connected_card();
            return refresh_wifi_page_state(context);
        }
        return {};
    }
    if (name == WIFI_CONNECTED_SCROLL_TIMER_NAME) {
        if (timer_id != pending_wifi_connected_scroll_timer_id_) {
            BROOKESIA_LOGW(
                "Ignore stale Settings Wi-Fi connected scroll timer: timer_id(%1%), pending(%2%)",
                timer_id, pending_wifi_connected_scroll_timer_id_
            );
            return {};
        }
        pending_wifi_connected_scroll_timer_id_ = system::core::INVALID_TIMER_ID;
        if (current_page_ != PAGE_WIFI || !is_wifi_connected_card_visible()) {
            return {};
        }
        if (auto page_result = refresh_wifi_page_state(context); !page_result) {
            BROOKESIA_LOGW("Failed to refresh Wi-Fi page before connected card scroll: %1%", page_result.error());
            return {};
        }
        if (!is_wifi_connected_card_visible()) {
            return {};
        }
        if (auto scroll_result = scroll_wifi_connected_card_to_top(context); !scroll_result) {
            BROOKESIA_LOGW("Failed to scroll Wi-Fi connected card to top: %1%", scroll_result.error());
        }
        return {};
    }
    if (name != LANGUAGE_SWITCH_TIMER_NAME) {
        return {};
    }
    if (timer_id != pending_language_timer_id_) {
        BROOKESIA_LOGW(
            "Ignore stale Settings language switch timer: timer_id(%1%), pending(%2%)",
            timer_id, pending_language_timer_id_
        );
        return {};
    }

    pending_language_timer_id_ = system::core::INVALID_TIMER_ID;
    std::string locale = std::move(pending_language_locale_);
    pending_language_locale_.clear();
    if (locale.empty()) {
        hide_language_loading_if_visible(context);
        return {};
    }
    return commit_language_switch(context, locale);
}

std::expected<void, std::string> SettingsApp::on_action(
    system::core::AppContext &context,
    std::string_view action
)
{
    BROOKESIA_LOGD("Settings action: %1%", action);
    std::string effective_action_storage;
    std::string_view effective_action = action;
    if (action == ACTION_HEADER_BACK) {
        effective_action = get_header_back_action_for_page(current_page_);
    }
    if (action == ACTION_OPEN_HOME && current_page_ == PAGE_WIFI_CONNECT) {
        effective_action_storage = ACTION_BACK_WIFI_CONNECT;
        effective_action = effective_action_storage;
    }

    if (effective_action == ACTION_WIFI_PASSWORD_EDIT) {
        return show_wifi_password_keyboard(context);
    }
    if (effective_action == ACTION_WIFI_CONNECT_SUBMIT) {
        return submit_wifi_connect(context);
    }
    if (effective_action == ACTION_WIFI_CONNECT_CANCEL) {
        effective_action_storage = ACTION_BACK_WIFI_CONNECT;
        effective_action = effective_action_storage;
    }
    if (effective_action == WIFI_TOGGLE_ACTION) {
        if (wifi_ui_state_.enabled) {
            return disconnect_wifi(context);
        }
        const auto generation = ++wifi_operation_generation_;
        const auto save_handler = [this, generation](service::FunctionResult && result) {
            if (context_ == nullptr || generation != wifi_operation_generation_) {
                return;
            }
            if (result.success) {
                return;
            }
            BROOKESIA_LOGW("Failed to save Settings Wi-Fi enabled preference: %1%", result.error_message);
            if (context_ == nullptr) {
                return;
            }
            wifi_ui_state_.enabled = false;
            wifi_ui_state_.scanning = false;
            wifi_ui_state_.connecting = false;
            available_wifi_networks_.clear();
            reset_available_wifi_scan_visibility();
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
        auto save_result = submit_wifi_enabled_preference_save(true, save_handler);
        if (!save_result) {
            if (auto refresh_result = refresh_wifi_page_state(context); !refresh_result) {
                BROOKESIA_LOGW(
                    "Failed to restore Wi-Fi page after preference save failure: %1%",
                    refresh_result.error()
                );
            }
            return save_result;
        }
        wifi_ui_state_.enabled = true;
        if (!saved_wifi_networks_loaded_) {
            request_saved_wifi_networks_refresh();
        }
        auto result = start_wifi_scan(context);
        if (!result) {
            wifi_ui_state_.enabled = false;
            if (auto rollback_save = submit_wifi_enabled_preference_save(false); !rollback_save) {
                BROOKESIA_LOGW("Failed to roll back Settings Wi-Fi preference: %1%", rollback_save.error());
            }
            if (auto refresh_result = refresh_wifi_page_state(context); !refresh_result) {
                BROOKESIA_LOGW("Failed to roll back Wi-Fi page after start failure: %1%", refresh_result.error());
            }
            return result;
        }
        return {};
    }
    if (effective_action == WIFI_SCAN_ACTION) {
        return start_wifi_scan(context);
    }
    if (effective_action == WIFI_DISCONNECT_ACTION) {
        return disconnect_current_wifi(context);
    }
    if (effective_action == WIFI_SAVED_PREV_ACTION) {
        if (saved_wifi_page_ > 0) {
            --saved_wifi_page_;
        }
        return refresh_wifi_lists(context);
    }
    if (effective_action == WIFI_SAVED_NEXT_ACTION) {
        const auto page_count = get_page_count(saved_wifi_networks_.size());
        if (saved_wifi_page_ + 1 < page_count) {
            ++saved_wifi_page_;
        }
        return refresh_wifi_lists(context);
    }
    if (effective_action == WIFI_AVAILABLE_PREV_ACTION) {
        if (available_wifi_page_ > 0) {
            --available_wifi_page_;
        }
        return refresh_wifi_lists(context);
    }
    if (effective_action == WIFI_AVAILABLE_NEXT_ACTION) {
        const auto page_count = get_page_count(available_wifi_networks_.size());
        if (available_wifi_page_ + 1 < page_count) {
            ++available_wifi_page_;
        }
        return refresh_wifi_lists(context);
    }
    if (const auto timezone = get_time_zone_for_action(effective_action); timezone.has_value()) {
        return set_time_zone(context, *timezone);
    }

    if (is_navigation_action(effective_action)) {
        const auto previous_page = current_page_;
        auto result = context.gui().trigger_screen_flow(CONTENT_FLOW_ID, effective_action);
        if (!result) {
            return result;
        }
        if (const auto page = get_navigation_page(effective_action); page.has_value()) {
            current_page_ = *page;
        }
        if (current_page_ != PAGE_WIFI) {
            update_wifi_refresh_animation(context);
        }
        if (effective_action == "settings.open.wifi") {
            wifi_ui_state_.service_ready = wifi_service_binding_.is_valid() && WifiHelper::is_running();
            if (wifi_ui_state_.enabled) {
                if (auto wifi_result = refresh_wifi_service_state(context); !wifi_result) {
                    BROOKESIA_LOGW("Failed to refresh Wi-Fi state: %1%", wifi_result.error());
                }
            } else {
                if (auto page_result = refresh_wifi_page_state(context); !page_result) {
                    BROOKESIA_LOGW("Failed to refresh Wi-Fi page: %1%", page_result.error());
                }
                if (auto list_result = refresh_wifi_lists(context); !list_result) {
                    BROOKESIA_LOGW("Failed to refresh Wi-Fi lists: %1%", list_result.error());
                }
                if (auto summary_result = refresh_wifi_summary_state(context); !summary_result) {
                    BROOKESIA_LOGW("Failed to refresh Wi-Fi summary: %1%", summary_result.error());
                }
            }
            if (wifi_ui_state_.service_ready && wifi_ui_state_.enabled && previous_page != PAGE_WIFI_CONNECT) {
                if (auto scan_result = start_wifi_scan(context); !scan_result) {
                    BROOKESIA_LOGW("Failed to start Wi-Fi scan: %1%", scan_result.error());
                }
            }
        }
        if (current_page_ == PAGE_DEVICE) {
            if (auto device_result = refresh_my_device_state(context); !device_result) {
                BROOKESIA_LOGW("Failed to refresh My Device page: %1%", device_result.error());
            }
        }
        if (current_page_ == PAGE_TIME_ZONE) {
            if (auto time_zone_result = refresh_time_zone_state(context); !time_zone_result) {
                BROOKESIA_LOGW("Failed to refresh Time zone page: %1%", time_zone_result.error());
            }
        }
        auto header_result = refresh_header(context);
        if (!header_result) {
            return header_result;
        }
        if (previous_page == PAGE_WIFI_CONNECT && current_page_ == PAGE_WIFI) {
            schedule_wifi_connected_card_scroll(context);
        }
        return {};
    }

    if (is_theme_action(effective_action)) {
        const std::string next_theme_id = effective_action == "settings.display.dark" ? THEME_DARK : THEME_LIGHT;
        return schedule_theme_switch(context, next_theme_id);
    }

    return {};
}

system::core::AppManifest SettingsAppProvider::get_manifest() const
{
    return SettingsApp().get_manifest();
}

std::shared_ptr<system::core::IApp> SettingsAppProvider::create_app()
{
    return std::make_shared<SettingsApp>();
}

BROOKESIA_SYSTEM_CORE_APP_PROVIDER_REGISTER_WITH_SYMBOL(
    SettingsAppProvider,
    "brookesia.general.settings",
    app_settings_provider_symbol
);

std::expected<void, std::string> SettingsApp::subscribe_actions(system::core::AppContext &context)
{
    for (const auto *action : NAVIGATION_ACTIONS) {
        auto result = context.gui().subscribe_action(action);
        if (!result) {
            return result;
        }
    }
    for (const auto *action : THEME_ACTIONS) {
        auto result = context.gui().subscribe_action(action);
        if (!result) {
            return result;
        }
    }
    for (const auto *action : TIME_ZONE_ACTIONS) {
        auto result = context.gui().subscribe_action(action);
        if (!result) {
            return result;
        }
    }
    for (const auto *action : {
                ACTION_WIFI_PASSWORD_EDIT, ACTION_WIFI_CONNECT_SUBMIT, ACTION_WIFI_CONNECT_CANCEL,
                WIFI_TOGGLE_ACTION, WIFI_SCAN_ACTION, WIFI_DISCONNECT_ACTION,
                WIFI_SAVED_PREV_ACTION, WIFI_SAVED_NEXT_ACTION,
                WIFI_AVAILABLE_PREV_ACTION, WIFI_AVAILABLE_NEXT_ACTION, ACTION_HEADER_BACK
            }) {
        auto result = context.gui().subscribe_action(action);
        if (!result) {
            return result;
        }
    }
    language_action_connection_.disconnect();
    language_action_connection_ = context.gui().subscribe_action(
                                      LANGUAGE_SELECT_ACTION,
    [this](const gui::Event & event) {
        handle_language_event(event);
    }
                                  );
    if (!language_action_connection_.connected()) {
        return std::unexpected(std::string("Failed to subscribe language action: ") + LANGUAGE_SELECT_ACTION);
    }

    wifi_select_action_connection_.disconnect();
    wifi_select_action_connection_ = context.gui().subscribe_action(
                                         WIFI_SELECT_ACTION,
    [this](const gui::Event & event) {
        handle_wifi_select_event(event);
    }
                                     );
    if (!wifi_select_action_connection_.connected()) {
        return std::unexpected(std::string("Failed to subscribe Wi-Fi action: ") + WIFI_SELECT_ACTION);
    }

    wifi_forget_action_connection_.disconnect();
    wifi_forget_action_connection_ = context.gui().subscribe_action(
                                         WIFI_SAVED_FORGET_ACTION,
    [this](const gui::Event & event) {
        handle_wifi_forget_event(event);
    }
                                     );
    if (!wifi_forget_action_connection_.connected()) {
        return std::unexpected(std::string("Failed to subscribe Wi-Fi action: ") + WIFI_SAVED_FORGET_ACTION);
    }

    brightness_action_connection_.disconnect();
    const auto brightness_callback = [this](const gui::Event & event) {
        handle_brightness_event(event);
    };
    brightness_action_connection_ = context.gui().subscribe_action(ACTION_DISPLAY_BRIGHTNESS, brightness_callback);
    if (!brightness_action_connection_.connected()) {
        return std::unexpected(std::string("Failed to subscribe action: ") + ACTION_DISPLAY_BRIGHTNESS);
    }

    volume_action_connection_.disconnect();
    const auto volume_callback = [this](const gui::Event & event) {
        handle_volume_event(event);
    };
    volume_action_connection_ = context.gui().subscribe_action(ACTION_SOUND_VOLUME, volume_callback);
    if (!volume_action_connection_.connected()) {
        return std::unexpected(std::string("Failed to subscribe action: ") + ACTION_SOUND_VOLUME);
    }

    mute_action_connection_.disconnect();
    const auto mute_callback = [this](const gui::Event & event) {
        handle_mute_event(event);
    };
    mute_action_connection_ = context.gui().subscribe_action(ACTION_SOUND_MUTE, mute_callback);
    if (!mute_action_connection_.connected()) {
        return std::unexpected(std::string("Failed to subscribe action: ") + ACTION_SOUND_MUTE);
    }
    return {};
}
