/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "boost/json.hpp"
#include "brookesia/gui_interface.hpp"
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/runtime_manager/types.hpp"
#include "brookesia/system_core/macro_configs.h"

namespace esp_brookesia::system::core {

using AppId = uint32_t;
inline constexpr AppId INVALID_APP_ID = 0;
using TimerId = uint64_t;
inline constexpr TimerId INVALID_TIMER_ID = 0;
using KeyboardRequestId = uint64_t;
inline constexpr KeyboardRequestId INVALID_KEYBOARD_REQUEST_ID = 0;
using MessageDialogRequestId = uint64_t;
inline constexpr MessageDialogRequestId INVALID_MESSAGE_DIALOG_REQUEST_ID = 0;

struct KeyboardRequestOptions {
    std::string title;
    std::string placeholder;
    std::string initial_text;
    int32_t max_length = 0;
    bool password = false;
    std::string mode = "text";
    std::vector<std::string> allowed_modes;
};
BROOKESIA_DESCRIBE_STRUCT(
    KeyboardRequestOptions,
    (),
    (title, placeholder, initial_text, max_length, password, mode, allowed_modes)
)

struct KeyboardResult {
    KeyboardRequestId request_id = INVALID_KEYBOARD_REQUEST_ID;
    AppId app_id = INVALID_APP_ID;
    bool confirmed = false;
    std::string text;
};
BROOKESIA_DESCRIBE_STRUCT(KeyboardResult, (), (request_id, app_id, confirmed, text))

using KeyboardResultHandler = std::function<void(const KeyboardResult &)>;

struct SystemInfo {
    std::string name;
    std::string version;
};
BROOKESIA_DESCRIBE_STRUCT(SystemInfo, (), (name, version))

enum class MessageDialogCloseReason {
    Button,
    Timeout,
    Closed,
};
BROOKESIA_DESCRIBE_ENUM(MessageDialogCloseReason, Button, Timeout, Closed)

enum class MessageDialogIcon {
    None,
    Information,
    Question,
    Warning,
    Critical,
};
BROOKESIA_DESCRIBE_ENUM(MessageDialogIcon, None, Information, Question, Warning, Critical)

enum class MessageDialogButtonRole {
    Invalid,
    Accept,
    Reject,
    Destructive,
    Action,
    Help,
    Yes,
    No,
};
BROOKESIA_DESCRIBE_ENUM(
    MessageDialogButtonRole,
    Invalid,
    Accept,
    Reject,
    Destructive,
    Action,
    Help,
    Yes,
    No
)

struct MessageDialogButton {
    std::string text;
    MessageDialogButtonRole role = MessageDialogButtonRole::Action;
};
BROOKESIA_DESCRIBE_STRUCT(MessageDialogButton, (), (text, role))

struct MessageDialogOptions {
    std::string text;
    std::string informative_text;
    MessageDialogIcon icon = MessageDialogIcon::None;
    std::vector<MessageDialogButton> buttons;
    int32_t auto_close_ms = 0;
};
BROOKESIA_DESCRIBE_STRUCT(MessageDialogOptions, (), (text, informative_text, icon, buttons, auto_close_ms))

struct MessageDialogResult {
    MessageDialogRequestId request_id = INVALID_MESSAGE_DIALOG_REQUEST_ID;
    AppId app_id = INVALID_APP_ID;
    int32_t button_index = -1;
    MessageDialogButtonRole button_role = MessageDialogButtonRole::Invalid;
    MessageDialogCloseReason reason = MessageDialogCloseReason::Closed;
};
BROOKESIA_DESCRIBE_STRUCT(MessageDialogResult, (), (request_id, app_id, button_index, button_role, reason))

using MessageDialogResultHandler = std::function<void(const MessageDialogResult &)>;

enum class AppKind {
    Native,
    Runtime,
};
BROOKESIA_DESCRIBE_ENUM(AppKind, Native, Runtime)

enum class AppState {
    Installed,
    Starting,
    Running,
    Paused,
    Stopping,
    Stopped,
    Error,
};
BROOKESIA_DESCRIBE_ENUM(AppState, Installed, Starting, Running, Paused, Stopping, Stopped, Error)

enum class GuiRootKind {
    None,
    File,
    JsonString,
};
BROOKESIA_DESCRIBE_ENUM(GuiRootKind, None, File, JsonString)

enum class GuiAppLayer {
    AppDefault,
    AppTop,
    SystemBottom,
    SystemTop,
};
BROOKESIA_DESCRIBE_ENUM(GuiAppLayer, AppDefault, AppTop, SystemBottom, SystemTop)

struct GuiScreenFlowEntry {
    std::string screen_flow;
    GuiAppLayer layer = GuiAppLayer::AppDefault;
    gui::MountStackMode mount_mode = gui::MountStackMode::Replace;
    int32_t z_order = 0;
};
BROOKESIA_DESCRIBE_STRUCT(GuiScreenFlowEntry, (), (screen_flow, layer, mount_mode, z_order))

struct AppGuiResources {
    std::vector<gui::RuntimeImageResource> images;
    std::vector<gui::RuntimeFontResource> fonts;
};
BROOKESIA_DESCRIBE_STRUCT(AppGuiResources, (), (images, fonts))

struct AppGuiDescriptor {
    GuiRootKind root_kind = GuiRootKind::None;
    std::string root;
    AppGuiResources resources;
    std::vector<GuiScreenFlowEntry> screen_flows;
};
BROOKESIA_DESCRIBE_STRUCT(AppGuiDescriptor, (), (root_kind, root, resources, screen_flows))

struct RuntimeAppResourceDescriptor {
    std::string icon_id;
    AppGuiDescriptor gui;
};
BROOKESIA_DESCRIBE_STRUCT(RuntimeAppResourceDescriptor, (), (icon_id, gui))

struct AppManifest {
    std::string id;
    std::string name;
    std::map<std::string, std::string> localized_names;
    std::string version = "0.1.0";
    AppKind kind = AppKind::Native;
    bool visible = true;
    std::string icon_id;
    std::vector<std::string> supported_systems;
    std::string icon_path;
    runtime::BackendType runtime_type = runtime::BackendType::Unknown;
    std::string app_path;
    std::string entry;
    std::string resource_dir;
    std::vector<std::string> arguments;
};
BROOKESIA_DESCRIBE_STRUCT(
    AppManifest,
    (),
    (
        id,
        name,
        localized_names,
        version,
        kind,
        visible,
        icon_id,
        supported_systems,
        icon_path,
        runtime_type,
        app_path,
        entry,
        resource_dir,
        arguments
    )
)

inline std::string resolve_app_display_name(const AppManifest &manifest, std::string_view language = {})
{
    auto find_localized_name = [&manifest](std::string_view key) {
        auto it = manifest.localized_names.find(std::string(key));
        return it != manifest.localized_names.end() ? it->second : std::string();
    };

    if (!language.empty()) {
        auto name = find_localized_name(language);
        if (!name.empty()) {
            return name;
        }
    }
    auto english_name = find_localized_name("en");
    if (!english_name.empty()) {
        return english_name;
    }
    for (const auto &[_, name] : manifest.localized_names) {
        if (!name.empty()) {
            return name;
        }
    }
    if (!manifest.name.empty()) {
        return manifest.name;
    }
    return manifest.id;
}

inline bool has_app_icon_image(const AppManifest &manifest)
{
    return !manifest.icon_id.empty() && !manifest.icon_path.empty();
}

inline const std::string &resolve_app_icon_resource_id(const AppManifest &manifest)
{
    // Global GUI image ids are scoped to manifest.id so apps can reuse local
    // icon image ids such as "launcher_icon" inside their package index.json.
    return manifest.id;
}

struct AppInfo {
    AppId app_id = INVALID_APP_ID;
    AppManifest manifest;
    AppState state = AppState::Installed;
    std::string last_error;
};
BROOKESIA_DESCRIBE_STRUCT(AppInfo, (), (app_id, manifest, state, last_error))

} // namespace esp_brookesia::system::core
