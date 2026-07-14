/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

void AppStoreApp::bind_http_service()
{
    http_available_ = false;
    if (!HttpHelper::is_available()) {
        BROOKESIA_LOGW("HTTP service is unavailable; App Store remote operations are disabled");
        return;
    }
    auto binding = service::ServiceManager::get_instance().bind(HttpHelper::get_name().data());
    if (!binding.is_valid()) {
        BROOKESIA_LOGW("Failed to bind HTTP service");
        return;
    }
    http_binding_ = std::move(binding);
    http_available_ = HttpHelper::is_running();
    if (!http_available_) {
        BROOKESIA_LOGW("HTTP service is not running");
    }
}

void AppStoreApp::release_http_service()
{
    http_binding_.release();
    http_available_ = false;
    http_general_state_.reset();
}

void AppStoreApp::bind_device_service()
{
    device_available_ = false;
    if (device_binding_.is_valid()) {
        device_available_ = DeviceHelper::is_running();
        return;
    }
    if (!DeviceHelper::is_available()) {
        BROOKESIA_LOGW("Device service is unavailable; App Store network checks are disabled");
        return;
    }
    auto binding = service::ServiceManager::get_instance().bind(DeviceHelper::get_name().data());
    if (!binding.is_valid()) {
        BROOKESIA_LOGW("Failed to bind Device service; App Store network checks are disabled");
        return;
    }
    device_binding_ = std::move(binding);
    device_available_ = DeviceHelper::is_running();
    if (!device_available_) {
        BROOKESIA_LOGW("Device service is not running; App Store network checks are disabled");
    }
}

void AppStoreApp::release_device_service()
{
    device_binding_.release();
    device_available_ = false;
}

void AppStoreApp::bind_storage_service()
{
    storage_available_ = false;
    if (!StorageHelper::is_available()) {
        BROOKESIA_LOGW("Storage service is unavailable; App Store storage capacity will be hidden");
        return;
    }
    auto binding = service::ServiceManager::get_instance().bind(StorageHelper::get_name().data());
    if (!binding.is_valid()) {
        BROOKESIA_LOGW("Failed to bind Storage service");
        return;
    }
    storage_binding_ = std::move(binding);
    storage_available_ = StorageHelper::is_running();
}

void AppStoreApp::release_storage_service()
{
    storage_binding_.release();
    storage_available_ = false;
}

void AppStoreApp::disconnect_http_events()
{
    http_event_connections_.clear();
}

void AppStoreApp::subscribe_http_events()
{
    if (!http_available_) {
        return;
    }

    auto on_progress = [this](
                           const std::string &, double request_id, const boost::json::object & progress_json
    ) {
        if (context_ != nullptr) {
            schedule_http_event_processing(*context_, PendingHttpEventType::Progress, request_id, progress_json);
        }
    };
    auto on_completed = [this](
                            const std::string &, double request_id, const boost::json::object & response_json
    ) {
        if (context_ != nullptr) {
            schedule_http_event_processing(*context_, PendingHttpEventType::Completed, request_id, response_json);
        }
    };
    auto on_failed = [this](
                         const std::string &, double request_id, const boost::json::object & response_json
    ) {
        if (context_ != nullptr) {
            schedule_http_event_processing(*context_, PendingHttpEventType::Failed, request_id, response_json);
        }
    };
    auto on_canceled = [this](
                           const std::string &, double request_id, const boost::json::object & response_json
    ) {
        if (context_ != nullptr) {
            schedule_http_event_processing(*context_, PendingHttpEventType::Canceled, request_id, response_json);
        }
    };
    auto on_general_state_changed = [this](const std::string &, const std::string & state) {
        handle_http_general_state_changed(state);
    };

    http_event_connections_.push_back(HttpHelper::subscribe_event(HttpHelper::EventId::RequestProgress, on_progress));
    http_event_connections_.push_back(HttpHelper::subscribe_event(HttpHelper::EventId::RequestCompleted, on_completed));
    http_event_connections_.push_back(HttpHelper::subscribe_event(HttpHelper::EventId::RequestFailed, on_failed));
    http_event_connections_.push_back(HttpHelper::subscribe_event(HttpHelper::EventId::RequestCanceled, on_canceled));
    http_event_connections_.push_back(
        HttpHelper::subscribe_event(HttpHelper::EventId::GeneralStateChanged, on_general_state_changed)
    );
}

