/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/system_core/macro_configs.h"
#if !BROOKESIA_SYSTEM_CORE_SERVICE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "private/service/utils.hpp"
#include "brookesia/service_helper.hpp"

#include <algorithm>
#include <array>
#include <utility>
#include <vector>

namespace esp_brookesia::system::core {

namespace {

std::expected<std::string, std::string> get_optional_string_field(
    const boost::json::object &object,
    std::string_view primary,
    std::string_view fallback,
    std::string default_value = {}
)
{
    if (auto it = object.find(primary); it != object.end()) {
        if (!it->value().is_string()) {
            return std::unexpected("Field '" + std::string(primary) + "' must be a string");
        }
        return std::string(it->value().as_string());
    }
    if (auto it = object.find(fallback); it != object.end()) {
        if (!it->value().is_string()) {
            return std::unexpected("Field '" + std::string(fallback) + "' must be a string");
        }
        return std::string(it->value().as_string());
    }
    return default_value;
}

std::expected<int32_t, std::string> get_optional_int_field(
    const boost::json::object &object,
    std::string_view primary,
    std::string_view fallback,
    int32_t default_value = 0
)
{
    if (auto it = object.find(primary); it != object.end()) {
        if (!it->value().is_number()) {
            return std::unexpected("Field '" + std::string(primary) + "' must be a number");
        }
        if (it->value().is_int64()) {
            return static_cast<int32_t>(it->value().as_int64());
        }
        if (it->value().is_uint64()) {
            return static_cast<int32_t>(it->value().as_uint64());
        }
        return static_cast<int32_t>(it->value().as_double());
    }
    if (auto it = object.find(fallback); it != object.end()) {
        if (!it->value().is_number()) {
            return std::unexpected("Field '" + std::string(fallback) + "' must be a number");
        }
        if (it->value().is_int64()) {
            return static_cast<int32_t>(it->value().as_int64());
        }
        if (it->value().is_uint64()) {
            return static_cast<int32_t>(it->value().as_uint64());
        }
        return static_cast<int32_t>(it->value().as_double());
    }
    return default_value;
}

std::expected<bool, std::string> get_optional_bool_field(
    const boost::json::object &object,
    std::string_view primary,
    std::string_view fallback,
    bool default_value = false
)
{
    if (auto it = object.find(primary); it != object.end()) {
        if (!it->value().is_bool()) {
            return std::unexpected("Field '" + std::string(primary) + "' must be a boolean");
        }
        return it->value().as_bool();
    }
    if (auto it = object.find(fallback); it != object.end()) {
        if (!it->value().is_bool()) {
            return std::unexpected("Field '" + std::string(fallback) + "' must be a boolean");
        }
        return it->value().as_bool();
    }
    return default_value;
}

bool is_supported_keyboard_mode(std::string_view mode)
{
    return mode == "text" || mode == "upper" || mode == "number" || mode == "special";
}

std::expected<std::vector<std::string>, std::string> get_optional_keyboard_modes_field(
    const boost::json::object &object,
    std::string_view primary,
    std::string_view fallback
)
{
    const boost::json::value *value = nullptr;
    if (auto it = object.find(primary); it != object.end()) {
        value = &it->value();
    } else if (auto it = object.find(fallback); it != object.end()) {
        value = &it->value();
    }
    if (value == nullptr) {
        return std::vector<std::string>();
    }
    if (!value->is_array() || value->as_array().empty()) {
        return std::unexpected("Keyboard allowedModes must be a non-empty string array");
    }

    std::vector<std::string> modes;
    for (const auto &entry : value->as_array()) {
        if (!entry.is_string()) {
            return std::unexpected("Keyboard allowedModes must only contain strings");
        }
        std::string mode(entry.as_string().c_str());
        if (!is_supported_keyboard_mode(mode)) {
            return std::unexpected("Keyboard allowedModes contains unsupported mode: " + mode);
        }
        if (std::find(modes.begin(), modes.end(), mode) != modes.end()) {
            return std::unexpected("Keyboard allowedModes contains duplicate mode: " + mode);
        }
        modes.push_back(std::move(mode));
    }
    return modes;
}

std::expected<KeyboardRequestOptions, std::string> keyboard_options_from_json(
    const boost::json::object &object
)
{
    auto title = get_optional_string_field(object, "title", "Title");
    auto placeholder = get_optional_string_field(object, "placeholder", "Placeholder");
    auto initial_text = get_optional_string_field(object, "initial_text", "initialText");
    auto max_length = get_optional_int_field(object, "max_length", "maxLength");
    auto password = get_optional_bool_field(object, "password", "Password");
    auto mode = get_optional_string_field(object, "mode", "Mode", "text");
    auto allowed_modes = get_optional_keyboard_modes_field(object, "allowedModes", "allowed_modes");
    if (!title) {
        return std::unexpected(title.error());
    }
    if (!placeholder) {
        return std::unexpected(placeholder.error());
    }
    if (!initial_text) {
        return std::unexpected(initial_text.error());
    }
    if (!max_length) {
        return std::unexpected(max_length.error());
    }
    if (!password) {
        return std::unexpected(password.error());
    }
    if (!mode) {
        return std::unexpected(mode.error());
    }
    if (!allowed_modes) {
        return std::unexpected(allowed_modes.error());
    }
    if (*max_length < 0) {
        return std::unexpected("Keyboard max_length must not be negative");
    }
    if (!is_supported_keyboard_mode(*mode)) {
        return std::unexpected("Keyboard mode must be one of: text, upper, number, special");
    }

    return KeyboardRequestOptions{
        .title = std::move(*title),
        .placeholder = std::move(*placeholder),
        .initial_text = std::move(*initial_text),
        .max_length = *max_length,
        .password = *password,
        .mode = std::move(*mode),
        .allowed_modes = std::move(*allowed_modes),
    };
}

std::expected<MessageDialogOptions, std::string> message_dialog_options_from_json(
    const boost::json::object &object
)
{
    auto text = get_optional_string_field(object, "text", "Text");
    auto informative_text = get_optional_string_field(object, "informative_text", "informativeText");
    auto icon_string = get_optional_string_field(object, "icon", "Icon", "None");
    auto auto_close_ms = get_optional_int_field(object, "auto_close_ms", "autoCloseMs");
    if (!text) {
        return std::unexpected(text.error());
    }
    if (!informative_text) {
        return std::unexpected(informative_text.error());
    }
    if (!icon_string) {
        return std::unexpected(icon_string.error());
    }
    if (!auto_close_ms) {
        return std::unexpected(auto_close_ms.error());
    }

    MessageDialogIcon icon = MessageDialogIcon::None;
    if (!BROOKESIA_DESCRIBE_STR_TO_ENUM_FLEXIBLE(*icon_string, icon)) {
        return std::unexpected("Invalid message dialog icon: " + *icon_string);
    }

    std::vector<MessageDialogButton> buttons;
    const boost::json::value *buttons_value = nullptr;
    if (auto it = object.find("buttons"); it != object.end()) {
        buttons_value = &it->value();
    } else if (auto it = object.find("Buttons"); it != object.end()) {
        buttons_value = &it->value();
    }
    if (buttons_value != nullptr) {
        if (!buttons_value->is_array()) {
            return std::unexpected("Message dialog buttons must be an array");
        }
        const auto &buttons_array = buttons_value->as_array();
        if (buttons_array.size() > 3) {
            return std::unexpected("Message dialog supports at most 3 buttons");
        }
        buttons.reserve(buttons_array.size());
        for (const auto &entry : buttons_array) {
            if (!entry.is_object()) {
                return std::unexpected("Message dialog buttons must only contain objects");
            }
            const auto &button_object = entry.as_object();
            auto button_text = get_optional_string_field(button_object, "text", "Text");
            auto role_string = get_optional_string_field(button_object, "role", "Role", "Action");
            if (!button_text) {
                return std::unexpected(button_text.error());
            }
            if (!role_string) {
                return std::unexpected(role_string.error());
            }
            MessageDialogButtonRole role = MessageDialogButtonRole::Action;
            if (!BROOKESIA_DESCRIBE_STR_TO_ENUM_FLEXIBLE(*role_string, role) ||
                    role == MessageDialogButtonRole::Invalid) {
                return std::unexpected("Invalid message dialog button role: " + *role_string);
            }
            buttons.push_back(MessageDialogButton{
                .text = std::move(*button_text),
                .role = role,
            });
        }
    }

    return MessageDialogOptions{
        .text = std::move(*text),
        .informative_text = std::move(*informative_text),
        .icon = icon,
        .buttons = std::move(buttons),
        .auto_close_ms = *auto_close_ms,
    };
}

FunctionSchema make_keyboard_options_schema()
{
    return {
        .name = BROOKESIA_DESCRIBE_TO_STR(SystemCoreHelper::FunctionId::ShowKeyboard),
        .description = "Show caller app keyboard input overlay",
        .parameters = {
            {
                .name = BROOKESIA_DESCRIBE_TO_STR(SystemCoreHelper::KeyboardOptionsParam::Options),
                .description = "Keyboard request options",
                .type = FunctionValueType::Object,
            },
        },
        .require_scheduler = true,
    };
}

FunctionSchema make_keyboard_request_schema()
{
    return {
        .name = BROOKESIA_DESCRIBE_TO_STR(SystemCoreHelper::FunctionId::HideKeyboard),
        .description = "Hide caller app keyboard input overlay",
        .parameters = {
            {
                .name = BROOKESIA_DESCRIBE_TO_STR(SystemCoreHelper::KeyboardRequestParam::RequestId),
                .description = "Keyboard request id",
                .type = FunctionValueType::Number,
            },
        },
        .require_scheduler = true,
    };
}

FunctionSchema make_storage_location_schema(SystemCoreHelper::FunctionId id, std::string description)
{
    return {
        .name = BROOKESIA_DESCRIBE_TO_STR(id),
        .description = std::move(description),
        .parameters = {
            {
                .name = BROOKESIA_DESCRIBE_TO_STR(SystemCoreHelper::StorageLocationParam::Location),
                .description = "Storage location object: {kind, volume_id?, relative_path?}",
                .type = FunctionValueType::Object,
            },
        },
        .require_scheduler = true,
    };
}

FunctionSchema make_storage_location_schema(
    SystemCoreHelper::FunctionId id, std::string description, FunctionValueType return_type, std::string return_description
)
{
    auto schema = make_storage_location_schema(id, std::move(description));
    schema.return_value = service::FunctionReturnSchema{
        .type = return_type,
        .description = std::move(return_description),
    };
    return schema;
}

FunctionSchema with_return(
    FunctionSchema schema, FunctionValueType return_type, std::string return_description
)
{
    schema.return_value = service::FunctionReturnSchema{
        .type = return_type,
        .description = std::move(return_description),
    };
    return schema;
}

FunctionSchema make_storage_write_schema()
{
    return {
        .name = BROOKESIA_DESCRIBE_TO_STR(SystemCoreHelper::FunctionId::StorageWrite),
        .description = "Write data to a caller-owned storage file",
        .parameters = {
            {
                .name = BROOKESIA_DESCRIBE_TO_STR(SystemCoreHelper::StorageWriteParam::Location),
                .description = "Storage location object: {kind, volume_id?, relative_path?}",
                .type = FunctionValueType::Object,
            },
            {
                .name = BROOKESIA_DESCRIBE_TO_STR(SystemCoreHelper::StorageWriteParam::Data),
                .description = "File data",
                .type = FunctionValueType::String,
            },
        },
        .require_scheduler = true,
    };
}

FunctionSchema make_storage_rename_schema()
{
    return {
        .name = BROOKESIA_DESCRIBE_TO_STR(SystemCoreHelper::FunctionId::StorageRename),
        .description = "Rename a caller-owned storage file or directory",
        .parameters = {
            {
                .name = BROOKESIA_DESCRIBE_TO_STR(SystemCoreHelper::StorageRenameParam::From),
                .description = "Source storage location object",
                .type = FunctionValueType::Object,
            },
            {
                .name = BROOKESIA_DESCRIBE_TO_STR(SystemCoreHelper::StorageRenameParam::To),
                .description = "Destination storage location object",
                .type = FunctionValueType::Object,
            },
        },
        .require_scheduler = true,
    };
}

FunctionSchema make_message_dialog_options_schema()
{
    return {
        .name = BROOKESIA_DESCRIBE_TO_STR(SystemCoreHelper::FunctionId::ShowMessageDialog),
        .description = "Show caller app message dialog",
        .parameters = {
            {
                .name = BROOKESIA_DESCRIBE_TO_STR(SystemCoreHelper::MessageDialogOptionsParam::Options),
                .description = "Message dialog options",
                .type = FunctionValueType::Object,
            },
        },
        .require_scheduler = true,
        .return_value = service::FunctionReturnSchema{
            .type = FunctionValueType::Object,
            .description = "Message dialog request object. Example: {\"RequestId\":1}",
        },
    };
}

FunctionSchema make_message_dialog_request_schema()
{
    return {
        .name = BROOKESIA_DESCRIBE_TO_STR(SystemCoreHelper::FunctionId::HideMessageDialog),
        .description = "Hide caller app message dialog",
        .parameters = {
            {
                .name = BROOKESIA_DESCRIBE_TO_STR(SystemCoreHelper::MessageDialogRequestParam::RequestId),
                .description = "Message dialog request id",
                .type = FunctionValueType::Number,
            },
        },
        .require_scheduler = true,
    };
}

FunctionSchema make_message_dialog_update_schema()
{
    return {
        .name = BROOKESIA_DESCRIBE_TO_STR(SystemCoreHelper::FunctionId::UpdateMessageDialog),
        .description = "Update caller app message dialog",
        .parameters = {
            {
                .name = BROOKESIA_DESCRIBE_TO_STR(SystemCoreHelper::MessageDialogUpdateParam::RequestId),
                .description = "Message dialog request id",
                .type = FunctionValueType::Number,
            },
            {
                .name = BROOKESIA_DESCRIBE_TO_STR(SystemCoreHelper::MessageDialogUpdateParam::Options),
                .description = "Message dialog options",
                .type = FunctionValueType::Object,
            },
        },
        .require_scheduler = true,
    };
}

FunctionSchema make_storage_install_schema()
{
    return {
        .name = BROOKESIA_DESCRIBE_TO_STR(SystemCoreHelper::FunctionId::SetDefaultInstallStorage),
        .description = "Set default runtime app install storage. Native system app only.",
        .parameters = {
            {
                .name = BROOKESIA_DESCRIBE_TO_STR(SystemCoreHelper::StorageInstallParam::Target),
                .description = "Install target: Auto, Internal, or External",
                .type = FunctionValueType::String,
            },
            {
                .name = BROOKESIA_DESCRIBE_TO_STR(SystemCoreHelper::StorageInstallParam::PreferredExternalId),
                .description = "Preferred external volume id",
                .type = FunctionValueType::String,
                .default_value = FunctionValue(std::string()),
            },
        },
        .require_scheduler = true,
    };
}

boost::json::object to_json_object(const auto &value)
{
    auto json_value = BROOKESIA_DESCRIBE_TO_JSON(value);
    return json_value.is_object() ? json_value.as_object() : boost::json::object{};
}

boost::json::array to_json_array(const auto &value)
{
    auto json_value = BROOKESIA_DESCRIBE_TO_JSON(value);
    return json_value.is_array() ? json_value.as_array() : boost::json::array{};
}

std::string get_optional_flexible_string(
    const boost::json::object &object,
    std::string_view primary,
    std::string_view fallback
)
{
    if (auto it = object.find(primary); it != object.end() && it->value().is_string()) {
        return std::string(it->value().as_string());
    }
    if (auto it = object.find(fallback); it != object.end() && it->value().is_string()) {
        return std::string(it->value().as_string());
    }
    return {};
}

std::expected<StorageFileLocation, std::string> storage_location_from_json(const boost::json::object &object)
{
    auto kind_string = get_flexible_object_string(object, "Kind", "kind");
    if (!kind_string) {
        return std::unexpected(kind_string.error());
    }
    StoragePathKind kind = StoragePathKind::AppFiles;
    if (!BROOKESIA_DESCRIBE_STR_TO_ENUM_FLEXIBLE(*kind_string, kind)) {
        return std::unexpected("Invalid storage path kind: " + *kind_string);
    }
    return StorageFileLocation{
        .kind = kind,
        .volume_id = get_optional_flexible_string(object, "VolumeId", "volume_id"),
        .relative_path = get_optional_flexible_string(object, "RelativePath", "relative_path"),
    };
}

} // namespace

std::span<const service::FunctionSchema> SystemCoreHelper::get_function_schemas()
{
    static const std::array<service::FunctionSchema, BROOKESIA_DESCRIBE_ENUM_TO_NUM(FunctionId::Max)> SCHEMAS = {{
            with_return(
                make_no_param_schema(FunctionId::GetSystemType, "Get current system type"),
                FunctionValueType::String,
                "System type string"
            ),
            with_return(
                make_no_param_schema(FunctionId::GetSystemInfo, "Get current system information"),
                FunctionValueType::Object,
                "System information object. Example: {\"name\":\"SuperOS\",\"version\":\"0.8.0\"}"
            ),
            with_return(
                make_no_param_schema(FunctionId::GetEnvironment, "Get current GUI environment"),
                FunctionValueType::Object,
                "GUI environment object. Example: {\"widthDp\":800,\"heightDp\":480,\"language\":\"en\"}"
            ),
            with_return(
                make_no_param_schema(FunctionId::GetActiveApp, "Get active app snapshot"),
                FunctionValueType::Object,
                "Active app object, or empty object when no app is active. Example: {\"app_id\":1,\"manifest\":{},\"state\":\"Running\"}"
            ),
            with_return(
                make_no_param_schema(FunctionId::ListApps, "List installed apps"),
                FunctionValueType::Array,
                "Installed app array. Example: [{\"app_id\":1,\"manifest\":{},\"state\":\"Stopped\"}]"
            ),
            make_app_id_schema(FunctionId::RequestCloseApp, "Request closing an app"),
            make_app_id_schema(FunctionId::StartApp, "Start an app"),
            make_app_id_schema(FunctionId::StopApp, "Stop an app"),
            make_no_param_schema(FunctionId::ShowLoading, "Show caller app loading overlay"),
            make_no_param_schema(FunctionId::HideLoading, "Hide caller app loading overlay"),
            with_return(
                make_keyboard_options_schema(),
                FunctionValueType::Object,
                "Keyboard request object. Example: {\"RequestId\":1}"
            ),
            make_keyboard_request_schema(),
            make_message_dialog_options_schema(),
            make_message_dialog_request_schema(),
            make_message_dialog_update_schema(),
            with_return(
                make_no_param_schema(FunctionId::GetStorageLayout, "Get system storage layout"),
                FunctionValueType::Object,
                "Storage layout object. Example: {\"internal\":{},\"external\":[]}"
            ),
            with_return(
                make_no_param_schema(FunctionId::GetAppStoragePaths, "Get caller app storage paths"),
                FunctionValueType::Object,
                "Caller app storage paths object. Example: {\"internal\":{\"files\":\"/path\"},\"external\":[{\"volume_id\":\"sdcard\",\"files\":\"/sdcard/apps/app/files\"}]}"
            ),
            with_return(
                make_no_param_schema(FunctionId::GetPublicStoragePaths, "Get public storage paths"),
                FunctionValueType::Object,
                "Public storage paths object. Example: {\"internal\":{},\"external\":[]}"
            ),
            make_storage_install_schema(),
            make_storage_location_schema(
                FunctionId::StorageList,
                "List caller-owned storage directory",
                FunctionValueType::Array,
                "Storage entry array. Example: [{\"name\":\"game.nes\",\"info\":{\"type\":\"File\",\"size\":1024}}]"
            ),
            make_storage_location_schema(
                FunctionId::StorageStat,
                "Stat caller-owned storage path",
                FunctionValueType::Object,
                "Storage file info object. Example: {\"type\":\"Directory\",\"size\":0}"
            ),
            make_storage_location_schema(FunctionId::StorageMkdir, "Create caller-owned storage directory"),
            make_storage_location_schema(
                FunctionId::StorageRead,
                "Read caller-owned storage file",
                FunctionValueType::String,
                "File data"
            ),
            make_storage_write_schema(),
            make_storage_location_schema(FunctionId::StorageRemove, "Remove caller-owned storage file or directory"),
            make_storage_rename_schema(),
        }
    };
    return std::span<const service::FunctionSchema>(SCHEMAS);
}

std::span<const service::EventSchema> SystemCoreHelper::get_event_schemas()
{
    static const std::array<service::EventSchema, BROOKESIA_DESCRIBE_ENUM_TO_NUM(EventId::Max)> SCHEMAS = {{
            {
                .name = BROOKESIA_DESCRIBE_TO_STR(EventId::AppChanged),
                .description = "System app state changed",
                .require_scheduler = false,
            },
            {
                .name = BROOKESIA_DESCRIBE_TO_STR(EventId::KeyboardClosed),
                .description = "System keyboard input request closed",
                .items = {
                    {
                        .name = "RequestId",
                        .description = "Keyboard request id",
                        .type = service::EventItemType::Number,
                    },
                    {
                        .name = "AppId",
                        .description = "Owner app id",
                        .type = service::EventItemType::Number,
                    },
                    {
                        .name = "Confirmed",
                        .description = "Whether user confirmed the input",
                        .type = service::EventItemType::Boolean,
                    },
                    {
                        .name = "Text",
                        .description = "Confirmed text",
                        .type = service::EventItemType::String,
                    },
                },
                .require_scheduler = false,
            },
            {
                .name = BROOKESIA_DESCRIBE_TO_STR(EventId::MessageDialogClosed),
                .description = "System message dialog request closed",
                .items = {
                    {
                        .name = "RequestId",
                        .description = "Message dialog request id",
                        .type = service::EventItemType::Number,
                    },
                    {
                        .name = "AppId",
                        .description = "Owner app id",
                        .type = service::EventItemType::Number,
                    },
                    {
                        .name = "ButtonIndex",
                        .description = "Clicked button index, or -1 when no button was clicked",
                        .type = service::EventItemType::Number,
                    },
                    {
                        .name = "ButtonRole",
                        .description = "Clicked button role, or Invalid when no button was clicked",
                        .type = service::EventItemType::String,
                    },
                    {
                        .name = "Reason",
                        .description = "Close reason: Button, Timeout, or Closed",
                        .type = service::EventItemType::String,
                    },
                },
                .require_scheduler = false,
            },
        }
    };
    return std::span<const service::EventSchema>(SCHEMAS);
}

SystemService::SystemService(System &system)
    : ServiceBase({
    .name = SystemCoreHelper::get_name().data(),
    .dependencies = {
        service::helper::Device::get_name().data(),
    },
})
, system_(system)
{}

bool SystemService::publish_keyboard_closed(const KeyboardResult &result)
{
    boost::json::object data;
    data["RequestId"] = static_cast<double>(result.request_id);
    data["AppId"] = static_cast<double>(result.app_id);
    data["Confirmed"] = result.confirmed;
    data["Text"] = result.text;
    return publish_event(BROOKESIA_DESCRIBE_TO_STR(SystemCoreHelper::EventId::KeyboardClosed), std::move(data));
}

bool SystemService::publish_message_dialog_closed(const MessageDialogResult &result)
{
    boost::json::object data;
    data["RequestId"] = static_cast<double>(result.request_id);
    data["AppId"] = static_cast<double>(result.app_id);
    data["ButtonIndex"] = result.button_index;
    data["ButtonRole"] = std::string(BROOKESIA_DESCRIBE_ENUM_TO_STR(result.button_role));
    data["Reason"] = std::string(BROOKESIA_DESCRIBE_ENUM_TO_STR(result.reason));
    return publish_event(BROOKESIA_DESCRIBE_TO_STR(SystemCoreHelper::EventId::MessageDialogClosed), std::move(data));
}

std::vector<service::FunctionSchema> SystemService::get_function_schemas()
{
    auto schemas = SystemCoreHelper::get_function_schemas();
    return std::vector<service::FunctionSchema>(schemas.begin(), schemas.end());
}

std::vector<service::EventSchema> SystemService::get_event_schemas()
{
    auto schemas = SystemCoreHelper::get_event_schemas();
    return std::vector<service::EventSchema>(schemas.begin(), schemas.end());
}

service::ServiceBase::FunctionHandlerMap SystemService::get_function_handlers()
{
    using FunctionId = SystemCoreHelper::FunctionId;
    return {
        {
            BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetSystemType), [this](FunctionParameterMap &&)
            {
                return get_system_type();
            }
        },
        {
            BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetSystemInfo), [this](FunctionParameterMap &&)
            {
                return get_system_info();
            }
        },
        {
            BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetEnvironment), [this](FunctionParameterMap &&)
            {
                return get_environment();
            }
        },
        {
            BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetActiveApp), [this](FunctionParameterMap &&)
            {
                return get_active_app();
            }
        },
        {
            BROOKESIA_DESCRIBE_TO_STR(FunctionId::ListApps), [this](FunctionParameterMap &&)
            {
                return list_apps();
            }
        },
        {
            BROOKESIA_DESCRIBE_TO_STR(FunctionId::RequestCloseApp), [this](FunctionParameterMap &&params)
            {
                return request_close_app(std::move(params));
            }
        },
        {
            BROOKESIA_DESCRIBE_TO_STR(FunctionId::StartApp), [this](FunctionParameterMap &&params)
            {
                return start_app(std::move(params));
            }
        },
        {
            BROOKESIA_DESCRIBE_TO_STR(FunctionId::StopApp), [this](FunctionParameterMap &&params)
            {
                return stop_app(std::move(params));
            }
        },
        {
            BROOKESIA_DESCRIBE_TO_STR(FunctionId::ShowLoading), [this](FunctionParameterMap &&)
            {
                return show_loading();
            }
        },
        {
            BROOKESIA_DESCRIBE_TO_STR(FunctionId::HideLoading), [this](FunctionParameterMap &&)
            {
                return hide_loading();
            }
        },
        {
            BROOKESIA_DESCRIBE_TO_STR(FunctionId::ShowKeyboard), [this](FunctionParameterMap &&params)
            {
                return show_keyboard(std::move(params));
            }
        },
        {
            BROOKESIA_DESCRIBE_TO_STR(FunctionId::HideKeyboard), [this](FunctionParameterMap &&params)
            {
                return hide_keyboard(std::move(params));
            }
        },
        {
            BROOKESIA_DESCRIBE_TO_STR(FunctionId::ShowMessageDialog), [this](FunctionParameterMap &&params)
            {
                return show_message_dialog(std::move(params));
            }
        },
        {
            BROOKESIA_DESCRIBE_TO_STR(FunctionId::HideMessageDialog), [this](FunctionParameterMap &&params)
            {
                return hide_message_dialog(std::move(params));
            }
        },
        {
            BROOKESIA_DESCRIBE_TO_STR(FunctionId::UpdateMessageDialog), [this](FunctionParameterMap &&params)
            {
                return update_message_dialog(std::move(params));
            }
        },
        {
            BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetStorageLayout), [this](FunctionParameterMap &&)
            {
                return get_storage_layout();
            }
        },
        {
            BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetAppStoragePaths), [this](FunctionParameterMap &&)
            {
                return get_app_storage_paths();
            }
        },
        {
            BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetPublicStoragePaths), [this](FunctionParameterMap &&)
            {
                return get_public_storage_paths();
            }
        },
        {
            BROOKESIA_DESCRIBE_TO_STR(FunctionId::SetDefaultInstallStorage), [this](FunctionParameterMap &&params)
            {
                return set_default_install_storage(std::move(params));
            }
        },
        {
            BROOKESIA_DESCRIBE_TO_STR(FunctionId::StorageList), [this](FunctionParameterMap &&params)
            {
                return storage_list(std::move(params));
            }
        },
        {
            BROOKESIA_DESCRIBE_TO_STR(FunctionId::StorageStat), [this](FunctionParameterMap &&params)
            {
                return storage_stat(std::move(params));
            }
        },
        {
            BROOKESIA_DESCRIBE_TO_STR(FunctionId::StorageMkdir), [this](FunctionParameterMap &&params)
            {
                return storage_mkdir(std::move(params));
            }
        },
        {
            BROOKESIA_DESCRIBE_TO_STR(FunctionId::StorageRead), [this](FunctionParameterMap &&params)
            {
                return storage_read(std::move(params));
            }
        },
        {
            BROOKESIA_DESCRIBE_TO_STR(FunctionId::StorageWrite), [this](FunctionParameterMap &&params)
            {
                return storage_write(std::move(params));
            }
        },
        {
            BROOKESIA_DESCRIBE_TO_STR(FunctionId::StorageRemove), [this](FunctionParameterMap &&params)
            {
                return storage_remove(std::move(params));
            }
        },
        {
            BROOKESIA_DESCRIBE_TO_STR(FunctionId::StorageRename), [this](FunctionParameterMap &&params)
            {
                return storage_rename(std::move(params));
            }
        },
    };
}

