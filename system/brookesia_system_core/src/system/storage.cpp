/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "brookesia/system_core/macro_configs.h"
#if !BROOKESIA_SYSTEM_CORE_SYSTEM_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/system/impl.hpp"
#include "private/utils.hpp"

#include <algorithm>
#include <cctype>
#include <expected>
#include <fstream>
#include <set>
#include <string_view>

#include "brookesia/service_helper.hpp"

namespace esp_brookesia::system::core {
namespace {

constexpr const char *DIR_APPS = "apps";
constexpr const char *DIR_MUSIC = "music";
constexpr const char *DIR_DOWNLOAD = "download";
constexpr const char *DIR_MOVIES = "movies";
constexpr const char *DIR_PICTURES = "pictures";
constexpr const char *DIR_DOCUMENTS = "documents";
constexpr const char *DIR_SYSTEM = "system";
constexpr const char *DIR_CACHE = "cache";
constexpr const char *DIR_DATA = "data";
constexpr const char *DIR_FILES = "files";

constexpr const char *DEFAULT_INTERNAL_ID = "internal";
constexpr const char *STORAGE_KV_NAMESPACE = "brookesia.system.storage";
constexpr const char *STORAGE_KV_DEFAULT_INSTALL_TARGET = "DefaultInstallTarget";
constexpr const char *STORAGE_KV_PREFERRED_EXTERNAL_ID = "PreferredExternalId";
constexpr uint32_t STORAGE_KV_TIMEOUT_MS = 500;

std::expected<std::string, std::string> make_storage_kv_namespace(std::string_view raw_namespace)
{
    auto result = service::helper::Storage::make_kv_namespace({raw_namespace}, ".", STORAGE_KV_TIMEOUT_MS);
    if (!result) {
        return std::unexpected("Failed to make storage KV namespace '" + std::string(raw_namespace) + "': " +
                               result.error());
    }
    if (result->hashed) {
        BROOKESIA_LOGW("%1%", result->warning);
    }
    return result->name;
}

std::expected<std::string, std::string> make_storage_kv_key(std::string_view raw_key)
{
    auto result = service::helper::Storage::make_kv_key({raw_key}, ".", STORAGE_KV_TIMEOUT_MS);
    if (!result) {
        return std::unexpected("Failed to make storage KV key '" + std::string(raw_key) + "': " + result.error());
    }
    if (result->hashed) {
        BROOKESIA_LOGW("%1%", result->warning);
    }
    return result->name;
}

std::filesystem::path normalize_path(const std::filesystem::path &path)
{
    std::error_code error_code;
    auto normalized = std::filesystem::weakly_canonical(path, error_code);
    if (!error_code) {
        return normalized.lexically_normal();
    }
    error_code.clear();
    normalized = std::filesystem::absolute(path, error_code);
    if (!error_code) {
        return normalized.lexically_normal();
    }
    return path.lexically_normal();
}

bool path_has_prefix(const std::filesystem::path &path, const std::filesystem::path &prefix)
{
    const auto path_string = normalize_path(path).generic_string();
    const auto prefix_string = normalize_path(prefix).generic_string();
    return path_string == prefix_string ||
           (path_string.starts_with(prefix_string) && path_string.size() > prefix_string.size() &&
            path_string[prefix_string.size()] == '/');
}

std::string sanitize_volume_id(std::string value)
{
    for (auto &ch : value) {
        if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '_' && ch != '-') {
            ch = '_';
        }
    }
    while (!value.empty() && value.front() == '_') {
        value.erase(value.begin());
    }
    while (!value.empty() && value.back() == '_') {
        value.pop_back();
    }
    return value.empty() ? std::string("storage") : value;
}

std::string make_volume_id(const hal::storage::FileSystemIface::Info &info, StoragePartition partition)
{
    if (partition == StoragePartition::Internal) {
        return DEFAULT_INTERNAL_ID;
    }
    std::string source = info.mount_point == nullptr ? std::string() : std::string(info.mount_point);
    if (source.empty()) {
        source = info.root_path;
    }
    return "external_" + sanitize_volume_id(source);
}

std::string root_path_for_info(const hal::storage::FileSystemIface::Info &info)
{
    if (!info.root_path.empty()) {
        return std::filesystem::path(info.root_path).lexically_normal().generic_string();
    }
    if (info.mount_point != nullptr) {
        return std::filesystem::path(info.mount_point).lexically_normal().generic_string();
    }
    return {};
}

StorageVolume make_volume(
    const hal::storage::FileSystemIface::Info &info,
    StoragePartition partition
)
{
    return StorageVolume{
        .id = make_volume_id(info, partition),
        .partition = partition,
        .mount_point = info.mount_point == nullptr ? std::string() : std::string(info.mount_point),
        .root_path = root_path_for_info(info),
        .medium_type = info.medium_type,
        .fs_type = info.fs_type,
        .available = info.supports_directories && !root_path_for_info(info).empty(),
    };
}

int internal_priority(const StorageVolume &volume)
{
    using Medium = hal::storage::FileSystemIface::MediumType;
    using Fs = hal::storage::FileSystemIface::FileSystemType;

    if (volume.medium_type == Medium::Flash && volume.fs_type == Fs::LittleFS) {
        return 0;
    }
    if (volume.medium_type == Medium::Flash && volume.fs_type == Fs::FATFS) {
        return 1;
    }
    if (volume.medium_type == Medium::SDCard && volume.fs_type == Fs::FATFS) {
        return 2;
    }
    if (volume.fs_type == Fs::LittleFS) {
        return 3;
    }
    if (volume.fs_type == Fs::FATFS) {
        return 4;
    }
    return 100;
}

bool is_sdcard(const StorageVolume &volume)
{
    return volume.medium_type == hal::storage::FileSystemIface::MediumType::SDCard;
}

std::string storage_install_target_to_string(StorageInstallTarget target)
{
    return std::string(BROOKESIA_DESCRIBE_ENUM_TO_STR(target));
}

bool is_safe_app_directory_name(std::string_view name)
{
    return !name.empty() &&
           name != "." &&
           name != ".." &&
           (name.find('/') == std::string_view::npos) &&
           (name.find('\\') == std::string_view::npos);
}

std::optional<hal::storage::FileSystemIface::Info> storage_info_from_json(const boost::json::value &value)
{
    if (!value.is_object()) {
        return std::nullopt;
    }
    const auto &object = value.as_object();
    auto get_string = [&object](std::string_view key) -> std::optional<std::string> {
        auto it = object.find(key);
        if (it == object.end() || !it->value().is_string())
        {
            return std::nullopt;
        }
        return std::string(it->value().as_string());
    };
    auto get_optional_string = [&object](std::string_view key) {
        auto it = object.find(key);
        if (it == object.end() || !it->value().is_string()) {
            return std::string();
        }
        return std::string(it->value().as_string());
    };
    auto get_bool = [&object](std::string_view key) -> std::optional<bool> {
        auto it = object.find(key);
        if (it == object.end() || !it->value().is_bool())
        {
            return std::nullopt;
        }
        return it->value().as_bool();
    };

    auto fs_type_string = get_string("fs_type");
    auto medium_type_string = get_string("medium_type");
    auto mount_point = get_string("mount_point");
    auto supports_directories = get_bool("supports_directories");
    if (!fs_type_string || !medium_type_string || !mount_point || !supports_directories) {
        return std::nullopt;
    }

    hal::storage::FileSystemIface::FileSystemType fs_type;
    hal::storage::FileSystemIface::MediumType medium_type;
    if (!BROOKESIA_DESCRIBE_STR_TO_ENUM_FLEXIBLE(*fs_type_string, fs_type) ||
            !BROOKESIA_DESCRIBE_STR_TO_ENUM_FLEXIBLE(*medium_type_string, medium_type)) {
        return std::nullopt;
    }

    static thread_local std::string thread_mount_point;
    thread_mount_point = std::move(*mount_point);
    return hal::storage::FileSystemIface::Info{
        .fs_type = fs_type,
        .medium_type = medium_type,
        .mount_point = thread_mount_point.c_str(),
        .root_path = get_optional_string("root_path"),
        .supports_directories = *supports_directories,
    };
}

std::expected<void, std::string> create_dir(const std::filesystem::path &path)
{
    std::error_code error_code;
    std::filesystem::create_directories(path, error_code);
    if (error_code) {
        return std::unexpected(
                   "Failed to create directory '" + path.generic_string() + "': " + error_code.message()
               );
    }
    return {};
}

std::expected<void, std::string> ensure_volume_dirs(const StorageVolume &volume, bool internal)
{
    if (!volume.available) {
        return {};
    }
    const std::filesystem::path root(volume.root_path);
    for (const auto *dir : {
                DIR_APPS, DIR_MUSIC, DIR_DOWNLOAD, DIR_MOVIES, DIR_PICTURES, DIR_DOCUMENTS
            }) {
        auto result = create_dir(root / dir);
        if (!result) {
            return result;
        }
    }
    if (internal) {
        auto result = create_dir(root / DIR_SYSTEM);
        if (!result) {
            return result;
        }
    }
    return {};
}

bool is_safe_relative_path(std::string_view path)
{
    std::filesystem::path fs_path{std::string(path)};
    if (fs_path.is_absolute()) {
        return false;
    }
    for (const auto &part : fs_path) {
        if (part == "..") {
            return false;
        }
    }
    return true;
}

std::filesystem::path append_relative_path(
    const std::filesystem::path &base,
    std::string_view relative_path
)
{
    if (relative_path.empty()) {
        return base;
    }
    return (base / std::filesystem::path(std::string(relative_path))).lexically_normal();
}

AppStorageDirs make_app_dirs(const StorageVolume &volume, std::string_view manifest_id)
{
    const auto root = (std::filesystem::path(volume.root_path) / DIR_APPS / std::string(manifest_id)).lexically_normal();
    return AppStorageDirs{
        .volume_id = volume.id,
        .partition = volume.partition,
        .available = volume.available,
        .root = root.generic_string(),
        .cache = (root / DIR_CACHE).generic_string(),
        .data = (root / DIR_DATA).generic_string(),
        .files = (root / DIR_FILES).generic_string(),
    };
}

PublicStorageDirs make_public_dirs(const StorageVolume &volume)
{
    const auto root = std::filesystem::path(volume.root_path).lexically_normal();
    return PublicStorageDirs{
        .volume_id = volume.id,
        .partition = volume.partition,
        .available = volume.available,
        .music = (root / DIR_MUSIC).generic_string(),
        .download = (root / DIR_DOWNLOAD).generic_string(),
        .movies = (root / DIR_MOVIES).generic_string(),
        .pictures = (root / DIR_PICTURES).generic_string(),
        .documents = (root / DIR_DOCUMENTS).generic_string(),
    };
}

std::optional<StorageVolume> find_volume(const StorageLayout &layout, std::string_view volume_id)
{
    if (volume_id.empty() || volume_id == layout.internal.id) {
        return layout.internal;
    }
    for (const auto &volume : layout.external) {
        if (volume.id == volume_id) {
            return volume;
        }
    }
    return std::nullopt;
}

std::optional<StorageVolume> find_app_install_volume(const StorageLayout &layout, const AppManifest &manifest)
{
    if (manifest.id.empty() || manifest.app_path.empty()) {
        return std::nullopt;
    }

    const auto app_path = normalize_path(std::filesystem::path(manifest.app_path));
    auto matches_volume = [&](const StorageVolume & volume) {
        if (!volume.available || volume.root_path.empty()) {
            return false;
        }
        const auto app_root = (std::filesystem::path(volume.root_path) / DIR_APPS / manifest.id).lexically_normal();
        return path_has_prefix(app_path, app_root);
    };

    if (matches_volume(layout.internal)) {
        return layout.internal;
    }
    for (const auto &volume : layout.external) {
        if (matches_volume(volume)) {
            return volume;
        }
    }
    return std::nullopt;
}

std::filesystem::path base_path_for_kind(
    const StorageVolume &volume,
    StoragePathKind kind,
    std::string_view manifest_id
)
{
    const auto root = std::filesystem::path(volume.root_path).lexically_normal();
    switch (kind) {
    case StoragePathKind::AppCache:
        return root / DIR_APPS / std::string(manifest_id) / DIR_CACHE;
    case StoragePathKind::AppData:
        return root / DIR_APPS / std::string(manifest_id) / DIR_DATA;
    case StoragePathKind::AppFiles:
        return root / DIR_APPS / std::string(manifest_id) / DIR_FILES;
    case StoragePathKind::Music:
        return root / DIR_MUSIC;
    case StoragePathKind::Download:
        return root / DIR_DOWNLOAD;
    case StoragePathKind::Movies:
        return root / DIR_MOVIES;
    case StoragePathKind::Pictures:
        return root / DIR_PICTURES;
    case StoragePathKind::Documents:
        return root / DIR_DOCUMENTS;
    default:
        return {};
    }
}

StorageFileInfo make_file_info(const std::filesystem::path &path)
{
    std::error_code error_code;
    if (!std::filesystem::exists(path, error_code)) {
        return StorageFileInfo{};
    }
    StorageFileInfo info;
    info.exists = true;
    if (std::filesystem::is_regular_file(path, error_code)) {
        info.type = StorageFileType::File;
        info.size = std::filesystem::file_size(path, error_code);
        if (error_code) {
            info.size = 0;
        }
    } else if (std::filesystem::is_directory(path, error_code)) {
        info.type = StorageFileType::Directory;
    } else {
        info.type = StorageFileType::Other;
    }
    return info;
}

void load_storage_preferences(StorageConfig &config)
{
    using StorageHelper = service::helper::Storage;

    auto namespace_result = make_storage_kv_namespace(STORAGE_KV_NAMESPACE);
    if (!namespace_result) {
        BROOKESIA_LOGW("Skip loading storage preferences: %1%", namespace_result.error());
        return;
    }
    auto default_target_key = make_storage_kv_key(STORAGE_KV_DEFAULT_INSTALL_TARGET);
    auto preferred_external_key = make_storage_kv_key(STORAGE_KV_PREFERRED_EXTERNAL_ID);
    if (!default_target_key || !preferred_external_key) {
        BROOKESIA_LOGW(
            "Skip loading storage preferences: default_target_key(%1%), preferred_external_key(%2%)",
            default_target_key ? "ok" : default_target_key.error(),
            preferred_external_key ? "ok" : preferred_external_key.error()
        );
        return;
    }

    if (auto target = StorageHelper::get_key_value<std::string>(
                          namespace_result.value(), default_target_key.value(), STORAGE_KV_TIMEOUT_MS
                      )) {
        StorageInstallTarget parsed_target = StorageInstallTarget::Auto;
        if (BROOKESIA_DESCRIBE_STR_TO_ENUM_FLEXIBLE(*target, parsed_target)) {
            config.default_install_target = parsed_target;
        } else {
            BROOKESIA_LOGW("Ignore invalid persisted default install target: %1%", *target);
        }
    }

    if (auto preferred = StorageHelper::get_key_value<std::string>(
                             namespace_result.value(), preferred_external_key.value(), STORAGE_KV_TIMEOUT_MS
                         )) {
        config.preferred_external_id = *preferred;
    }
}

std::expected<void, std::string> save_storage_preferences(
    StorageInstallTarget target,
    const std::string &preferred_external_id
)
{
    using StorageHelper = service::helper::Storage;

    auto namespace_result = make_storage_kv_namespace(STORAGE_KV_NAMESPACE);
    if (!namespace_result) {
        return std::unexpected(namespace_result.error());
    }
    auto default_target_key = make_storage_kv_key(STORAGE_KV_DEFAULT_INSTALL_TARGET);
    if (!default_target_key) {
        return std::unexpected(default_target_key.error());
    }
    auto preferred_external_key = make_storage_kv_key(STORAGE_KV_PREFERRED_EXTERNAL_ID);
    if (!preferred_external_key) {
        return std::unexpected(preferred_external_key.error());
    }

    auto target_result = StorageHelper::save_key_value(
                             namespace_result.value(),
                             default_target_key.value(),
                             storage_install_target_to_string(target),
                             STORAGE_KV_TIMEOUT_MS
                         );
    if (!target_result) {
        return std::unexpected(target_result.error());
    }
    auto preferred_result = StorageHelper::save_key_value(
                                namespace_result.value(),
                                preferred_external_key.value(),
                                preferred_external_id,
                                STORAGE_KV_TIMEOUT_MS
                            );
    if (!preferred_result) {
        return std::unexpected(preferred_result.error());
    }
    return {};
}

} // namespace

