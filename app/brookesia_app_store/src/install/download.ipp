/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

void AppStoreApp::start_or_cancel_download(system::core::AppContext &context, size_t index)
{
    if (index >= entries_.size()) {
        return;
    }
    auto &entry = entries_[index];
    if (refresh_icon_purpose_ == IconUpdatePurpose::LazyVisiblePage) {
        cancel_refresh_icon_update(context);
    }
    cancel_size_metadata_request(entry);
    if (entry.downloading) {
        const auto request_id = entry.download_request_id;
        cancel_download(entry);
        if (message_dialog_purpose_ == MessageDialogPurpose::Download &&
                download_dialog_request_id_ == request_id) {
            hide_message_dialog_if_visible(context);
        }
        (void)refresh_entry(context, index);
        return;
    }
    if (is_store_install_pending(index)) {
        return;
    }
    if (entry.metadata_loading) {
        return;
    }
    if (entry.installed && !entry.update_available) {
        return;
    }
    if (entry.download_url.empty() || entry.download_size == 0) {
        apply_cached_metadata(context, entry);
    }
    if (entry.cached && !entry.cached_package_path.empty()) {
        install_store_package(context, index, entry.cached_package_path);
        return;
    }
    if (entry.download_url.empty()) {
        start_metadata_request(context, index);
        return;
    }
    start_download(context, index);
}

void AppStoreApp::start_metadata_request(system::core::AppContext &context, size_t index)
{
    if (!http_available_ || index >= entries_.size()) {
        return;
    }
    auto &entry = entries_[index];
    if ((entry.installed && !entry.update_available) || entry.package_name.empty() || entry.latest_version.empty() ||
            entry.metadata_url.empty() || entry.schema_error == "manifest_id must match package_name") {
        return;
    }

    show_download_preparing_dialog(context, entry);
    auto request = make_get_request(entry.metadata_url);
    request.max_response_size = METADATA_MAX_RESPONSE_SIZE;
    entry.schema_error.clear();
    entry.metadata_request_id = 0;
    entry.metadata_loading = true;
    entry.downloaded = 0;
    entry.total = 0;
    start_metadata_watchdog(context, entry);
    (void)refresh_entry(context, index);

    auto success_handler = [this, index](uint64_t request_id) {
        if (context_ == nullptr || index >= entries_.size() || !entries_[index].metadata_loading ||
                entries_[index].metadata_request_id != 0) {
            if (!HttpHelper::call_function_async(
                        HttpHelper::FunctionId::CancelRequest,
                        static_cast<double>(request_id)
                    )) {
                BROOKESIA_LOGW("Failed to submit cancel for stale metadata request %1%", request_id);
            }
            return;
        }
        auto &entry = entries_[index];
        entry.metadata_request_id = request_id;
        download_dialog_request_id_ = request_id;
        (void)refresh_entry(*context_, index);
    };
    auto failure_handler = [this, index](std::string error_message) {
        if (context_ == nullptr || index >= entries_.size() || !entries_[index].metadata_loading ||
                entries_[index].metadata_request_id != 0) {
            return;
        }
        auto &entry = entries_[index];
        entry.metadata_loading = false;
        entry.schema_error = "Metadata request failed: " + error_message;
        stop_metadata_watchdog(entry);
        show_download_failed_dialog(*context_, entry, entry.schema_error);
        (void)refresh_entry(*context_, index);
    };
    if (!submit_http_request_async(context, std::move(request), std::move(success_handler), std::move(failure_handler))) {
        entry.metadata_loading = false;
        entry.schema_error = "Metadata request failed: submit failed";
        stop_metadata_watchdog(entry);
        show_download_failed_dialog(context, entry, entry.schema_error);
        (void)refresh_entry(context, index);
    }
}

