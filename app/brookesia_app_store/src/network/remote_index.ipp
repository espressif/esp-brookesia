/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

bool AppStoreApp::start_cached_index_load_async(system::core::AppContext &context)
{
    startup_cache_candidates_ = cache_file_candidates(context, INDEX_CACHE_FILE);
    startup_cache_cursor_ = 0;
    startup_cache_last_error_.clear();
    startup_cache_load_in_progress_ = true;

    if (startup_cache_candidates_.empty()) {
        startup_cache_load_in_progress_ = false;
        startup_cache_last_error_ = "No cached App Store index";
        return false;
    }

    submit_next_cached_index_load(async_generation_);
    return startup_cache_load_in_progress_;
}

void AppStoreApp::submit_next_cached_index_load(uint64_t generation)
{
    if (context_ == nullptr || generation != async_generation_ || !startup_cache_load_in_progress_) {
        return;
    }

    while (startup_cache_cursor_ < startup_cache_candidates_.size()) {
        const auto path = startup_cache_candidates_[startup_cache_cursor_++];
        auto result_handler = [this, generation, path](service::FunctionResult && result) mutable {
            handle_cached_index_load_result(generation, path, std::move(result));
        };
        if (StorageHelper::call_function_async(
                    StorageHelper::FunctionId::FSReadText,
                    path.generic_string(),
                    service::ServiceBase::FunctionResultHandler(std::move(result_handler))
                )) {
            return;
        }

        startup_cache_last_error_ = "Failed to submit cached App Store index read: " + path.generic_string();
        BROOKESIA_LOGW("%1%", startup_cache_last_error_);
    }

    finish_cached_index_load_without_cache(startup_cache_last_error_.empty() ?
                                           "No cached App Store index" : startup_cache_last_error_);
}

void AppStoreApp::handle_cached_index_load_result(
    uint64_t generation,
    std::filesystem::path path,
    service::FunctionResult &&result
)
{
    if (context_ == nullptr || generation != async_generation_ || !startup_cache_load_in_progress_) {
        return;
    }

    auto cached_json = StorageHelper::process_function_result<std::string>(result);
    if (!cached_json || cached_json->empty()) {
        if (!cached_json) {
            startup_cache_last_error_ = "Failed to read cached App Store index " + path.generic_string() + ": " +
                                        cached_json.error();
        }
        submit_next_cached_index_load(generation);
        return;
    }

    BROOKESIA_LOGI("Loading App Store index cache: %1%", path.generic_string());
    auto parse_result = parse_index_json(*context_, *cached_json, configured_index_url(), IndexLoadMode::CacheOnly);
    if (!parse_result) {
        startup_cache_last_error_ = "Failed to parse cached App Store index " + path.generic_string() + ": " +
                                    parse_result.error();
        BROOKESIA_LOGW("%1%", startup_cache_last_error_);
        submit_next_cached_index_load(generation);
        return;
    }

    startup_cache_load_in_progress_ = false;
    startup_load_phase_ = StartupLoadPhase::None;
    startup_cache_candidates_.clear();
    startup_cache_last_error_.clear();
    startup_cache_cursor_ = 0;
    start_local_package_scan(*context_, false);
    if (!configured_index_url().empty()) {
        (void)submit_network_check(*context_, NetworkCheckPurpose::StartupCache);
    }
}

void AppStoreApp::finish_cached_index_load_without_cache(std::string error_message)
{
    if (context_ == nullptr || startup_load_phase_ != StartupLoadPhase::LoadingCache) {
        return;
    }

    startup_cache_load_in_progress_ = false;
    startup_load_phase_ = StartupLoadPhase::None;
    startup_cache_candidates_.clear();
    startup_cache_last_error_.clear();
    startup_cache_cursor_ = 0;

    BROOKESIA_LOGI("App Store cache load skipped or failed: %1%", error_message);
    if (configured_index_url().empty()) {
        status_text_ = tr("error.index_url_not_configured");
        (void)refresh_ui(*context_);
        start_local_package_scan(*context_, false);
        return;
    }

    refresh_in_progress_ = true;
    refresh_request_id_ = 0;
    reset_refresh_icon_state();
    status_text_ = tr("status.checking_network");
    ensure_message_dialog(
        *context_,
        tr("dialog.checking_network"),
        {},
        system::core::MessageDialogIcon::Information,
        0
    );
    if (!submit_network_check(*context_, NetworkCheckPurpose::StartupRemoteRefresh)) {
        finish_remote_refresh_with_error(*context_, tr("error.failed_check_network"));
    }
    (void)refresh_ui(*context_);
    start_local_package_scan(*context_, false);
}

void AppStoreApp::show_remote_refresh_pending_dialog(system::core::AppContext &context)
{
    if (refresh_in_progress_) {
        ensure_message_dialog(
            context,
            refresh_dialog_message_.empty() ?
            tr("dialog.refreshing_app_list") : refresh_dialog_message_,
            {},
            system::core::MessageDialogIcon::Information,
            0
        );
        return;
    }

    status_text_ = tr("status.checking_network");
    ensure_message_dialog(
        context,
        tr("dialog.checking_network"),
        {},
        system::core::MessageDialogIcon::Information,
        0
    );
    (void)refresh_ui(context);
}