std::expected<void, std::string> System::Impl::initialize_storage_layout()
{
    using StorageHelper = service::helper::Storage;

    load_storage_preferences(config_.storage);

    std::vector<StorageVolume> discovered;
    if (StorageHelper::is_running()) {
        auto fs_infos = StorageHelper::call_function_sync<boost::json::array>(
                            StorageHelper::FunctionId::GetFileSystems,
                            service::helper::Timeout(1000)
                        );
        if (fs_infos) {
            for (const auto &entry : *fs_infos) {
                auto info = storage_info_from_json(entry);
                if (!info) {
                    BROOKESIA_LOGW("Skip invalid storage filesystem info from Storage service");
                    continue;
                }
                auto root_path = root_path_for_info(*info);
                if (!info->supports_directories || root_path.empty()) {
                    continue;
                }
                discovered.push_back(make_volume(
                                         *info,
                                         info->medium_type == hal::storage::FileSystemIface::MediumType::SDCard ?
                                         StoragePartition::External :
                                         StoragePartition::Internal
                                     ));
            }
        } else {
            BROOKESIA_LOGW("Failed to query storage filesystems from Storage service: %1%", fs_infos.error());
        }
    } else {
        BROOKESIA_LOGW("Storage service is not running while initializing storage layout");
    }

    if (config_.storage.internal_override.has_value()) {
        storage_layout_.internal = *config_.storage.internal_override;
        storage_layout_.internal.partition = StoragePartition::Internal;
        storage_layout_.internal.available = !storage_layout_.internal.root_path.empty();
    } else {
        std::vector<StorageVolume> candidates;
        for (auto volume : discovered) {
            if (!volume.available) {
                continue;
            }
            volume.partition = StoragePartition::Internal;
            volume.id = DEFAULT_INTERNAL_ID;
            candidates.push_back(std::move(volume));
        }
        std::sort(candidates.begin(), candidates.end(), [](const auto & left, const auto & right) {
            return internal_priority(left) < internal_priority(right);
        });
        if (!candidates.empty()) {
            storage_layout_.internal = std::move(candidates.front());
        }
    }

    storage_layout_.external.clear();
    if (!config_.storage.external_overrides.empty()) {
        for (auto volume : config_.storage.external_overrides) {
            volume.partition = StoragePartition::External;
            volume.available = !volume.root_path.empty();
            if (volume.id.empty()) {
                volume.id = "external_" + sanitize_volume_id(volume.root_path);
            }
            storage_layout_.external.push_back(std::move(volume));
        }
    } else {
        std::set<std::string> ids;
        for (auto volume : discovered) {
            if (!volume.available || !is_sdcard(volume)) {
                continue;
            }
            volume.partition = StoragePartition::External;
            volume.id = make_volume_id(
            hal::storage::FileSystemIface::Info{
                .fs_type = volume.fs_type,
                .medium_type = volume.medium_type,
                .mount_point = volume.mount_point.c_str(),
                .root_path = volume.root_path,
                .supports_directories = true,
            },
            StoragePartition::External
                        );
            if (ids.insert(volume.id).second) {
                storage_layout_.external.push_back(std::move(volume));
            }
        }
    }

    storage_layout_.preferred_external_id = config_.storage.preferred_external_id;
    storage_layout_.default_install_target = config_.storage.default_install_target;
    if (storage_layout_.preferred_external_id.empty()) {
        storage_layout_.preferred_external_id = config_.storage.preferred_external_id;
    }

    if (!storage_layout_.internal.available) {
#if defined(ESP_PLATFORM)
        storage_layout_.internal = StorageVolume {
            .id = DEFAULT_INTERNAL_ID,
            .partition = StoragePartition::Internal,
            .mount_point = "/littlefs",
            .root_path = "/littlefs",
            .medium_type = hal::storage::FileSystemIface::MediumType::Flash,
            .fs_type = hal::storage::FileSystemIface::FileSystemType::LittleFS,
            .available = true,
        };
#else
        storage_layout_.internal = StorageVolume {
            .id = DEFAULT_INTERNAL_ID,
            .partition = StoragePartition::Internal,
            .mount_point = "littlefs",
            .root_path = "brookesia",
            .medium_type = hal::storage::FileSystemIface::MediumType::Flash,
            .fs_type = hal::storage::FileSystemIface::FileSystemType::LittleFS,
            .available = true,
        };
#endif
        BROOKESIA_LOGW("Using fallback internal storage root: %1%", storage_layout_.internal.root_path);
    }

    auto dirs_result = ensure_standard_storage_dirs();
    if (!dirs_result) {
        return dirs_result;
    }

    BROOKESIA_LOGI(
        "Storage layout initialized: internal(id=%1%, root=%2%), external_count(%3%)",
        storage_layout_.internal.id,
        storage_layout_.internal.root_path,
        storage_layout_.external.size()
    );
    return {};
}