bool AppStoreApp::start_size_metadata_request(system::core::AppContext &context, size_t index)
{
    if (!http_available_ || index >= entries_.size()) {
        return false;
    }
    auto &entry = entries_[index];
    if (!should_fetch_size_metadata(entry)) {
        return false;
    }

    auto request = make_get_request(entry.metadata_url);
    request.max_response_size = METADATA_MAX_RESPONSE_SIZE;
    entry.size_metadata_request_id = 0;
    entry.size_metadata_loading = true;
    entry.size_metadata_failed = false;

    auto success_handler = [this, index](uint64_t request_id) {
        if (context_ == nullptr || index >= entries_.size() || !entries_[index].size_metadata_loading ||
                entries_[index].size_metadata_request_id != 0) {
            if (!HttpHelper::call_function_async(
                        HttpHelper::FunctionId::CancelRequest,
                        static_cast<double>(request_id)
                    )) {
                BROOKESIA_LOGW("Failed to submit cancel for stale size metadata request %1%", request_id);
            }
            return;
        }
        entries_[index].size_metadata_request_id = request_id;
    };
    auto failure_handler = [this, index](std::string error_message) {
        if (context_ == nullptr || index >= entries_.size() || !entries_[index].size_metadata_loading ||
                entries_[index].size_metadata_request_id != 0) {
            return;
        }
        auto &entry = entries_[index];
        entry.size_metadata_loading = false;
        if (error_message.find("Too many concurrent HTTP requests") != std::string::npos) {
            BROOKESIA_LOGD(
                "Delay App Store size metadata request: package(%1%), error(%2%)",
                entry.package_name, error_message
            );
            schedule_size_metadata_step(*context_, SIZE_METADATA_RETRY_DELAY_MS);
            return;
        }

        entry.size_metadata_failed = true;
        BROOKESIA_LOGW(
            "Failed to start App Store size metadata request: package(%1%), error(%2%)",
            entry.package_name, error_message
        );
    };
    if (!submit_http_request_async(context, std::move(request), std::move(success_handler), std::move(failure_handler))) {
        entry.size_metadata_loading = false;
        entry.size_metadata_failed = true;
        BROOKESIA_LOGW("Failed to submit App Store size metadata request: package(%1%)", entry.package_name);
        return false;
    }
    return true;
}

void AppStoreApp::start_download(system::core::AppContext &context, size_t index)
{
    if (!http_available_ || index >= entries_.size()) {
        return;
    }
    auto &entry = entries_[index];
    if ((entry.installed && !entry.update_available) || entry.package_name.empty() || entry.download_url.empty() ||
            entry.schema_error == "manifest_id must match package_name") {
        return;
    }

    if (storage_capacity_known_ && entry.download_size > 0) {
        const uint64_t required_bytes = entry.download_size + DOWNLOAD_FREE_SPACE_MARGIN_BYTES;
        if (storage_free_bytes_ < required_bytes) {
            BROOKESIA_LOGW(
                "Skip App Store download due to insufficient space: package(%1%), required(%2%), free(%3%)",
                entry.package_name, required_bytes, storage_free_bytes_
            );
            show_insufficient_space_dialog(context, entry, required_bytes);
            (void)refresh_entry(context, index);
            return;
        }
    }

    auto output = writable_cache_file(context, package_cache_relative_path(entry, true));
    if (!output) {
        entry.schema_error = "Failed to create download directory";
        show_download_failed_dialog(context, entry, entry.schema_error);
        (void)refresh_entry(context, index);
        return;
    }
    remove_cache_file_if_exists(*output, "stale partial package before download");

    entry.downloaded = 0;
    entry.total = entry.download_size > 0 ? static_cast<uint32_t>(std::min<uint64_t>(
                      entry.download_size,
                      std::numeric_limits<uint32_t>::max()
                  )) : 0;
    show_download_preparing_dialog(context, entry);

    auto request = make_get_request(entry.download_url);
    request.download_path = output->generic_string();
    request.max_file_size = BPK_MAX_FILE_SIZE;
    entry.schema_error.clear();
    entry.download_request_id = 0;
    entry.download_path = *output;
    entry.downloading = true;
    entry.metadata_loading = false;
    entry.metadata_request_id = 0;
    entry.installed = false;
    ensure_download_progress_dialog(context, entry);
    start_download_watchdog(context, entry);
    (void)refresh_entry(context, index);

    const auto output_path = *output;
    auto success_handler = [this, index, output_path](uint64_t request_id) {
        if (context_ == nullptr || index >= entries_.size() || !entries_[index].downloading ||
                entries_[index].download_request_id != 0 || entries_[index].download_path != output_path) {
            if (!HttpHelper::call_function_async(
                        HttpHelper::FunctionId::CancelRequest,
                        static_cast<double>(request_id)
                    )) {
                BROOKESIA_LOGW("Failed to submit cancel for stale download request %1%", request_id);
            }
            remove_cache_file_if_exists(output_path, "stale download request submitted");
            return;
        }
        auto &entry = entries_[index];
        entry.download_request_id = request_id;
        terminal_download_request_ids_.erase(request_id);
        download_dialog_request_id_ = request_id;
        start_download_watchdog(*context_, entry);
        ensure_download_progress_dialog(*context_, entry);
        (void)refresh_entry(*context_, index);
    };
    auto failure_handler = [this, index, output_path](std::string error_message) {
        if (context_ == nullptr || index >= entries_.size() || !entries_[index].downloading ||
                entries_[index].download_request_id != 0 || entries_[index].download_path != output_path) {
            remove_cache_file_if_exists(output_path, "download request submit failed");
            return;
        }
        auto &entry = entries_[index];
        entry.downloading = false;
        entry.schema_error = "Download failed: " + error_message;
        stop_download_watchdog(entry);
        show_download_failed_dialog(*context_, entry, entry.schema_error);
        remove_cache_file_if_exists(output_path, "download request submit failed");
        entry.download_path.clear();
        entry.downloaded = 0;
        entry.total = 0;
        (void)refresh_entry(*context_, index);
    };
    if (!submit_http_request_async(context, std::move(request), std::move(success_handler), std::move(failure_handler))) {
        entry.downloading = false;
        entry.schema_error = "Download failed: submit failed";
        stop_download_watchdog(entry);
        show_download_failed_dialog(context, entry, entry.schema_error);
        remove_cache_file_if_exists(*output, "download request submit failed");
        entry.download_path.clear();
        entry.downloaded = 0;
        entry.total = 0;
        (void)refresh_entry(context, index);
    }
}

