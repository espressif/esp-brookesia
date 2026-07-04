/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "boost/json.hpp"
#include "brookesia/gui_interface.hpp"
#include "brookesia/lib_utils.hpp"
#include "brookesia/runtime_manager.hpp"
#include "brookesia/system_core/app/iapp.hpp"
#include "brookesia/system_core/system/batch.hpp"
#include "brookesia/system_core/system/gui_access.hpp"
#include "brookesia/system_core/system/storage.hpp"

namespace esp_brookesia::system::core {

class SystemService;
class GuiService;
class TimerService;

/**
 * @brief Core Brookesia system runtime that owns apps, GUI access, timers, storage, and services.
 *
 * System coordinates native and runtime applications, mounts their GUI resources, dispatches app
 * lifecycle callbacks, and exposes storage, GUI, keyboard, dialog, and timer facilities through
 * AppContext and service APIs.
 */
class System {
public:
    /**
     * @brief Configuration used to initialize the core system runtime.
     */
    struct Config {
        /**
         * @brief Optional startup overlay shown while the system is preparing the shell.
         */
        struct StartupOverlay {
            /** Whether startup overlay mounting is enabled. */
            bool enabled = false;
            /** GUI resource root containing the startup overlay document. */
            std::string root_path;
            /** Absolute screen path inside the startup overlay document. */
            std::string screen_path = "/startup";
            /** GUI mount target used for the overlay. */
            gui::MountTarget target = {
                .display_id = {},
                .layer = gui::GuiLayer::System,
            };
            /** Minimum duration in milliseconds before the overlay can be dismissed. */
            int32_t min_present_ms = 16;
        };

        /**
         * @brief Optional launch transition shown when an app surface is being opened.
         */
        struct AppLaunchTransition {
            /** Whether app launch transition rendering is enabled. */
            bool enabled = false;
            /** GUI resource root containing the launch transition document. */
            std::string root_path;
            /** Absolute screen path inside the launch transition document. */
            std::string screen_path = "/app_launch";
            /** Path of the app surface placeholder used by the transition. */
            std::string surface_path = "/app_launch/surface";
            /** Path of the app icon image in the transition document. */
            std::string icon_path = "/app_launch/icon_box/icon_image";
            /** Path of the fallback label when an icon is not available. */
            std::string fallback_label_path = "/app_launch/icon_box/fallback_label";
            /** GUI mount target used for the launch transition. */
            gui::MountTarget target = {
                .display_id = {},
                .layer = gui::GuiLayer::System,
            };
            /** Transition animation duration in milliseconds. */
            int32_t duration_ms = 220;
            /** Maximum time to wait for app surface readiness. */
            int32_t timeout_ms = 500;
            /** Final app icon size in pixels. */
            int32_t final_icon_size = 96;
        };

        /** GUI backend implementation used by the system runtime. */
        std::unique_ptr<gui::IBackend> gui_backend;
        /** GUI and platform environment exposed to apps. */
        gui::Environment environment;
        /** Storage roots and install target policy. */
        StorageConfig storage = {};
        /** Optional startup overlay configuration. */
        StartupOverlay startup_overlay = {};
        /** Optional app launch transition configuration. */
        AppLaunchTransition app_launch_transition = {};
        /** System type string exposed to services and applications. */
        std::string system_type = BROOKESIA_SYSTEM_CORE_DEFAULT_SYSTEM_TYPE;
        /** Whether the service manager should be started by System::start(). */
        bool start_service_manager = true;
        /** Whether statically registered native app providers should be installed. */
        bool install_registered_apps = true;
        /** Whether package apps should be discovered from configured package roots. */
        bool install_package_apps = true;
        /** Whether GUI view debug metadata should be enabled at startup. */
        bool enable_gui_view_debug = false;
        /** Whether GUI live preview should be enabled at startup. */
        bool enable_gui_live_preview = false;
        /** Live preview options applied when startup live preview is enabled. */
        gui::LivePreviewOptions gui_live_preview_options = {};
        /** Poll interval used by startup live preview in milliseconds. */
        int32_t gui_live_preview_poll_interval_ms = 100;
    };

    /**
     * @brief Create a core system runtime.
     */
    System();
    System(const System &) = delete;
    System &operator=(const System &) = delete;
    virtual ~System();