std::filesystem::path AppStoreApp::resolve_resource_dir(system::core::AppContext &context) const
{
    std::filesystem::path resource_dir(BROOKESIA_APP_STORE_RESOURCE_DIR);
    if (resource_dir.is_relative()) {
        auto app_paths = context.system_service().get_app_storage_paths();
        if (app_paths && app_paths->internal.available) {
            resource_dir = std::filesystem::path(app_paths->internal.root);
        }
    }
    return resource_dir.lexically_normal();
}

std::vector<system::core::AppStorageDirs> AppStoreApp::app_storage_dirs(system::core::AppContext &context) const
{
    std::vector<system::core::AppStorageDirs> dirs;
    auto append_unique_dir = [&dirs](const system::core::AppStorageDirs & dir) {
        if (!dir.available || dir.cache.empty()) {
            return;
        }
        const auto cache_path = std::filesystem::path(dir.cache).lexically_normal().generic_string();
        const auto duplicate = std::find_if(dirs.begin(), dirs.end(), [&cache_path](const auto & existing) {
            return std::filesystem::path(existing.cache).lexically_normal().generic_string() == cache_path;
        }) != dirs.end();
        if (!duplicate) {
            dirs.push_back(dir);
        }
    };

    auto app_paths = context.system_service().get_app_storage_paths();
    if (!app_paths) {
        BROOKESIA_LOGD("Failed to resolve App Store storage directories: %1%", app_paths.error());
        return dirs;
    }

    for (const auto &external : app_paths->external) {
        append_unique_dir(external);
    }
    append_unique_dir(app_paths->internal);
    return dirs;
}

std::filesystem::path AppStoreApp::cache_dir(system::core::AppContext &context) const
{
    for (const auto &dir : app_storage_dirs(context)) {
        return std::filesystem::path(dir.cache).lexically_normal();
    }
    BROOKESIA_LOGW("Failed to resolve App Store cache directory; using relative fallback");
    return (std::filesystem::path("apps") / APP_ID / "cache").lexically_normal();
}

std::vector<std::filesystem::path> AppStoreApp::cache_dirs(system::core::AppContext &context) const
{
    std::vector<std::filesystem::path> paths;
    for (const auto &dir : app_storage_dirs(context)) {
        append_unique_path(paths, std::filesystem::path(dir.cache));
    }
    if (paths.empty()) {
        append_unique_path(paths, cache_dir(context));
    }
    return paths;
}

std::filesystem::path AppStoreApp::download_dir(system::core::AppContext &context) const
{
    for (const auto &dir : download_dirs(context)) {
        if (directory_is_writable(dir)) {
            return dir.lexically_normal();
        }
    }
    return (cache_dir(context) / APP_CACHE_DIR).lexically_normal();
}

std::vector<std::filesystem::path> AppStoreApp::download_dirs(system::core::AppContext &context) const
{
    std::vector<std::filesystem::path> paths;
    for (const auto &dir : cache_dirs(context)) {
        append_unique_path(paths, dir / APP_CACHE_DIR);
    }
    return paths;
}

std::filesystem::path AppStoreApp::app_cache_relative_dir(std::string_view package_name) const
{
    return (std::filesystem::path(APP_CACHE_DIR) / safe_name(package_name)).lexically_normal();
}

std::filesystem::path AppStoreApp::app_cache_relative_file(
    std::string_view package_name,
    const std::filesystem::path &file_name
) const
{
    return (app_cache_relative_dir(package_name) / file_name).lexically_normal();
}

std::filesystem::path AppStoreApp::metadata_cache_relative_path(const StoreEntry &entry) const
{
    return app_cache_relative_file(entry.package_name, METADATA_CACHE_FILE);
}

std::filesystem::path AppStoreApp::icon_cache_relative_path(std::string_view package_name) const
{
    return app_cache_relative_file(package_name, ICON_CACHE_FILE);
}

