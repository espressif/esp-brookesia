/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "brookesia/service_manager.hpp"
#include "brookesia/system_core/app/types.hpp"

namespace esp_brookesia::system::core {

class System;

class SystemCoreHelper {
public:
    enum class FunctionId : uint8_t {
        GetSystemType,
        GetSystemInfo,
        GetEnvironment,
        GetActiveApp,
        ListApps,
        RequestCloseApp,
        StartApp,
        StopApp,
        ShowLoading,
        HideLoading,
        ShowKeyboard,
        HideKeyboard,
        ShowMessageDialog,
        HideMessageDialog,
        UpdateMessageDialog,
        GetStorageLayout,
        GetAppStoragePaths,
        GetPublicStoragePaths,
        SetDefaultInstallStorage,
        StorageList,
        StorageStat,
        StorageMkdir,
        StorageRead,
        StorageWrite,
        StorageRemove,
        StorageRename,
        Max,
    };
    enum class EventId : uint8_t {
        AppChanged,
        KeyboardClosed,
        MessageDialogClosed,
        Max,
    };
    enum class AppIdParam : uint8_t {
        AppId,
    };
    enum class KeyboardOptionsParam : uint8_t {
        Options,
    };
    enum class KeyboardRequestParam : uint8_t {
        RequestId,
    };
    enum class MessageDialogOptionsParam : uint8_t {
        Options,
    };
    enum class MessageDialogRequestParam : uint8_t {
        RequestId,
    };
    enum class MessageDialogUpdateParam : uint8_t {
        RequestId,
        Options,
    };
    enum class StorageInstallParam : uint8_t {
        Target,
        PreferredExternalId,
    };
    enum class StorageLocationParam : uint8_t {
        Location,
    };
    enum class StorageWriteParam : uint8_t {
        Location,
        Data,
    };
    enum class StorageRenameParam : uint8_t {
        From,
        To,
    };

    static constexpr std::string_view get_name()
    {
        return BROOKESIA_SYSTEM_CORE_SERVICE_NAME;
    }
    static std::span<const service::FunctionSchema> get_function_schemas();
    static std::span<const service::EventSchema> get_event_schemas();
};
BROOKESIA_DESCRIBE_ENUM(
    SystemCoreHelper::FunctionId,
    GetSystemType,
    GetSystemInfo,
    GetEnvironment,
    GetActiveApp,
    ListApps,
    RequestCloseApp,
    StartApp,
    StopApp,
    ShowLoading,
    HideLoading,
    ShowKeyboard,
    HideKeyboard,
    ShowMessageDialog,
    HideMessageDialog,
    UpdateMessageDialog,
    GetStorageLayout,
    GetAppStoragePaths,
    GetPublicStoragePaths,
    SetDefaultInstallStorage,
    StorageList,
    StorageStat,
    StorageMkdir,
    StorageRead,
    StorageWrite,
    StorageRemove,
    StorageRename,
    Max
)
BROOKESIA_DESCRIBE_ENUM(SystemCoreHelper::EventId, AppChanged, KeyboardClosed, MessageDialogClosed, Max)
BROOKESIA_DESCRIBE_ENUM(SystemCoreHelper::AppIdParam, AppId)
BROOKESIA_DESCRIBE_ENUM(SystemCoreHelper::KeyboardOptionsParam, Options)
BROOKESIA_DESCRIBE_ENUM(SystemCoreHelper::KeyboardRequestParam, RequestId)
BROOKESIA_DESCRIBE_ENUM(SystemCoreHelper::MessageDialogOptionsParam, Options)
BROOKESIA_DESCRIBE_ENUM(SystemCoreHelper::MessageDialogRequestParam, RequestId)
BROOKESIA_DESCRIBE_ENUM(SystemCoreHelper::MessageDialogUpdateParam, RequestId, Options)
BROOKESIA_DESCRIBE_ENUM(SystemCoreHelper::StorageInstallParam, Target, PreferredExternalId)
BROOKESIA_DESCRIBE_ENUM(SystemCoreHelper::StorageLocationParam, Location)
BROOKESIA_DESCRIBE_ENUM(SystemCoreHelper::StorageWriteParam, Location, Data)
BROOKESIA_DESCRIBE_ENUM(SystemCoreHelper::StorageRenameParam, From, To)

class SystemService final: public service::ServiceBase {
public:
    explicit SystemService(System &system);
    bool publish_keyboard_closed(const KeyboardResult &result);
    bool publish_message_dialog_closed(const MessageDialogResult &result);

private:
    std::vector<service::FunctionSchema> get_function_schemas() override;
    std::vector<service::EventSchema> get_event_schemas() override;
    FunctionHandlerMap get_function_handlers() override;

    service::FunctionResult get_system_type();
    service::FunctionResult get_system_info();
    service::FunctionResult get_environment();
    service::FunctionResult get_active_app();
    service::FunctionResult list_apps();
    service::FunctionResult request_close_app(service::FunctionParameterMap &&params);
    service::FunctionResult start_app(service::FunctionParameterMap &&params);
    service::FunctionResult stop_app(service::FunctionParameterMap &&params);
    service::FunctionResult show_loading();
    service::FunctionResult hide_loading();
    service::FunctionResult show_keyboard(service::FunctionParameterMap &&params);
    service::FunctionResult hide_keyboard(service::FunctionParameterMap &&params);
    service::FunctionResult show_message_dialog(service::FunctionParameterMap &&params);
    service::FunctionResult hide_message_dialog(service::FunctionParameterMap &&params);
    service::FunctionResult update_message_dialog(service::FunctionParameterMap &&params);
    service::FunctionResult get_storage_layout();
    service::FunctionResult get_app_storage_paths();
    service::FunctionResult get_public_storage_paths();
    service::FunctionResult set_default_install_storage(service::FunctionParameterMap &&params);
    service::FunctionResult storage_list(service::FunctionParameterMap &&params);
    service::FunctionResult storage_stat(service::FunctionParameterMap &&params);
    service::FunctionResult storage_mkdir(service::FunctionParameterMap &&params);
    service::FunctionResult storage_read(service::FunctionParameterMap &&params);
    service::FunctionResult storage_write(service::FunctionParameterMap &&params);
    service::FunctionResult storage_remove(service::FunctionParameterMap &&params);
    service::FunctionResult storage_rename(service::FunctionParameterMap &&params);

    System &system_;
};

} // namespace esp_brookesia::system::core