    /**
     * @brief Initialize the system runtime with explicit configuration.
     *
     * @param config Runtime configuration. Ownership of gui_backend is transferred into the system.
     * @return Empty result on success, or an error string.
     */
    std::expected<void, std::string> init(Config config);

    /**
     * @brief Start services, install configured apps, and enter the running state.
     *
     * @return Empty result on success, or an error string.
     */
    std::expected<void, std::string> start();

    /**
     * @brief Stop running apps and services.
     */
    void stop();

    /**
     * @brief Release runtime resources after the system has stopped.
     */
    void deinit();

    /**
     * @brief Process pending timer events from the host event loop.
     */
    void process_timers();

    std::expected<AppId, std::string> install_app(std::shared_ptr<IApp> app);
    std::expected<AppId, std::string> install_runtime_app(const AppManifest &manifest);
    std::expected<void, std::string> install_registered_apps();
    std::expected<void, std::string> uninstall_app(AppId app_id);

    struct AppStartOptions {
        std::optional<gui::ViewFrame> launch_origin_frame;
    };

    std::expected<void, std::string> start_app(AppId app_id);
    std::expected<void, std::string> start_app(AppId app_id, const AppStartOptions &options);
    std::expected<void, std::string> stop_app(AppId app_id);
    std::expected<void, std::string> pause_app(AppId app_id);
    std::expected<void, std::string> resume_app(AppId app_id);
    std::expected<void, std::string> request_close_app(AppId app_id);
    std::expected<void, std::string> show_app_loading(AppId app_id);
    std::expected<void, std::string> hide_app_loading(AppId app_id);
    std::expected<KeyboardRequestId, std::string> show_app_keyboard(
        AppId app_id,
        KeyboardRequestOptions options,
        KeyboardResultHandler handler = {}
    );
    std::expected<void, std::string> hide_app_keyboard(AppId app_id, KeyboardRequestId request_id);
    std::expected<void, std::string> complete_app_keyboard(
        AppId app_id,
        KeyboardRequestId request_id,
        bool confirmed,
        std::string text
    );
    std::expected<MessageDialogRequestId, std::string> show_app_message_dialog(
        AppId app_id,
        MessageDialogOptions options,
        MessageDialogResultHandler handler = {}
    );
    std::expected<void, std::string> hide_app_message_dialog(AppId app_id, MessageDialogRequestId request_id);
    std::expected<void, std::string> update_app_message_dialog(
        AppId app_id,
        MessageDialogRequestId request_id,
        MessageDialogOptions options
    );
    std::expected<void, std::string> complete_app_message_dialog(
        AppId app_id,
        MessageDialogRequestId request_id,
        int32_t button_index,
        MessageDialogCloseReason reason
    );
    std::expected<MessageDialogRequestId, std::string> show_system_message_dialog(
        MessageDialogOptions options,
        MessageDialogResultHandler handler = {}
    );
    std::expected<void, std::string> hide_system_message_dialog(MessageDialogRequestId request_id);
    std::expected<void, std::string> update_system_message_dialog(
        MessageDialogRequestId request_id,
        MessageDialogOptions options
    );

    std::vector<AppInfo> list_apps() const;
    std::optional<AppInfo> get_app(AppId app_id) const;
    std::optional<AppInfo> get_active_app() const;

    void set_system_type(std::string type);
    std::string get_system_type() const;
    SystemInfo get_system_info() const;
    gui::Environment get_environment() const;
    boost::json::object get_environment_json() const;
    StorageLayout get_storage_layout() const;
    std::expected<AppStoragePaths, std::string> get_app_storage_paths(AppId app_id);
    std::expected<PublicStoragePaths, std::string> get_public_storage_paths();
    std::expected<void, std::string> set_default_install_storage(
        StorageInstallTarget target,
        std::string preferred_external_id = {}
    );
    std::expected<std::vector<StorageFileEntry>, std::string> storage_list(
        AppId app_id,
        const StorageFileLocation &location
    );
    std::expected<StorageFileInfo, std::string> storage_stat(
        AppId app_id,
        const StorageFileLocation &location
    );
    std::expected<void, std::string> storage_mkdir(
        AppId app_id,
        const StorageFileLocation &location
    );
    std::expected<std::string, std::string> storage_read(
        AppId app_id,
        const StorageFileLocation &location
    );
    std::expected<void, std::string> storage_write(
        AppId app_id,
        const StorageFileLocation &location,
        std::string_view data
    );
    std::expected<void, std::string> storage_remove(
        AppId app_id,
        const StorageFileLocation &location
    );
    std::expected<void, std::string> storage_rename(
        AppId app_id,
        const StorageFileLocation &from,
        const StorageFileLocation &to
    );