FunctionResult SystemService::get_system_type()
{
    return make_success(system_.get_system_type());
}

FunctionResult SystemService::get_system_info()
{
    return make_success(to_json_object(system_.get_system_info()));
}

FunctionResult SystemService::get_environment()
{
    return make_success(system_.get_environment_json());
}

FunctionResult SystemService::get_active_app()
{
    auto app = system_.get_active_app();
    return make_success(
               app.has_value() ? app_info_to_json(*app, system_.get_environment().language) : boost::json::object{}
           );
}

FunctionResult SystemService::list_apps()
{
    boost::json::array apps;
    const auto language = system_.get_environment().language;
    for (const auto &app : system_.list_apps()) {
        apps.push_back(app_info_to_json(app, language));
    }
    return make_success(std::move(apps));
}

FunctionResult SystemService::request_close_app(FunctionParameterMap &&params)
{
    auto app_id = get_number_param(params, BROOKESIA_DESCRIBE_TO_STR(SystemCoreHelper::AppIdParam::AppId));
    if (!app_id) {
        return make_error(app_id.error());
    }
    const auto target_app_id = to_app_id(*app_id);
    auto caller = get_runtime_caller(system_);
    if (!caller) {
        return make_error(caller.error());
    }
    if (caller->has_value() && (caller->value().app_id != target_app_id) && !is_native_system_app(caller->value())) {
        return make_error("App can only request closing itself");
    }
    return service::ServiceBase::to_function_result(system_.request_close_app(target_app_id));
}