std::expected<void, std::string> System::Impl::ensure_standard_storage_dirs()
{
    auto internal_result = ensure_volume_dirs(storage_layout_.internal, true);
    if (!internal_result) {
        return internal_result;
    }
    for (const auto &volume : storage_layout_.external) {
        auto result = ensure_volume_dirs(volume, false);
        if (!result) {
            return result;
        }
    }
    return {};
}

std::expected<AppStoragePaths, std::string> System::Impl::ensure_app_storage_paths(std::string_view manifest_id)
{
    if (!is_safe_app_directory_name(manifest_id)) {
        return std::unexpected("App manifest id is not a safe storage directory name: " + std::string(manifest_id));
    }

    AppStoragePaths paths;
    paths.internal = make_app_dirs(storage_layout_.internal, manifest_id);
    if (paths.internal.available) {
        for (const auto &dir : {
                    paths.internal.cache, paths.internal.data, paths.internal.files
                }) {
            auto result = create_dir(dir);
            if (!result) {
                return std::unexpected(result.error());
            }
        }
    }

    for (const auto &volume : storage_layout_.external) {
        auto dirs = make_app_dirs(volume, manifest_id);
        if (dirs.available) {
            for (const auto &dir : {
                        dirs.cache, dirs.data, dirs.files
                    }) {
                auto result = create_dir(dir);
                if (!result) {
                    return std::unexpected(result.error());
                }
            }
        }
        paths.external.push_back(std::move(dirs));
    }
    return paths;
}

