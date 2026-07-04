/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <expected>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "boost/json/object.hpp"

#include "brookesia/service_manager.hpp"
#include "brookesia/system_core.hpp"
#include "brookesia/system_super/system.hpp"
#include "private/system_constants.hpp"

namespace esp_brookesia::system::super {

class ShellApp final: public core::IApp {
public:
    explicit ShellApp(System &owner);

    core::AppManifest get_manifest() const override;
    core::AppGuiDescriptor get_gui_descriptor() const override;
    std::expected<void, std::string> on_start(core::AppContext &context) override;
    std::expected<void, std::string> on_stop(core::AppContext &context) override;
    std::expected<void, std::string> on_timer(
        core::AppContext &context,
        core::TimerId timer_id,
        std::string_view name
    ) override;

    std::expected<void, std::string> show_page(ShellPage page);
    std::expected<void, std::string> set_foreground_app(const std::optional<core::AppInfo> &app);
    std::expected<void, std::string> set_system_ui_expanded(bool expanded);
    std::expected<void, std::string> show_loading(core::AppId app_id);
    void hide_loading(core::AppId app_id);
    std::expected<void, std::string> show_keyboard(
        core::AppId app_id,
        core::KeyboardRequestId request_id,
        const core::KeyboardRequestOptions &options
    );
    void hide_keyboard(core::AppId app_id, core::KeyboardRequestId request_id);
    std::expected<void, std::string> show_message_dialog(
        core::AppId app_id,
        core::MessageDialogRequestId request_id,
        const core::MessageDialogOptions &options
    );
    std::expected<void, std::string> update_message_dialog(
        core::AppId app_id,
        core::MessageDialogRequestId request_id,
        const core::MessageDialogOptions &options
    );
    void hide_message_dialog(core::AppId app_id, core::MessageDialogRequestId request_id);
    void handle_message_dialog_idle();
    std::expected<void, std::string> refresh_launcher();
    std::expected<void, std::string> refresh_environment();

private:
    struct DisplaySourceRestoreRecord {
        std::string output_name;
        std::string source_name;
    };

    std::expected<void, std::string> mount_background(core::AppContext &context);
    void unmount_background();
    std::expected<void, std::string> load_fonts(core::AppContext &context);
    std::expected<void, std::string> apply_language_preference(core::AppContext &context);
    std::expected<void, std::string> load_themes(core::AppContext &context);
    std::expected<void, std::string> mount_overlay(core::AppContext &context);
    void unmount_overlay();
    std::string get_current_page_title() const;
    std::string get_app_display_name(const core::AppInfo &app) const;
    std::string get_app_icon_text(std::string_view name) const;
    std::expected<void, std::string> apply_i18n_updates();
    std::expected<void, std::string> refresh_system_ui_bindings();
    std::expected<void, std::string> refresh_system_ui_position_bindings();
    std::expected<void, std::string> refresh_system_ui_state_bindings();
    std::expected<void, std::string> show_launch_overlay(
        const core::AppInfo &app,
        const std::optional<gui::ViewFrame> &origin,
        std::function<void()> completed_handler
    );
    void finish_launch_overlay();
    void cancel_pending_launch();
    void start_app_after_launch(core::AppId app_id);
    void schedule_app_start_after_launch(core::AppId app_id);
    std::expected<void, std::string> prepare_app_modal_for_app_start();
    std::expected<void, std::string> apply_app_modal_after_launch();
    std::expected<void, std::string> show_loading_overlay();
    std::expected<void, std::string> hide_app_modal();
    std::expected<void, std::string> refresh_app_modal_for_loading();
    std::expected<void, std::string> mount_keyboard();
    void unmount_keyboard();
    std::expected<void, std::string> refresh_keyboard_bindings(const core::KeyboardRequestOptions &options);
    std::expected<void, std::string> refresh_keyboard_password_bindings();
    std::expected<void, std::string> start_keyboard_show_animation();
    void start_keyboard_hide_animation(std::function<void()> completed_handler);
    void stop_keyboard_animations();
    void reset_keyboard_state();
    void finish_keyboard(bool confirmed);
    std::expected<void, std::string> mount_message_dialog();
    void unmount_message_dialog();
    std::expected<void, std::string> refresh_message_dialog_bindings(const core::MessageDialogOptions &options);
    std::expected<void, std::string> clear_message_dialog_bindings();
    void start_message_dialog_auto_close_timer(int32_t auto_close_ms);
    void stop_message_dialog_auto_close_timer();
    void reset_message_dialog_state();
    void finish_message_dialog(int32_t button_index, core::MessageDialogCloseReason reason);
    std::expected<void, std::string> populate_launcher(core::AppContext &context);
    void disconnect_launcher_actions();
    void disconnect_overlay_actions();
    void handle_launcher_event(const gui::Event &event);
    bool ensure_display_service_binding();
    void release_display_service_binding();
    bool configure_display_gesture();
    void disconnect_display_gesture();
    std::vector<std::string> get_gui_display_output_names() const;
    bool is_display_source_available(std::string_view source_name) const;
    bool is_display_source_role_available(std::string_view role) const;
    void switch_display_to_gui_for_system_ui();
    void restore_display_after_system_ui();
    void clear_display_source_restore_records();
    bool ensure_wifi_service_binding();
    void release_wifi_service_binding();
    void refresh_wifi_status();
    void set_status_wifi_state(bool visible, bool connected);
    bool ensure_sntp_service_binding();
    void release_sntp_service_binding();
    void subscribe_sntp_events();
    void disconnect_sntp_events();
    void refresh_status_clock();
    void schedule_status_clock_timer();
    void stop_status_clock_timer();
    void handle_display_touch_gesture(const boost::json::object &info_json);
    void reset_gesture_indicator();
    void cancel_gesture_exit_hold_timer();
    void cancel_status_peek_auto_hide_timer();
    bool can_start_status_peek() const;
    void trigger_status_peek();
    void collapse_status_peek(bool restore_display);
    void reset_status_peek_session(bool restore_display, bool collapse_immediately, const char *reason);
    void update_gesture_indicator_width(int32_t width);
    void animate_gesture_indicator_rebound();
    void trigger_gesture_exit();