FunctionResult SystemService::start_app(FunctionParameterMap &&params)
{
    auto app_id = get_number_param(params, BROOKESIA_DESCRIBE_TO_STR(SystemCoreHelper::AppIdParam::AppId));
    if (!app_id) {
        return make_error(app_id.error());
    }
    auto caller = get_runtime_caller(system_);
    if (!caller) {
        return make_error(caller.error());
    }
    if (caller->has_value() && !is_native_system_app(caller->value())) {
        return make_error("Only native system apps can start other apps through SystemCore");
    }
    return service::ServiceBase::to_function_result(system_.start_app(to_app_id(*app_id)));
}

FunctionResult SystemService::stop_app(FunctionParameterMap &&params)
{
    auto app_id = get_number_param(params, BROOKESIA_DESCRIBE_TO_STR(SystemCoreHelper::AppIdParam::AppId));
    if (!app_id) {
        return make_error(app_id.error());
    }
    auto caller = get_runtime_caller(system_);
    if (!caller) {
        return make_error(caller.error());
    }
    if (caller->has_value() && !is_native_system_app(caller->value())) {
        return make_error("Only native system apps can stop other apps through SystemCore");
    }
    return service::ServiceBase::to_function_result(system_.stop_app(to_app_id(*app_id)));
}

FunctionResult SystemService::show_loading()
{
    auto caller = get_runtime_caller(system_);
    if (!caller) {
        return make_error(caller.error());
    }
    if (!caller->has_value()) {
        return make_error("Loading overlay requires an app caller");
    }
    return service::ServiceBase::to_function_result(system_.show_app_loading(caller->value().app_id));
}