std::expected<std::filesystem::path, std::string> System::Impl::get_default_install_apps_root() const
{
    const auto make_apps_root = [](const StorageVolume & volume) {
        return std::filesystem::path(volume.root_path) / DIR_APPS;
    };

    auto find_external_by_id = [this](std::string_view id) -> const StorageVolume * {
        if (id.empty())
        {
            return nullptr;
        }
        for (const auto &volume : storage_layout_.external)
        {
            if (volume.id == id && volume.available) {
                return &volume;
            }
        }
        return nullptr;
    };

    if (storage_layout_.default_install_target == StorageInstallTarget::Internal) {
        return make_apps_root(storage_layout_.internal);
    }
    if (storage_layout_.default_install_target == StorageInstallTarget::External ||
            storage_layout_.default_install_target == StorageInstallTarget::Auto) {
        if (const auto *preferred = find_external_by_id(storage_layout_.preferred_external_id); preferred != nullptr) {
            return make_apps_root(*preferred);
        }
        if (!storage_layout_.external.empty()) {
            return make_apps_root(storage_layout_.external.front());
        }
    }
    if (storage_layout_.internal.available) {
        return make_apps_root(storage_layout_.internal);
    }
    return std::unexpected("No storage volume is available for runtime app installation");
}

std::vector<std::filesystem::path> System::Impl::get_app_scan_roots() const
{
    std::vector<std::filesystem::path> roots;
    if (storage_layout_.internal.available) {
        roots.push_back(std::filesystem::path(storage_layout_.internal.root_path) / DIR_APPS);
    }

    auto append_external = [&roots](const StorageVolume & volume) {
        if (!volume.available) {
            return;
        }
        const auto path = (std::filesystem::path(volume.root_path) / DIR_APPS).lexically_normal();
        const auto path_string = path.generic_string();
        const auto duplicate = std::find_if(roots.begin(), roots.end(), [&path_string](const auto & existing) {
            return existing.lexically_normal().generic_string() == path_string;
        }) != roots.end();
        if (!duplicate) {
            roots.push_back(path);
        }
    };

    if (!storage_layout_.preferred_external_id.empty()) {
        for (const auto &volume : storage_layout_.external) {
            if (volume.id == storage_layout_.preferred_external_id) {
                append_external(volume);
                break;
            }
        }
    }
    for (const auto &volume : storage_layout_.external) {
        append_external(volume);
    }
    return roots;
}

