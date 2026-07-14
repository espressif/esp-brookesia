/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/system_core/macro_configs.h"
#if !BROOKESIA_SYSTEM_CORE_SYSTEM_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "private/system/impl.hpp"

#include <algorithm>
#include "brookesia/service_helper.hpp"
#include "brookesia/system_core/app/package.hpp"

namespace esp_brookesia::system::core {
namespace {

constexpr const char *UNSUPPORTED_SYSTEM_ERROR_PREFIX = "Package does not support system type";
constexpr uint32_t STORAGE_FS_TIMEOUT_MS = 5000;

bool is_unsupported_system_error(std::string_view error)
{
    return error.starts_with(UNSUPPORTED_SYSTEM_ERROR_PREFIX);
}

} // namespace

std::expected<void, std::string> System::Impl::install_unpacked_apps()
{
    auto scan_app_root = [this](const std::string & root, std::string_view label) -> std::expected<void, std::string> {
        if (root.empty())
        {
            BROOKESIA_LOGI("Runtime app root is empty, skip package app installation: root(%1%)", label);
            return {};
        }

        const std::filesystem::path app_path(root);
        auto root_info = service::helper::Storage::fs_stat(app_path.generic_string(), STORAGE_FS_TIMEOUT_MS);
        if (!root_info || !root_info->exists)
        {
            BROOKESIA_LOGI(
                "Runtime app path does not exist, skip: root(%1%), path(%2%)",
                label, app_path.string()
            );
            return {};
        }
        if (root_info->type != service::helper::Storage::FileType::Directory)
        {
            BROOKESIA_LOGW(
                "Runtime app path is not a directory, skip: root(%1%), path(%2%)",
                label, app_path.string()
            );
            return {};
        }

        std::vector<std::filesystem::path> package_dirs;
        auto entries = service::helper::Storage::fs_list(app_path.generic_string(), STORAGE_FS_TIMEOUT_MS);
        if (!entries)
        {
            BROOKESIA_LOGW(
                "Runtime app directory scan failed: root(%1%), path(%2%), error(%3%)",
                label, app_path.string(), entries.error()
            );
        } else
        {
            for (const auto &entry : entries.value()) {
                if (entry.info.type != service::helper::Storage::FileType::Directory) {
                    continue;
                }
                auto package_dir = app_path / entry.name;
                auto manifest_info = service::helper::Storage::fs_stat(
                                         (package_dir / APP_MANIFEST_FILE).generic_string(), STORAGE_FS_TIMEOUT_MS
                                     );
                if (manifest_info && manifest_info->exists) {
                    package_dirs.push_back(std::move(package_dir));
                }
            }
        }
        std::sort(package_dirs.begin(), package_dirs.end());

        for (const auto &package_dir : package_dirs)
        {
            auto manifest = read_unpacked_app_manifest(package_dir.generic_string(), system_type_);
            if (!manifest) {
                if (is_unsupported_system_error(manifest.error())) {
                    BROOKESIA_LOGI(
                        "Skip runtime app package unsupported by system: path(%1%), system(%2%)",
                        package_dir.string(), system_type_
                    );
                    continue;
                }
                BROOKESIA_LOGW(
                    "Skip runtime app package: path(%1%), error(%2%)",
                    package_dir.string(), manifest.error()
                );
                continue;
            }
            if (manifest_id_to_app_.contains(manifest->id)) {
                BROOKESIA_LOGW("Skip duplicate runtime app: root(%1%), manifest(%2%)", label, manifest->id);
                continue;
            }
            auto install_result = owner_.install_runtime_app(*manifest);
            if (!install_result) {
                return std::unexpected(install_result.error());
            }
        }
        return {};
    };

    const auto app_roots = get_app_scan_roots();
    for (size_t index = 0; index < app_roots.size(); ++index) {
        auto install_result = scan_app_root(app_roots[index].generic_string(), "storage_app_" + std::to_string(index));
        if (!install_result) {
            return install_result;
        }
    }
    return {};
}

} // namespace esp_brookesia::system::core