FunctionResult SystemService::hide_loading()
{
    auto caller = get_runtime_caller(system_);
    if (!caller) {
        return make_error(caller.error());
    }
    if (!caller->has_value()) {
        return make_error("Loading overlay requires an app caller");
    }
    return service::ServiceBase::to_function_result(system_.hide_app_loading(caller->value().app_id));
}

FunctionResult SystemService::show_keyboard(FunctionParameterMap &&params)
{
    auto caller = get_runtime_caller(system_);
    if (!caller) {
        return make_error(caller.error());
    }
    if (!caller->has_value()) {
        return make_error("Keyboard input requires an app caller");
    }
    auto options_object = get_object_param(
                              params,
                              BROOKESIA_DESCRIBE_TO_STR(SystemCoreHelper::KeyboardOptionsParam::Options)
                          );
    if (!options_object) {
        return make_error(options_object.error());
    }
    auto options = keyboard_options_from_json(*options_object);
    if (!options) {
        return make_error(options.error());
    }

    auto request_id = system_.show_app_keyboard(caller->value().app_id, std::move(*options));
    if (!request_id) {
        return make_error(request_id.error());
    }
    boost::json::object data;
    data["RequestId"] = static_cast<double>(*request_id);
    return make_success(std::move(data));
}

FunctionResult SystemService::hide_keyboard(FunctionParameterMap &&params)
{
    auto caller = get_runtime_caller(system_);
    if (!caller) {
        return make_error(caller.error());
    }
    if (!caller->has_value()) {
        return make_error("Keyboard input requires an app caller");
    }
    auto request_id = get_number_param(
                          params,
                          BROOKESIA_DESCRIBE_TO_STR(SystemCoreHelper::KeyboardRequestParam::RequestId)
                      );
    if (!request_id) {
        return make_error(request_id.error());
    }
    return service::ServiceBase::to_function_result(
               system_.hide_app_keyboard(
                   caller->value().app_id,
                   static_cast<KeyboardRequestId>(*request_id)
               )
           );
}