std::expected<void, std::string> AppStoreApp::apply_metadata_json_to_entry(
    StoreEntry &entry,
    std::string_view json,
    bool require_download_url,
    bool require_size,
    bool require_identity
) const
{
    boost::system::error_code error_code;
    auto parsed = boost::json::parse(json, error_code);
    if (error_code || !parsed.is_object()) {
        return std::unexpected("App metadata is not a JSON object");
    }

    const auto &root = parsed.as_object();
    const auto package_name = get_string_field(root, "package_name");
    const auto version = get_string_field(root, "version");
    if (require_identity && package_name.empty()) {
        return std::unexpected("Metadata missing package_name");
    }
    if (!package_name.empty() && package_name != entry.package_name) {
        return std::unexpected("Metadata package_name mismatch");
    }
    if (require_identity && version.empty()) {
        return std::unexpected("Metadata missing version");
    }
    if (!version.empty() && !entry.latest_version.empty() && version != entry.latest_version) {
        return std::unexpected("Metadata version mismatch");
    }

    bool updated = false;
    auto download_url = resolve_relative_url(entry.metadata_url, get_string_field(root, "download_url"));
    if (require_download_url && download_url.empty()) {
        return std::unexpected("Metadata missing download_url");
    }
    if (!download_url.empty() && (entry.download_url.empty() || require_download_url)) {
        entry.download_url = std::move(download_url);
        updated = true;
    }

    if (auto size = get_size_field(root, "size_download")) {
        entry.download_size = *size;
        updated = true;
    } else if (require_size) {
        return std::unexpected("Metadata missing size_download");
    }

    const auto sha256 = get_string_field(root, "hash_sha256");
    if (require_download_url || !sha256.empty()) {
        entry.sha256 = sha256;
        updated = true;
    }
    if (!updated && !require_download_url && !require_size) {
        return std::unexpected("Metadata missing cacheable fields");
    }
    return {};
}

void AppStoreApp::apply_cached_metadata(system::core::AppContext &context, StoreEntry &entry)
{
    if (entry.package_name.empty() || entry.latest_version.empty()) {
        return;
    }

    for (const auto &path : cache_file_candidates(context, metadata_cache_relative_path(entry))) {
        const auto cached_json = read_text_file(path);
        if (cached_json.empty()) {
            continue;
        }
        auto result = apply_metadata_json_to_entry(entry, cached_json, false, false, true);
        if (result) {
            BROOKESIA_LOGI("Loaded App Store metadata cache: %1%", path.generic_string());
            entry.size_metadata_failed = false;
            return;
        }
        BROOKESIA_LOGW(
            "Skip invalid App Store metadata cache: path(%1%), error(%2%)",
            path.generic_string(), result.error()
        );
    }
}