bool System::Impl::is_managed_app_directory(const std::filesystem::path &path) const
{
    for (const auto &root : get_app_scan_roots()) {
        if (path_has_prefix(path, root) && normalize_path(path) != normalize_path(root)) {
            return true;
        }
    }
    return false;
}

std::string System::Impl::get_system_storage_dir() const
{
    if (!storage_layout_.internal.available) {
        return {};
    }
    return (std::filesystem::path(storage_layout_.internal.root_path) / DIR_SYSTEM).lexically_normal().generic_string();
}

StorageLayout System::get_storage_layout() const
{
    return impl_->storage_layout_;
}

std::expected<AppStoragePaths, std::string> System::get_app_storage_paths(AppId app_id)
{
    auto record = impl_->get_record(app_id);
    if (!record) {
        return std::unexpected(record.error());
    }
    return impl_->ensure_app_storage_paths(record.value()->info.manifest.id);
}

std::expected<PublicStoragePaths, std::string> System::get_public_storage_paths()
{
    auto dirs_result = impl_->ensure_standard_storage_dirs();
    if (!dirs_result) {
        return std::unexpected(dirs_result.error());
    }

    PublicStoragePaths paths;
    paths.internal = make_public_dirs(impl_->storage_layout_.internal);
    for (const auto &volume : impl_->storage_layout_.external) {
        paths.external.push_back(make_public_dirs(volume));
    }
    return paths;
}