std::filesystem::path AppStoreApp::package_cache_relative_path(const StoreEntry &entry, bool partial) const
{
    auto file_name = safe_name(entry.latest_version.empty() ? "latest" : entry.latest_version) + ".bpk";
    if (partial) {
        file_name += BPK_PART_EXTENSION;
    }
    return app_cache_relative_file(entry.package_name, file_name);
}

std::vector<std::filesystem::path> AppStoreApp::cached_icon_file_candidates(
    system::core::AppContext &context,
    const std::vector<std::string> &keys
) const
{
    std::vector<std::filesystem::path> paths;
    for (const auto &key : keys) {
        if (key.empty()) {
            continue;
        }
        for (const auto &base_dir : cache_dirs(context)) {
            append_unique_path(paths, base_dir / icon_cache_relative_path(key));
            append_unique_path(paths, base_dir / ICON_DIR / (safe_name(key) + ".png"));
        }
    }
    return paths;
}

std::vector<std::filesystem::path> AppStoreApp::cache_file_candidates(
    system::core::AppContext &context,
    const std::filesystem::path &relative_path
) const
{
    std::vector<std::filesystem::path> paths;
    for (const auto &base_dir : cache_dirs(context)) {
        append_unique_path(paths, base_dir / relative_path);
    }
    return paths;
}

std::optional<std::filesystem::path> AppStoreApp::writable_cache_file(
    system::core::AppContext &context,
    const std::filesystem::path &relative_path
) const
{
    for (const auto &path : cache_file_candidates(context, relative_path)) {
        auto directory = path;
        if (!relative_path.empty() && relative_path.has_filename()) {
            directory = path.parent_path();
        }
        if (directory_is_writable(directory)) {
            return path.lexically_normal();
        }
        BROOKESIA_LOGD("App Store cache path is not writable, trying fallback: %1%", directory.generic_string());
    }
    return std::nullopt;
}

bool AppStoreApp::write_cache_text_file(
    system::core::AppContext &context,
    const std::filesystem::path &relative_path,
    std::string_view text
) const
{
    for (const auto &path : cache_file_candidates(context, relative_path)) {
        if (write_text_file(path, text)) {
            BROOKESIA_LOGI("Wrote App Store cache file: %1%", path.generic_string());
            return true;
        }
        BROOKESIA_LOGD("Failed to write App Store cache file, trying fallback: %1%", path.generic_string());
    }
    return false;
}

void AppStoreApp::prefer_external_install_storage(system::core::AppContext &context) const
{
    constexpr auto target = system::core::StorageInstallTarget::External;
    const auto layout = context.system_service().get_storage_layout();
    std::string preferred_external_id;
    auto find_available_external = [&layout](std::string_view id) -> const system::core::StorageVolume * {
        if (id.empty())
        {
            return nullptr;
        }
        auto it = std::find_if(layout.external.begin(), layout.external.end(), [id](const auto & volume)
        {
            return volume.id == id && volume.available;
        });
        return (it == layout.external.end()) ? nullptr :&(*it);
    };

    if (const auto *preferred = find_available_external(layout.preferred_external_id); preferred != nullptr) {
        preferred_external_id = preferred->id;
    } else {
        auto it = std::find_if(layout.external.begin(), layout.external.end(), [](const auto & volume) {
            return volume.available;
        });
        if (it != layout.external.end()) {
            preferred_external_id = it->id;
        }
    }

    if (layout.default_install_target == target && layout.preferred_external_id == preferred_external_id) {
        return;
    }

    auto result = context.system_service().set_default_install_storage(
                      target,
                      preferred_external_id
                  );
    if (!result) {
        BROOKESIA_LOGW("Failed to prefer external app install storage: %1%", result.error());
        return;
    }
    BROOKESIA_LOGI(
        "App Store default install storage prefers external: volume(%1%)",
        preferred_external_id.empty() ? std::string("<auto>") : preferred_external_id
    );
}

void AppStoreApp::refresh_storage_capacity(system::core::AppContext &context)
{
    const auto download_text = tr("storage.download_prefix") + download_dir(context).generic_string();
    storage_text_ = download_text + " | " + tr("storage.unavailable");
    if (!storage_available_) {
        return;
    }

    const auto generation = async_generation_;
    auto handler = [this, generation](service::FunctionResult && result) {
        handle_storage_filesystems_result(generation, std::move(result));
    };
    if (!StorageHelper::call_function_async(StorageHelper::FunctionId::GetFileSystems, handler)) {
        BROOKESIA_LOGW("Failed to submit storage filesystems request");
        return;
    }
}