void AppStoreApp::write_metadata_cache(
    system::core::AppContext &context,
    const StoreEntry &entry,
    std::string_view json
) const
{
    if (entry.package_name.empty() || json.empty()) {
        return;
    }
    if (!write_cache_text_file(context, metadata_cache_relative_path(entry), json)) {
        BROOKESIA_LOGW("Failed to write App Store metadata cache: package(%1%)", entry.package_name);
    }
}

std::expected<void, std::string> AppStoreApp::parse_metadata_json(
    system::core::AppContext &context,
    size_t index,
    std::string_view json
)
{
    if (index >= entries_.size()) {
        return {};
    }

    auto &entry = entries_[index];
    auto result = apply_metadata_json_to_entry(entry, json, true, false, true);
    if (!result) {
        return result;
    }
    write_metadata_cache(context, entry, json);
    entry.size_metadata_failed = false;
    entry.metadata_loading = false;
    entry.metadata_request_id = 0;
    entry.schema_error.clear();
    start_download(context, index);
    return {};
}

std::expected<void, std::string> AppStoreApp::parse_size_metadata_json(
    system::core::AppContext &context,
    size_t index,
    std::string_view json
)
{
    if (index >= entries_.size()) {
        return {};
    }

    auto &entry = entries_[index];
    auto result = apply_metadata_json_to_entry(entry, json, false, true, false);
    if (!result) {
        return result;
    }
    write_metadata_cache(context, entry, json);
    return {};
}

std::expected<void, std::string> AppStoreApp::install_package(
    system::core::AppContext &context,
    const std::filesystem::path &path,
    std::string_view name
)
{
    const std::string app_name(name);
    prefer_external_install_storage(context);
    auto result = context.system_service().install_runtime_app_package(path.generic_string(), true);
    if (!result) {
        status_text_ = result.error();
        ensure_message_dialog(
            context,
            tr("dialog.install_failed_prefix") + app_name,
            result.error(),
            system::core::MessageDialogIcon::Warning,
            0,
        MessageDialogPurpose::Install, {
            system::core::MessageDialogButton{
                .text = tr("action.close"),
                .role = system::core::MessageDialogButtonRole::Reject,
            },
        }
        );
        return std::unexpected(result.error());
    }

    refresh_installed_apps(context);
    start_local_package_scan(context, false);
    refresh_storage_capacity(context);
    status_text_ = tr("status.installed_prefix") + app_name;
    ensure_message_dialog(
        context,
        tr("dialog.install_success_prefix") + app_name,
        {},
        system::core::MessageDialogIcon::Information,
        INSTALL_SUCCESS_DIALOG_AUTO_CLOSE_MS,
        MessageDialogPurpose::Install
    );
    (void)populate_entries(context);
    return {};
}

void AppStoreApp::install_local_package(system::core::AppContext &context, size_t index)
{
    if (index >= local_packages_.size()) {
        return;
    }
    if (is_local_install_pending(index)) {
        return;
    }
    auto &entry = local_packages_[index];
    if (get_local_package_action(entry) == LocalPackageAction::None) {
        return;
    }
    schedule_package_install(
        context,
        entry.package_path,
        localized(entry.app_names),
        DeferredOperationType::InstallLocalPackage,
        index,
        entry.manifest_id
    );
    (void)refresh_ui(context);
}

void AppStoreApp::install_store_package(
    system::core::AppContext &context,
    size_t index,
    const std::filesystem::path &path
)
{
    if (index >= entries_.size()) {
        return;
    }
    if (is_store_install_pending(index)) {
        return;
    }
    schedule_package_install(
        context,
        path,
        localized(entries_[index].app_names),
        DeferredOperationType::InstallPackage,
        index,
        entries_[index].package_name,
        entries_[index].download_request_id
    );
    (void)refresh_entry(context, index);
}

