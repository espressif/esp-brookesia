/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

system::core::AppManifest AppStoreApp::get_manifest() const
{
    return {
        .id = APP_ID,
        .name = localized_app_name(LOCALE_EN),
        .localized_names = {
            {LOCALE_EN, localized_app_name(LOCALE_EN)},
            {LOCALE_ZH_CN, localized_app_name(LOCALE_ZH_CN)},
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
        .resource_dir = BROOKESIA_APP_STORE_RESOURCE_DIR,
        .arguments = {},
    };
}

system::core::AppGuiDescriptor AppStoreApp::get_gui_descriptor() const
{
    return {
        .root_kind = system::core::GuiRootKind::File,
        .root = GUI_ROOT,
        .resources = {},
        .screen_flows = {
            system::core::GuiScreenFlowEntry{
                .screen_flow = FLOW_ID,
                .layer = system::core::GuiAppLayer::AppDefault,
            },
        },
    };
}

std::expected<void, std::string> AppStoreApp::on_start(system::core::AppContext &context)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    context_ = &context;
    network_ready_ = true;
    ++async_generation_;
    if (auto i18n_result = refresh_i18n(context); !i18n_result) {
        BROOKESIA_LOGW("Failed to load App Store i18n: %1%", i18n_result.error());
    }
    auto result = subscribe_actions(context);
    if (!result) {
        context_ = nullptr;
        return result;
    }

    prefer_external_install_storage(context);
    refresh_installed_apps(context);
    bind_device_service();
    bind_storage_service();
    scan_local_packages(context);
    refresh_storage_capacity(context);
    if (!configured_index_url().empty()) {
        bind_http_service();
        subscribe_http_events();
    }

    status_text_ = tr("status.loading_cache");
    auto cache_result = load_cached_index(context);
    if (cache_result) {
        auto refresh_result = refresh_ui(context);
        if (!configured_index_url().empty()) {
            (void)submit_network_check(context, NetworkCheckPurpose::StartupCache);
        }
        return refresh_result;
    }
    BROOKESIA_LOGI("App Store cache load skipped or failed: %1%", cache_result.error());

    if (configured_index_url().empty()) {
        status_text_ = tr("error.index_url_not_configured");
        return refresh_ui(context);
    }

    refresh_in_progress_ = true;
    refresh_request_id_ = 0;
    refresh_icon_request_id_ = 0;
    refresh_icon_index_ = 0;
    status_text_ = tr("status.checking_network");
    ensure_message_dialog(
        context,
        tr("dialog.checking_network"),
        {},
        system::core::MessageDialogIcon::Information,
        0
    );
    if (!submit_network_check(context, NetworkCheckPurpose::StartupRemoteRefresh)) {
        finish_remote_refresh_with_error(context, tr("error.failed_check_network"));
    }
    return refresh_ui(context);
}

std::expected<void, std::string> AppStoreApp::on_stop(system::core::AppContext &context)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    ++async_generation_;
    cancel_remote_refresh();
    stop_view_mode_load_timer(context);
    stop_refresh_icon_timer(context);
    stop_time_sync_timers(context);
    hide_message_dialog_if_visible(context);
    for (auto &entry : entries_) {
        cancel_download(entry);
    }
    primary_action_connection_.disconnect();
    disconnect_http_events();
    clear_entry_views(context);
    unregister_icons(context);
    release_http_service();
    release_device_service();
    release_storage_service();
    entries_.clear();
    local_packages_.clear();
    installed_runtime_apps_.clear();
    local_package_by_manifest_id_.clear();
    installed_manifest_ids_.clear();
    installed_version_by_manifest_id_.clear();
    applied_i18n_locale_.clear();
    i18n_strings_.clear();
    status_text_.clear();
    storage_text_.clear();
    refresh_dialog_message_.clear();
    refresh_request_id_ = 0;
    refresh_icon_request_id_ = 0;
    refresh_icon_index_ = 0;
    pending_view_mode_.reset();
    http_general_state_.reset();
    message_dialog_purpose_ = MessageDialogPurpose::None;
    refresh_in_progress_ = false;
    storage_match_warning_logged_ = false;
    time_sync_waiting_ = false;
    context_ = nullptr;
    return {};
}