std::expected<void, std::string> System::set_default_install_storage(
    StorageInstallTarget target,
    std::string preferred_external_id
)
{
    if (target == StorageInstallTarget::External && !preferred_external_id.empty()) {
        const auto matched = std::find_if(
                                 impl_->storage_layout_.external.begin(),
                                 impl_->storage_layout_.external.end(),
        [&preferred_external_id](const auto & volume) {
            return volume.id == preferred_external_id && volume.available;
        }
                             );
        if (matched == impl_->storage_layout_.external.end()) {
            return std::unexpected("Preferred external storage volume is not available: " + preferred_external_id);
        }
    }
    impl_->storage_layout_.default_install_target = target;
    impl_->storage_layout_.preferred_external_id = std::move(preferred_external_id);
    impl_->config_.storage.default_install_target = target;
    impl_->config_.storage.preferred_external_id = impl_->storage_layout_.preferred_external_id;
    auto save_result = save_storage_preferences(target, impl_->storage_layout_.preferred_external_id);
    if (!save_result) {
        return std::unexpected("Failed to persist storage install preference: " + save_result.error());
    }
    return {};
}

std::expected<std::filesystem::path, std::string> System::Impl::resolve_storage_location_path(
    AppId app_id,
    const StorageFileLocation &location,
    bool ensure_base
)
{
    auto record = get_record(app_id);
    if (!record) {
        return std::unexpected(record.error());
    }
    if (!is_safe_relative_path(location.relative_path)) {
        return std::unexpected("Storage relative path is invalid or escapes storage root");
    }
    std::string volume_id = location.volume_id;
    if (volume_id.empty() && location.kind == StoragePathKind::AppFiles) {
        auto install_volume = find_app_install_volume(storage_layout_, record.value()->info.manifest);
        if (!install_volume || !install_volume->available) {
            return std::unexpected(
                       "Failed to resolve AppFiles storage volume for app: " + record.value()->info.manifest.id
                   );
        }
        volume_id = install_volume->id;
    }
    auto volume = find_volume(storage_layout_, volume_id);
    if (!volume || !volume->available) {
        return std::unexpected("Storage volume is not available: " + volume_id);
    }
    const auto base = base_path_for_kind(*volume, location.kind, record.value()->info.manifest.id);
    if (base.empty()) {
        return std::unexpected("Unsupported storage path kind");
    }
    if (ensure_base) {
        auto result = create_dir(base);
        if (!result) {
            return std::unexpected(result.error());
        }
    }
    auto path = append_relative_path(base, location.relative_path);
    if (!path_has_prefix(path, base)) {
        return std::unexpected("Storage path escapes allowed root");
    }
    return path;
}