bool AppStoreApp::schedule_package_install(
    system::core::AppContext &context,
    const std::filesystem::path &path,
    std::string app_name,
    DeferredOperationType type,
    size_t index,
    std::string manifest_id,
    uint64_t download_request_id
)
{
    if (download_request_id == 0 ||
            !transition_download_dialog_to_install_if_current(context, download_request_id, app_name, path)) {
        ensure_message_dialog(
            context,
            tr("dialog.installing_prefix") + app_name,
            path.filename().generic_string(),
            system::core::MessageDialogIcon::Information,
            0,
            MessageDialogPurpose::Install
        );
    }
    const auto install_failed_text = tr("dialog.install_failed_prefix") + app_name;
    auto scheduled = schedule_deferred_operation(
        context,
        DeferredOperation{
            .type = type,
            .index = index,
            .download_request_id = download_request_id,
            .package_path = path,
            .app_name = std::move(app_name),
            .manifest_id = std::move(manifest_id),
        }
    );
    if (!scheduled) {
        BROOKESIA_LOGW("Failed to queue App Store package install: path(%1%)", path.generic_string());
        ensure_message_dialog(
            context,
            install_failed_text,
            path.filename().generic_string(),
            system::core::MessageDialogIcon::Warning,
            0,
            MessageDialogPurpose::Install
        );
        return false;
    }
    return true;
}

void AppStoreApp::uninstall_installed_app(system::core::AppContext &context, size_t index)
{
    if (index >= installed_runtime_apps_.size()) {
        return;
    }
    const auto app_id = installed_runtime_apps_[index].app.app_id;
    const auto manifest_id = installed_runtime_apps_[index].app.manifest.id;
    const auto app_name = system::core::resolve_app_display_name(
                              installed_runtime_apps_[index].app.manifest,
                              current_language()
                          );
    ensure_message_dialog(
        context,
        tr("dialog.uninstalling_prefix") + app_name,
        manifest_id,
        system::core::MessageDialogIcon::Information,
        0,
        MessageDialogPurpose::Uninstall
    );
    DeferredOperation operation;
    operation.type = DeferredOperationType::UninstallInstalledApp;
    operation.index = index;
    operation.app_id = app_id;
    operation.app_name = app_name;
    operation.manifest_id = manifest_id;
    if (!schedule_deferred_operation(context, std::move(operation))) {
        BROOKESIA_LOGW("Failed to queue App Store uninstall: id(%1%)", manifest_id);
    }
    (void)populate_entries(context);
}

void AppStoreApp::uninstall_installed_app(
    system::core::AppContext &context,
    system::core::AppId app_id,
    std::string manifest_id,
    std::string app_name
)
{
    auto result = context.system_service().uninstall_app(app_id);
    if (!result) {
        BROOKESIA_LOGW("Failed to uninstall runtime app: id(%1%), error(%2%)", manifest_id, result.error());
        status_text_ = tr("status.uninstall_failed");
        ensure_message_dialog(
            context,
            tr("dialog.uninstall_failed_prefix") + app_name,
            result.error(),
            system::core::MessageDialogIcon::Warning,
            0,
        MessageDialogPurpose::Uninstall, {
            system::core::MessageDialogButton{
                .text = tr("action.close"),
                .role = system::core::MessageDialogButtonRole::Reject,
            },
        }
        );
        (void)refresh_ui(context);
        return;
    }

    refresh_installed_apps(context);
    start_local_package_scan(context, false);
    refresh_storage_capacity(context);
    status_text_ = tr("status.uninstalled_prefix") + app_name;
    ensure_message_dialog(
        context,
        tr("dialog.uninstall_success_prefix") + app_name,
        {},
        system::core::MessageDialogIcon::Information,
        UNINSTALL_SUCCESS_DIALOG_AUTO_CLOSE_MS,
        MessageDialogPurpose::Uninstall
    );
    (void)populate_entries(context);
}

bool AppStoreApp::is_store_install_pending(size_t index) const
{
    return std::any_of(
        pending_deferred_operations_.begin(),
        pending_deferred_operations_.end(),
        [index](const DeferredOperation &operation) {
            return operation.type == DeferredOperationType::InstallPackage && operation.index == index;
        }
    );
}

bool AppStoreApp::is_local_install_pending(size_t index) const
{
    return std::any_of(
        pending_deferred_operations_.begin(),
        pending_deferred_operations_.end(),
        [index](const DeferredOperation &operation) {
            return operation.type == DeferredOperationType::InstallLocalPackage && operation.index == index;
        }
    );
}

bool AppStoreApp::is_uninstall_pending(system::core::AppId app_id) const
{
    return std::any_of(
        pending_deferred_operations_.begin(),
        pending_deferred_operations_.end(),
        [app_id](const DeferredOperation &operation) {
            return operation.type == DeferredOperationType::UninstallInstalledApp && operation.app_id == app_id;
        }
    );
}