void AppStoreApp::handle_storage_filesystems_result(uint64_t generation, service::FunctionResult &&result)
{
    if (generation != async_generation_ || context_ == nullptr) {
        return;
    }

    auto filesystems_result = StorageHelper::process_function_result<boost::json::array>(result);
    if (!filesystems_result) {
        BROOKESIA_LOGW("Failed to get storage filesystems: %1%", filesystems_result.error());
        (void)refresh_ui(*context_);
        return;
    }

    const auto cache_path = normalize_path_for_prefix(cache_dir(*context_));
    struct StorageMount {
        std::string mount_point;
        std::string root_path;
    };
    std::vector<StorageMount> mounts;
    for (const auto &value : *filesystems_result) {
        if (!value.is_object()) {
            continue;
        }
        const auto &object = value.as_object();
        auto it = object.find("mount_point");
        if (it == object.end()) {
            it = object.find("mountPoint");
        }
        if (it != object.end() && it->value().is_string()) {
            StorageMount mount;
            mount.mount_point = it->value().as_string().c_str();
            auto root_it = object.find("root_path");
            if (root_it == object.end()) {
                root_it = object.find("rootPath");
            }
            mount.root_path = (root_it != object.end() && root_it->value().is_string()) ?
                              std::string(root_it->value().as_string().c_str()) :
                              mount.mount_point;
            mounts.push_back(std::move(mount));
        }
    }
    if (mounts.empty()) {
        BROOKESIA_LOGW("Storage filesystem list is empty");
        (void)refresh_ui(*context_);
        return;
    }

    std::string best_mount_point;
    size_t best_match_length = 0;
    for (const auto &item : mounts) {
        for (const auto &candidate : {
                    item.root_path, item.mount_point
                }) {
            const std::string prefix = normalize_path_for_prefix(candidate);
            if (path_has_prefix(cache_path, prefix) && prefix.size() > best_match_length) {
                best_match_length = prefix.size();
                best_mount_point = normalize_path_for_prefix(item.mount_point);
            }
        }
    }
    if (best_mount_point.empty() && mounts.size() == 1) {
        best_mount_point = normalize_path_for_prefix(mounts.front().mount_point);
    }
    if (best_mount_point.empty()) {
        if (!storage_match_warning_logged_) {
            BROOKESIA_LOGW("No storage filesystem matches App Store cache path: %1%", cache_path);
            storage_match_warning_logged_ = true;
        }
        (void)refresh_ui(*context_);
        return;
    }

    auto handler = [this, generation](service::FunctionResult && capacity_result) {
        handle_storage_capacity_result(generation, std::move(capacity_result));
    };
    if (!StorageHelper::call_function_async(
                StorageHelper::FunctionId::GetFileSystemCapacity,
                best_mount_point,
                handler
            )) {
        BROOKESIA_LOGW("Failed to submit storage capacity request");
        (void)refresh_ui(*context_);
    }
}

void AppStoreApp::handle_storage_capacity_result(uint64_t generation, service::FunctionResult &&result)
{
    if (generation != async_generation_ || context_ == nullptr) {
        return;
    }

    auto capacity_result = StorageHelper::process_function_result<boost::json::object>(result);
    if (!capacity_result) {
        BROOKESIA_LOGW("Failed to get storage capacity: %1%", capacity_result.error());
        (void)refresh_ui(*context_);
        return;
    }
    StorageHelper::FileSystemCapacity capacity;
    if (!BROOKESIA_DESCRIBE_FROM_JSON(*capacity_result, capacity)) {
        BROOKESIA_LOGW("Failed to parse storage capacity");
        (void)refresh_ui(*context_);
        return;
    }
    storage_free_bytes_ = capacity.free_bytes;
    storage_capacity_known_ = true;
    storage_text_ = tr("storage.download_prefix") + download_dir(*context_).generic_string() + " | " +
                    tr("storage.free_prefix") + format_bytes(capacity.free_bytes) + " / " +
                    format_bytes(capacity.total_bytes);
    (void)refresh_ui(*context_);
}

