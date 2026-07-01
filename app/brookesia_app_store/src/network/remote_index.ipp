/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

std::expected<void, std::string> AppStoreApp::load_cached_index(system::core::AppContext &context)
{
    std::optional<std::string> last_error;
    for (const auto &path : cache_file_candidates(context, INDEX_CACHE_FILE)) {
        const auto cached_json = read_text_file(path);
        if (cached_json.empty()) {
            continue;
        }
        BROOKESIA_LOGI("Loading App Store index cache: %1%", path.generic_string());
        auto parse_result = parse_index_json(context, cached_json, configured_index_url(), IndexLoadMode::CacheOnly);
        if (parse_result) {
            return {};
        }
        last_error = "Failed to parse cached App Store index " + path.generic_string() + ": " + parse_result.error();
        BROOKESIA_LOGW("%1%", *last_error);
    }
    if (last_error) {
        return std::unexpected(*last_error);
    }
    return std::unexpected("No cached App Store index");
}

std::expected<void, std::string> AppStoreApp::start_remote_refresh(system::core::AppContext &context)
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
        return {};
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
    (void)refresh_ui(context);

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
    refresh_icon_request_id_ = 0;
    refresh_icon_index_ = 0;
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
    auto request_id = HttpHelper::call_function_sync<double>(
                          HttpHelper::FunctionId::RequestAsync,
                          BROOKESIA_DESCRIBE_TO_JSON(request).as_object()
                      );
    if (!request_id) {
        if (is_http_time_syncing_error(request_id.error())) {
            begin_time_sync_wait(context);
            return {};
        }
        finish_remote_refresh_with_error(context, "HTTP request failed: " + request_id.error());
        return {};
    }
    refresh_request_id_ = static_cast<uint64_t>(*request_id);
    start_refresh_request_watchdog(context, request.timeout_ms, request.retry_count);
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

    refresh_installed_apps(context);
    clear_entry_views(context);
    unregister_icons(context);
    entries_.clear();
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
        entries_.push_back(std::move(entry));
    }
    apply_installed_state();
    for (auto &entry : entries_) {
        entry.icon_resource_id = register_cached_icon(context, {entry.package_name, entry.manifest_id});
    }
    status_text_ = entries_.empty() ? tr("status.no_apps_index") :
                   tr("status.ready");

    auto populate_result = populate_entries(context);
    if (!populate_result) {
        return populate_result;
    }
    return {};
}