void AppStoreApp::cancel_download(StoreEntry &entry)
{
    stop_metadata_watchdog(entry);
    stop_download_watchdog(entry);
    cancel_size_metadata_request(entry);

    if (entry.metadata_loading && entry.metadata_request_id != 0 && http_available_) {
        const auto request_id = entry.metadata_request_id;
        if (!HttpHelper::call_function_async(
                    HttpHelper::FunctionId::CancelRequest,
                    static_cast<double>(request_id)
                )) {
            BROOKESIA_LOGW("Failed to submit cancel for metadata request %1%", request_id);
        }
    }
    entry.metadata_loading = false;
    entry.metadata_request_id = 0;

    if (!entry.downloading || entry.download_request_id == 0 || !http_available_) {
        entry.downloading = false;
        entry.download_request_id = 0;
        remove_cache_file_if_exists(entry.download_path, "download canceled before active HTTP request");
        entry.download_path.clear();
        entry.downloaded = 0;
        entry.total = 0;
        return;
    }
    const auto request_id = entry.download_request_id;
    mark_download_request_terminal(request_id);
    if (!HttpHelper::call_function_async(
                HttpHelper::FunctionId::CancelRequest,
                static_cast<double>(request_id)
            )) {
        BROOKESIA_LOGW("Failed to submit cancel for download request %1%", request_id);
    }
    entry.downloading = false;
    entry.download_request_id = 0;
    remove_cache_file_if_exists(entry.download_path, "download canceled");
    entry.download_path.clear();
    entry.downloaded = 0;
    entry.total = 0;
}

void AppStoreApp::close_download_dialog_if_current(system::core::AppContext &context, uint64_t request_id)
{
    if (request_id == 0 || message_dialog_purpose_ != MessageDialogPurpose::Download ||
            download_dialog_request_id_ != request_id) {
        return;
    }
    hide_message_dialog_if_visible(context);
}

void AppStoreApp::clear_download_dialog_binding_if_current(uint64_t request_id)
{
    if (request_id != 0 && message_dialog_purpose_ == MessageDialogPurpose::Download &&
            download_dialog_request_id_ == request_id) {
        download_dialog_request_id_ = 0;
    }
}

void AppStoreApp::mark_download_request_terminal(uint64_t request_id)
{
    if (request_id != 0) {
        terminal_download_request_ids_.insert(request_id);
    }
}

bool AppStoreApp::is_download_request_terminal(uint64_t request_id) const
{
    return request_id != 0 && terminal_download_request_ids_.contains(request_id);
}

void AppStoreApp::start_metadata_watchdog(system::core::AppContext &context, StoreEntry &entry)
{
    stop_metadata_watchdog(entry);
    const uint64_t delay_ms = static_cast<uint64_t>(HTTP_TIMEOUT_MS) * 2U + METADATA_REQUEST_TIMEOUT_EXTRA_MS;
    const int timer_delay_ms = static_cast<int>(
                                   std::min<uint64_t>(delay_ms, static_cast<uint64_t>(std::numeric_limits<int>::max()))
                               );
    auto timer = context.timer().start_delayed(METADATA_WATCHDOG_TIMER_NAME, timer_delay_ms);
    if (!timer) {
        BROOKESIA_LOGW("Failed to schedule App Store metadata watchdog: %1%", timer.error());
        return;
    }
    entry.metadata_watchdog_timer_id = *timer;
}

void AppStoreApp::start_download_watchdog(system::core::AppContext &context, StoreEntry &entry)
{
    stop_download_watchdog(entry);
    auto timer = context.timer().start_delayed(DOWNLOAD_WATCHDOG_TIMER_NAME, DOWNLOAD_STALL_TIMEOUT_MS);
    if (!timer) {
        BROOKESIA_LOGW("Failed to schedule App Store download watchdog: %1%", timer.error());
        return;
    }
    entry.download_watchdog_timer_id = *timer;
}

void AppStoreApp::stop_metadata_watchdog(StoreEntry &entry)
{
    if (entry.metadata_watchdog_timer_id == system::core::INVALID_TIMER_ID) {
        return;
    }
    if (context_ != nullptr && !context_->timer().stop(entry.metadata_watchdog_timer_id)) {
        BROOKESIA_LOGW("Failed to stop App Store metadata watchdog: %1%", entry.metadata_watchdog_timer_id);
    }
    entry.metadata_watchdog_timer_id = system::core::INVALID_TIMER_ID;
}

