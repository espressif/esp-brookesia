/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

void FileManagerApp::bind_storage_service()
{
    storage_service_binding_.release();
    if (!StorageHelper::is_available()) {
        BROOKESIA_LOGW("Storage service is unavailable for File Manager capacity display");
        return;
    }
    storage_service_binding_ = service::ServiceManager::get_instance().bind(StorageHelper::get_name().data());
    if (!storage_service_binding_.is_valid()) {
        BROOKESIA_LOGW("Failed to bind Storage service for File Manager capacity display");
    }
}

void FileManagerApp::release_storage_service()
{
    storage_service_binding_.release();
}

void FileManagerApp::refresh_volume_capacities(system::core::AppContext &context)
{
    (void)context;
    ++storage_capacity_generation_;
    for (auto &volume : volumes_) {
        volume.capacity_text.clear();
    }
    if (!storage_service_binding_.is_valid()) {
        return;
    }

    const auto generation = storage_capacity_generation_;
    auto handler = [this, generation](service::FunctionResult && result) {
        handle_storage_filesystems_result(generation, std::move(result));
    };
    if (!StorageHelper::call_function_async(StorageHelper::FunctionId::GetFileSystems, handler)) {
        BROOKESIA_LOGW("Failed to submit File Manager storage filesystems request");
    }
}

void FileManagerApp::handle_storage_filesystems_result(uint64_t generation, service::FunctionResult &&result)
{
    if (generation != storage_capacity_generation_ || context_ == nullptr) {
        return;
    }

    auto filesystems_result = StorageHelper::process_function_result<boost::json::array>(result);
    if (!filesystems_result) {
        BROOKESIA_LOGW("Failed to get File Manager storage filesystems: %1%", filesystems_result.error());
        return;
    }

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
        auto mount_it = object.find("mount_point");
        if (mount_it == object.end()) {
            mount_it = object.find("mountPoint");
        }
        if (mount_it == object.end() || !mount_it->value().is_string()) {
            continue;
        }
        auto root_it = object.find("root_path");
        if (root_it == object.end()) {
            root_it = object.find("rootPath");
        }
        StorageMount mount;
        mount.mount_point = mount_it->value().as_string().c_str();
        mount.root_path = (root_it != object.end() && root_it->value().is_string()) ?
                          std::string(root_it->value().as_string().c_str()) :
                          mount.mount_point;
        mounts.push_back(std::move(mount));
    }
    if (mounts.empty()) {
        BROOKESIA_LOGW("File Manager storage filesystem list is empty");
        return;
    }

    for (size_t volume_index = 0; volume_index < volumes_.size(); ++volume_index) {
        const auto volume_root = normalize_path_for_prefix(volumes_[volume_index].volume.root_path);
        std::string best_mount_point;
        size_t best_match_length = 0;
        for (const auto &mount : mounts) {
            for (const auto &candidate : {
                        mount.root_path, mount.mount_point
                    }) {
                const auto prefix = normalize_path_for_prefix(candidate);
                if (path_has_prefix(volume_root, prefix) && prefix.size() > best_match_length) {
                    best_match_length = prefix.size();
                    best_mount_point = normalize_path_for_prefix(mount.mount_point);
                }
            }
        }
        if (best_mount_point.empty() && mounts.size() == 1) {
            best_mount_point = normalize_path_for_prefix(mounts.front().mount_point);
        }
        if (best_mount_point.empty()) {
            BROOKESIA_LOGW("No storage filesystem matches File Manager volume: %1%", volume_root);
            continue;
        }

        auto handler = [this, generation, volume_index](service::FunctionResult && capacity_result) {
            handle_storage_capacity_result(generation, volume_index, std::move(capacity_result));
        };
        if (!StorageHelper::call_function_async(
                    StorageHelper::FunctionId::GetFileSystemCapacity,
                    best_mount_point,
                    handler
                )) {
            BROOKESIA_LOGW("Failed to submit File Manager storage capacity request");
        }
    }
}

void FileManagerApp::handle_storage_capacity_result(
    uint64_t generation,
    size_t volume_index,
    service::FunctionResult &&result
)
{
    if (generation != storage_capacity_generation_ || context_ == nullptr || volume_index >= volumes_.size()) {
        return;
    }

    auto capacity_result = StorageHelper::process_function_result<boost::json::object>(result);
    if (!capacity_result) {
        BROOKESIA_LOGW("Failed to get File Manager storage capacity: %1%", capacity_result.error());
        return;
    }
    StorageHelper::FileSystemCapacity capacity;
    if (!BROOKESIA_DESCRIBE_FROM_JSON(*capacity_result, capacity)) {
        BROOKESIA_LOGW("Failed to parse File Manager storage capacity");
        return;
    }

    volumes_[volume_index].capacity_text = tr("storage_free_prefix") + format_bytes(capacity.free_bytes) + " / " +
                                           format_bytes(capacity.total_bytes);
    if (!current_volume_index_ && volume_index < entries_.size()) {
        entries_[volume_index].detail = describe_volume(
                                            volumes_[volume_index].volume,
                                            volumes_[volume_index].capacity_text
                                        );
        if (auto refresh_result = refresh_ui(*context_); !refresh_result) {
            BROOKESIA_LOGW("Failed to refresh File Manager volume capacity: %1%", refresh_result.error());
        }
    }
}
