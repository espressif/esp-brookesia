/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <expected>
#include <string>
#include <string_view>

#include "brookesia/runtime_manager/types.hpp"
#include "brookesia/system_core/app/types.hpp"

namespace esp_brookesia::system::core {

/**
 * @brief Unpack a .bpk/.rpk package (ZIP) to an install directory and return the parsed app manifest.
 *
 * The output directory is <install_dir>/<manifest.package.id>/ so that the unpacked
 * app always lands in a deterministic, id-based subdirectory regardless of the original
 * package filename. This aligns with the esp-brookesia-toolkit naming convention:
 *   <package.id>.<debug|release>.<version>.bpk
 *
 * The app package manifest is owned by system_core because it describes system-level app metadata
 * and runtime configuration.
 */
std::expected<AppManifest, std::string> unpack_app_package_to(
    std::string_view package_path,
    std::string_view install_dir,
    std::string_view system_type = {}
);

/**
 * @brief Read app package manifest.json directly from a .bpk package without extracting files.
 */
std::expected<AppManifest, std::string> read_app_package_manifest(
    std::string_view package_path,
    std::string_view system_type = {}
);

/**
 * @brief Read an unpacked app package manifest from package_dir/manifest.json.
 */
std::expected<AppManifest, std::string> read_unpacked_app_manifest(
    std::string_view package_dir,
    std::string_view system_type = {}
);

/**
 * @brief Read runtime app resource descriptor from package_dir/resource_dir/profile.json.
 *
 * Runtime profile.json is a system_core resource descriptor. Its root field points to the JSON UI document.
 */
std::expected<RuntimeAppResourceDescriptor, std::string> read_runtime_app_resource_descriptor(
    const AppManifest &manifest
);

/**
 * @brief Read runtime app GUI startup descriptor from package_dir/resource_dir/profile.json.
 */
std::expected<AppGuiDescriptor, std::string> read_runtime_app_gui_descriptor(const AppManifest &manifest);

/**
 * @brief Convert a system_core app manifest to a runtime app config.
 */
std::expected<runtime::AppConfig, std::string> make_runtime_app_config(const AppManifest &manifest);

} // namespace esp_brookesia::system::core