void AppStoreApp::stop_download_watchdog(StoreEntry &entry)
{
    if (entry.download_watchdog_timer_id == system::core::INVALID_TIMER_ID) {
        return;
    }
    if (context_ != nullptr && !context_->timer().stop(entry.download_watchdog_timer_id)) {
        BROOKESIA_LOGW("Failed to stop App Store download watchdog: %1%", entry.download_watchdog_timer_id);
    }
    entry.download_watchdog_timer_id = system::core::INVALID_TIMER_ID;
}

std::expected<void, std::string> AppStoreApp::process_metadata_watchdog_timeout(
    system::core::AppContext &context,
    system::core::TimerId timer_id
)
{
    for (size_t i = 0; i < entries_.size(); ++i) {
        auto &entry = entries_[i];
        if (entry.metadata_watchdog_timer_id != timer_id) {
            continue;
        }

        entry.metadata_watchdog_timer_id = system::core::INVALID_TIMER_ID;
        if (!entry.metadata_loading) {
            return {};
        }
        const auto request_id = entry.metadata_request_id;
        BROOKESIA_LOGW(
            "App Store metadata request timeout: package(%1%), request_id(%2%)",
            entry.package_name, request_id
        );
        if (request_id != 0 && http_available_) {
            if (!HttpHelper::call_function_async(
                        HttpHelper::FunctionId::CancelRequest,
                        static_cast<double>(request_id)
                    )) {
                BROOKESIA_LOGW("Failed to submit cancel for timed out metadata request %1%", request_id);
            }
        }
        entry.schema_error = tr("error.metadata_request_timeout");
        entry.metadata_loading = false;
        entry.metadata_request_id = 0;
        entry.downloaded = 0;
        entry.total = 0;
        show_download_failed_dialog(context, entry, entry.schema_error);
        return refresh_entry(context, i);
    }

    BROOKESIA_LOGW("Ignore stale App Store metadata watchdog timer: timer_id(%1%)", timer_id);
    return {};
}

std::expected<void, std::string> AppStoreApp::process_download_watchdog_timeout(
    system::core::AppContext &context,
    system::core::TimerId timer_id
)
{
    for (size_t i = 0; i < entries_.size(); ++i) {
        auto &entry = entries_[i];
        if (entry.download_watchdog_timer_id != timer_id) {
            continue;
        }

        entry.download_watchdog_timer_id = system::core::INVALID_TIMER_ID;
        if (!entry.downloading) {
            return {};
        }
        const auto request_id = entry.download_request_id;
        BROOKESIA_LOGW(
            "App Store download stalled: package(%1%), request_id(%2%)",
            entry.package_name, request_id
        );
        if (request_id != 0 && http_available_) {
            if (!HttpHelper::call_function_async(
                        HttpHelper::FunctionId::CancelRequest,
                        static_cast<double>(request_id)
                    )) {
                BROOKESIA_LOGW("Failed to submit cancel for stalled download request %1%", request_id);
            }
        }
        entry.schema_error = tr("error.download_timeout");
        mark_download_request_terminal(request_id);
        clear_download_dialog_binding_if_current(request_id);
        entry.downloading = false;
        entry.download_request_id = 0;
        remove_cache_file_if_exists(entry.download_path, "download stalled");
        entry.download_path.clear();
        entry.downloaded = 0;
        entry.total = 0;
        show_download_failed_dialog(context, entry, entry.schema_error);
        return refresh_entry(context, i);
    }

    BROOKESIA_LOGW("Ignore stale App Store download watchdog timer: timer_id(%1%)", timer_id);
    return {};
}

void AppStoreApp::cancel_size_metadata_request(StoreEntry &entry)
{
    if (entry.size_metadata_loading && entry.size_metadata_request_id != 0 && http_available_) {
        const auto request_id = entry.size_metadata_request_id;
        if (!HttpHelper::call_function_async(
                    HttpHelper::FunctionId::CancelRequest,
                    static_cast<double>(request_id)
                )) {
            BROOKESIA_LOGW("Failed to submit cancel for size metadata request %1%", request_id);
        }
    }
    entry.size_metadata_loading = false;
    entry.size_metadata_request_id = 0;
}

