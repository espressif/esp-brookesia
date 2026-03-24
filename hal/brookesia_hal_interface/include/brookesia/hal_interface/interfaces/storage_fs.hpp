/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file storage_fs.hpp
 * @brief Storage file-system interface definitions for HAL devices.
 */
#pragma once

#include <string_view>
#include <vector>
#include <utility>
#include "brookesia/lib_utils/describe_helpers.hpp"

namespace esp_brookesia::hal {

/**
 * @brief Abstract file-system interface exposed by storage-capable devices.
 */
class StorageFsIface {
public:
    static constexpr std::string_view interface_name = "StorageFsIface"; ///< Unique interface name used for runtime lookup.

    /**
     * @brief Supported file-system backends.
     */
    enum class FsType {
        Spiffs,   ///< SPIFFS backend.
        Fatfs,    ///< FATFS backend.
        Littlefs  ///< LittleFS backend.
    };

    /**
     * @brief Constructs an empty storage file-system interface.
     */
    StorageFsIface() = default;

    /**
     * @brief Destroys the storage file-system interface.
     */
    virtual ~StorageFsIface() = default;

    /**
     * @brief Returns all mounted base paths exposed by the device.
     *
     * @return Vector of `(file system type, base path)` pairs.
     */
    virtual std::vector<std::pair<FsType, const char *>> get_all_base_path() = 0;
};

BROOKESIA_DESCRIBE_ENUM(StorageFsIface::FsType, Spiffs, Fatfs, Littlefs);

} // namespace esp_brookesia::hal
