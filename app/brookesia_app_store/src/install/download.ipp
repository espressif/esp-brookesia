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
        auto result = install_package(context, entry.cached_package_path, localized(entry.app_names));
        if (!result) {
            entry.schema_error = result.error();
            (void)refresh_entry(context, index);
        }
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
    auto result = HttpHelper::call_function_sync<double>(
                      HttpHelper::FunctionId::RequestAsync,
                      BROOKESIA_DESCRIBE_TO_JSON(request).as_object()
                  );
    if (!result) {
        entry.schema_error = "Metadata request failed: " + result.error();
        show_download_failed_dialog(context, entry, entry.schema_error);
        (void)refresh_entry(context, index);
        return;
    }

    entry.schema_error.clear();
    entry.metadata_request_id = static_cast<uint64_t>(*result);
    download_dialog_request_id_ = entry.metadata_request_id;
    entry.metadata_loading = true;
    entry.downloaded = 0;
    entry.total = 0;
    (void)refresh_entry(context, index);
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
    auto result = HttpHelper::call_function_sync<double>(
                      HttpHelper::FunctionId::RequestAsync,
                      BROOKESIA_DESCRIBE_TO_JSON(request).as_object()
                  );
    if (!result) {
        if (result.error().find("Too many concurrent HTTP requests") != std::string::npos) {
            BROOKESIA_LOGD(
                "Delay App Store size metadata request: package(%1%), error(%2%)",
                entry.package_name, result.error()
            );
            schedule_size_metadata_step(context, SIZE_METADATA_RETRY_DELAY_MS);
            return true;
        }

        entry.size_metadata_failed = true;
        BROOKESIA_LOGW(
            "Failed to start App Store size metadata request: package(%1%), error(%2%)",
            entry.package_name, result.error()
        );
        return false;
    }

    entry.size_metadata_request_id = static_cast<uint64_t>(*result);
    entry.size_metadata_loading = true;
    entry.size_metadata_failed = false;
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
    ensure_download_progress_dialog(context, entry);

    auto request = make_get_request(entry.download_url);
    request.download_path = output->generic_string();
    request.max_file_size = BPK_MAX_FILE_SIZE;
    auto result = HttpHelper::call_function_sync<double>(
                      HttpHelper::FunctionId::RequestAsync,
                      BROOKESIA_DESCRIBE_TO_JSON(request).as_object()
                  );
    if (!result) {
        entry.schema_error = "Download failed: " + result.error();
        show_download_failed_dialog(context, entry, entry.schema_error);
        remove_cache_file_if_exists(*output, "download request submit failed");
        (void)refresh_entry(context, index);
        return;
    }
    entry.schema_error.clear();
    entry.download_request_id = static_cast<uint64_t>(*result);
    download_dialog_request_id_ = entry.download_request_id;
    entry.download_path = *output;
    entry.downloading = true;
    entry.metadata_loading = false;
    entry.metadata_request_id = 0;
    entry.installed = false;
    ensure_download_progress_dialog(context, entry);
    (void)refresh_entry(context, index);
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
    ensure_message_dialog(
        context,
        tr("dialog.installing_prefix") + app_name,
        path.filename().generic_string(),
        system::core::MessageDialogIcon::Information,
        0,
        MessageDialogPurpose::Install
    );

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
    scan_local_packages(context);
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
    auto &entry = local_packages_[index];
    if (get_local_package_action(entry) == LocalPackageAction::None) {
        return;
    }
    auto result = install_package(context, entry.package_path, localized(entry.app_names));
    if (!result) {
        entry.schema_error = result.error();
        (void)refresh_ui(context);
    }
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
    scan_local_packages(context);
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

void AppStoreApp::cancel_download(StoreEntry &entry)
{
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
    if (entry.download_request_id != 0) {
        download_dialog_request_id_ = entry.download_request_id;
    }
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

void AppStoreApp::show_download_failed_dialog(
    system::core::AppContext &context,
    const StoreEntry &entry,
    std::string error_message
)
{
    const auto request_id = entry.download_request_id != 0 ? entry.download_request_id : entry.metadata_request_id;
    if (request_id != 0) {
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
