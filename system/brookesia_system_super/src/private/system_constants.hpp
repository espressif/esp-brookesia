/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "brookesia/gui_interface.hpp"
#include "brookesia/system_super/macro_configs.h"
#include "brookesia/system_super/system.hpp"

namespace esp_brookesia::system::super {

#define BROOKESIA_SYSTEM_SUPER_STATE_HOME "home"
#define BROOKESIA_SYSTEM_SUPER_STATE_BACKGROUND "background"
#define BROOKESIA_SYSTEM_SUPER_STATE_LAUNCHER "launcher"
#define BROOKESIA_SYSTEM_SUPER_STATE_NOTIFICATIONS "notifications"
#define BROOKESIA_SYSTEM_SUPER_STATE_OVERLAY "overlay"
#define BROOKESIA_SYSTEM_SUPER_STATE_KEYBOARD_HIDDEN "keyboard_hidden"
#define BROOKESIA_SYSTEM_SUPER_STATE_KEYBOARD_INPUT "keyboard_input"
#define BROOKESIA_SYSTEM_SUPER_STATE_MESSAGE_DIALOG_HIDDEN "message_dialog_hidden"
#define BROOKESIA_SYSTEM_SUPER_STATE_MESSAGE_DIALOG "message_dialog"
#define BROOKESIA_SYSTEM_SUPER_PATH_HOME "/" BROOKESIA_SYSTEM_SUPER_STATE_HOME
#define BROOKESIA_SYSTEM_SUPER_PATH_BACKGROUND "/" BROOKESIA_SYSTEM_SUPER_STATE_BACKGROUND
#define BROOKESIA_SYSTEM_SUPER_PATH_LAUNCHER "/" BROOKESIA_SYSTEM_SUPER_STATE_LAUNCHER
#define BROOKESIA_SYSTEM_SUPER_PATH_NOTIFICATIONS "/" BROOKESIA_SYSTEM_SUPER_STATE_NOTIFICATIONS
#define BROOKESIA_SYSTEM_SUPER_PATH_OVERLAY "/" BROOKESIA_SYSTEM_SUPER_STATE_OVERLAY
#define BROOKESIA_SYSTEM_SUPER_PATH_KEYBOARD_HIDDEN "/" BROOKESIA_SYSTEM_SUPER_STATE_KEYBOARD_HIDDEN
#define BROOKESIA_SYSTEM_SUPER_PATH_KEYBOARD_INPUT "/" BROOKESIA_SYSTEM_SUPER_STATE_KEYBOARD_INPUT
#define BROOKESIA_SYSTEM_SUPER_PATH_MESSAGE_DIALOG_HIDDEN "/" BROOKESIA_SYSTEM_SUPER_STATE_MESSAGE_DIALOG_HIDDEN
#define BROOKESIA_SYSTEM_SUPER_PATH_MESSAGE_DIALOG "/" BROOKESIA_SYSTEM_SUPER_STATE_MESSAGE_DIALOG
#define BROOKESIA_SYSTEM_SUPER_PATH_OVERLAY_STATUS BROOKESIA_SYSTEM_SUPER_PATH_OVERLAY "/status"
#define BROOKESIA_SYSTEM_SUPER_PATH_SYSTEM_UI_MASK BROOKESIA_SYSTEM_SUPER_PATH_OVERLAY "/system_ui_mask"
#define BROOKESIA_SYSTEM_SUPER_PATH_APP_MODAL BROOKESIA_SYSTEM_SUPER_PATH_OVERLAY "/app_modal"
#define BROOKESIA_SYSTEM_SUPER_PATH_APP_MODAL_LAUNCH BROOKESIA_SYSTEM_SUPER_PATH_APP_MODAL "/launch_group"
#define BROOKESIA_SYSTEM_SUPER_PATH_APP_MODAL_LOADING BROOKESIA_SYSTEM_SUPER_PATH_APP_MODAL "/loading_group"
#define BROOKESIA_SYSTEM_SUPER_PATH_APP_MODAL_LAUNCH_ICON_BOX \
    BROOKESIA_SYSTEM_SUPER_PATH_APP_MODAL_LAUNCH "/icon_box"
#define BROOKESIA_SYSTEM_SUPER_PATH_LAUNCHER_CONTENT BROOKESIA_SYSTEM_SUPER_PATH_LAUNCHER "/content"
#define BROOKESIA_SYSTEM_SUPER_PATH_LAUNCHER_GRID_STAGE \
    BROOKESIA_SYSTEM_SUPER_PATH_LAUNCHER_CONTENT "/grid_stage"
#define BROOKESIA_SYSTEM_SUPER_PATH_LAUNCHER_SLOT_GRID \
    BROOKESIA_SYSTEM_SUPER_PATH_LAUNCHER_GRID_STAGE "/slot_grid"
#define BROOKESIA_SYSTEM_SUPER_PATH_LAUNCHER_ITEM_LAYER \
    BROOKESIA_SYSTEM_SUPER_PATH_LAUNCHER_GRID_STAGE "/item_layer"
#define BROOKESIA_SYSTEM_SUPER_PATH_LAUNCHER_DRAG_LAYER \
    BROOKESIA_SYSTEM_SUPER_PATH_LAUNCHER_GRID_STAGE "/drag_layer"
#define BROOKESIA_SYSTEM_SUPER_LAUNCHER_INSTANCE_PREFIX "app_"
#define BROOKESIA_SYSTEM_SUPER_LAUNCHER_SLOT_INSTANCE_PREFIX "slot_"

#define BROOKESIA_SYSTEM_SUPER_ACTION_NAV_PREFIX "super.nav."
#define BROOKESIA_SYSTEM_SUPER_ACTION_BACKGROUND_PREFIX "super.background."
#define BROOKESIA_SYSTEM_SUPER_ACTION_LAUNCH_PREFIX "super.launch.app"
#define BROOKESIA_SYSTEM_SUPER_ACTION_KEYBOARD_PREFIX "super.keyboard."
#define BROOKESIA_SYSTEM_SUPER_ACTION_MESSAGE_DIALOG_PREFIX "super.message_dialog."

inline constexpr const char *SUPER_RESOURCE_DIR = BROOKESIA_SYSTEM_SUPER_RESOURCE_DIR;
inline constexpr const char *SUPER_SHELL_APP_ID = "brookesia.system.super.shell";
inline constexpr const char *SUPER_SHELL_ROOT_JSON = "shell/root.json";
inline constexpr const char *SUPER_BACKGROUND_FLOW_ID = "background";
inline constexpr const char *SUPER_SHELL_PAGES_FLOW_ID = "shell_pages";
inline constexpr const char *SUPER_OVERLAY_FLOW_ID = "overlay";
inline constexpr const char *SUPER_KEYBOARD_FLOW_ID = "keyboard";
inline constexpr const char *SUPER_MESSAGE_DIALOG_FLOW_ID = "message_dialog";
inline constexpr const char *SUPER_FONT_DIR = "fonts";
inline constexpr const char *SUPER_DEFAULT_FONT_ID = "default";
inline constexpr const char *SUPER_THEME_DIR = "themes";
inline constexpr const char *SUPER_LIGHT_THEME_ID = "light";
inline constexpr const char *SUPER_DARK_THEME_ID = "dark";
inline constexpr const char *SUPER_DEFAULT_THEME_ID = SUPER_LIGHT_THEME_ID;
inline constexpr const char *SUPER_DEFAULT_IMAGE_ID = "default";

inline constexpr const char *SUPER_BACKGROUND_SCREEN_PATH = BROOKESIA_SYSTEM_SUPER_PATH_BACKGROUND;
inline constexpr const char *SUPER_HOME_SCREEN_PATH = BROOKESIA_SYSTEM_SUPER_PATH_HOME;
inline constexpr const char *SUPER_APP_LAUNCHER_SCREEN_PATH = BROOKESIA_SYSTEM_SUPER_PATH_LAUNCHER;
inline constexpr const char *SUPER_NOTIFICATIONS_SCREEN_PATH = BROOKESIA_SYSTEM_SUPER_PATH_NOTIFICATIONS;
inline constexpr const char *SUPER_OVERLAY_SCREEN_PATH = BROOKESIA_SYSTEM_SUPER_PATH_OVERLAY;
inline constexpr const char *SUPER_KEYBOARD_HIDDEN_SCREEN_PATH = BROOKESIA_SYSTEM_SUPER_PATH_KEYBOARD_HIDDEN;
inline constexpr const char *SUPER_KEYBOARD_INPUT_SCREEN_PATH = BROOKESIA_SYSTEM_SUPER_PATH_KEYBOARD_INPUT;
inline constexpr const char *SUPER_MESSAGE_DIALOG_HIDDEN_SCREEN_PATH =
    BROOKESIA_SYSTEM_SUPER_PATH_MESSAGE_DIALOG_HIDDEN;
inline constexpr const char *SUPER_MESSAGE_DIALOG_SCREEN_PATH = BROOKESIA_SYSTEM_SUPER_PATH_MESSAGE_DIALOG;

inline constexpr const char *SUPER_STATUS_BAR_PATH = BROOKESIA_SYSTEM_SUPER_PATH_OVERLAY_STATUS;
inline constexpr const char *SUPER_SYSTEM_UI_MASK_PATH = BROOKESIA_SYSTEM_SUPER_PATH_SYSTEM_UI_MASK;
inline constexpr const char *SUPER_STATUS_WIFI_PATH = BROOKESIA_SYSTEM_SUPER_PATH_OVERLAY_STATUS
        "/status_right/wifi_pill";
inline constexpr const char *SUPER_STATUS_CLOCK_PATH = BROOKESIA_SYSTEM_SUPER_PATH_OVERLAY_STATUS
        "/status_right/clock/label";
inline constexpr const char *SUPER_GESTURE_INDICATOR_PATH =
    BROOKESIA_SYSTEM_SUPER_PATH_OVERLAY "/gesture_indicator";
inline constexpr const char *SUPER_GESTURE_INDICATOR_BAR_PATH =
    BROOKESIA_SYSTEM_SUPER_PATH_OVERLAY "/gesture_indicator/bar";
inline constexpr const char *SUPER_APP_MODAL_PATH = BROOKESIA_SYSTEM_SUPER_PATH_APP_MODAL;
inline constexpr const char *SUPER_APP_MODAL_LAUNCH_PATH = BROOKESIA_SYSTEM_SUPER_PATH_APP_MODAL_LAUNCH;
inline constexpr const char *SUPER_APP_MODAL_LOADING_PATH = BROOKESIA_SYSTEM_SUPER_PATH_APP_MODAL_LOADING;
inline constexpr const char *SUPER_APP_MODAL_LAUNCH_SURFACE_PATH =
    BROOKESIA_SYSTEM_SUPER_PATH_APP_MODAL_LAUNCH "/surface";
inline constexpr const char *SUPER_APP_MODAL_LAUNCH_ICON_BOX_PATH =
    BROOKESIA_SYSTEM_SUPER_PATH_APP_MODAL_LAUNCH_ICON_BOX;
inline constexpr const char *SUPER_APP_MODAL_LAUNCH_ICON_IMAGE_PATH =
    BROOKESIA_SYSTEM_SUPER_PATH_APP_MODAL_LAUNCH_ICON_BOX "/icon_image";
inline constexpr const char *SUPER_APP_MODAL_LAUNCH_FALLBACK_LABEL_PATH =
    BROOKESIA_SYSTEM_SUPER_PATH_APP_MODAL_LAUNCH_ICON_BOX "/fallback_label";
inline constexpr const char *SUPER_KEYBOARD_INPUT_PATH = BROOKESIA_SYSTEM_SUPER_PATH_KEYBOARD_INPUT;
inline constexpr const char *SUPER_KEYBOARD_INPUT_BAR_PATH =
    BROOKESIA_SYSTEM_SUPER_PATH_KEYBOARD_INPUT "/input_bar";
inline constexpr const char *SUPER_KEYBOARD_INPUT_TEXT_PATH =
    BROOKESIA_SYSTEM_SUPER_PATH_KEYBOARD_INPUT "/input_bar/input";
inline constexpr const char *SUPER_KEYBOARD_PASSWORD_TOGGLE_PATH =
    BROOKESIA_SYSTEM_SUPER_PATH_KEYBOARD_INPUT "/input_bar/password_toggle";
inline constexpr const char *SUPER_KEYBOARD_PASSWORD_TOGGLE_ICON_PATH =
    BROOKESIA_SYSTEM_SUPER_PATH_KEYBOARD_INPUT "/input_bar/password_toggle/icon";
inline constexpr const char *SUPER_KEYBOARD_INPUT_KEYBOARD_PANEL_PATH =
    BROOKESIA_SYSTEM_SUPER_PATH_KEYBOARD_INPUT "/keyboard_panel";
inline constexpr const char *SUPER_KEYBOARD_INPUT_KEYBOARD_PATH =
    BROOKESIA_SYSTEM_SUPER_PATH_KEYBOARD_INPUT "/keyboard_panel/keyboard";
inline constexpr const char *SUPER_MESSAGE_DIALOG_PANEL_PATH =
    BROOKESIA_SYSTEM_SUPER_PATH_MESSAGE_DIALOG "/panel";
inline constexpr const char *SUPER_MESSAGE_DIALOG_ICON_PATH =
    BROOKESIA_SYSTEM_SUPER_PATH_MESSAGE_DIALOG "/panel/icon";
inline constexpr const char *SUPER_MESSAGE_DIALOG_TEXT_PATH =
    BROOKESIA_SYSTEM_SUPER_PATH_MESSAGE_DIALOG "/panel/text";
inline constexpr const char *SUPER_MESSAGE_DIALOG_INFORMATIVE_TEXT_PATH =
    BROOKESIA_SYSTEM_SUPER_PATH_MESSAGE_DIALOG "/panel/informative_text";
inline constexpr const char *SUPER_MESSAGE_DIALOG_ACTIONS_PATH =
    BROOKESIA_SYSTEM_SUPER_PATH_MESSAGE_DIALOG "/panel/actions";
inline constexpr const char *SUPER_MESSAGE_DIALOG_BUTTON0_PATH =
    BROOKESIA_SYSTEM_SUPER_PATH_MESSAGE_DIALOG "/panel/actions/button0";
inline constexpr const char *SUPER_MESSAGE_DIALOG_BUTTON0_LABEL_PATH =
    BROOKESIA_SYSTEM_SUPER_PATH_MESSAGE_DIALOG "/panel/actions/button0/label";
inline constexpr const char *SUPER_MESSAGE_DIALOG_BUTTON1_PATH =
    BROOKESIA_SYSTEM_SUPER_PATH_MESSAGE_DIALOG "/panel/actions/button1";
inline constexpr const char *SUPER_MESSAGE_DIALOG_BUTTON1_LABEL_PATH =
    BROOKESIA_SYSTEM_SUPER_PATH_MESSAGE_DIALOG "/panel/actions/button1/label";
inline constexpr const char *SUPER_MESSAGE_DIALOG_BUTTON2_PATH =
    BROOKESIA_SYSTEM_SUPER_PATH_MESSAGE_DIALOG "/panel/actions/button2";
inline constexpr const char *SUPER_MESSAGE_DIALOG_BUTTON2_LABEL_PATH =
    BROOKESIA_SYSTEM_SUPER_PATH_MESSAGE_DIALOG "/panel/actions/button2/label";

inline constexpr const char *SUPER_ACTION_HOME = BROOKESIA_SYSTEM_SUPER_ACTION_NAV_PREFIX "home";
inline constexpr const char *SUPER_ACTION_OPEN_LAUNCHER = BROOKESIA_SYSTEM_SUPER_ACTION_NAV_PREFIX "open_launcher";
inline constexpr const char *SUPER_ACTION_OPEN_NOTIFICATIONS = BROOKESIA_SYSTEM_SUPER_ACTION_NAV_PREFIX "open_notifications";
inline constexpr const char *SUPER_ACTION_SYSTEM_UI_MASK_CLICK =
    BROOKESIA_SYSTEM_SUPER_ACTION_NAV_PREFIX "mask.click";
inline constexpr const char *SUPER_ACTION_SYSTEM_UI_MASK_GESTURE =
    BROOKESIA_SYSTEM_SUPER_ACTION_NAV_PREFIX "mask.gesture";
inline constexpr const char *SUPER_ACTION_BACKGROUND_SHELL =
    BROOKESIA_SYSTEM_SUPER_ACTION_BACKGROUND_PREFIX "shell";
inline constexpr const char *SUPER_ACTION_BACKGROUND_APP =
    BROOKESIA_SYSTEM_SUPER_ACTION_BACKGROUND_PREFIX "app";

inline constexpr const char *SUPER_ACTION_LAUNCH_APP = BROOKESIA_SYSTEM_SUPER_ACTION_LAUNCH_PREFIX;
inline constexpr const char *SUPER_ACTION_KEYBOARD_TEXT_CHANGED =
    BROOKESIA_SYSTEM_SUPER_ACTION_KEYBOARD_PREFIX "text_changed";
inline constexpr const char *SUPER_ACTION_KEYBOARD_SUBMIT_INPUT =
    BROOKESIA_SYSTEM_SUPER_ACTION_KEYBOARD_PREFIX "submit.input";
inline constexpr const char *SUPER_ACTION_KEYBOARD_SUBMIT_KEYBOARD =
    BROOKESIA_SYSTEM_SUPER_ACTION_KEYBOARD_PREFIX "submit.keyboard";
inline constexpr const char *SUPER_ACTION_KEYBOARD_CANCEL_INPUT =
    BROOKESIA_SYSTEM_SUPER_ACTION_KEYBOARD_PREFIX "cancel.input";
inline constexpr const char *SUPER_ACTION_KEYBOARD_CANCEL_KEYBOARD =
    BROOKESIA_SYSTEM_SUPER_ACTION_KEYBOARD_PREFIX "cancel.keyboard";
inline constexpr const char *SUPER_ACTION_KEYBOARD_CANCEL_BACKDROP =
    BROOKESIA_SYSTEM_SUPER_ACTION_KEYBOARD_PREFIX "cancel.backdrop";
inline constexpr const char *SUPER_ACTION_KEYBOARD_TOGGLE_PASSWORD =
    BROOKESIA_SYSTEM_SUPER_ACTION_KEYBOARD_PREFIX "toggle_password";
inline constexpr const char *SUPER_ACTION_KEYBOARD_SHOW =
    BROOKESIA_SYSTEM_SUPER_ACTION_KEYBOARD_PREFIX "show";
inline constexpr const char *SUPER_ACTION_KEYBOARD_HIDE =
    BROOKESIA_SYSTEM_SUPER_ACTION_KEYBOARD_PREFIX "hide";
inline constexpr const char *SUPER_ACTION_MESSAGE_DIALOG_BUTTON0 =
    BROOKESIA_SYSTEM_SUPER_ACTION_MESSAGE_DIALOG_PREFIX "button0";
inline constexpr const char *SUPER_ACTION_MESSAGE_DIALOG_BUTTON1 =
    BROOKESIA_SYSTEM_SUPER_ACTION_MESSAGE_DIALOG_PREFIX "button1";
inline constexpr const char *SUPER_ACTION_MESSAGE_DIALOG_BUTTON2 =
    BROOKESIA_SYSTEM_SUPER_ACTION_MESSAGE_DIALOG_PREFIX "button2";
inline constexpr const char *SUPER_ACTION_MESSAGE_DIALOG_SHOW =
    BROOKESIA_SYSTEM_SUPER_ACTION_MESSAGE_DIALOG_PREFIX "show";
inline constexpr const char *SUPER_ACTION_MESSAGE_DIALOG_HIDE =
    BROOKESIA_SYSTEM_SUPER_ACTION_MESSAGE_DIALOG_PREFIX "hide";
inline constexpr const char *SUPER_MESSAGE_DIALOG_TIMEOUT_TIMER_NAME = "super.message_dialog.timeout";
inline constexpr const char *SUPER_GESTURE_EXIT_HOLD_TIMER_NAME = "super.gesture.exit_hold";
inline constexpr const char *SUPER_STATUS_PEEK_AUTO_HIDE_TIMER_NAME = "super.status.peek_auto_hide";
inline constexpr const char *SUPER_STATUS_CLOCK_TIMER_NAME = "super.status.clock";
inline constexpr const char *SUPER_MESSAGE_DIALOG_INFORMATION_IMAGE_ID = "msg_dialog/information";
inline constexpr const char *SUPER_MESSAGE_DIALOG_QUESTION_IMAGE_ID = "msg_dialog/question";
inline constexpr const char *SUPER_MESSAGE_DIALOG_WARNING_IMAGE_ID = "msg_dialog/warning";
inline constexpr const char *SUPER_MESSAGE_DIALOG_CRITICAL_IMAGE_ID = "msg_dialog/critical";

inline constexpr const char *SUPER_LAUNCHER_BUTTON_TEMPLATE_ID = "launcher_app_button";
inline constexpr const char *SUPER_LAUNCHER_SLOT_TEMPLATE_ID = "launcher_slot";
inline constexpr const char *SUPER_LAUNCHER_CONTENT_PATH = BROOKESIA_SYSTEM_SUPER_PATH_LAUNCHER_CONTENT;
inline constexpr const char *SUPER_LAUNCHER_GRID_STAGE_PATH = BROOKESIA_SYSTEM_SUPER_PATH_LAUNCHER_GRID_STAGE;
inline constexpr const char *SUPER_LAUNCHER_SLOT_GRID_PATH = BROOKESIA_SYSTEM_SUPER_PATH_LAUNCHER_SLOT_GRID;
inline constexpr const char *SUPER_LAUNCHER_ITEM_LAYER_PATH = BROOKESIA_SYSTEM_SUPER_PATH_LAUNCHER_ITEM_LAYER;
inline constexpr const char *SUPER_LAUNCHER_DRAG_LAYER_PATH = BROOKESIA_SYSTEM_SUPER_PATH_LAUNCHER_DRAG_LAYER;
inline constexpr const char *SUPER_LAUNCHER_INSTANCE_PREFIX = BROOKESIA_SYSTEM_SUPER_LAUNCHER_INSTANCE_PREFIX;
inline constexpr const char *SUPER_LAUNCHER_SLOT_INSTANCE_PREFIX =
    BROOKESIA_SYSTEM_SUPER_LAUNCHER_SLOT_INSTANCE_PREFIX;
inline constexpr const char *SUPER_LAUNCHER_PATH_PREFIX =
    BROOKESIA_SYSTEM_SUPER_PATH_LAUNCHER_ITEM_LAYER "/" BROOKESIA_SYSTEM_SUPER_LAUNCHER_INSTANCE_PREFIX;
inline constexpr const char *SUPER_LAUNCHER_SLOT_PATH_PREFIX =
    BROOKESIA_SYSTEM_SUPER_PATH_LAUNCHER_SLOT_GRID "/" BROOKESIA_SYSTEM_SUPER_LAUNCHER_SLOT_INSTANCE_PREFIX;
inline constexpr const char *SUPER_LAUNCHER_LABEL_ID = "title_box/app_name";
inline constexpr const char *SUPER_LAUNCHER_ICON_BOX_ID = "icon_box";
inline constexpr const char *SUPER_LAUNCHER_FALLBACK_ICON_ID = "icon_box/icon";
inline constexpr const char *SUPER_LAUNCHER_IMAGE_ICON_ID = "icon_box/icon_image";

#undef BROOKESIA_SYSTEM_SUPER_PATH_HOME
#undef BROOKESIA_SYSTEM_SUPER_PATH_BACKGROUND
#undef BROOKESIA_SYSTEM_SUPER_PATH_LAUNCHER
#undef BROOKESIA_SYSTEM_SUPER_PATH_NOTIFICATIONS
#undef BROOKESIA_SYSTEM_SUPER_PATH_OVERLAY
#undef BROOKESIA_SYSTEM_SUPER_PATH_KEYBOARD_HIDDEN
#undef BROOKESIA_SYSTEM_SUPER_PATH_KEYBOARD_INPUT
#undef BROOKESIA_SYSTEM_SUPER_PATH_MESSAGE_DIALOG_HIDDEN
#undef BROOKESIA_SYSTEM_SUPER_PATH_MESSAGE_DIALOG
#undef BROOKESIA_SYSTEM_SUPER_PATH_OVERLAY_STATUS
#undef BROOKESIA_SYSTEM_SUPER_PATH_APP_MODAL
#undef BROOKESIA_SYSTEM_SUPER_PATH_APP_MODAL_LAUNCH
#undef BROOKESIA_SYSTEM_SUPER_PATH_APP_MODAL_LOADING
#undef BROOKESIA_SYSTEM_SUPER_PATH_APP_MODAL_LAUNCH_ICON_BOX
#undef BROOKESIA_SYSTEM_SUPER_PATH_LAUNCHER_GRID_STAGE
#undef BROOKESIA_SYSTEM_SUPER_PATH_LAUNCHER_SLOT_GRID
#undef BROOKESIA_SYSTEM_SUPER_PATH_LAUNCHER_ITEM_LAYER
#undef BROOKESIA_SYSTEM_SUPER_PATH_LAUNCHER_DRAG_LAYER
#undef BROOKESIA_SYSTEM_SUPER_LAUNCHER_INSTANCE_PREFIX
#undef BROOKESIA_SYSTEM_SUPER_LAUNCHER_SLOT_INSTANCE_PREFIX
#undef BROOKESIA_SYSTEM_SUPER_ACTION_NAV_PREFIX
#undef BROOKESIA_SYSTEM_SUPER_ACTION_BACKGROUND_PREFIX
#undef BROOKESIA_SYSTEM_SUPER_ACTION_LAUNCH_PREFIX
#undef BROOKESIA_SYSTEM_SUPER_ACTION_KEYBOARD_PREFIX
#undef BROOKESIA_SYSTEM_SUPER_ACTION_MESSAGE_DIALOG_PREFIX
#undef BROOKESIA_SYSTEM_SUPER_STATE_HOME
#undef BROOKESIA_SYSTEM_SUPER_STATE_BACKGROUND
#undef BROOKESIA_SYSTEM_SUPER_STATE_LAUNCHER
#undef BROOKESIA_SYSTEM_SUPER_STATE_NOTIFICATIONS
#undef BROOKESIA_SYSTEM_SUPER_STATE_OVERLAY
#undef BROOKESIA_SYSTEM_SUPER_STATE_KEYBOARD_HIDDEN
#undef BROOKESIA_SYSTEM_SUPER_STATE_KEYBOARD_INPUT
#undef BROOKESIA_SYSTEM_SUPER_STATE_MESSAGE_DIALOG_HIDDEN
#undef BROOKESIA_SYSTEM_SUPER_STATE_MESSAGE_DIALOG

inline constexpr int32_t SUPER_STATUS_BAR_EXPANDED_Y = 0;
inline constexpr int32_t SUPER_STATUS_BAR_COLLAPSED_Y = -44;
inline constexpr int32_t SUPER_SYSTEM_UI_ANIMATION_MS = 180;
inline constexpr int32_t SUPER_GESTURE_EXIT_HOLD_MS = 50;
inline constexpr int32_t SUPER_GESTURE_REBOUND_MS = 180;
inline constexpr int32_t SUPER_STATUS_PEEK_HOLD_MS = 1500;
inline constexpr int32_t SUPER_APP_LAUNCH_ANIMATION_MS = 220;
inline constexpr bool SUPER_APP_LAUNCH_ANIMATION_ENABLED = BROOKESIA_SYSTEM_SUPER_ENABLE_APP_LAUNCH_ANIMATION;
inline constexpr int32_t SUPER_APP_LAUNCH_POST_COMPLETE_HOLD_MS =
    BROOKESIA_SYSTEM_SUPER_APP_LAUNCH_POST_COMPLETE_HOLD_MS;
inline constexpr int32_t SUPER_APP_LAUNCH_NO_ANIMATION_MIN_HOLD_MS = 50;
inline constexpr int32_t SUPER_APP_LAUNCH_FINAL_ICON_SIZE = 96;
inline constexpr const char *SUPER_APP_LAUNCH_HOLD_TIMER_NAME = "super.app_launch.hold";
inline constexpr int32_t SUPER_KEYBOARD_ANIMATION_MS = 280;
inline constexpr const char *SUPER_KEYBOARD_EYE_HIDE_IMAGE_ID = "icon.eyeHide";
inline constexpr const char *SUPER_KEYBOARD_EYE_SHOW_IMAGE_ID = "icon.eyeShow";

inline const char *to_screen_path(ShellPage page)
{
    switch (page) {
    case ShellPage::Home:
        return SUPER_APP_LAUNCHER_SCREEN_PATH;
    case ShellPage::AppLauncher:
        return SUPER_APP_LAUNCHER_SCREEN_PATH;
    case ShellPage::Notifications:
        return SUPER_APP_LAUNCHER_SCREEN_PATH;
    }
    return SUPER_APP_LAUNCHER_SCREEN_PATH;
}

inline const char *to_page_title(ShellPage page)
{
    switch (page) {
    case ShellPage::Home:
        return "App Launcher";
    case ShellPage::AppLauncher:
        return "App Launcher";
    case ShellPage::Notifications:
        return "App Launcher";
    }
    return "App Launcher";
}

inline const char *to_screen_flow_state(ShellPage page)
{
    switch (page) {
    case ShellPage::Home:
        return "launcher";
    case ShellPage::AppLauncher:
        return "launcher";
    case ShellPage::Notifications:
        return "launcher";
    }
    return "launcher";
}

inline const char *to_screen_flow_action(ShellPage page)
{
    switch (page) {
    case ShellPage::Home:
        return SUPER_ACTION_OPEN_LAUNCHER;
    case ShellPage::AppLauncher:
        return SUPER_ACTION_OPEN_LAUNCHER;
    case ShellPage::Notifications:
        return SUPER_ACTION_OPEN_LAUNCHER;
    }
    return SUPER_ACTION_OPEN_LAUNCHER;
}

inline std::optional<ShellPage> to_shell_page(std::string_view screen_flow_state)
{
    if (screen_flow_state == "launcher") {
        return ShellPage::AppLauncher;
    }
    return std::nullopt;
}

inline void add_binding_update(
    std::vector<gui::BindingValueUpdate> &updates,
    std::string absolute_path,
    std::string key,
    std::string value
)
{
    updates.push_back(gui::BindingValueUpdate{
        .absolute_path = std::move(absolute_path),
        .key = std::move(key),
        .value = std::move(value),
    });
}

} // namespace esp_brookesia::system::super