void AppStoreApp::refresh_installed_apps(system::core::AppContext &context)
{
    installed_manifest_ids_.clear();
    installed_version_by_manifest_id_.clear();
    installed_runtime_apps_.clear();
    for (const auto &app : context.system_service().list_apps()) {
        if (!app.manifest.id.empty()) {
            installed_manifest_ids_.insert(app.manifest.id);
            installed_version_by_manifest_id_[app.manifest.id] = app.manifest.version;
        }
        if (app.manifest.kind == system::core::AppKind::Runtime) {
            installed_runtime_apps_.push_back(InstalledAppEntry{
                .app = app,
                .item_path = {},
                .icon_resource_id = {},
                .pending_icon_resource_id = {},
            });
        }
    }
    std::sort(
        installed_runtime_apps_.begin(),
        installed_runtime_apps_.end(),
    [this](const InstalledAppEntry & lhs, const InstalledAppEntry & rhs) {
        const auto lhs_name = system::core::resolve_app_display_name(lhs.app.manifest, current_language());
        const auto rhs_name = system::core::resolve_app_display_name(rhs.app.manifest, current_language());
        if (lhs_name != rhs_name) {
            return lhs_name < rhs_name;
        }
        return lhs.app.manifest.id < rhs.app.manifest.id;
    }
    );
    apply_installed_state();
}

void AppStoreApp::apply_installed_state()
{
    for (auto &entry : entries_) {
        entry.installed = !entry.package_name.empty() && installed_manifest_ids_.contains(entry.package_name);
        entry.installed_version.clear();
        entry.update_available = false;
        if (entry.installed) {
            if (auto version_it = installed_version_by_manifest_id_.find(entry.package_name);
                    version_it != installed_version_by_manifest_id_.end()) {
                entry.installed_version = version_it->second;
            }
            entry.update_available = !entry.latest_version.empty() && !entry.installed_version.empty() &&
                                     compare_versions(entry.latest_version, entry.installed_version) > 0;
        }
        entry.cached = false;
        entry.cached_package_path.clear();
        entry.cached_package_size = 0;
        if (auto it = local_package_by_manifest_id_.find(entry.package_name); it != local_package_by_manifest_id_.end() &&
                it->second < local_packages_.size()) {
            const auto &local_package = local_packages_[it->second];
            if (local_package.schema_error.empty() &&
                    (entry.latest_version.empty() || local_package.version == entry.latest_version)) {
                entry.cached = true;
                entry.cached_package_path = local_package.package_path;
                entry.cached_package_size = local_package.file_size;
            }
        }
        if ((!entry.installed || entry.update_available) && !entry.downloading && !entry.metadata_loading) {
            entry.downloaded = 0;
            entry.total = 0;
        }
    }

    for (auto &entry : local_packages_) {
        const auto installed = !entry.manifest_id.empty() && installed_manifest_ids_.contains(entry.manifest_id);
        entry.installed = installed;
    }
}

void AppStoreApp::rebuild_local_package_index()
{
    apply_installed_state();
    for (auto &entry : local_packages_) {
        entry.icon_resource_id = LOCAL_BPK_ICON_ID;
    }
    std::sort(
        local_packages_.begin(),
        local_packages_.end(),
    [this](const LocalPackageEntry & lhs, const LocalPackageEntry & rhs) {
        if (lhs.installed != rhs.installed) {
            return !lhs.installed;
        }
        const auto lhs_name = localized(lhs.app_names);
        const auto rhs_name = localized(rhs.app_names);
        if (lhs_name != rhs_name) {
            return lhs_name < rhs_name;
        }
        return lhs.file_name < rhs.file_name;
    }
    );
    local_package_by_manifest_id_.clear();
    for (size_t i = 0; i < local_packages_.size(); ++i) {
        const auto &package = local_packages_[i];
        if (package.manifest_id.empty() || !package.schema_error.empty()) {
            continue;
        }
        auto remote_latest = std::string();
        for (const auto &entry : entries_) {
            if (entry.package_name == package.manifest_id) {
                remote_latest = entry.latest_version;
                break;
            }
        }
        auto it = local_package_by_manifest_id_.find(package.manifest_id);
        if (it == local_package_by_manifest_id_.end()) {
            local_package_by_manifest_id_.emplace(package.manifest_id, i);
            continue;
        }
        const auto &current = local_packages_[it->second];
        const auto package_matches_latest = !remote_latest.empty() && package.version == remote_latest;
        const auto current_matches_latest = !remote_latest.empty() && current.version == remote_latest;
        if (package_matches_latest && !current_matches_latest) {
            it->second = i;
        } else if (package_matches_latest == current_matches_latest &&
                   compare_versions(package.version, current.version) > 0) {
            it->second = i;
        }
    }
    apply_installed_state();
}

