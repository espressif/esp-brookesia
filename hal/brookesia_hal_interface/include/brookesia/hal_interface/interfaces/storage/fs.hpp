/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/hal_interface/interface.hpp"

/**
 * @file fs.hpp
 * @brief Declares the storage file-system HAL interface.
 */

namespace esp_brookesia::hal {

/**
 * @brief File-system discovery interface for storage-capable devices.
 */
class StorageFsIface: public Interface {
public:
    static constexpr const char *NAME = "StorageFs";  ///< Interface registry name.

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
        FileSystemType fs_type;   ///< File-system type.
        MediumType medium_type;   ///< Storage medium type.
        const char *mount_point;  ///< Mount point.
    };

    /**
     * @brief Construct a storage file-system interface.
     */
    StorageFsIface()
        : Interface(NAME)
    {
    }

    /**
     * @brief Virtual destructor for polymorphic storage interfaces.
     */
    virtual ~StorageFsIface() = default;

    /**
     * @brief Enumerate all mounted file-system entries.
     *
     * @return Collection of file-system metadata entries.
     */
    const std::vector<Info> &get_all_info() const
    {
        return info_list_;
    }

protected:
    std::vector<Info> info_list_;
};

BROOKESIA_DESCRIBE_ENUM(StorageFsIface::MediumType, Flash, SDCard);
BROOKESIA_DESCRIBE_ENUM(StorageFsIface::FileSystemType, SPIFFS, FATFS, LittleFS);
BROOKESIA_DESCRIBE_STRUCT(StorageFsIface::Info, (), (fs_type, medium_type, mount_point));

} // namespace esp_brookesia::hal