    System &owner_;
    core::AppContext *context_ = nullptr;
    service::ServiceBinding display_service_binding_;
    service::ServiceBinding wifi_service_binding_;
    service::ServiceBinding sntp_service_binding_;
    service::EventRegistry::SignalConnection display_gesture_connection_;
    service::EventRegistry::SignalConnection wifi_event_connection_;
    service::EventRegistry::SignalConnection sntp_event_connection_;
    bool background_mounted_ = false;
    bool overlay_mounted_ = false;
    bool system_ui_expanded_ = true;
    bool foreground_is_shell_ = true;
    bool launch_overlay_active_ = false;
    core::TimerId launch_hold_timer_id_ = core::INVALID_TIMER_ID;
    std::optional<core::AppId> pending_launch_app_id_;
    std::string current_theme_id_ = SUPER_DEFAULT_THEME_ID;
    gui::SubscriptionId status_bar_animation_id_ = 0;
    gui::SubscriptionId gesture_indicator_animation_id_ = 0;
    gui::SubscriptionId gesture_indicator_x_animation_id_ = 0;
    uint64_t system_ui_animation_generation_ = 0;
    uint64_t gesture_generation_ = 0;
    uint64_t status_peek_generation_ = 0;
    uint64_t wifi_status_generation_ = 0;
    bool gesture_tracking_ = false;
    bool gesture_exit_armed_ = false;
    bool gesture_exit_triggered_ = false;
    bool status_peek_tracking_ = false;
    bool status_peek_visible_ = false;
    bool wifi_connected_ = false;
    int32_t gesture_indicator_max_width_ = 0;
    core::TimerId gesture_exit_hold_timer_id_ = core::INVALID_TIMER_ID;
    core::TimerId status_peek_auto_hide_timer_id_ = core::INVALID_TIMER_ID;
    core::TimerId status_clock_timer_id_ = core::INVALID_TIMER_ID;
    std::vector<gui::ScopedConnection> overlay_action_connections_;
    std::vector<gui::ScopedConnection> launcher_action_connections_;
    std::vector<gui::ScopedConnection> keyboard_action_connections_;
    std::vector<core::AppId> loading_owners_;
    std::optional<core::KeyboardRequestId> active_keyboard_request_id_;
    core::AppId active_keyboard_owner_ = core::INVALID_APP_ID;
    std::string active_keyboard_text_;
    bool keyboard_mounted_ = false;
    bool keyboard_closing_ = false;
    bool keyboard_accepting_actions_ = false;
    bool active_keyboard_password_requested_ = false;
    bool active_keyboard_password_hidden_ = false;
    gui::SubscriptionId keyboard_input_animation_id_ = 0;
    gui::SubscriptionId keyboard_body_animation_id_ = 0;
    uint64_t keyboard_animation_generation_ = 0;
    std::vector<gui::ScopedConnection> message_dialog_action_connections_;
    std::optional<core::MessageDialogRequestId> active_message_dialog_request_id_;
    core::AppId active_message_dialog_owner_ = core::INVALID_APP_ID;
    core::TimerId message_dialog_auto_close_timer_id_ = core::INVALID_TIMER_ID;
    core::MessageDialogOptions active_message_dialog_options_;
    bool message_dialog_mounted_ = false;
    bool message_dialog_closing_ = false;
    bool message_dialog_needs_reset_ = false;
    std::vector<core::AppId> launcher_order_;
    std::unordered_map<std::string, core::AppId> launcher_instance_to_app_;
    size_t launcher_slot_count_ = 0;
    std::vector<DisplaySourceRestoreRecord> display_source_restore_records_;
    bool launcher_populated_ = false;
    std::string applied_i18n_locale_;
};

} // namespace esp_brookesia::system::super