std::expected<std::vector<StorageFileEntry>, std::string> System::storage_list(
    AppId app_id,
    const StorageFileLocation &location
)
{
    auto path = impl_->resolve_storage_location_path(app_id, location, true);
    if (!path) {
        return std::unexpected(path.error());
    }
    std::vector<StorageFileEntry> entries;
    std::error_code error_code;
    if (!std::filesystem::exists(*path, error_code)) {
        return entries;
    }
    if (!std::filesystem::is_directory(*path, error_code)) {
        return std::unexpected("Storage list path is not a directory: " + path->generic_string());
    }
    for (const auto &entry : std::filesystem::directory_iterator(*path, error_code)) {
        entries.push_back(StorageFileEntry{
            .name = entry.path().filename().generic_string(),
            .info = make_file_info(entry.path()),
        });
    }
    if (error_code) {
        return std::unexpected("Failed to list storage path '" + path->generic_string() + "': " + error_code.message());
    }
    std::sort(entries.begin(), entries.end(), [](const auto & left, const auto & right) {
        return left.name < right.name;
    });
    return entries;
}

std::expected<StorageFileInfo, std::string> System::storage_stat(
    AppId app_id,
    const StorageFileLocation &location
)
{
    auto path = impl_->resolve_storage_location_path(app_id, location, true);
    if (!path) {
        return std::unexpected(path.error());
    }
    return make_file_info(*path);
}

