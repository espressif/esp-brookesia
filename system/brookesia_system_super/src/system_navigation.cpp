/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/system_super/macro_configs.h"
#if !BROOKESIA_SYSTEM_SUPER_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "private/shell_app.hpp"
#include "brookesia/system_super/system.hpp"

namespace esp_brookesia::system::super {

bool System::is_shell_active() const
{
    const auto active = get_active_app();
    if (active.has_value()) {
        return active->app_id == shell_app_id_;
    }
    const auto shell = get_app(shell_app_id_);
    return shell.has_value() && (shell->state == core::AppState::Running || shell->state == core::AppState::Paused);
}

std::expected<void, std::string> System::open_shell_page(ShellPage page)
{
    pending_shell_page_ = page;

    const auto active = get_active_app();
    if (active.has_value() && active->app_id != shell_app_id_) {
        return stop_app(active->app_id);
    }
    return restore_shell();
}

std::expected<void, std::string> System::open_app_launcher()
{
    return open_shell_page(ShellPage::AppLauncher);
}

std::expected<void, std::string> System::open_quick_control()
{
    return std::unexpected("Shell Quick Control page is no longer available");
}

std::expected<void, std::string> System::open_system_monitor()
{
    return std::unexpected("Shell System Monitor page is no longer available");
}

std::expected<void, std::string> System::open_notifications()
{
    return open_shell_page(ShellPage::AppLauncher);
}

std::expected<void, std::string> System::close_active_app()
{
    auto active = get_active_app();
    if (!active || active->app_id == shell_app_id_) {
        return shell_app_ ? shell_app_->set_foreground_app(std::nullopt) : std::expected<void, std::string> {};
    }
    pending_shell_page_ = ShellPage::AppLauncher;
    return stop_app(active->app_id);
}

std::expected<void, std::string> System::go_home()
{
    return open_shell_page(ShellPage::AppLauncher);
}

std::expected<void, std::string> System::restore_shell()
{
    if (shell_app_id_ == core::INVALID_APP_ID) {
        return {};
    }
    auto shell = get_app(shell_app_id_);
    if (shell && (shell->state == core::AppState::Running || shell->state == core::AppState::Paused)) {
        if (shell_app_) {
            auto page_result = shell_app_->show_page(pending_shell_page_);
            if (!page_result) {
                return page_result;
            }
            return shell_app_->set_foreground_app(std::nullopt);
        }
        return {};
    }

    auto start_result = start_app(shell_app_id_);
    if (!start_result) {
        return start_result;
    }
    if (pending_shell_page_ == ShellPage::AppLauncher || shell_app_ == nullptr) {
        return {};
    }
    return shell_app_->show_page(pending_shell_page_);
}

} // namespace esp_brookesia::system::super