std::expected<void, std::string> AppStoreApp::start_remote_refresh(system::core::AppContext &context)
{
    if (refresh_in_progress_) {
        show_remote_refresh_pending_dialog(context);
        return {};
    }

    stop_size_metadata_timer(context);
    cancel_size_metadata_requests();
    cancel_refresh_icon_update(context);
    refresh_in_progress_ = true;
    refresh_request_id_ = 0;
    show_remote_refresh_pending_dialog(context);

    if (!submit_network_check(context, NetworkCheckPurpose::ManualRemoteRefresh)) {
        finish_remote_refresh_with_error(context, tr("error.failed_check_network"));
    }
    return {};
}

std::expected<void, std::string> AppStoreApp::start_remote_refresh_request(system::core::AppContext &context)
{
    if (!http_binding_.is_valid()) {
        bind_http_service();
        subscribe_http_events();
    }
    if (!http_available_) {
        finish_remote_refresh_with_error(context, tr("error.http_unavailable"));
        return {};
    }

    const auto index_url = configured_index_url();
    if (index_url.empty()) {
        finish_remote_refresh_with_error(context, tr("error.index_url_not_configured"));
        return {};
    }

    refresh_request_id_ = 0;
    reset_refresh_icon_state();
    status_text_ = tr("status.refreshing_app_list");
    ensure_message_dialog(
        context,
        tr("dialog.requesting_app_list"),
        {},
        system::core::MessageDialogIcon::Information,
        0
    );
    (void)refresh_ui(context);

    auto request = make_get_request(index_url);
    request.max_response_size = INDEX_MAX_RESPONSE_SIZE;
    const auto timeout_ms = request.timeout_ms;
    const auto retry_count = request.retry_count;
    auto success_handler = [this, timeout_ms, retry_count](uint64_t request_id) {
        if (context_ == nullptr || !refresh_in_progress_ || refresh_request_id_ != 0) {
            if (!HttpHelper::call_function_async(
                        HttpHelper::FunctionId::CancelRequest,
                        static_cast<double>(request_id)
                    )) {
                BROOKESIA_LOGW("Failed to submit cancel for stale App Store refresh request %1%", request_id);
            }
            return;
        }
        refresh_request_id_ = request_id;
        start_refresh_request_watchdog(*context_, timeout_ms, retry_count);
    };
    auto failure_handler = [this](std::string error_message) {
        if (context_ == nullptr) {
            return;
        }
        if (is_http_time_syncing_error(error_message)) {
            begin_time_sync_wait(*context_);
            return;
        }
        finish_remote_refresh_with_error(*context_, "HTTP request failed: " + error_message);
    };
    if (!submit_http_request_async(context, std::move(request), std::move(success_handler), std::move(failure_handler))) {
        finish_remote_refresh_with_error(context, "HTTP request failed: submit failed");
    }
    return {};
}

std::expected<void, std::string> AppStoreApp::parse_index_json(
    system::core::AppContext &context,
    std::string_view json,
    std::string_view index_url,
    IndexLoadMode mode
)
{
    (void)mode;
    boost::system::error_code error_code;
    auto parsed = boost::json::parse(json, error_code);
    if (error_code || !parsed.is_object()) {
        return std::unexpected("App Store index is not a JSON object");
    }
    const auto &root = parsed.as_object();
    auto apps_it = root.find("apps");
    if (apps_it == root.end() || !apps_it->value().is_array()) {
        return std::unexpected("App Store index missing apps[]");
    }

    clear_entry_views(context);
    unregister_icons(context);
    entries_.clear();
    terminal_download_request_ids_.clear();
    for (const auto &value : apps_it->value().as_array()) {
        if (!value.is_object()) {
            continue;
        }
        const auto &object = value.as_object();
        StoreEntry entry;
        entry.package_name = get_string_field(object, "package_name");
        entry.manifest_id = get_string_field(object, "manifest_id");
        if (entry.manifest_id.empty()) {
            entry.manifest_id = entry.package_name;
        }
        entry.latest_version = get_string_field(object, "latest_version");
        entry.icon_url = resolve_relative_url(index_url, get_string_field(object, "icon_url"));
        entry.download_url = resolve_relative_url(index_url, get_string_field(object, "bpk_url"));
        if (!entry.package_name.empty() && !entry.latest_version.empty()) {
            entry.metadata_url = make_metadata_url(index_url, entry.package_name, entry.latest_version);
        }
        entry.updated_at = get_string_field(object, "updated_at");
        entry.categories = get_string_array_field(object, "categories");
        if (auto size = get_size_field(object, "size_download")) {
            entry.download_size = *size;
        }
        if (auto it = object.find("app_name"); it != object.end()) {
            entry.app_names = parse_localized_map(it->value());
        }
        if (auto it = object.find("description"); it != object.end()) {
            entry.descriptions = parse_localized_map(it->value());
        }
        if (entry.package_name.empty()) {
            entry.schema_error = "Missing package_name";
        } else if (!entry.manifest_id.empty() && entry.manifest_id != entry.package_name) {
            entry.schema_error = "manifest_id must match package_name";
        } else if (entry.latest_version.empty()) {
            entry.schema_error = "Missing latest_version";
        }
        apply_cached_metadata(context, entry);
        entries_.push_back(std::move(entry));
    }
    apply_installed_state();
    status_text_ = entries_.empty() ? tr("status.no_apps_index") :
                   tr("status.ready");

    auto populate_result = populate_entries(context);
    if (!populate_result) {
        return populate_result;
    }
    return {};
}