void AppStoreApp::start_local_package_scan(system::core::AppContext &context, bool refresh_view)
{
    cancel_local_package_scan(context);
    local_package_scan_in_progress_ = true;
    local_package_scan_refresh_view_ = refresh_view;
    local_package_scan_phase_ = LocalPackageScanPhase::StatDirectory;
    local_package_scan_dirs_.clear();
    local_package_scan_candidates_.clear();
    local_package_scan_results_.clear();
    local_package_scan_seen_keys_.clear();
    local_package_scan_dir_cursor_ = 0;
    local_package_scan_candidate_cursor_ = 0;

    const auto writable_packages_dir = download_dir(context);
    if (!StorageHelper::call_function_async(
                StorageHelper::FunctionId::FSMkdir,
                writable_packages_dir.generic_string()
            )) {
        BROOKESIA_LOGW("Failed to submit App Store local package directory mkdir: %1%",
                       writable_packages_dir.generic_string());
    }

    for (const auto &dir : download_dirs(context)) {
        append_unique_path(local_package_scan_dirs_, dir);
    }
    for (const auto &dir : cache_dirs(context)) {
        append_unique_path(local_package_scan_dirs_, dir / DOWNLOAD_DIR);
    }
    auto public_paths = context.system_service().get_public_storage_paths();
    if (!public_paths) {
        BROOKESIA_LOGW("Failed to resolve public download directories: %1%", public_paths.error());
    } else {
        if (public_paths->internal.available && !public_paths->internal.download.empty()) {
            append_unique_path(local_package_scan_dirs_, std::filesystem::path(public_paths->internal.download));
        }
        for (const auto &dir : public_paths->external) {
            if (dir.available && !dir.download.empty()) {
                append_unique_path(local_package_scan_dirs_, std::filesystem::path(dir.download));
            }
        }
    }

    if (refresh_view) {
        status_text_ = tr("status.loading_local_packages");
        (void)populate_entries(context);
    }

    submit_next_local_package_scan_dir(async_generation_);
}

void AppStoreApp::cancel_local_package_scan(system::core::AppContext &context)
{
    if (local_package_scan_timer_id_ != system::core::INVALID_TIMER_ID) {
        if (!context.timer().stop(local_package_scan_timer_id_)) {
            BROOKESIA_LOGW("Failed to stop App Store local package scan timer: %1%",
                           local_package_scan_timer_id_);
        }
        local_package_scan_timer_id_ = system::core::INVALID_TIMER_ID;
    }
    local_package_scan_in_progress_ = false;
    local_package_scan_refresh_view_ = false;
    local_package_scan_phase_ = LocalPackageScanPhase::None;
    local_package_scan_dirs_.clear();
    local_package_scan_candidates_.clear();
    local_package_scan_results_.clear();
    local_package_scan_seen_keys_.clear();
    local_package_scan_dir_cursor_ = 0;
    local_package_scan_candidate_cursor_ = 0;
}

void AppStoreApp::submit_next_local_package_scan_dir(uint64_t generation)
{
    if (context_ == nullptr || generation != async_generation_ || !local_package_scan_in_progress_) {
        return;
    }

    while (local_package_scan_dir_cursor_ < local_package_scan_dirs_.size()) {
        const auto packages_dir = local_package_scan_dirs_[local_package_scan_dir_cursor_++];
        auto result_handler = [this, generation, packages_dir](service::FunctionResult && result) mutable {
            handle_local_package_scan_stat_result(generation, packages_dir, std::move(result));
        };
        if (StorageHelper::call_function_async(
                    StorageHelper::FunctionId::FSStat,
                    packages_dir.generic_string(),
                    service::ServiceBase::FunctionResultHandler(std::move(result_handler))
                )) {
            return;
        }
        BROOKESIA_LOGW("Failed to submit App Store local package stat: %1%", packages_dir.generic_string());
    }

    local_package_scan_phase_ = LocalPackageScanPhase::ParsePackages;
    schedule_local_package_scan_step(*context_);
}