FunctionResult SystemService::show_message_dialog(FunctionParameterMap &&params)
{
    auto caller = get_runtime_caller(system_);
    if (!caller) {
        return make_error(caller.error());
    }
    if (!caller->has_value()) {
        return make_error("Message dialog requires an app caller");
    }
    auto options_object = get_object_param(
                              params,
                              BROOKESIA_DESCRIBE_TO_STR(SystemCoreHelper::MessageDialogOptionsParam::Options)
                          );
    if (!options_object) {
        return make_error(options_object.error());
    }
    auto options = message_dialog_options_from_json(*options_object);
    if (!options) {
        return make_error(options.error());
    }

    auto request_id = system_.show_app_message_dialog(caller->value().app_id, std::move(*options));
    if (!request_id) {
        return make_error(request_id.error());
    }
    boost::json::object data;
    data["RequestId"] = static_cast<double>(*request_id);
    return make_success(std::move(data));
}

FunctionResult SystemService::hide_message_dialog(FunctionParameterMap &&params)
{
    auto caller = get_runtime_caller(system_);
    if (!caller) {
        return make_error(caller.error());
    }
    if (!caller->has_value()) {
        return make_error("Message dialog requires an app caller");
    }
    auto request_id = get_number_param(
                          params,
                          BROOKESIA_DESCRIBE_TO_STR(SystemCoreHelper::MessageDialogRequestParam::RequestId)
                      );
    if (!request_id) {
        return make_error(request_id.error());
    }
    return service::ServiceBase::to_function_result(
               system_.hide_app_message_dialog(
                   caller->value().app_id,
                   static_cast<MessageDialogRequestId>(*request_id)
               )
           );
}

