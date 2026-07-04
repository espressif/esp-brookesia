/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string>

#include "brookesia/system_core/macro_configs.h"
#if !BROOKESIA_SYSTEM_CORE_SYSTEM_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "private/system/impl.hpp"

namespace esp_brookesia::system::core {
namespace {

std::string make_component_version(int major, int minor, int patch)
{
    return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
}

} // namespace

System::System()
    : system_gui_access_(*this)
    , impl_(std::make_unique<Impl>(*this))
{}

System::~System()
{
    deinit();
}

SystemInfo System::on_get_system_info() const
{
    return {
        .name = BROOKESIA_SYSTEM_CORE_SYSTEM_NAME,
        .version = make_component_version(
            BROOKESIA_SYSTEM_CORE_VER_MAJOR,
            BROOKESIA_SYSTEM_CORE_VER_MINOR,
            BROOKESIA_SYSTEM_CORE_VER_PATCH
        ),
    };
}

std::expected<void, std::string> System::on_prepare_startup_overlay()
{
    return {};
}

std::expected<void, std::string> System::on_init()
{
    return {};
}

std::expected<void, std::string> System::on_start()
{
    return {};
}

void System::on_stop()
{}

void System::on_deinit()
{}

std::expected<void, std::string> System::on_app_installed(const AppInfo &)
{
    return {};
}

std::expected<void, std::string> System::on_app_uninstalled(const AppInfo &)
{
    return {};
}

std::expected<void, std::string> System::on_app_started(const AppInfo &)
{
    return {};
}

void System::on_app_start_failed(const AppInfo &, std::string_view)
{}

void System::on_app_stopped(const AppInfo &)
{}

void System::on_app_stop_failed(const AppInfo &, std::string_view)
{}

std::expected<void, std::string> System::on_theme_changed(std::string_view)
{
    return {};
}

std::expected<void, std::string> System::on_language_changed(std::string_view)
{
    return {};
}

std::expected<void, std::string> System::on_show_app_loading(AppId)
{
    return {};
}

void System::on_hide_app_loading(AppId)
{}

std::expected<void, std::string> System::on_show_app_keyboard(
    AppId,
    KeyboardRequestId,
    const KeyboardRequestOptions &
)
{
    return std::unexpected("Keyboard input is not supported by this system");
}

void System::on_hide_app_keyboard(AppId, KeyboardRequestId)
{}

std::expected<void, std::string> System::on_show_message_dialog(
    AppId,
    MessageDialogRequestId,
    const MessageDialogOptions &
)
{
    return std::unexpected("Message dialog is not supported by this system");
}

std::expected<void, std::string> System::on_update_message_dialog(
    AppId,
    MessageDialogRequestId,
    const MessageDialogOptions &
)
{
    return std::unexpected("Message dialog is not supported by this system");
}

void System::on_hide_message_dialog(AppId, MessageDialogRequestId)
{}

void System::on_message_dialog_idle()
{}

SystemGuiAccess &System::system_gui()
{
    return system_gui_access_;
}

} // namespace esp_brookesia::system::core
