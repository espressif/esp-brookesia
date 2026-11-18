/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "boost/json/array.hpp"
#include "brookesia/app_settings/macro_configs.h"
#include "brookesia/lib_utils/signal.hpp"

#include "brookesia/service_manager/service/base.hpp"
#include "brookesia/system_core.hpp"

namespace esp_brookesia::app::settings {

/**
 * @brief Built-in settings application for device, display, sound, Wi-Fi, language, and theme controls.
 */
class SettingsApp final: public system::core::IApp {
public:
    /**
     * @brief Create a settings application instance.
     */
    SettingsApp();

    /**
     * @brief Destroy the settings application instance.
     */
    ~SettingsApp() override;

    system::core::AppManifest get_manifest() const override;
    system::core::AppGuiDescriptor get_gui_descriptor() const override;
    std::span<const std::byte> gui_bundle() const override;
    const void *gui_runtime_config() const override;
    std::expected<void, std::string> on_install(system::core::AppContext &context) override;
    void on_uninstall(system::core::AppContext &context) override;
    std::expected<void, std::string> on_start(system::core::AppContext &context) override;
    std::expected<void, std::string> on_stop(system::core::AppContext &context) override;
    std::expected<void, std::string> on_pause(system::core::AppContext &context) override;
    std::expected<void, std::string> on_resume(system::core::AppContext &context) override;
    std::expected<void, std::string> on_action(
        system::core::AppContext &context,
        std::string_view action
    ) override;
    std::expected<void, std::string> on_timer(
        system::core::AppContext &context,
        system::core::TimerId timer_id,
        std::string_view name
    ) override;

private:
    struct DeviceUiState {
        bool display_service_ready = false;
        bool audio_service_ready = false;
        bool display_backlight_supported = false;
        bool audio_player_supported = false;
        std::optional<uint32_t> backlight_output_id;
        std::string backlight_output_name;
        int brightness = 0;
        int volume = 0;
        bool muted = false;
        std::string sound_error;
        bool sound_pending = false;
    };

    struct WifiNetworkState {
        std::string parent;
        std::string id;
        std::string ssid;
        std::string detail;
        std::string band;
        std::string password;
        bool saved = false;
        bool locked = true;
        bool enterprise = false;
        int signal_level = 0;
        int channel = 0;
        int rssi = 0;
    };

    enum class WifiConnectedCardState {
        Hidden,
        Connecting,
        Connected,
        DisconnectedGrace,
    };

    struct WifiUiState {
        bool service_ready = false;
        bool enabled = false;
        bool scanning = false;
        bool connecting = false;
        std::string general_state;
        std::string connected_ssid;
        std::string connected_card_ssid;
        WifiConnectedCardState connected_card_state = WifiConnectedCardState::Hidden;
    };

    struct TimeZoneUiState {
        bool service_ready = false;
        std::string timezone;
        std::string state;
    };

    struct DebugUiState {
        bool memory_enabled = false;
        bool thread_enabled = false;
        bool gui_view_debug_enabled = false;
        int memory_sample_interval_ms = BROOKESIA_APP_SETTINGS_DEBUG_MEMORY_SAMPLE_INTERVAL_DEFAULT_MS;
        int memory_internal_free_percent_threshold =
            BROOKESIA_APP_SETTINGS_DEBUG_MEMORY_INTERNAL_FREE_PERCENT_THRESHOLD_DEFAULT;
        int memory_internal_largest_free_block_threshold_kb =
            BROOKESIA_APP_SETTINGS_DEBUG_MEMORY_INTERNAL_LARGEST_FREE_BLOCK_THRESHOLD_DEFAULT_KB;
        int memory_external_free_percent_threshold =
            BROOKESIA_APP_SETTINGS_DEBUG_MEMORY_EXTERNAL_FREE_PERCENT_THRESHOLD_DEFAULT;
        int memory_external_largest_free_block_threshold_kb =
            BROOKESIA_APP_SETTINGS_DEBUG_MEMORY_EXTERNAL_LARGEST_FREE_BLOCK_THRESHOLD_DEFAULT_KB;
        int thread_profiling_interval_ms = BROOKESIA_APP_SETTINGS_DEBUG_THREAD_PROFILING_INTERVAL_DEFAULT_MS;
        int thread_sampling_duration_ms = BROOKESIA_APP_SETTINGS_DEBUG_THREAD_SAMPLING_DURATION_DEFAULT_MS;
        int thread_idle_cpu_percent_threshold =
            BROOKESIA_APP_SETTINGS_DEBUG_THREAD_IDLE_CPU_PERCENT_THRESHOLD_DEFAULT;
        int thread_stack_high_water_mark_threshold_bytes =
            BROOKESIA_APP_SETTINGS_DEBUG_THREAD_STACK_HIGH_WATER_MARK_THRESHOLD_DEFAULT_BYTES;
    };

