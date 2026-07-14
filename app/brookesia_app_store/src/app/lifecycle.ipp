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
        // Keep the JSON UI DOM warm by default; Kconfig lets PSRAM-constrained builds opt out.
        .preload_dom = BROOKESIA_APP_STORE_ENABLE_PRELOAD_DOM,
        .icon_id = APP_ICON_ID,
        .supported_systems = {},
        .icon_path = APP_ICON_PATH,
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
    terminal_download_request_ids_.clear();
    pending_delete_success_package_name_.clear();
    pending_delete_success_package_path_.clear();
    ++async_generation_;
    if (auto i18n_result = refresh_i18n(context); !i18n_result) {
        BROOKESIA_LOGW("Failed to load App Store i18n: %1%", i18n_result.error());
    }
    auto result = subscribe_actions(context);
    if (!result) {
        context_ = nullptr;
        return result;
    }

    status_text_ = tr("status.loading_cache");
    auto refresh_result = refresh_ui(context);
    if (!refresh_result) {
        context_ = nullptr;
        return refresh_result;
    }
    schedule_startup_load(context);
    return {};
}

std::expected<void, std::string> AppStoreApp::on_stop(system::core::AppContext &context)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    ++async_generation_;
    cancel_deferred_operation(context);
    cancel_startup_load(context);
    cancel_local_package_scan(context);
    cancel_remote_refresh();
    stop_view_mode_load_timer(context);
    stop_refresh_icon_timer(context);
    stop_size_metadata_timer(context);
    stop_time_sync_timers(context);
    hide_message_dialog_if_visible(context);
    for (auto &entry : entries_) {
        cancel_download(entry);
    }
    primary_action_connection_.disconnect();
    local_delete_action_connection_.disconnect();
    disconnect_http_events();
    stop_http_event_timer(context);
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
    pending_delete_local_package_name_.clear();
    pending_delete_local_package_path_.clear();
    pending_delete_success_package_name_.clear();
    pending_delete_success_package_path_.clear();
    terminal_download_request_ids_.clear();
    refresh_request_id_ = 0;
    reset_refresh_icon_state();
    startup_cache_candidates_.clear();
    startup_cache_last_error_.clear();
    startup_cache_cursor_ = 0;
    startup_load_phase_ = StartupLoadPhase::None;
    startup_cache_load_in_progress_ = false;
    local_package_scan_dirs_.clear();
    local_package_scan_candidates_.clear();
    local_package_scan_results_.clear();
    local_package_scan_seen_keys_.clear();
    local_package_scan_dir_cursor_ = 0;
    local_package_scan_candidate_cursor_ = 0;
    local_package_scan_phase_ = LocalPackageScanPhase::None;
    local_package_scan_in_progress_ = false;
    local_package_scan_refresh_view_ = false;
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
        if (!pending_deferred_operations_.empty()) {
            return {};
        }
        if (view_mode_ == ViewMode::Store) {
            show_remote_refresh_pending_dialog(context);
        } else {
            status_text_ = view_mode_ == ViewMode::Local ?
                           tr("status.loading_local_packages") :
                           tr("status.loading_installed_apps");
            ensure_message_dialog(
                context,
                status_text_,
                {},
                system::core::MessageDialogIcon::Information,
                0
            );
            (void)refresh_ui(context);
        }
        DeferredOperation operation;
        operation.type = DeferredOperationType::Refresh;
        operation.view_mode = view_mode_;
        schedule_deferred_operation(context, std::move(operation));
        return {};
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
    if (name == STARTUP_LOAD_TIMER_NAME) {
        if (timer_id != startup_load_timer_id_) {
            BROOKESIA_LOGW(
                "Ignore stale App Store startup load timer: timer_id(%1%), pending(%2%)",
                timer_id, startup_load_timer_id_
            );
            return {};
        }
        startup_load_timer_id_ = system::core::INVALID_TIMER_ID;
        return process_startup_load(context);
    }

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

    if (name == DEFERRED_OPERATION_TIMER_NAME) {
        return process_deferred_operation(context, timer_id);
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

    if (name == METADATA_WATCHDOG_TIMER_NAME) {
        return process_metadata_watchdog_timeout(context, timer_id);
    }

    if (name == DOWNLOAD_WATCHDOG_TIMER_NAME) {
        return process_download_watchdog_timeout(context, timer_id);
    }

    if (name == HTTP_EVENT_TIMER_NAME) {
        {
            std::lock_guard lock(pending_http_events_mutex_);
            if (timer_id != http_event_timer_id_) {
                BROOKESIA_LOGW(
                    "Ignore stale App Store HTTP event timer: timer_id(%1%), pending(%2%)",
                    timer_id, http_event_timer_id_
                );
                return {};
            }
            http_event_timer_id_ = system::core::INVALID_TIMER_ID;
        }
        process_pending_http_events(context);
        return {};
    }

    if (name == SIZE_METADATA_TIMER_NAME) {
        if (timer_id != size_metadata_timer_id_) {
            BROOKESIA_LOGW(
                "Ignore stale App Store size metadata timer: timer_id(%1%), pending(%2%)",
                timer_id, size_metadata_timer_id_
            );
            return {};
        }
        size_metadata_timer_id_ = system::core::INVALID_TIMER_ID;
        process_size_metadata_step(context);
        return {};
    }

    if (name == LOCAL_PACKAGE_SCAN_TIMER_NAME) {
        if (timer_id != local_package_scan_timer_id_) {
            BROOKESIA_LOGW(
                "Ignore stale App Store local package scan timer: timer_id(%1%), pending(%2%)",
                timer_id, local_package_scan_timer_id_
            );
            return {};
        }
        local_package_scan_timer_id_ = system::core::INVALID_TIMER_ID;
        return process_local_package_scan_step(context);
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

void AppStoreApp::schedule_startup_load(system::core::AppContext &context)
{
    cancel_startup_load(context);
    startup_load_phase_ = StartupLoadPhase::Prepare;

    auto timer = context.timer().start_delayed(STARTUP_LOAD_TIMER_NAME, STARTUP_LOAD_DELAY_MS);
    if (!timer) {
        BROOKESIA_LOGW("Failed to schedule App Store startup load: %1%", timer.error());
        (void)process_startup_load(context);
        return;
    }
    startup_load_timer_id_ = *timer;
}

std::expected<void, std::string> AppStoreApp::process_startup_load(system::core::AppContext &context)
{
    if (startup_load_phase_ != StartupLoadPhase::Prepare) {
        return {};
    }

    prefer_external_install_storage(context);
    refresh_installed_apps(context);
    bind_device_service();
    bind_storage_service();
    if (!configured_index_url().empty()) {
        bind_http_service();
        subscribe_http_events();
    }
    refresh_storage_capacity(context);

    startup_load_phase_ = StartupLoadPhase::LoadingCache;
    if (!start_cached_index_load_async(context)) {
        finish_cached_index_load_without_cache(startup_cache_last_error_.empty() ?
                                               "No cached App Store index" : startup_cache_last_error_);
    }
    return {};
}

void AppStoreApp::stop_startup_load_timer(system::core::AppContext &context)
{
    if (startup_load_timer_id_ == system::core::INVALID_TIMER_ID) {
        return;
    }
    if (!context.timer().stop(startup_load_timer_id_)) {
        BROOKESIA_LOGW("Failed to stop App Store startup load timer: %1%", startup_load_timer_id_);
    }
    startup_load_timer_id_ = system::core::INVALID_TIMER_ID;
}

void AppStoreApp::cancel_startup_load(system::core::AppContext &context)
{
    stop_startup_load_timer(context);
    startup_load_phase_ = StartupLoadPhase::None;
    startup_cache_load_in_progress_ = false;
    startup_cache_candidates_.clear();
    startup_cache_last_error_.clear();
    startup_cache_cursor_ = 0;
}

bool AppStoreApp::schedule_deferred_operation(system::core::AppContext &context, DeferredOperation operation)
{
    if (operation.type == DeferredOperationType::None) {
        return false;
    }
    if (operation.type == DeferredOperationType::Refresh) {
        if (!pending_deferred_operations_.empty() ||
                deferred_operation_timer_id_ != system::core::INVALID_TIMER_ID) {
            BROOKESIA_LOGD("Ignore App Store refresh while another deferred operation is pending");
            return false;
        }
    } else {
        const bool stop_refresh_timer = deferred_operation_timer_id_ != system::core::INVALID_TIMER_ID &&
                                        !pending_deferred_operations_.empty() &&
                                        pending_deferred_operations_.front().type == DeferredOperationType::Refresh;
        if (stop_refresh_timer && !context.timer().stop(deferred_operation_timer_id_)) {
            BROOKESIA_LOGW("Failed to stop App Store deferred refresh timer: %1%", deferred_operation_timer_id_);
        }
        if (stop_refresh_timer) {
            deferred_operation_timer_id_ = system::core::INVALID_TIMER_ID;
        }
        pending_deferred_operations_.erase(
            std::remove_if(
                pending_deferred_operations_.begin(),
                pending_deferred_operations_.end(),
                [](const DeferredOperation &pending) {
                    return pending.type == DeferredOperationType::Refresh;
                }
            ),
            pending_deferred_operations_.end()
        );
    }

    operation.generation = async_generation_;
    pending_deferred_operations_.push_back(std::move(operation));
    return start_deferred_operation_timer(context);
}

bool AppStoreApp::start_deferred_operation_timer(system::core::AppContext &context)
{
    if (pending_deferred_operations_.empty()) {
        return true;
    }
    if (deferred_operation_timer_id_ != system::core::INVALID_TIMER_ID) {
        return true;
    }

    auto timer = context.timer().start_delayed(DEFERRED_OPERATION_TIMER_NAME, DEFERRED_OPERATION_DELAY_MS);
    if (!timer) {
        BROOKESIA_LOGW("Failed to schedule App Store deferred operation: %1%", timer.error());
        auto immediate_operation = std::move(pending_deferred_operations_.front());
        pending_deferred_operations_.pop_front();
        if (immediate_operation.generation == async_generation_) {
            (void)execute_deferred_operation(context, std::move(immediate_operation));
        }
        return start_deferred_operation_timer(context);
    }
    deferred_operation_timer_id_ = *timer;
    return true;
}

void AppStoreApp::cancel_deferred_operation(system::core::AppContext &context)
{
    if (deferred_operation_timer_id_ != system::core::INVALID_TIMER_ID &&
            !context.timer().stop(deferred_operation_timer_id_)) {
        BROOKESIA_LOGW("Failed to stop App Store deferred operation timer: %1%", deferred_operation_timer_id_);
    }
    deferred_operation_timer_id_ = system::core::INVALID_TIMER_ID;
    pending_deferred_operations_.clear();
}

std::expected<void, std::string> AppStoreApp::process_deferred_operation(
    system::core::AppContext &context,
    system::core::TimerId timer_id
)
{
    if (timer_id != deferred_operation_timer_id_) {
        BROOKESIA_LOGW(
            "Ignore stale App Store deferred operation timer: timer_id(%1%), pending(%2%)",
            timer_id, deferred_operation_timer_id_
        );
        return {};
    }
    deferred_operation_timer_id_ = system::core::INVALID_TIMER_ID;
    if (pending_deferred_operations_.empty()) {
        return {};
    }

    auto operation = std::move(pending_deferred_operations_.front());
    pending_deferred_operations_.pop_front();
    std::expected<void, std::string> result = {};
    if (operation.generation == async_generation_) {
        result = execute_deferred_operation(context, std::move(operation));
    }
    if (!pending_deferred_operations_.empty()) {
        (void)start_deferred_operation_timer(context);
    }
    return result;
}

std::expected<void, std::string> AppStoreApp::execute_deferred_operation(
    system::core::AppContext &context,
    DeferredOperation operation
)
{
    switch (operation.type) {
    case DeferredOperationType::Refresh:
        return execute_deferred_refresh(context, operation.view_mode);
    case DeferredOperationType::InstallLocalPackage: {
        ensure_message_dialog(
            context,
            tr("dialog.installing_prefix") + operation.app_name,
            operation.package_path.filename().generic_string(),
            system::core::MessageDialogIcon::Information,
            0,
            MessageDialogPurpose::Install
        );
        auto result = install_package(context, operation.package_path, operation.app_name);
        if (!result && operation.index < local_packages_.size()) {
            local_packages_[operation.index].schema_error = result.error();
            (void)refresh_ui(context);
        }
        return {};
    }
    case DeferredOperationType::InstallPackage: {
        ensure_message_dialog(
            context,
            tr("dialog.installing_prefix") + operation.app_name,
            operation.package_path.filename().generic_string(),
            system::core::MessageDialogIcon::Information,
            0,
            MessageDialogPurpose::Install
        );
        auto result = install_package(context, operation.package_path, operation.app_name);
        if (!result && operation.index < entries_.size() &&
                entries_[operation.index].package_name == operation.manifest_id) {
            entries_[operation.index].schema_error = result.error();
            (void)refresh_entry(context, operation.index);
        }
        return {};
    }
    case DeferredOperationType::UninstallInstalledApp:
        ensure_message_dialog(
            context,
            tr("dialog.uninstalling_prefix") + operation.app_name,
            operation.manifest_id,
            system::core::MessageDialogIcon::Information,
            0,
            MessageDialogPurpose::Uninstall
        );
        uninstall_installed_app(
            context,
            operation.app_id,
            std::move(operation.manifest_id),
            std::move(operation.app_name)
        );
        return {};
    case DeferredOperationType::DeleteLocalPackage:
        ensure_message_dialog(
            context,
            tr("dialog.deleting_local_package_prefix") + operation.app_name,
            operation.package_path.filename().generic_string(),
            system::core::MessageDialogIcon::Information,
            0,
            MessageDialogPurpose::DeleteLocalPackage
        );
        delete_local_package(context, operation.package_path, std::move(operation.app_name));
        return {};
    case DeferredOperationType::None:
        return {};
    }
    return {};
}

std::expected<void, std::string> AppStoreApp::execute_deferred_refresh(
    system::core::AppContext &context,
    ViewMode requested_view_mode
)
{
    cancel_startup_load(context);
    cancel_local_package_scan(context);
    refresh_installed_apps(context);
    refresh_storage_capacity(context);
    if (requested_view_mode == ViewMode::Store && view_mode_ == ViewMode::Store) {
        start_local_package_scan(context, false);
        auto result = start_remote_refresh(context);
        if (!result) {
            status_text_ = result.error();
            return populate_entries(context);
        }
        return {};
    }
    if (requested_view_mode == ViewMode::Local && view_mode_ == ViewMode::Local) {
        start_local_package_scan(context, true);
        return {};
    }
    return populate_entries(context);
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
    local_delete_action_connection_ = context.gui().subscribe_action(
                                          ACTION_LOCAL_DELETE,
    [this](const gui::Event & event) {
        handle_local_delete_action(event);
    }
                                      );
    if (!local_delete_action_connection_.connected()) {
        return std::unexpected("Failed to subscribe App Store local delete actions");
    }
    return {};
}
