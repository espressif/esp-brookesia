/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>

#include "brookesia/service_helper/system/storage.hpp"
#include "private/utils.hpp"

namespace esp_brookesia::system::core {
namespace detail {

inline constexpr uint32_t STORAGE_FS_TIMEOUT_MS = 5000;

} // namespace detail

// Each package install works in `<app root>/.installing/<unique>/`. While an update is
// replacing an installed app, the old directory waits in `previous/`. Once the new version
// is registered, `committing` is written and renamed to `committed`; only `committed`
// counts. Without it, boot recovery puts `previous/` back and moves the unconfirmed copy
// to `rejected/`.
inline constexpr const char *APP_INSTALL_STAGING_DIR = ".installing";
inline constexpr const char *APP_PREVIOUS_VERSION_DIR = "previous";
inline constexpr const char *APP_REJECTED_VERSION_DIR = "rejected";
inline constexpr const char *APP_INSTALL_COMMITTING_FILE = "committing";
inline constexpr const char *APP_INSTALL_COMMITTED_FILE = "committed";

enum class PathState {
    Missing,
    Exists,
    Unknown,
};

inline PathState query_path_state(const std::filesystem::path &path)
{
    auto info = service::helper::Storage::fs_stat(path.generic_string(), detail::STORAGE_FS_TIMEOUT_MS);
    if (!info) {
        return PathState::Unknown;
    }
    return info->exists ? PathState::Exists : PathState::Missing;
}

inline std::expected<std::uintmax_t, std::string> remove_path_tree(
    const std::filesystem::path &path,
    std::string_view label
)
{
    if (path.empty()) {
        return std::unexpected("Failed to remove " + std::string(label) + ": path is empty");
    }

    auto before = service::helper::Storage::fs_stat(path.generic_string(), detail::STORAGE_FS_TIMEOUT_MS);
    if (!before) {
        return std::unexpected(
                   "Failed to check " + std::string(label) + " before removal '" + path.generic_string() +
                   "': " + before.error()
               );
    }
    const bool existed_before = before->exists;

    auto remove_result = service::helper::Storage::fs_remove(path.generic_string(), detail::STORAGE_FS_TIMEOUT_MS);
    if (!remove_result) {
        return std::unexpected(
                   "Failed to remove " + std::string(label) + " '" + path.generic_string() + "': " +
                   remove_result.error()
               );
    }

    auto after = service::helper::Storage::fs_stat(path.generic_string(), detail::STORAGE_FS_TIMEOUT_MS);
    if (!after) {
        return std::unexpected(
                   "Failed to check " + std::string(label) + " after removal '" + path.generic_string() +
                   "': " + after.error()
               );
    }
    if (after->exists) {
        return std::unexpected(
                   "Failed to remove " + std::string(label) + " '" + path.generic_string() +
                   "': path still exists after recursive removal"
               );
    }

    if (!existed_before) {
        BROOKESIA_LOGW("%1% did not exist before removal: path(%2%)", std::string(label), path.generic_string());
    }
    BROOKESIA_LOGI(
        "Removed %1%: path(%2%), entries(%3%)",
        std::string(label),
        path.generic_string(),
        existed_before ? 1ULL : 0ULL
    );
    return existed_before ? 1 : 0;
}

} // namespace esp_brookesia::system::core
