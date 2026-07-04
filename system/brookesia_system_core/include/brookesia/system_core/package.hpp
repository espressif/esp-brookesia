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
#include "brookesia/system_core/app.hpp"

namespace esp_brookesia::system::core {

/**
 * @brief Unpack an .bpk package (ZIP) to an install directory and return the parsed app manifest.
 *
 * The app package manifest is owned by system_core because it describes system-level app metadata,
 * runtime configuration, and GUI integration fields.
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
 * @brief Convert a system_core app manifest to a RuntimeManager app config.
 */
std::expected<runtime::AppConfig, std::string> make_runtime_app_config(const AppManifest &manifest);

/**
 * @brief Options for verify_app_package_release (RSA public key PEM on filesystem).
 *
 * Verifies embedded release signing only: META-INF/hash.json + META-INF/signature.sig inside the .bpk
 * (same rules as tools/app_pack.py verify).
 */
struct AppPackageReleaseVerifyOptions {
    std::string public_key_pem_path;
};

/**
 * @brief Verify PKCS#1 RSA-PSS-SHA256 over the digest JSON and per-member SHA-256 (same rules as app_pack.py verify).
 *
 * On ESP-IDF with CONFIG_BROOKESIA_SYSTEM_CORE_ENABLE_PACKAGE_RELEASE_VERIFY, uses mbedTLS.
 * If that Kconfig is disabled, returns an error string. On PC builds, returns an error (use Python verify).
 */
std::expected<void, std::string> verify_app_package_release(
    std::string_view package_path,
    const AppPackageReleaseVerifyOptions &options
);

} // namespace esp_brookesia::system::core