void AppStoreApp::cancel_size_metadata_requests()
{
    for (auto &entry : entries_) {
        cancel_size_metadata_request(entry);
    }
}

void AppStoreApp::show_download_preparing_dialog(system::core::AppContext &context, const StoreEntry &entry)
{
    download_dialog_request_id_ = entry.metadata_request_id;
    ensure_message_dialog(
        context,
        tr("dialog.download_preparing_prefix") + localized(entry.app_names),
        entry.package_name,
        system::core::MessageDialogIcon::Information,
        0,
        MessageDialogPurpose::Download
    );
}

void AppStoreApp::ensure_download_progress_dialog(system::core::AppContext &context, const StoreEntry &entry)
{
    if (entry.download_request_id == 0) {
        return;
    }
    download_dialog_request_id_ = entry.download_request_id;
    ensure_message_dialog(
        context,
        tr("dialog.downloading_prefix") + localized(entry.app_names),
        make_download_progress_detail(entry),
        system::core::MessageDialogIcon::Information,
        0,
        MessageDialogPurpose::Download
    );
}

void AppStoreApp::update_download_progress_dialog_if_current(
    system::core::AppContext &context,
    const StoreEntry &entry
)
{
    if (message_dialog_purpose_ != MessageDialogPurpose::Download ||
            entry.download_request_id == 0 ||
            download_dialog_request_id_ != entry.download_request_id) {
        return;
    }

    update_message_dialog_if_visible(
        context,
        tr("dialog.downloading_prefix") + localized(entry.app_names),
        make_download_progress_detail(entry),
        system::core::MessageDialogIcon::Information,
        0,
        MessageDialogPurpose::Download
    );
}

bool AppStoreApp::transition_download_dialog_to_install_if_current(
    system::core::AppContext &context,
    uint64_t request_id,
    std::string_view app_name,
    const std::filesystem::path &package_path
)
{
    if (request_id == 0 || message_dialog_purpose_ != MessageDialogPurpose::Download ||
            download_dialog_request_id_ != request_id) {
        return false;
    }

    ensure_message_dialog(
        context,
        tr("dialog.installing_prefix") + std::string(app_name),
        package_path.filename().generic_string(),
        system::core::MessageDialogIcon::Information,
        0,
        MessageDialogPurpose::Install
    );
    return true;
}

void AppStoreApp::show_download_failed_dialog(
    system::core::AppContext &context,
    const StoreEntry &entry,
    std::string error_message
)
{
    const auto request_id = entry.download_request_id != 0 ? entry.download_request_id : entry.metadata_request_id;
    download_dialog_request_id_ = 0;
    if (request_id != 0 && !is_download_request_terminal(request_id)) {
        download_dialog_request_id_ = request_id;
    }
    ensure_message_dialog(
        context,
        tr("dialog.download_failed_prefix") + localized(entry.app_names),
        std::move(error_message),
        system::core::MessageDialogIcon::Warning,
        0,
    MessageDialogPurpose::Download, {
        system::core::MessageDialogButton{
            .text = tr("action.close"),
            .role = system::core::MessageDialogButtonRole::Reject,
        },
    }
    );
}

void AppStoreApp::show_insufficient_space_dialog(
    system::core::AppContext &context,
    const StoreEntry &entry,
    uint64_t required_bytes
)
{
    download_dialog_request_id_ = 0;
    ensure_message_dialog(
        context,
        tr("dialog.insufficient_space_prefix") + localized(entry.app_names),
        tr("dialog.insufficient_space_required_prefix") + format_bytes(required_bytes) +
        tr("dialog.insufficient_space_available_prefix") + format_bytes(storage_free_bytes_),
        system::core::MessageDialogIcon::Warning,
        0,
    MessageDialogPurpose::Download, {
        system::core::MessageDialogButton{
            .text = tr("action.close"),
            .role = system::core::MessageDialogButtonRole::Reject,
        },
    }
    );
}

std::string AppStoreApp::make_download_progress_detail(const StoreEntry &entry) const
{
    const uint64_t downloaded = entry.downloaded;
    const uint64_t total = entry.total > 0 ? entry.total : entry.download_size;
    if (total > 0) {
        return format_bytes(downloaded) + " / " + format_bytes(total);
    }
    if (downloaded > 0) {
        return format_bytes(downloaded);
    }
    return entry.package_name;
}
