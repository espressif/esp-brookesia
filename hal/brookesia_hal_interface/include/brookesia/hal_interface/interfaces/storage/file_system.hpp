/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/hal_interface/interface.hpp"

/**
 * @file file_system.hpp
 * @brief Declares the storage file-system HAL interface.
 */

namespace esp_brookesia::hal::storage {

/**
 * @brief File-system discovery interface for storage-capable devices.
 */
class FileSystemIface: public Interface {
public:
    static constexpr const char *NAME = "StorageFileSystem";  ///< Interface registry name.

    /**
     * @brief Supported storage medium type.
     */
    enum class MediumType {
        Flash,   ///< Flash medium.
        SDCard,  ///< SD / TF card medium.
    };

    /**
     * @brief Supported file-system type.
     */
    enum class FileSystemType {
        SPIFFS,   ///< SPIFFS file system.
        FATFS,    ///< FAT file system.
        LittleFS, ///< LittleFS file system.
    };

    /**
     * @brief Metadata for one mounted file-system entry.
     */
    struct Info {
        FileSystemType fs_type;           ///< File-system type.
        MediumType medium_type;           ///< Storage medium type.
        const char *mount_point;          ///< Mount point.
        std::string root_path;            ///< Process-visible root path. Empty means same as mount point.
        bool supports_directories = false; ///< Whether this file system supports directories.
    };

    /**
     * @brief Dynamic capacity snapshot for one mounted file-system entry.
     */
    struct Capacity {
        uint64_t total_bytes = 0; ///< Total size in bytes.
        uint64_t used_bytes = 0;  ///< Used size in bytes.
        uint64_t free_bytes = 0;  ///< Free size in bytes.
    };

    /**
     * @brief Construct a storage file-system interface.
     */
    FileSystemIface()
        : Interface(NAME)
    {
    }

    /**
     * @brief Virtual destructor for polymorphic storage interfaces.
     */
    virtual ~FileSystemIface() = default;

    /**
     * @brief Enumerate all mounted file-system entries.
     *
     * @return Collection of file-system metadata entries.
     */
    const std::vector<Info> &get_all_info() const
    {
        return info_list_;
    }

    /**
     * @brief Query the capacity of a mounted file system.
     *
     * @param mount_point Mount point returned by @c get_all_info().
     * @param capacity Output capacity snapshot.
     *
     * @return true if the capacity was queried successfully, otherwise false.
     */
    virtual bool get_capacity(const char *mount_point, Capacity &capacity) = 0;

protected:
    std::vector<Info> info_list_;
};

BROOKESIA_DESCRIBE_ENUM(FileSystemIface::MediumType, Flash, SDCard);
BROOKESIA_DESCRIBE_ENUM(FileSystemIface::FileSystemType, SPIFFS, FATFS, LittleFS);
BROOKESIA_DESCRIBE_STRUCT(FileSystemIface::Info, (), (fs_type, medium_type, mount_point, root_path, supports_directories));
BROOKESIA_DESCRIBE_STRUCT(FileSystemIface::Capacity, (), (total_bytes, used_bytes, free_bytes));

} // namespace esp_brookesia::hal::storage
