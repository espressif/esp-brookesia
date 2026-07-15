/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <memory>
#include <expected>
#include <optional>
#include <string>
#include <string_view>

#include "brookesia/service_manager.hpp"
#include "brookesia/system_core.hpp"
#include "brookesia/system_super/macro_configs.h"

namespace esp_brookesia::system::super {

/**
 * @brief Built-in shell pages managed by the Super system shell application.
 */
enum class ShellPage {
    /** Home page that restores the shell surface. */
    Home,
    /** Installed application launcher page. */
    AppLauncher,
    /** Notification center page. */
    Notifications,
};

class ShellApp;

/**
 * @brief User-facing Brookesia system implementation with the built-in shell experience.
 *
 * Super extends system core with launcher, notification, quick control, keyboard, loading,
 * dialog, theme, and language handling suitable for end-user products.
 */
class System: public core::System {
    friend class ShellApp;

public:
    /**
     * @brief Configuration used to initialize the Super system.
     */
    struct Config {
        /** Core system configuration forwarded to the base system implementation. */
        core::System::Config core_config;
    };

    /**
     * @brief Initialize the system with default Super configuration.
     *
     * @return Empty result on success, or an error string.
     */
    std::expected<void, std::string> init();

    /**
     * @brief Initialize the system with explicit Super configuration.
     *
     * @param config Super system configuration.
     * @return Empty result on success, or an error string.
     */
    std::expected<void, std::string> init(Config config);

protected:
    core::SystemInfo on_get_system_info() const override;
    /** @brief Prepare shell resources required before startup overlays are shown. */
    std::expected<void, std::string> on_prepare_startup_overlay() override;
    /** @brief Initialize shell state after the core system has been configured. */
    std::expected<void, std::string> on_init() override;
    /** @brief Start the shell and restore persisted shell preferences. */
    std::expected<void, std::string> on_start() override;
    /** @brief Stop shell activity before the core system stops. */
    void on_stop() override;
    /** @brief Release shell resources during system deinitialization. */
    void on_deinit() override;
    std::expected<void, std::string> on_app_installed(const core::AppInfo &app) override;
    std::expected<void, std::string> on_app_uninstalled(const core::AppInfo &app) override;
    std::expected<void, std::string> on_app_started(const core::AppInfo &app) override;
    void on_app_start_failed(const core::AppInfo &app, std::string_view reason) override;
    void on_app_stopped(const core::AppInfo &app) override;
    void on_app_stop_failed(const core::AppInfo &app, std::string_view reason) override;
    std::expected<void, std::string> on_theme_changed(std::string_view theme_id) override;
    std::expected<void, std::string> on_language_changed(std::string_view language) override;
    std::expected<void, std::string> on_show_app_loading(core::AppId app_id) override;
    void on_hide_app_loading(core::AppId app_id) override;
    std::expected<void, std::string> on_show_app_keyboard(
        core::AppId app_id,
        core::KeyboardRequestId request_id,
        const core::KeyboardRequestOptions &options
    ) override;
    void on_hide_app_keyboard(core::AppId app_id, core::KeyboardRequestId request_id) override;
    std::expected<void, std::string> on_show_message_dialog(
        core::AppId app_id,
        core::MessageDialogRequestId request_id,
        const core::MessageDialogOptions &options
    ) override;
    std::expected<void, std::string> on_update_message_dialog(
        core::AppId app_id,
        core::MessageDialogRequestId request_id,
        const core::MessageDialogOptions &options
    ) override;
    void on_hide_message_dialog(core::AppId app_id, core::MessageDialogRequestId request_id) override;
    void on_message_dialog_idle() override;

private:
    std::expected<void, std::string> prepare_shell_fonts();
    std::expected<void, std::string> prepare_shell_themes();
    void start_sntp_if_needed();
    std::expected<void, std::string> open_shell_page(ShellPage page);
    std::expected<void, std::string> open_app_launcher();
    std::expected<void, std::string> open_quick_control();
    std::expected<void, std::string> open_system_monitor();
    std::expected<void, std::string> open_notifications();
    std::expected<void, std::string> close_active_app();
    std::expected<void, std::string> go_home();
    std::expected<void, std::string> restore_shell();
    bool is_shell_active() const;
    std::optional<std::string> get_stored_gui_theme_preference() const;
    std::optional<std::string> get_stored_gui_language_preference() const;
    void begin_shell_gui_preferences_restore();
    void mark_shell_gui_preferences_restored();

    core::AppId shell_app_id_ = core::INVALID_APP_ID;
    std::shared_ptr<ShellApp> shell_app_;
    service::ServiceBinding utils_service_binding_;
    service::EventRegistry::SignalConnection utils_debug_state_connection_;
    service::EventRegistry::SignalConnection utils_memory_snapshot_connection_;
    service::EventRegistry::SignalConnection utils_thread_snapshot_connection_;
    service::ServiceBinding sntp_service_binding_;
    ShellPage pending_shell_page_ = ShellPage::AppLauncher;
    bool stopping_ = false;
    bool shell_fonts_prepared_ = false;
    bool shell_themes_prepared_ = false;
};

} // namespace esp_brookesia::system::super