    void set_gui_view_debug_enabled(bool enabled);
    bool is_gui_view_debug_enabled() const;
    std::expected<void, std::string> enable_app_gui_live_preview(
        AppId app_id,
        const gui::LivePreviewOptions &options = {}
    );
    bool disable_app_gui_live_preview(AppId app_id);
    void poll_gui_live_preview();

    std::expected<void, std::string> gui_set_binding_value(
        AppId app_id,
        std::string_view absolute_path,
        std::string_view key,
        std::string value
    );
    std::expected<void, std::string> gui_set_binding_values(
        AppId app_id,
        const std::vector<gui::BindingValueUpdate> &updates
    );
    std::optional<std::string> gui_get_binding_value(
        AppId app_id,
        std::string_view absolute_path,
        std::string_view key
    ) const;
    std::expected<boost::json::value, std::string> gui_get_constant_value(
        AppId app_id,
        std::string_view path
    ) const;
    std::expected<void, std::string> gui_set_text(AppId app_id, std::string_view absolute_path, std::string_view text);
    std::expected<void, std::string> gui_set_value(AppId app_id, std::string_view absolute_path, int32_t value);
    std::expected<void, std::string> gui_set_checked(AppId app_id, std::string_view absolute_path, bool checked);
    std::expected<gui::View, std::string> gui_create_view(
        AppId app_id,
        std::string_view template_id,
        std::string_view parent_absolute_path,
        std::string_view instance_id
    );
    bool gui_destroy_view(AppId app_id, std::string_view absolute_path);
    std::expected<void, std::string> gui_subscribe_action(AppId app_id, std::string_view action);
    gui::ScopedConnection gui_subscribe_action(
        AppId app_id,
        std::string_view action,
        std::function<void(const gui::Event &)> handler
    );
    std::expected<void, std::string> gui_trigger_screen_flow(
        AppId app_id,
        std::string_view screen_flow,
        std::string_view action
    );
    std::optional<std::string> gui_get_screen_flow_state(AppId app_id, std::string_view screen_flow) const;
    std::optional<gui::ViewFrame> gui_get_view_frame(AppId app_id, std::string_view absolute_path) const;
    std::expected<void, std::string> gui_set_view_src(
        AppId app_id,
        std::string_view absolute_path,
        std::string_view src
    );
    std::expected<gui::SubscriptionId, std::string> gui_start_view_animation_with_id(
        AppId app_id,
        std::string_view absolute_path,
        const gui::Animation &animation,
        gui::Runtime::AnimationCompletedHandler completed_handler = {}
    );
    std::expected<gui::RuntimeAnimationStartResult, std::string> gui_start_view_animation_with_result(
        AppId app_id,
        std::string_view absolute_path,
        const gui::Animation &animation,
        gui::Runtime::AnimationCompletedHandler completed_handler = {}
    );
    bool gui_stop_animation(AppId app_id, gui::SubscriptionId subscription_id);
    std::expected<void, std::string> gui_scroll_to_view(
        AppId app_id,
        std::string_view absolute_path,
        bool animated = true
    );
    std::expected<GuiBatchResult, std::string> gui_execute_batch(
        AppId app_id,
        const std::vector<GuiBatchCommand> &commands
    );

    std::expected<TimerId, std::string> timer_start_periodic(
        AppId app_id,
        std::string_view name,
        int interval_ms
    );
    std::expected<TimerId, std::string> timer_start_delayed(
        AppId app_id,
        std::string_view name,
        int delay_ms
    );
    bool timer_stop(AppId app_id, TimerId timer_id);

