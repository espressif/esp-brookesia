/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <filesystem>
#include <string_view>

#include "brookesia/system_super/macro_configs.h"
#if !BROOKESIA_SYSTEM_SUPER_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "private/shell_app.hpp"
#include "private/system_constants.hpp"
#include "brookesia/system_super/system.hpp"

namespace esp_brookesia::system::super {
namespace {

std::string make_share_resource_dir(const System &system)
{
    const auto layout = system.get_storage_layout();
    return (std::filesystem::path(layout.internal.root_path) / "system").lexically_normal().generic_string();
}

std::string make_system_resource_dir(const System &system)
{
    const auto layout = system.get_storage_layout();
    return (std::filesystem::path(layout.internal.root_path) / "system" / "super").lexically_normal().generic_string();
}

std::string make_font_index_relative_path()
{
    return (std::filesystem::path(SUPER_FONT_DIR) / "index.json").generic_string();
}

std::string make_theme_relative_path(std::string_view file_name)
{
    return (std::filesystem::path(SUPER_THEME_DIR) / file_name).generic_string();
}

std::string make_preload_theme_id(const std::optional<std::string> &stored_theme_id)
{
    if (!stored_theme_id.has_value() || stored_theme_id->empty()) {
        return SUPER_DEFAULT_THEME_ID;
    }
    if (*stored_theme_id == SUPER_LIGHT_THEME_ID || *stored_theme_id == SUPER_DARK_THEME_ID) {
        return *stored_theme_id;
    }
    return SUPER_DEFAULT_THEME_ID;
}

} // namespace

std::expected<void, std::string> System::prepare_shell_fonts()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    if (shell_fonts_prepared_) {
        return {};
    }

    const auto share_dir = make_share_resource_dir(*this);
    const auto font_index = std::filesystem::path(share_dir) / make_font_index_relative_path();
    std::error_code error_code;
    if (!std::filesystem::exists(font_index, error_code) ||
            !std::filesystem::is_regular_file(font_index, error_code)) {
#if BROOKESIA_SYSTEM_SUPER_ENABLE_RESOURCE_FONT_COPY
        return std::unexpected("Shell font index does not exist: " + font_index.generic_string());
#else
        BROOKESIA_LOGI(
            "Super shell fonts skipped because built-in font resource staging is disabled: %1%",
            font_index.generic_string()
        );
        shell_fonts_prepared_ = true;
        return {};
#endif
    }
    auto font_result = system_gui().register_font_file(share_dir, make_font_index_relative_path());
    if (!font_result) {
        return std::unexpected("Failed to register Shell font index: " + font_result.error());
    }
    font_result = system_gui().set_default_font_for_language("en", SUPER_DEFAULT_EN_FONT_ID);
    if (!font_result) {
        return std::unexpected("Failed to set default English font: " + font_result.error());
    }
    font_result = system_gui().set_default_font_for_language("zh_CN", SUPER_DEFAULT_ZH_CN_FONT_ID);
    if (!font_result) {
        return std::unexpected("Failed to set default Chinese font: " + font_result.error());
    }
    if (system_gui().get_language().empty()) {
        font_result = system_gui().set_language("en", false);
        if (!font_result) {
            return std::unexpected("Failed to set default Shell language: " + font_result.error());
        }
    }
    BROOKESIA_LOGI("Super shell fonts preloaded: %1%", font_index.generic_string());
    shell_fonts_prepared_ = true;
    return {};
}