FunctionResult SystemService::update_message_dialog(FunctionParameterMap &&params)
{
    auto caller = get_runtime_caller(system_);
    if (!caller) {
        return make_error(caller.error());
    }
    if (!caller->has_value()) {
        return make_error("Message dialog requires an app caller");
    }
    auto request_id = get_number_param(
                          params,
                          BROOKESIA_DESCRIBE_TO_STR(SystemCoreHelper::MessageDialogUpdateParam::RequestId)
                      );
    if (!request_id) {
        return make_error(request_id.error());
    }
    auto options_object = get_object_param(
                              params,
                              BROOKESIA_DESCRIBE_TO_STR(SystemCoreHelper::MessageDialogUpdateParam::Options)
                          );
    if (!options_object) {
        return make_error(options_object.error());
    }
    auto options = message_dialog_options_from_json(*options_object);
    if (!options) {
        return make_error(options.error());
    }
    return service::ServiceBase::to_function_result(
               system_.update_app_message_dialog(
                   caller->value().app_id,
                   static_cast<MessageDialogRequestId>(*request_id),
                   std::move(*options)
               )
           );
}

FunctionResult SystemService::get_storage_layout()
{
    return make_success(to_json_object(system_.get_storage_layout()));
}

FunctionResult SystemService::get_app_storage_paths()
{
    auto caller = get_runtime_caller(system_);
    if (!caller) {
        return make_error(caller.error());
    }
    if (!caller->has_value()) {
        return make_error("App storage paths require an app caller");
    }
    auto result = system_.get_app_storage_paths(caller->value().app_id);
    if (!result) {
        return make_error(result.error());
    }
    return make_success(to_json_object(*result));
}