    std::expected<void, std::string> subscribe_actions(system::core::AppContext &context);
    std::expected<void, std::string> apply_locale(system::core::AppContext &context);
    std::expected<void, std::string> refresh_header(system::core::AppContext &context);
    std::expected<void, std::string> refresh_theme_state(system::core::AppContext &context);
    std::expected<void, std::string> refresh_language_state(system::core::AppContext &context);
    std::expected<void, std::string> refresh_device_state(system::core::AppContext &context);
    std::expected<void, std::string> refresh_my_device_state(system::core::AppContext &context);
    std::expected<void, std::string> refresh_gsp_home_summaries(system::core::AppContext &context);
    void clear_hardware_groups(system::core::AppContext &context);
    std::expected<void, std::string> refresh_display_state(system::core::AppContext &context);
    std::expected<void, std::string> refresh_sound_state(system::core::AppContext &context);
    std::expected<void, std::string> load_audio_state();
    std::expected<void, std::string> drain_sound_events(system::core::AppContext &context);
    std::expected<void, std::string> play_test_sound(system::core::AppContext &context);
    std::expected<void, std::string> start_gsp_pages(system::core::AppContext &context);
    std::expected<void, std::string> handle_gsp_action(
        system::core::AppContext &context,
        std::string_view action
    );
    void load_backlight_state();
    void shutdown_brightness_callbacks();
    void apply_scheduled_brightness(system::core::AppContext &context, uint64_t timer_id);
    std::string page_title_text(std::string_view page) const;
    std::string brightness_status_text() const;
    std::string brightness_caption_text() const;
    void subscribe_brightness_events();
    void subscribe_device_events();
    void disconnect_device_events();
    void handle_brightness_event(const gui::Event &event);
    void handle_volume_event(const gui::Event &event);
    void handle_mute_event(const gui::Event &event);
    void set_brightness(system::core::AppContext &context, int brightness);
    void set_volume(system::core::AppContext &context, int volume);
    void set_mute(system::core::AppContext &context, bool muted);
    void bind_storage_service();
    void release_storage_service();
    void bind_device_service();
    void release_device_service();
    void bind_display_service();
    void release_display_service();
    void bind_audio_service();
    void release_audio_service();
    void bind_wifi_service();
    void release_wifi_service();
    void bind_sntp_service();
    void release_sntp_service();
    void bind_utils_debug_service();
    void release_utils_debug_service();
    void refresh_utils_debug_capabilities();
    void subscribe_sntp_events();
    void disconnect_sntp_events();
    std::expected<void, std::string> refresh_time_zone_state(system::core::AppContext &context);
    std::expected<void, std::string> refresh_time_zone_page_state(system::core::AppContext &context);
    std::expected<void, std::string> set_time_zone(
        system::core::AppContext &context,
        std::string_view timezone
    );
    DebugUiState load_debug_preferences();
    std::expected<void, std::string> save_debug_preference(std::string_view key, bool value);
    std::expected<void, std::string> save_debug_preference(std::string_view key, int value);
    std::expected<void, std::string> refresh_debug_state(system::core::AppContext &context);
    std::expected<void, std::string> push_debug_config_to_utils();
    void handle_debug_device_name_click(system::core::AppContext &context);
    void reset_debug_entry_click_state();
    void handle_debug_switch_event(const gui::Event &event);
    void handle_debug_slider_event(const gui::Event &event);
    bool load_wifi_enabled_preference();
    std::expected<void, std::string> submit_wifi_enabled_preference_save(
        bool enabled,
        service::ServiceBase::FunctionResultHandler handler = nullptr
    );
    void apply_wifi_enabled_preference_to_service();
    std::expected<void, std::string> open_wifi_page(system::core::AppContext &context);
    void close_wifi_page(system::core::AppContext &context);
    void subscribe_wifi_page_events();
    std::expected<void, std::string> sync_wifi_page(system::core::AppContext &context);
    std::expected<void, std::string> render_wifi_page(system::core::AppContext &context);
    std::expected<void, std::string> scan_wifi_page(system::core::AppContext &context);
    std::expected<void, std::string> toggle_wifi_page(system::core::AppContext &context);
    std::expected<void, std::string> forget_wifi_page_network(system::core::AppContext &context);
    std::expected<void, std::string> select_wifi_page_network(system::core::AppContext &context, size_t row);
    void cancel_wifi_page_selection();
    std::expected<void, std::string> back_wifi_connect(system::core::AppContext &context);
    std::expected<void, std::string> read_wifi_page_state();
    void stop_wifi_page_scan();
    void subscribe_wifi_events();
    void disconnect_wifi_events();
    void request_saved_wifi_networks_refresh();
    bool is_saved_wifi_ssid(std::string_view ssid) const;
    void remove_unavailable_wifi_networks_from_available_list();
    void rebuild_available_wifi_networks();
    std::expected<void, std::string> refresh_wifi_service_state(system::core::AppContext &context);
    std::expected<void, std::string> refresh_wifi_page_state(system::core::AppContext &context);
    std::expected<void, std::string> refresh_wifi_lists(system::core::AppContext &context);
    std::expected<void, std::string> start_wifi_scan(system::core::AppContext &context);
    std::expected<void, std::string> start_wifi_scan(system::core::AppContext &context, bool allow_interrupted_retry);
    void cancel_wifi_scan_retry_timer(system::core::AppContext &context);
    void schedule_wifi_scan_retry(system::core::AppContext &context);
    void stop_wifi_scan_after_first_result();
    std::expected<void, std::string> connect_wifi_network(
        system::core::AppContext &context,
        const WifiNetworkState &network,
        std::string_view password
    );
    void invalidate_wifi_connect_operation(system::core::AppContext &context);
    bool sync_wifi_connect_operation(system::core::AppContext &context);
    std::expected<void, std::string> disconnect_wifi(system::core::AppContext &context);
    std::expected<void, std::string> disconnect_current_wifi(system::core::AppContext &context);
    void reset_available_wifi_scan_visibility();
    void cancel_wifi_connected_hide_timer(system::core::AppContext &context);
    void schedule_wifi_connected_hide_timer(system::core::AppContext &context);
    void cancel_wifi_connected_scroll_timer(system::core::AppContext &context);
    void schedule_wifi_connected_card_scroll(system::core::AppContext &context);
    void show_wifi_connected_card(
        WifiConnectedCardState state,
        std::string ssid
    );
    void hide_wifi_connected_card();
    bool is_wifi_connected_card_visible() const;
    void sync_wifi_connected_card_from_service_state();
    std::expected<void, std::string> scroll_wifi_connected_card_to_top(system::core::AppContext &context);
    void handle_wifi_general_event(std::string_view event, bool unexpected);
    void handle_wifi_scan_state_changed(bool running);
    void handle_wifi_scan_ap_infos_updated(const boost::json::array &ap_infos_json);
    std::expected<void, std::string> populate_language_options(system::core::AppContext &context);
    void clear_language_options(system::core::AppContext &context);
    void handle_language_event(const gui::Event &event);
    std::expected<void, std::string> schedule_language_switch(
        system::core::AppContext &context,
        std::string_view locale
    );
    std::expected<void, std::string> schedule_theme_switch(
        system::core::AppContext &context,
        std::string_view theme_id
    );
    enum class RestartPromptKind {
        None,
        Language,
        Theme,
        Failure,
    };
    std::expected<void, std::string> show_restart_prompt(
        system::core::AppContext &context,
        RestartPromptKind kind
    );
    void handle_restart_dialog_result(
        system::core::AppContext &context,
        RestartPromptKind kind,
        const system::core::MessageDialogResult &result
    );
    void show_restart_failure(system::core::AppContext &context, std::string reason);
    void show_preference_save_failure(
        system::core::AppContext &context,
        bool language,
        std::string reason
    );
    system::core::MessageDialogOptions make_message_dialog_options(
        std::string text,
        system::core::MessageDialogIcon icon,
        int32_t auto_close_ms,
        std::vector<system::core::MessageDialogButton> buttons = {}
    ) const;
    void ensure_message_dialog(
        system::core::AppContext &context,
        std::string text,
        system::core::MessageDialogIcon icon,
        int32_t auto_close_ms,
        std::vector<system::core::MessageDialogButton> buttons = {}
    );
    void update_message_dialog_if_visible(
        system::core::AppContext &context,
        std::string text,
        system::core::MessageDialogIcon icon,
        int32_t auto_close_ms,
        std::vector<system::core::MessageDialogButton> buttons = {}
    );
    void hide_message_dialog_if_visible(system::core::AppContext &context);
    void clamp_wifi_list_pages();
    std::expected<void, std::string> populate_wifi_networks(system::core::AppContext &context);
    std::expected<void, std::string> sync_wifi_network_section(
        system::core::AppContext &context,
        std::string_view parent,
        std::string_view id_prefix,
        const std::vector<WifiNetworkState> &networks,
        size_t page_start,
        size_t visible_count,
        size_t &slot_count
    );
    void clear_wifi_networks(system::core::AppContext &context);
    void update_wifi_refresh_animation(system::core::AppContext &context);
    void handle_wifi_select_event(const gui::Event &event);
    void handle_wifi_forget_event(const gui::Event &event);
    std::expected<void, std::string> open_wifi_connect(
        system::core::AppContext &context,
        WifiNetworkState network
    );
    std::expected<void, std::string> refresh_wifi_connect_state(system::core::AppContext &context);
    std::expected<void, std::string> refresh_wifi_summary_state(system::core::AppContext &context);
    std::expected<void, std::string> show_wifi_password_keyboard(system::core::AppContext &context);
    void handle_wifi_keyboard_result(const system::core::KeyboardResult &result);
    std::expected<void, std::string> submit_wifi_connect(system::core::AppContext &context);
    void cancel_wifi_keyboard_if_active(system::core::AppContext &context);

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

class SettingsAppProvider final: public system::core::IAppProvider {
public:
    /**
     * @brief Get the manifest for the built-in settings app.
     *
     * @return Settings application manifest.
     */
    system::core::AppManifest get_manifest() const override;

    /**
     * @brief Create a settings application instance.
     *
     * @return Shared application instance.
     */
    std::shared_ptr<system::core::IApp> create_app() override;
};

} // namespace esp_brookesia::app::settings