std::expected<void, std::string> System::prepare_shell_themes()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    if (shell_themes_prepared_) {
        return {};
    }

    const auto resource_dir = make_system_resource_dir(*this);
    for (const auto &relative_path : {
                make_theme_relative_path("light.json"),
                make_theme_relative_path("dark.json"),
            }) {
        auto result = system_gui().load_theme_file(resource_dir, relative_path);
        if (!result) {
            return std::unexpected("Failed to preload Shell theme '" + relative_path + "': " + result.error());
        }
    }

    std::string theme_id = make_preload_theme_id(get_stored_gui_theme_preference());
    auto result = system_gui().set_theme(theme_id, false);
    if (!result && theme_id != SUPER_DEFAULT_THEME_ID) {
        BROOKESIA_LOGW(
            "Failed to preload stored system GUI theme '%1%': %2%; falling back to %3%",
            theme_id,
            result.error(),
            SUPER_DEFAULT_THEME_ID
        );
        theme_id = SUPER_DEFAULT_THEME_ID;
        result = system_gui().set_theme(theme_id, false);
    }
    if (!result) {
        return std::unexpected("Failed to preload Shell theme '" + theme_id + "': " + result.error());
    }

    BROOKESIA_LOGI("Super shell theme preloaded: %1%", theme_id);
    shell_themes_prepared_ = true;
    return {};
}

std::expected<void, std::string> System::on_prepare_startup_overlay()
{
    return prepare_shell_fonts();
}

std::expected<void, std::string> System::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    set_system_type(BROOKESIA_SYSTEM_SUPER_SYSTEM_TYPE);

    auto font_result = prepare_shell_fonts();
    if (!font_result) {
        return font_result;
    }

    auto theme_result = prepare_shell_themes();
    if (!theme_result) {
        return theme_result;
    }

    if (shell_app_id_ == core::INVALID_APP_ID) {
        shell_app_ = std::make_shared<ShellApp>(*this);
        auto result = install_app(shell_app_);
        if (!result) {
            shell_app_.reset();
            return std::unexpected(result.error());
        }
        shell_app_id_ = *result;
    }

    BROOKESIA_LOGI("Super system initialized");
    return {};
}

std::expected<void, std::string> System::on_start()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    stopping_ = false;
    pending_shell_page_ = ShellPage::AppLauncher;

    if (shell_app_id_ != core::INVALID_APP_ID) {
        auto result = restore_shell();
        if (!result) {
            return result;
        }
    }
    BROOKESIA_LOGI("Super shell started");
    return {};
}

void System::on_stop()
{
    stopping_ = true;
}

void System::on_deinit()
{
    shell_app_.reset();
    shell_app_id_ = core::INVALID_APP_ID;
    pending_shell_page_ = ShellPage::AppLauncher;
    stopping_ = false;
    shell_fonts_prepared_ = false;
    shell_themes_prepared_ = false;
}

std::optional<std::string> System::get_stored_gui_theme_preference() const
{
    return get_stored_gui_theme_id();
}

std::optional<std::string> System::get_stored_gui_language_preference() const
{
    return get_stored_gui_language();
}

void System::begin_shell_gui_preferences_restore()
{
    begin_gui_preferences_restore();
}

void System::mark_shell_gui_preferences_restored()
{
    mark_gui_preferences_restored();
}

std::expected<void, std::string> System::on_app_installed(const core::AppInfo &app)
{
    (void)app;
    BROOKESIA_LOGD("Super launcher app available: id(%1%), name(%2%)", app.app_id, app.manifest.name);
    if (!shell_app_) {
        return {};
    }
    return shell_app_->refresh_launcher();
}

std::expected<void, std::string> System::on_app_uninstalled(const core::AppInfo &app)
{
    (void)app;
    BROOKESIA_LOGD("Super launcher app removed: id(%1%), name(%2%)", app.app_id, app.manifest.name);
    if (!shell_app_) {
        return {};
    }
    return shell_app_->refresh_launcher();
}

std::expected<void, std::string> System::on_app_started(const core::AppInfo &app)
{
    BROOKESIA_LOGI("Super foreground app: id(%1%), name(%2%)", app.app_id, app.manifest.name);
    if (shell_app_) {
        auto result = shell_app_->set_foreground_app(app.app_id == shell_app_id_ ? std::nullopt : std::optional(app));
        if (!result) {
            BROOKESIA_LOGW("Failed to update Shell foreground state: %1%", result.error());
        }
    }
    return {};
}