void AppStoreApp::handle_local_package_scan_stat_result(
    uint64_t generation,
    std::filesystem::path packages_dir,
    service::FunctionResult &&result
)
{
    if (context_ == nullptr || generation != async_generation_ || !local_package_scan_in_progress_) {
        return;
    }

    auto info_json = StorageHelper::process_function_result<boost::json::object>(result);
    if (!info_json) {
        submit_next_local_package_scan_dir(generation);
        return;
    }
    StorageHelper::FileInfo info;
    if (!BROOKESIA_DESCRIBE_FROM_JSON(*info_json, info) ||
            !info.exists || info.type != StorageHelper::FileType::Directory) {
        submit_next_local_package_scan_dir(generation);
        return;
    }

    auto result_handler = [this, generation, packages_dir](service::FunctionResult && list_result) mutable {
        handle_local_package_scan_list_result(generation, packages_dir, std::move(list_result));
    };
    if (!StorageHelper::call_function_async(
                StorageHelper::FunctionId::FSList,
                packages_dir.generic_string(),
                service::ServiceBase::FunctionResultHandler(std::move(result_handler))
            )) {
        BROOKESIA_LOGW("Failed to submit App Store local package list: %1%", packages_dir.generic_string());
        submit_next_local_package_scan_dir(generation);
    }
}

void AppStoreApp::handle_local_package_scan_list_result(
    uint64_t generation,
    std::filesystem::path packages_dir,
    service::FunctionResult &&result
)
{
    if (context_ == nullptr || generation != async_generation_ || !local_package_scan_in_progress_) {
        return;
    }

    auto entries_json = StorageHelper::process_function_result<boost::json::array>(result);
    if (!entries_json) {
        BROOKESIA_LOGW(
            "Failed to scan local package directory: path(%1%), error(%2%)",
            packages_dir.generic_string(), entries_json.error()
        );
        submit_next_local_package_scan_dir(generation);
        return;
    }

    std::vector<StorageHelper::FileEntry> entries;
    if (!BROOKESIA_DESCRIBE_FROM_JSON(*entries_json, entries)) {
        BROOKESIA_LOGW("Failed to parse local package directory entries: %1%", packages_dir.generic_string());
        submit_next_local_package_scan_dir(generation);
        return;
    }

    for (const auto &entry : entries) {
        const auto package_path = packages_dir / entry.name;
        if (entry.info.type == StorageHelper::FileType::Directory && packages_dir.filename() == APP_CACHE_DIR) {
            append_unique_path(local_package_scan_dirs_, package_path);
            continue;
        }
        if (entry.info.type != StorageHelper::FileType::File || package_path.extension() != ".bpk") {
            continue;
        }
        local_package_scan_candidates_.push_back(LocalPackageScanCandidate{
            .package_path = package_path.lexically_normal(),
            .file_size = entry.info.size,
        });
    }

    submit_next_local_package_scan_dir(generation);
}

void AppStoreApp::schedule_local_package_scan_step(system::core::AppContext &context)
{
    if (!local_package_scan_in_progress_ ||
            local_package_scan_timer_id_ != system::core::INVALID_TIMER_ID) {
        return;
    }

    auto timer = context.timer().start_delayed(LOCAL_PACKAGE_SCAN_TIMER_NAME, LOCAL_PACKAGE_SCAN_STEP_DELAY_MS);
    if (!timer) {
        BROOKESIA_LOGW("Failed to schedule App Store local package scan step: %1%", timer.error());
        (void)process_local_package_scan_step(context);
        return;
    }
    local_package_scan_timer_id_ = *timer;
}

