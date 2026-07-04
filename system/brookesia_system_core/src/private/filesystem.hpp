/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include "private/utils.hpp"

namespace esp_brookesia::system::core {
namespace detail {

inline std::string make_errno_error(std::string_view action, const std::filesystem::path &path)
{
    return std::string(action) + " '" + path.generic_string() + "': " + std::strerror(errno);
}

inline std::expected<std::uintmax_t, std::string> remove_path_tree_recursive(const std::filesystem::path &path)
{
    const auto path_string = path.generic_string();
    struct stat info = {};
    if (stat(path_string.c_str(), &info) != 0) {
        if (errno == ENOENT) {
            return 0;
        }
        return std::unexpected(make_errno_error("Failed to stat path", path));
    }

    if (!S_ISDIR(info.st_mode)) {
        if (unlink(path_string.c_str()) != 0) {
            return std::unexpected(make_errno_error("Failed to remove file", path));
        }
        return 1;
    }

    DIR *raw_dir = opendir(path_string.c_str());
    if (raw_dir == nullptr) {
        return std::unexpected(make_errno_error("Failed to open directory", path));
    }

    std::uintmax_t removed_count = 0;
    while (true) {
        errno = 0;
        dirent *entry = readdir(raw_dir);
        if (entry == nullptr) {
            if (errno != 0) {
                const auto error = make_errno_error("Failed to read directory", path);
                closedir(raw_dir);
                return std::unexpected(error);
            }
            break;
        }

        const std::string_view name(entry->d_name);
        if (name == "." || name == "..") {
            continue;
        }

        auto child_result = remove_path_tree_recursive(path / std::string(name));
        if (!child_result) {
            closedir(raw_dir);
            return child_result;
        }
        removed_count += *child_result;
    }

    if (closedir(raw_dir) != 0) {
        return std::unexpected(make_errno_error("Failed to close directory", path));
    }
    if (rmdir(path_string.c_str()) != 0) {
        return std::unexpected(make_errno_error("Failed to remove directory", path));
    }
    return removed_count + 1;
}

} // namespace detail

inline std::expected<std::uintmax_t, std::string> remove_path_tree(
    const std::filesystem::path &path,
    std::string_view label
)
{
    if (path.empty()) {
        return std::unexpected("Failed to remove " + std::string(label) + ": path is empty");
    }

    const auto path_string = path.generic_string();
    struct stat before = {};
    const bool existed_before = stat(path_string.c_str(), &before) == 0;
    if (!existed_before && errno != ENOENT) {
        return std::unexpected(detail::make_errno_error("Failed to check path before removal", path));
    }

    auto removed_count = detail::remove_path_tree_recursive(path);
    if (!removed_count) {
        return std::unexpected(
                   "Failed to remove " + std::string(label) + " '" + path.generic_string() + "': " +
                   removed_count.error()
               );
    }

    struct stat after = {};
    if (stat(path_string.c_str(), &after) == 0) {
        return std::unexpected(
                   "Failed to remove " + std::string(label) + " '" + path.generic_string() +
                   "': path still exists after recursive removal"
               );
    }
    if (errno != ENOENT) {
        return std::unexpected(detail::make_errno_error("Failed to check path after removal", path));
    }

    if (!existed_before) {
        BROOKESIA_LOGW("%1% did not exist before removal: path(%2%)", std::string(label), path.generic_string());
    }
    BROOKESIA_LOGI(
        "Removed %1%: path(%2%), entries(%3%)",
        std::string(label),
        path.generic_string(),
        static_cast<unsigned long long>(*removed_count)
    );
    return removed_count;
}

} // namespace esp_brookesia::system::core
