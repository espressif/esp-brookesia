/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "brookesia/hal_interface/interfaces/storage/file_system.hpp"
#include "brookesia/lib_utils/describe_helpers.hpp"

namespace esp_brookesia::system::core {

enum class StoragePartition {
    Internal,
    External,
};

enum class StorageInstallTarget {
    Auto,
    Internal,
    External,
};

enum class StoragePathKind {
    AppCache,
    AppData,
    AppFiles,
    Music,
    Download,
    Movies,
    Pictures,
    Documents,
};

enum class StorageFileType {
    Missing,
    File,
    Directory,
    Other,
};

struct StorageVolume {
    std::string id;
    StoragePartition partition = StoragePartition::Internal;
    std::string mount_point;
    std::string root_path;
    hal::storage::FileSystemIface::MediumType medium_type = hal::storage::FileSystemIface::MediumType::Flash;
    hal::storage::FileSystemIface::FileSystemType fs_type = hal::storage::FileSystemIface::FileSystemType::LittleFS;
    bool available = false;
};

struct StorageLayout {
    StorageVolume internal = {};
    std::vector<StorageVolume> external = {};
    std::string preferred_external_id;
    StorageInstallTarget default_install_target = StorageInstallTarget::Auto;
};

struct StorageConfig {
    std::optional<StorageVolume> internal_override;
    std::vector<StorageVolume> external_overrides;
    std::string preferred_external_id;
    StorageInstallTarget default_install_target = StorageInstallTarget::Auto;
};

struct AppStorageDirs {
    std::string volume_id;
    StoragePartition partition = StoragePartition::Internal;
    bool available = false;
    std::string root;
    std::string cache;
    std::string data;
    std::string files;
};

struct AppStoragePaths {
    AppStorageDirs internal = {};
    std::vector<AppStorageDirs> external = {};
};

struct PublicStorageDirs {
    std::string volume_id;
    StoragePartition partition = StoragePartition::Internal;
    bool available = false;
    std::string music;
    std::string download;
    std::string movies;
    std::string pictures;
    std::string documents;
};

struct PublicStoragePaths {
    PublicStorageDirs internal = {};
    std::vector<PublicStorageDirs> external = {};
};

struct StorageFileInfo {
    StorageFileType type = StorageFileType::Missing;
    std::uintmax_t size = 0;
    bool exists = false;
};

struct StorageFileEntry {
    std::string name;
    StorageFileInfo info;
};

struct StorageFileLocation {
    StoragePathKind kind = StoragePathKind::AppFiles;
    std::string volume_id;
    std::string relative_path;
};

BROOKESIA_DESCRIBE_ENUM(StoragePartition, Internal, External)
BROOKESIA_DESCRIBE_ENUM(StorageInstallTarget, Auto, Internal, External)
BROOKESIA_DESCRIBE_ENUM(StoragePathKind, AppCache, AppData, AppFiles, Music, Download, Movies, Pictures, Documents)
BROOKESIA_DESCRIBE_ENUM(StorageFileType, Missing, File, Directory, Other)
BROOKESIA_DESCRIBE_STRUCT(
    StorageVolume,
    (),
    (id, partition, mount_point, root_path, medium_type, fs_type, available)
)
BROOKESIA_DESCRIBE_STRUCT(
    StorageLayout,
    (),
    (internal, external, preferred_external_id, default_install_target)
)
BROOKESIA_DESCRIBE_STRUCT(
    StorageConfig,
    (),
    (internal_override, external_overrides, preferred_external_id, default_install_target)
)
BROOKESIA_DESCRIBE_STRUCT(AppStorageDirs, (), (volume_id, partition, available, root, cache, data, files))
BROOKESIA_DESCRIBE_STRUCT(AppStoragePaths, (), (internal, external))
BROOKESIA_DESCRIBE_STRUCT(PublicStorageDirs, (), (volume_id, partition, available, music, download, movies, pictures, documents))
BROOKESIA_DESCRIBE_STRUCT(PublicStoragePaths, (), (internal, external))
BROOKESIA_DESCRIBE_STRUCT(StorageFileInfo, (), (type, size, exists))
BROOKESIA_DESCRIBE_STRUCT(StorageFileEntry, (), (name, info))
BROOKESIA_DESCRIBE_STRUCT(StorageFileLocation, (), (kind, volume_id, relative_path))

} // namespace esp_brookesia::system::core