std::expected<void, std::string> AppStoreApp::process_local_package_scan_step(system::core::AppContext &context)
{
    if (!local_package_scan_in_progress_ ||
            local_package_scan_phase_ != LocalPackageScanPhase::ParsePackages) {
        return {};
    }

    if (local_package_scan_candidate_cursor_ >= local_package_scan_candidates_.size()) {
        finish_local_package_scan(context);
        return {};
    }

    const auto candidate = local_package_scan_candidates_[local_package_scan_candidate_cursor_++];
    LocalPackageEntry package;
    package.package_path = candidate.package_path;
    package.file_name = package.package_path.filename().generic_string();
    package.file_size = candidate.file_size;

    auto manifest = system::core::read_app_package_manifest(
                        package.package_path.generic_string(),
                        context.system_service().get_system_type()
                    );
    if (!manifest) {
        BROOKESIA_LOGW(
            "Skip invalid App Store local package: path(%1%), error(%2%)",
            package.package_path.generic_string(), manifest.error()
        );
        schedule_local_package_scan_step(context);
        return {};
    }
    if (manifest->id.empty()) {
        BROOKESIA_LOGW(
            "Skip App Store local package with empty manifest id: path(%1%)",
            package.package_path.generic_string()
        );
        schedule_local_package_scan_step(context);
        return {};
    }

    package.manifest_id = manifest->id;
    package.app_names = manifest->localized_names;
    if (package.app_names.empty() && !manifest->name.empty()) {
        package.app_names.emplace("en", manifest->name);
    }
    package.version = manifest->version;

    const auto package_key = package.manifest_id + "#" + package.version;
    if (local_package_scan_seen_keys_.contains(package_key)) {
        BROOKESIA_LOGI(
            "Skip duplicate App Store local package: key(%1%), path(%2%)",
            package_key, package.package_path.generic_string()
        );
        schedule_local_package_scan_step(context);
        return {};
    }

    local_package_scan_seen_keys_.insert(package_key);
    local_package_scan_results_.push_back(std::move(package));
    schedule_local_package_scan_step(context);
    return {};
}

void AppStoreApp::finish_local_package_scan(system::core::AppContext &context)
{
    const auto refresh_view = local_package_scan_refresh_view_;
    local_packages_ = std::move(local_package_scan_results_);
    rebuild_local_package_index();

    local_package_scan_in_progress_ = false;
    local_package_scan_refresh_view_ = false;
    local_package_scan_phase_ = LocalPackageScanPhase::None;
    local_package_scan_dirs_.clear();
    local_package_scan_candidates_.clear();
    local_package_scan_results_.clear();
    local_package_scan_seen_keys_.clear();
    local_package_scan_dir_cursor_ = 0;
    local_package_scan_candidate_cursor_ = 0;

    if (refresh_view && view_mode_ == ViewMode::Local) {
        status_text_ = tr("status.local_packages_loaded");
        (void)populate_entries(context);
        if (pending_delete_success_package_name_.empty()) {
            update_message_dialog_if_visible(
                context,
                tr("status.local_packages_loaded"),
                {},
                system::core::MessageDialogIcon::Information,
                VIEW_MODE_LOAD_COMPLETE_DIALOG_AUTO_CLOSE_MS
            );
        } else {
            show_pending_delete_local_package_success_dialog(context);
        }
        return;
    }

    if (view_mode_ == ViewMode::Local) {
        (void)populate_entries(context);
    } else if (view_mode_ == ViewMode::Store || view_mode_ == ViewMode::Installed) {
        (void)refresh_ui(context);
    }
    show_pending_delete_local_package_success_dialog(context);
}

void AppStoreApp::show_pending_delete_local_package_success_dialog(system::core::AppContext &context)
{
    if (pending_delete_success_package_name_.empty()) {
        return;
    }

    auto package_name = std::move(pending_delete_success_package_name_);
    auto package_path = std::move(pending_delete_success_package_path_);
    pending_delete_success_package_name_.clear();
    pending_delete_success_package_path_.clear();
    status_text_ = tr("status.deleted_prefix") + package_name;
    ensure_message_dialog(
        context,
        status_text_,
        package_path.filename().generic_string(),
        system::core::MessageDialogIcon::Information,
        DELETE_SUCCESS_DIALOG_AUTO_CLOSE_MS,
        MessageDialogPurpose::DeleteLocalPackage
    );
}