FunctionResult SystemService::get_public_storage_paths()
{
    auto result = system_.get_public_storage_paths();
    if (!result) {
        return make_error(result.error());
    }
    return make_success(to_json_object(*result));
}

FunctionResult SystemService::set_default_install_storage(FunctionParameterMap &&params)
{
    auto caller = get_runtime_caller(system_);
    if (!caller) {
        return make_error(caller.error());
    }
    if (caller->has_value() && !is_native_system_app(caller->value())) {
        return make_error("Only native system apps can set default install storage");
    }

    auto target_string = get_string_param(params, BROOKESIA_DESCRIBE_TO_STR(SystemCoreHelper::StorageInstallParam::Target));
    if (!target_string) {
        return make_error(target_string.error());
    }
    StorageInstallTarget target = StorageInstallTarget::Auto;
    if (!BROOKESIA_DESCRIBE_STR_TO_ENUM_FLEXIBLE(*target_string, target)) {
        return make_error("Invalid storage install target: " + *target_string);
    }

    std::string preferred_external_id;
    if (auto it = params.find(BROOKESIA_DESCRIBE_TO_STR(SystemCoreHelper::StorageInstallParam::PreferredExternalId));
            it != params.end()) {
        const auto *value = std::get_if<std::string>(&it->second);
        if (value == nullptr) {
            return make_error("PreferredExternalId must be string");
        }
        preferred_external_id = *value;
    }
    return service::ServiceBase::to_function_result(
               system_.set_default_install_storage(target, std::move(preferred_external_id))
           );
}