    std::expected<AppId, std::string> get_current_runtime_app_owner() const;

protected:
    virtual SystemInfo on_get_system_info() const;
    virtual std::expected<void, std::string> on_prepare_startup_overlay();
    virtual std::expected<void, std::string> on_init();
    virtual std::expected<void, std::string> on_start();
    virtual void on_stop();
    virtual void on_deinit();
    virtual std::expected<void, std::string> on_app_installed(const AppInfo &app);
    virtual std::expected<void, std::string> on_app_uninstalled(const AppInfo &app);
    virtual std::expected<void, std::string> on_app_started(const AppInfo &app);
    virtual void on_app_start_failed(const AppInfo &app, std::string_view reason);
    virtual void on_app_stopped(const AppInfo &app);
    virtual void on_app_stop_failed(const AppInfo &app, std::string_view reason);
    virtual std::expected<void, std::string> on_theme_changed(std::string_view theme_id);
    virtual std::expected<void, std::string> on_language_changed(std::string_view language);
    virtual std::expected<void, std::string> on_show_app_loading(AppId app_id);
    virtual void on_hide_app_loading(AppId app_id);
    virtual std::expected<void, std::string> on_show_app_keyboard(
        AppId app_id,
        KeyboardRequestId request_id,
        const KeyboardRequestOptions &options
    );
    virtual void on_hide_app_keyboard(AppId app_id, KeyboardRequestId request_id);
    virtual std::expected<void, std::string> on_show_message_dialog(
        AppId app_id,
        MessageDialogRequestId request_id,
        const MessageDialogOptions &options
    );
    virtual std::expected<void, std::string> on_update_message_dialog(
        AppId app_id,
        MessageDialogRequestId request_id,
        const MessageDialogOptions &options
    );
    virtual void on_hide_message_dialog(AppId app_id, MessageDialogRequestId request_id);
    virtual void on_message_dialog_idle();

    SystemGuiAccess &system_gui();
    std::optional<std::string> get_stored_gui_theme_id() const;
    std::optional<std::string> get_stored_gui_language() const;
    void begin_gui_preferences_restore();
    void mark_gui_preferences_restored();

    std::expected<gui::DocumentId, std::string> system_gui_load_file(
        std::string_view resource_dir,
        std::string_view path
    );
    std::expected<gui::DocumentId, std::string> system_gui_load_json(
        std::string_view root_path,
        std::string_view json,
        std::string_view resource_dir
    );
    std::expected<gui::View, std::string> system_gui_mount_screen(
        gui::DocumentId document_id,
        std::string_view absolute_path,
        const gui::MountTarget &target = {}
    );
    bool system_gui_unmount_screen(gui::DocumentId document_id, std::string_view absolute_path);
    bool system_gui_unload(gui::DocumentId document_id);
    std::expected<void, std::string> system_gui_enable_live_preview(
        gui::DocumentId document_id,
        const gui::LivePreviewOptions &options = {}
    );
    bool system_gui_disable_live_preview(gui::DocumentId document_id);
    gui::ScopedConnection system_gui_subscribe_action(
        gui::DocumentId document_id,
        std::string_view action,
        gui::Runtime::ActionHandler handler
    );

private:
    friend class AppGuiRuntime;
    friend class AppTimerRuntime;
    friend class SystemService;
    friend class GuiService;
    friend class TimerService;
    friend class SystemGuiAccess;

    class Impl;
    std::expected<void, std::string> show_next_message_dialog();
    std::expected<MessageDialogRequestId, std::string> enqueue_message_dialog(
        AppId app_id,
        bool require_app,
        MessageDialogOptions options,
        MessageDialogResultHandler handler
    );
    std::expected<void, std::string> close_message_dialog(
        AppId app_id,
        MessageDialogRequestId request_id,
        int32_t button_index,
        MessageDialogCloseReason reason,
        bool validate_owner,
        bool notify_shell
    );
    std::expected<void, std::string> update_message_dialog(
        AppId app_id,
        MessageDialogRequestId request_id,
        MessageDialogOptions options,
        bool validate_owner
    );
    void close_message_dialogs_for_app(AppId app_id);
    void publish_message_dialog_closed(MessageDialogResult result, MessageDialogResultHandler handler);
    void remove_queued_message_dialog(MessageDialogRequestId request_id);
    SystemGuiAccess system_gui_access_;
    std::unique_ptr<Impl> impl_;
};

} // namespace esp_brookesia::system::core
