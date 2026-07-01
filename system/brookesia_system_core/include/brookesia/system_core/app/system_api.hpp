/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <expected>
#include <optional>
#include <string>
#include <vector>

#include "boost/json.hpp"
#include "brookesia/system_core/app/types.hpp"
#include "brookesia/system_core/system/storage.hpp"

namespace esp_brookesia::system::core {

class System;

class SystemApi {
public:
    SystemApi() = default;
    SystemApi(System &system, AppId app_id);

    std::string get_system_type() const;
    SystemInfo get_system_info() const;
    boost::json::object get_environment() const;
    StorageLayout get_storage_layout() const;
    std::expected<AppStoragePaths, std::string> get_app_storage_paths(
        std::optional<AppId> app_id = std::nullopt
    ) const;
    std::expected<PublicStoragePaths, std::string> get_public_storage_paths() const;
    std::expected<void, std::string> set_default_install_storage(
        StorageInstallTarget target,
        std::string preferred_external_id = {}
    ) const;
    std::expected<std::vector<StorageFileEntry>, std::string> storage_list(
        const StorageFileLocation &location
    ) const;
    std::expected<StorageFileInfo, std::string> storage_stat(const StorageFileLocation &location) const;
    std::expected<void, std::string> storage_mkdir(const StorageFileLocation &location) const;
    std::expected<std::string, std::string> storage_read(const StorageFileLocation &location) const;
    std::expected<void, std::string> storage_write(
        const StorageFileLocation &location,
        std::string_view data
    ) const;
    std::expected<void, std::string> storage_remove(const StorageFileLocation &location) const;
    std::expected<void, std::string> storage_rename(
        const StorageFileLocation &from,
        const StorageFileLocation &to
    ) const;
    std::optional<AppInfo> get_active_app() const;
    std::vector<AppInfo> list_apps() const;
    std::expected<void, std::string> request_close_app(AppId app_id) const;
    std::expected<void, std::string> start_app(AppId app_id) const;
    std::expected<void, std::string> stop_app(AppId app_id) const;
    std::expected<void, std::string> uninstall_app(AppId app_id) const;
    std::expected<AppId, std::string> install_runtime_app_package(
        std::string_view package_path,
        bool replace_existing = true
    ) const;
    std::expected<void, std::string> show_loading() const;
    std::expected<void, std::string> hide_loading() const;
    std::expected<KeyboardRequestId, std::string> show_keyboard(
        KeyboardRequestOptions options,
        KeyboardResultHandler handler = {}
    ) const;
    std::expected<void, std::string> hide_keyboard(KeyboardRequestId request_id) const;
    std::expected<MessageDialogRequestId, std::string> show_message_dialog(
        MessageDialogOptions options,
        MessageDialogResultHandler handler = {}
    ) const;
    std::expected<void, std::string> hide_message_dialog(MessageDialogRequestId request_id) const;
    std::expected<void, std::string> update_message_dialog(
        MessageDialogRequestId request_id,
        MessageDialogOptions options
    ) const;

private:
    bool is_native_system_app() const;

    System *system_ = nullptr;
    AppId app_id_ = INVALID_APP_ID;
};

} // namespace esp_brookesia::system::core