std::expected<std::pair<AppId, StorageFileLocation>, std::string> get_caller_location(
    System &system,
    FunctionParameterMap &&params
)
{
    auto caller = get_runtime_caller(system);
    if (!caller) {
        return std::unexpected(caller.error());
    }
    if (!caller->has_value()) {
        return std::unexpected("Storage file operation requires an app caller");
    }
    auto location_object = get_object_param(
                               params,
                               BROOKESIA_DESCRIBE_TO_STR(SystemCoreHelper::StorageLocationParam::Location)
                           );
    if (!location_object) {
        return std::unexpected(location_object.error());
    }
    auto location = storage_location_from_json(*location_object);
    if (!location) {
        return std::unexpected(location.error());
    }
    return std::pair<AppId, StorageFileLocation> {caller->value().app_id, std::move(*location)};
}

FunctionResult SystemService::storage_list(FunctionParameterMap &&params)
{
    auto caller_location = get_caller_location(system_, std::move(params));
    if (!caller_location) {
        return make_error(caller_location.error());
    }
    auto result = system_.storage_list(caller_location->first, caller_location->second);
    if (!result) {
        return make_error(result.error());
    }
    return make_success(to_json_array(*result));
}

FunctionResult SystemService::storage_stat(FunctionParameterMap &&params)
{
    auto caller_location = get_caller_location(system_, std::move(params));
    if (!caller_location) {
        return make_error(caller_location.error());
    }
    auto result = system_.storage_stat(caller_location->first, caller_location->second);
    if (!result) {
        return make_error(result.error());
    }
    return make_success(to_json_object(*result));
}

FunctionResult SystemService::storage_mkdir(FunctionParameterMap &&params)
{
    auto caller_location = get_caller_location(system_, std::move(params));
    if (!caller_location) {
        return make_error(caller_location.error());
    }
    return service::ServiceBase::to_function_result(
               system_.storage_mkdir(caller_location->first, caller_location->second)
           );
}

FunctionResult SystemService::storage_read(FunctionParameterMap &&params)
{
    auto caller_location = get_caller_location(system_, std::move(params));
    if (!caller_location) {
        return make_error(caller_location.error());
    }
    return service::ServiceBase::to_function_result(
               system_.storage_read(caller_location->first, caller_location->second)
           );
}

FunctionResult SystemService::storage_write(FunctionParameterMap &&params)
{
    auto caller = get_runtime_caller(system_);
    if (!caller) {
        return make_error(caller.error());
    }
    if (!caller->has_value()) {
        return make_error("Storage write requires an app caller");
    }
    auto location_object = get_object_param(
                               params,
                               BROOKESIA_DESCRIBE_TO_STR(SystemCoreHelper::StorageWriteParam::Location)
                           );
    if (!location_object) {
        return make_error(location_object.error());
    }
    auto data = get_string_param(params, BROOKESIA_DESCRIBE_TO_STR(SystemCoreHelper::StorageWriteParam::Data));
    if (!data) {
        return make_error(data.error());
    }
    auto location = storage_location_from_json(*location_object);
    if (!location) {
        return make_error(location.error());
    }
    return service::ServiceBase::to_function_result(
               system_.storage_write(caller->value().app_id, *location, *data)
           );
}

FunctionResult SystemService::storage_remove(FunctionParameterMap &&params)
{
    auto caller_location = get_caller_location(system_, std::move(params));
    if (!caller_location) {
        return make_error(caller_location.error());
    }
    return service::ServiceBase::to_function_result(
               system_.storage_remove(caller_location->first, caller_location->second)
           );
}

FunctionResult SystemService::storage_rename(FunctionParameterMap &&params)
{
    auto caller = get_runtime_caller(system_);
    if (!caller) {
        return make_error(caller.error());
    }
    if (!caller->has_value()) {
        return make_error("Storage rename requires an app caller");
    }
    auto from_object = get_object_param(params, BROOKESIA_DESCRIBE_TO_STR(SystemCoreHelper::StorageRenameParam::From));
    auto to_object = get_object_param(params, BROOKESIA_DESCRIBE_TO_STR(SystemCoreHelper::StorageRenameParam::To));
    if (!from_object || !to_object) {
        return make_error(!from_object ? from_object.error() : to_object.error());
    }
    auto from = storage_location_from_json(*from_object);
    auto to = storage_location_from_json(*to_object);
    if (!from || !to) {
        return make_error(!from ? from.error() : to.error());
    }
    return service::ServiceBase::to_function_result(
               system_.storage_rename(caller->value().app_id, *from, *to)
           );
}

} // namespace esp_brookesia::system::core
