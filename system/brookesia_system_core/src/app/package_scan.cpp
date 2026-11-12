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
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "brookesia/service_helper/system/storage.hpp"
#include "brookesia/system_core/app/package.hpp"
#include "private/filesystem.hpp"

namespace esp_brookesia::system::core {
namespace {

constexpr const char *UNSUPPORTED_SYSTEM_ERROR_PREFIX = "Package does not support system type";
constexpr uint32_t STORAGE_FS_TIMEOUT_MS = 5000;

bool is_unsupported_system_error(std::string_view error)
{
    return error.starts_with(UNSUPPORTED_SYSTEM_ERROR_PREFIX);
}

bool rename_path(const std::filesystem::path &from, const std::filesystem::path &to)
{
    auto renamed = service::helper::Storage::fs_rename(from.generic_string(), to.generic_string(), STORAGE_FS_TIMEOUT_MS);
    if (!renamed) {
        BROOKESIA_LOGW("Failed to move %1% to %2%: %3%", from.generic_string(), to.generic_string(), renamed.error());
    }
    return renamed.has_value();
}

std::optional<std::string> read_install_target(const std::filesystem::path &staging_dir)
{
    const auto path = staging_dir / APP_INSTALL_TARGET_FILE;
    auto info = service::helper::Storage::fs_stat(path.generic_string(), STORAGE_FS_TIMEOUT_MS);
    if (!info || !info->exists || info->type != service::helper::Storage::FileType::File || info->size > 255) {
        return std::nullopt;
    }
    auto target = service::helper::Storage::fs_read_text(path.generic_string(), STORAGE_FS_TIMEOUT_MS);
    if (!target || !is_safe_app_directory_name(*target) ||
            !staging_dir.filename().generic_string().starts_with(*target + "-")) {
        return std::nullopt;
    }
    return std::move(*target);
}

bool is_install_transaction_target(const std::filesystem::path &install_dir, const std::filesystem::path &staging_dir)
{
    const auto path = install_dir / APP_INSTALL_TRANSACTION_FILE;
    auto info = service::helper::Storage::fs_stat(path.generic_string(), STORAGE_FS_TIMEOUT_MS);
    if (!info || !info->exists || info->type != service::helper::Storage::FileType::File || info->size > 512) {
        return false;
    }
    auto transaction = service::helper::Storage::fs_read_text(path.generic_string(), STORAGE_FS_TIMEOUT_MS);
    return transaction && *transaction == staging_dir.filename().generic_string();
}

// Returns true when the staging directory no longer holds anything needed to recover an app.
bool restore_previous_version(
    const std::filesystem::path &app_root, const std::filesystem::path &staging_dir, std::string_view system_type
)
{
    const auto previous_dir = staging_dir / APP_PREVIOUS_VERSION_DIR;
    const auto target = read_install_target(staging_dir);
    auto manifest = read_unpacked_app_manifest(previous_dir.generic_string(), system_type);
    if (!target || !manifest || manifest->id != *target) {
        BROOKESIA_LOGW("Keep previous version with unverified install target: %1%", previous_dir.generic_string());
        return false;
    }
    const auto install_dir = app_root / *target;
    const auto install_state = query_path_state(install_dir);
    if (install_state == PathState::Unknown) {
        BROOKESIA_LOGW("Keep previous version of %1%: cannot check %2%", manifest->id, install_dir.generic_string());
        return false;
    }
    if (install_state == PathState::Exists) {
        if (!is_install_transaction_target(install_dir, staging_dir)) {
            BROOKESIA_LOGW("Keep previous version; target belongs to another install: %1%", install_dir.generic_string());
            return false;
        }
        const auto rejected_dir = staging_dir / APP_REJECTED_VERSION_DIR;
        if ((query_path_state(rejected_dir) != PathState::Missing) || !rename_path(install_dir, rejected_dir)) {
            BROOKESIA_LOGW("Keep previous version of %1%: cannot move the unconfirmed copy aside", manifest->id);
            return false;
        }
    }
    if (!rename_path(previous_dir, install_dir)) {
        return false;
    }
    BROOKESIA_LOGW("Restored %1% %2% after an unfinished update", manifest->id, manifest->version);
    return true;
}

// A committed update may drop its backup only while the committed copy is in place.
bool committed_target_present(
    const std::filesystem::path &app_root, const std::filesystem::path &staging_dir, std::string_view system_type
)
{
    const auto previous_dir = staging_dir / APP_PREVIOUS_VERSION_DIR;
    const auto target = read_install_target(staging_dir);
    auto manifest = read_unpacked_app_manifest(previous_dir.generic_string(), system_type);
    if (!target || !manifest || manifest->id != *target || !is_install_transaction_target(app_root / *target, staging_dir)) {
        BROOKESIA_LOGW("Keep previous version with unverified committed target: %1%", previous_dir.generic_string());
        return false;
    }
    const auto target_manifest = app_root / manifest->id / APP_MANIFEST_FILE;
    if (query_path_state(target_manifest) != PathState::Exists) {
        BROOKESIA_LOGW("Keep previous version of %1%: the committed copy is not in place", manifest->id);
        return false;
    }
    return true;
}

void recover_interrupted_installs(const std::filesystem::path &app_root, std::string_view system_type)
{
    const auto installing_root = app_root / APP_INSTALL_STAGING_DIR;
    auto entries = service::helper::Storage::fs_list(installing_root.generic_string(), STORAGE_FS_TIMEOUT_MS);
    if (!entries) {
        return;
    }
    for (const auto &entry : entries.value()) {
        if (entry.info.type != service::helper::Storage::FileType::Directory || !is_safe_app_directory_name(entry.name)) {
            continue;
        }
        const auto staging_dir = installing_root / entry.name;
        const auto committed_state = query_path_state(staging_dir / APP_INSTALL_COMMITTED_FILE);
        const auto previous_state = query_path_state(staging_dir / APP_PREVIOUS_VERSION_DIR);
        if ((committed_state == PathState::Unknown) || (previous_state == PathState::Unknown)) {
            BROOKESIA_LOGW("Keep staging directory that cannot be checked: %1%", staging_dir.generic_string());
            continue;
        }
        bool removable = false;
        if (previous_state == PathState::Missing) {
            removable = true;
        } else if (committed_state == PathState::Exists) {
            removable = committed_target_present(app_root, staging_dir, system_type);
        } else {
            removable = restore_previous_version(app_root, staging_dir, system_type);
        }
        if (!removable) {
            continue;
        }
        auto removed = remove_path_tree(staging_dir, "finished runtime app staging directory");
        if (!removed) {
            BROOKESIA_LOGW("%1%", removed.error());
        }
    }
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

        recover_interrupted_installs(app_path, system_type_);

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

        std::vector<std::pair<std::string, std::string>> failed_packages;
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
                failed_packages.emplace_back(package_dir.string(), manifest.error());
                continue;
            }
            if (has_manifest(manifest->id)) {
                BROOKESIA_LOGW("Skip duplicate runtime app: root(%1%), manifest(%2%)", label, manifest->id);
                continue;
            }
            auto install_result = owner_.install_runtime_app(*manifest);
            if (!install_result) {
                failed_packages.emplace_back(manifest->id, install_result.error());
            }
        }
        if (!failed_packages.empty())
        {
            BROOKESIA_LOGW("Failed to load %1% runtime app packages from %2%", failed_packages.size(), label);
            for (const auto &[app, error] : failed_packages) {
                BROOKESIA_LOGW("Runtime app unavailable: app(%1%), error(%2%)", app, error);
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