std::expected<void, std::string> AppStoreApp::on_action(
    system::core::AppContext &context,
    std::string_view action
)
{
    if (action == ACTION_REFRESH) {
        refresh_installed_apps(context);
        scan_local_packages(context);
        refresh_storage_capacity(context);
        if (view_mode_ == ViewMode::Store) {
            auto result = start_remote_refresh(context);
            if (!result) {
                status_text_ = result.error();
                return populate_entries(context);
            }
            return {};
        }
        return populate_entries(context);
    }
    if (action == ACTION_PAGE_PREV) {
        if (list_page_ > 0) {
            --list_page_;
        }
        return populate_entries(context);
    }
    if (action == ACTION_PAGE_NEXT) {
        const auto page_count = get_page_count(visible_items_.size());
        if (list_page_ + 1 < page_count) {
            ++list_page_;
        }
        return populate_entries(context);
    }
    if (action == ACTION_TAB_STORE) {
        stop_view_mode_load_timer(context);
        pending_view_mode_.reset();
        set_view_mode(context, ViewMode::Store);
    } else if (action == ACTION_TAB_LOCAL) {
        start_view_mode_load(context, ViewMode::Local);
    } else if (action == ACTION_TAB_INSTALLED) {
        start_view_mode_load(context, ViewMode::Installed);
    }
    return {};
}

std::expected<void, std::string> AppStoreApp::on_timer(
    system::core::AppContext &context,
    system::core::TimerId timer_id,
    std::string_view name
)
{
    if (name == VIEW_MODE_LOAD_TIMER_NAME) {
        if (timer_id != view_mode_load_timer_id_) {
            BROOKESIA_LOGW(
                "Ignore stale App Store view mode load timer: timer_id(%1%), pending(%2%)",
                timer_id, view_mode_load_timer_id_
            );
            return {};
        }
        view_mode_load_timer_id_ = system::core::INVALID_TIMER_ID;
        return process_view_mode_load(context);
    }

    if (name == TIME_SYNC_TIMEOUT_TIMER_NAME) {
        if (timer_id != time_sync_timeout_timer_id_) {
            BROOKESIA_LOGW(
                "Ignore stale App Store time sync timeout timer: timer_id(%1%), pending(%2%)",
                timer_id, time_sync_timeout_timer_id_
            );
            return {};
        }
        time_sync_timeout_timer_id_ = system::core::INVALID_TIMER_ID;
        finish_time_sync_wait_timeout(context);
        return {};
    }

    if (name == TIME_SYNC_SUCCESS_CLOSE_TIMER_NAME) {
        if (timer_id != time_sync_success_close_timer_id_) {
            BROOKESIA_LOGW(
                "Ignore stale App Store time sync success close timer: timer_id(%1%), pending(%2%)",
                timer_id, time_sync_success_close_timer_id_
            );
            return {};
        }
        time_sync_success_close_timer_id_ = system::core::INVALID_TIMER_ID;
        process_time_sync_success_close(context);
        return {};
    }

    if (name == REFRESH_REQUEST_TIMEOUT_TIMER_NAME) {
        if (timer_id != refresh_request_timeout_timer_id_) {
            BROOKESIA_LOGW(
                "Ignore stale App Store refresh request timeout timer: timer_id(%1%), pending(%2%)",
                timer_id, refresh_request_timeout_timer_id_
            );
            return {};
        }
        refresh_request_timeout_timer_id_ = system::core::INVALID_TIMER_ID;
        process_refresh_request_timeout(context);
        return {};
    }

    if (name == REFRESH_RESULT_TIMER_NAME) {
        if (timer_id != refresh_result_timer_id_) {
            BROOKESIA_LOGW(
                "Ignore stale App Store refresh result timer: timer_id(%1%), pending(%2%)",
                timer_id, refresh_result_timer_id_
            );
            return {};
        }
        refresh_result_timer_id_ = system::core::INVALID_TIMER_ID;
        process_pending_refresh_result(context);
        return {};
    }

    if (name != REFRESH_ICON_TIMER_NAME) {
        return {};
    }
    if (timer_id != refresh_icon_timer_id_) {
        BROOKESIA_LOGW(
            "Ignore stale App Store refresh icon timer: timer_id(%1%), pending(%2%)",
            timer_id, refresh_icon_timer_id_
        );
        return {};
    }

    refresh_icon_timer_id_ = system::core::INVALID_TIMER_ID;
    return process_refresh_icon_step(context);
}

std::expected<void, std::string> AppStoreApp::subscribe_actions(system::core::AppContext &context)
{
    for (const auto *action : {
                ACTION_REFRESH, ACTION_TAB_STORE, ACTION_TAB_LOCAL, ACTION_TAB_INSTALLED,
                ACTION_PAGE_PREV, ACTION_PAGE_NEXT
            }) {
        auto result = context.gui().subscribe_action(action);
        if (!result) {
            return result;
        }
    }

    primary_action_connection_ = context.gui().subscribe_action(
                                     ACTION_PRIMARY,
    [this](const gui::Event & event) {
        handle_primary_action(event);
    }
                                 );
    if (!primary_action_connection_.connected()) {
        return std::unexpected("Failed to subscribe App Store item actions");
    }
    return {};
}