std::expected<void, std::string> System::storage_mkdir(
    AppId app_id,
    const StorageFileLocation &location
)
{
    auto path = impl_->resolve_storage_location_path(app_id, location, true);
    if (!path) {
        return std::unexpected(path.error());
    }
    return create_dir(*path);
}

std::expected<std::string, std::string> System::storage_read(
    AppId app_id,
    const StorageFileLocation &location
)
{
    auto path = impl_->resolve_storage_location_path(app_id, location, true);
    if (!path) {
        return std::unexpected(path.error());
    }
    std::ifstream stream(*path, std::ios::binary);
    if (!stream.is_open()) {
        return std::unexpected("Failed to open storage file for read: " + path->generic_string());
    }
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

std::expected<void, std::string> System::storage_write(
    AppId app_id,
    const StorageFileLocation &location,
    std::string_view data
)
{
    auto path = impl_->resolve_storage_location_path(app_id, location, true);
    if (!path) {
        return std::unexpected(path.error());
    }
    auto parent_result = create_dir(path->parent_path());
    if (!parent_result) {
        return std::unexpected(parent_result.error());
    }
    std::ofstream stream(*path, std::ios::binary | std::ios::trunc);
    if (!stream.is_open()) {
        return std::unexpected("Failed to open storage file for write: " + path->generic_string());
    }
    stream.write(data.data(), static_cast<std::streamsize>(data.size()));
    if (!stream.good()) {
        return std::unexpected("Failed to write storage file: " + path->generic_string());
    }
    return {};
}

std::expected<void, std::string> System::storage_remove(
    AppId app_id,
    const StorageFileLocation &location
)
{
    auto path = impl_->resolve_storage_location_path(app_id, location, false);
    if (!path) {
        return std::unexpected(path.error());
    }
    std::error_code error_code;
    std::filesystem::remove_all(*path, error_code);
    if (error_code) {
        return std::unexpected("Failed to remove storage path '" + path->generic_string() + "': " + error_code.message());
    }
    return {};
}

std::expected<void, std::string> System::storage_rename(
    AppId app_id,
    const StorageFileLocation &from,
    const StorageFileLocation &to
)
{
    auto from_path = impl_->resolve_storage_location_path(app_id, from, false);
    if (!from_path) {
        return std::unexpected(from_path.error());
    }
    auto to_path = impl_->resolve_storage_location_path(app_id, to, true);
    if (!to_path) {
        return std::unexpected(to_path.error());
    }
    auto parent_result = create_dir(to_path->parent_path());
    if (!parent_result) {
        return std::unexpected(parent_result.error());
    }
    std::error_code error_code;
    std::filesystem::rename(*from_path, *to_path, error_code);
    if (error_code) {
        return std::unexpected(
                   "Failed to rename storage path '" + from_path->generic_string() + "' to '" +
                   to_path->generic_string() + "': " + error_code.message()
               );
    }
    return {};
}

std::expected<std::string, std::string> System::Impl::resolve_storage_file_path(
    AppId app_id,
    const StorageFileLocation &location
)
{
    auto path = resolve_storage_location_path(app_id, location, true);
    if (!path) {
        return std::unexpected(path.error());
    }
    auto parent_result = create_dir(path->parent_path());
    if (!parent_result) {
        return std::unexpected(parent_result.error());
    }
    return path->generic_string();
}

} // namespace esp_brookesia::system::core