void System::on_app_start_failed(const core::AppInfo &app, std::string_view reason)
{
    BROOKESIA_LOGW(
        "Super app start failed: id(%1%), name(%2%), reason(%3%)",
        app.app_id,
        app.manifest.name,
        reason
    );
    if (stopping_ || app.app_id == shell_app_id_) {
        return;
    }
    auto result = restore_shell();
    if (!result) {
        BROOKESIA_LOGW("Failed to restore Super shell after app start failure: %1%", result.error());
    }
}

void System::on_app_stopped(const core::AppInfo &app)
{
    BROOKESIA_LOGI("Super app stopped: id(%1%), name(%2%)", app.app_id, app.manifest.name);
    if (stopping_ || app.app_id == shell_app_id_) {
        return;
    }
    auto result = restore_shell();
    if (!result) {
        BROOKESIA_LOGW("Failed to restore Super shell: %1%", result.error());
    }
}

void System::on_app_stop_failed(const core::AppInfo &app, std::string_view reason)
{
    BROOKESIA_LOGW(
        "Super app stop failed: id(%1%), name(%2%), reason(%3%)",
        app.app_id,
        app.manifest.name,
        reason
    );
    if (stopping_ || app.app_id == shell_app_id_) {
        return;
    }
    auto result = restore_shell();
    if (!result) {
        BROOKESIA_LOGW("Failed to restore Super shell after app stop failure: %1%", result.error());
    }
}

std::expected<void, std::string> System::on_language_changed(std::string_view)
{
    if (!shell_app_) {
        return {};
    }
    return shell_app_->refresh_environment();
}

std::expected<void, std::string> System::on_theme_changed(std::string_view)
{
    if (!shell_app_) {
        return {};
    }
    return shell_app_->refresh_environment();
}

std::expected<void, std::string> System::on_show_app_loading(core::AppId app_id)
{
    if (!shell_app_) {
        return std::unexpected("Shell app is not available");
    }
    return shell_app_->show_loading(app_id);
}

void System::on_hide_app_loading(core::AppId app_id)
{
    if (shell_app_) {
        shell_app_->hide_loading(app_id);
    }
}

std::expected<void, std::string> System::on_show_app_keyboard(
    core::AppId app_id,
    core::KeyboardRequestId request_id,
    const core::KeyboardRequestOptions &options
)
{
    if (!shell_app_) {
        return std::unexpected("Shell app is not available");
    }
    return shell_app_->show_keyboard(app_id, request_id, options);
}

void System::on_hide_app_keyboard(core::AppId app_id, core::KeyboardRequestId request_id)
{
    if (shell_app_) {
        shell_app_->hide_keyboard(app_id, request_id);
    }
}

std::expected<void, std::string> System::on_show_message_dialog(
    core::AppId app_id,
    core::MessageDialogRequestId request_id,
    const core::MessageDialogOptions &options
)
{
    if (!shell_app_) {
        return std::unexpected("Shell app is not available");
    }
    return shell_app_->show_message_dialog(app_id, request_id, options);
}

std::expected<void, std::string> System::on_update_message_dialog(
    core::AppId app_id,
    core::MessageDialogRequestId request_id,
    const core::MessageDialogOptions &options
)
{
    if (!shell_app_) {
        return std::unexpected("Shell app is not available");
    }
    return shell_app_->update_message_dialog(app_id, request_id, options);
}

void System::on_hide_message_dialog(core::AppId app_id, core::MessageDialogRequestId request_id)
{
    if (shell_app_) {
        shell_app_->hide_message_dialog(app_id, request_id);
    }
}

void System::on_message_dialog_idle()
{
    if (shell_app_) {
        shell_app_->handle_message_dialog_idle();
    }
}

} // namespace esp_brookesia::system::super
