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
#include "private/system_constants.hpp"
#include "brookesia/service_display.hpp"
#include "brookesia/service_helper/network/sntp.hpp"
#include "brookesia/service_wifi.hpp"

#include <algorithm>
#include <atomic>
#include <array>
#include <chrono>
#include <ctime>
#include <cstdlib>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#if BROOKESIA_SYSTEM_SUPER_ENABLE_PROFILE_LOG
#   define SYSTEM_SUPER_PROFILE_LOGI(...) BROOKESIA_LOGI(__VA_ARGS__)
#else
#   define SYSTEM_SUPER_PROFILE_LOGI(...) do { if (false) { BROOKESIA_LOGI(__VA_ARGS__); } } while (0)
#endif

namespace esp_brookesia::system::super {
namespace {

inline constexpr const char *SUPER_GUI_DISPLAY_SOURCE_ROLE = "gui";
using DisplayHelper = service::helper::Display;
using SNTPHelper = service::helper::SNTP;
using WifiHelper = service::helper::Wifi;
using SteadyClock = std::chrono::steady_clock;
using SteadyTimePoint = SteadyClock::time_point;

int64_t elapsed_ms_since(SteadyTimePoint started_at, SteadyTimePoint ended_at = SteadyClock::now())
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(ended_at - started_at).count();
}

struct WifiStatusState {
    bool visible = false;
    bool connected = false;
};

WifiStatusState get_wifi_status_from_state(std::string_view state)
{
    if (state == BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralState::Connected)) {
        return {
            .visible = true,
            .connected = true,
        };
    }
    if (state == BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralState::Started) ||
            state == BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralState::Connecting) ||
            state == BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralState::Disconnecting)) {
        return {
            .visible = true,
            .connected = false,
        };
    }
    return {};
}

WifiStatusState get_wifi_status_from_event(std::string_view event)
{
    if (event == BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralEvent::Connected)) {
        return {
            .visible = true,
            .connected = true,
        };
    }
    if (event == BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralEvent::Started) ||
            event == BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralEvent::Disconnected)) {
        return {
            .visible = true,
            .connected = false,
        };
    }
    return {};
}

struct AnimationCompletionBarrier {
    explicit AnimationCompletionBarrier(std::function<void()> completed_handler_in)
        : completed_handler(std::move(completed_handler_in))
    {
    }

    void add()
    {
        remaining.fetch_add(1, std::memory_order_relaxed);
    }

    void complete()
    {
        if (remaining.fetch_sub(1, std::memory_order_acq_rel) == 1 && armed.load(std::memory_order_acquire)) {
            fire();
        }
    }

    void arm()
    {
        armed.store(true, std::memory_order_release);
        if (remaining.load(std::memory_order_acquire) == 0) {
            fire();
        }
    }

    void fire()
    {
        bool expected = false;
        if (!fired.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
            return;
        }
        if (completed_handler) {
            completed_handler();
        }
    }

    std::atomic<int32_t> remaining = 0;
    std::atomic<bool> armed = false;
    std::atomic<bool> fired = false;
    std::function<void()> completed_handler;
};

gui::Animation make_position_animation(gui::AnimationProperty property, int32_t to)
{
    return gui::Animation{
        .id = {},
        .trigger = gui::AnimationTrigger::Manual,
        .property = property,
        .from_mode = gui::AnimationValueMode::Current,
        .from = 0,
        .to_mode = gui::AnimationValueMode::Absolute,
        .to = to,
        .duration = SUPER_SYSTEM_UI_ANIMATION_MS,
        .delay = 0,
        .easing = gui::AnimationEasing::EaseOut,
        .repeat = 0,
        .playback = false,
    };
}

gui::Animation make_timed_animation(gui::AnimationProperty property, int32_t to, int32_t duration_ms)
{
    return gui::Animation{
        .id = {},
        .trigger = gui::AnimationTrigger::Manual,
        .property = property,
        .from_mode = gui::AnimationValueMode::Current,
        .from = 0,
        .to_mode = gui::AnimationValueMode::Absolute,
        .to = to,
        .duration = duration_ms,
        .delay = 0,
        .easing = gui::AnimationEasing::EaseOut,
        .repeat = 0,
        .playback = false,
    };
}

gui::Animation make_modal_animation(gui::AnimationProperty property, int32_t to)
{
    return gui::Animation{
        .id = {},
        .trigger = gui::AnimationTrigger::Manual,
        .property = property,
        .from_mode = gui::AnimationValueMode::Current,
        .from = 0,
        .to_mode = gui::AnimationValueMode::Absolute,
        .to = to,
        .duration = SUPER_APP_LAUNCH_ANIMATION_MS,
        .delay = 0,
        .easing = gui::AnimationEasing::EaseOut,
        .repeat = 0,
        .playback = false,
    };
}

gui::Animation make_keyboard_animation(gui::AnimationProperty property, int32_t to)
{
    return gui::Animation{
        .id = {},
        .trigger = gui::AnimationTrigger::Manual,
        .property = property,
        .from_mode = gui::AnimationValueMode::Current,
        .from = 0,
        .to_mode = gui::AnimationValueMode::Absolute,
        .to = to,
        .duration = SUPER_KEYBOARD_ANIMATION_MS,
        .delay = 0,
        .easing = gui::AnimationEasing::EaseOut,
        .repeat = 0,
        .playback = false,
    };
}

gui::Animation make_keyboard_animation(gui::AnimationProperty property, int32_t from, int32_t to)
{
    return gui::Animation{
        .id = {},
        .trigger = gui::AnimationTrigger::Manual,
        .property = property,
        .from_mode = gui::AnimationValueMode::Absolute,
        .from = from,
        .to_mode = gui::AnimationValueMode::Absolute,
        .to = to,
        .duration = SUPER_KEYBOARD_ANIMATION_MS,
        .delay = 0,
        .easing = gui::AnimationEasing::EaseOut,
        .repeat = 0,
        .playback = false,
    };
}

std::string bool_to_binding(bool value)
{
    return value ? "true" : "false";
}

std::string make_clock_text()
{
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm local_time{};
#if defined(_WIN32)
    localtime_s(&local_time, &time);
#else
    localtime_r(&time, &local_time);
#endif
    char buffer[8] = {};
    if (std::strftime(buffer, sizeof(buffer), "%H:%M", &local_time) == 0) {
        return "--:--";
    }
    return buffer;
}

bool has_gesture_area(uint8_t areas, DisplayHelper::TouchGestureArea area)
{
    return (areas & static_cast<uint8_t>(area)) != 0;
}

int32_t get_gesture_exit_distance_px(const gui::Environment &environment)
{
    return std::clamp<int32_t>(std::max<int32_t>(environment.height_px / 5, 1), 80, 140);
}

int32_t get_status_peek_distance_px(const gui::Environment &environment)
{
    return std::clamp<int32_t>(std::max<int32_t>(environment.height_px / 10, 1), 36, 72);
}

uint16_t get_gesture_vertical_edge_px(uint32_t height)
{
    return static_cast<uint16_t>(std::clamp<uint32_t>(std::max<uint32_t>(height * 8 / 100, 24), 24, 96));
}

uint16_t get_gesture_horizontal_edge_px(uint32_t width)
{
    return static_cast<uint16_t>(std::clamp<uint32_t>(std::max<uint32_t>(width * 6 / 100, 24), 24, 96));
}

int32_t get_fallback_gesture_indicator_width(const gui::Environment &environment)
{
    return std::clamp<int32_t>(std::max<int32_t>(environment.width_px / 5, 1), 96, 180);
}

std::string message_dialog_icon_src(core::MessageDialogIcon icon)
{
    switch (icon) {
    case core::MessageDialogIcon::Information:
        return SUPER_MESSAGE_DIALOG_INFORMATION_IMAGE_ID;
    case core::MessageDialogIcon::Question:
        return SUPER_MESSAGE_DIALOG_QUESTION_IMAGE_ID;
    case core::MessageDialogIcon::Warning:
        return SUPER_MESSAGE_DIALOG_WARNING_IMAGE_ID;
    case core::MessageDialogIcon::Critical:
        return SUPER_MESSAGE_DIALOG_CRITICAL_IMAGE_ID;
    case core::MessageDialogIcon::None:
    default:
        return {};
    }
}

std::string message_dialog_icon_recolor(core::MessageDialogIcon icon)
{
    switch (icon) {
    case core::MessageDialogIcon::Information:
        return "#2563eb";
    case core::MessageDialogIcon::Question:
        return "#7c3aed";
    case core::MessageDialogIcon::Warning:
        return "#f59e0b";
    case core::MessageDialogIcon::Critical:
        return "#dc2626";
    case core::MessageDialogIcon::None:
    default:
        return {};
    }
}

std::string int_to_binding(int32_t value)
{
    return std::to_string(value);
}

int32_t get_centered_gesture_bar_x(int32_t max_width, int32_t width)
{
    return std::max<int32_t>((std::max<int32_t>(max_width, width) - width) / 2, 0);
}

void add_gesture_indicator_binding_updates(
    std::vector<gui::BindingValueUpdate> &updates,
    bool hidden,
    int32_t max_width,
    int32_t width
)
{
    const auto safe_max_width = std::max<int32_t>(max_width, 1);
    const auto clamped_width = std::clamp<int32_t>(width, 1, safe_max_width);
    add_binding_update(
        updates,
        SUPER_GESTURE_INDICATOR_PATH,
        "gesture_indicator_hidden",
        bool_to_binding(hidden)
    );
    add_binding_update(
        updates,
        SUPER_GESTURE_INDICATOR_BAR_PATH,
        "gesture_bar_x",
        int_to_binding(get_centered_gesture_bar_x(safe_max_width, clamped_width))
    );
    add_binding_update(
        updates,
        SUPER_GESTURE_INDICATOR_BAR_PATH,
        "gesture_bar_width",
        int_to_binding(clamped_width)
    );
}

gui::ViewFrame make_fallback_origin(const gui::Environment &environment)
{
    const auto icon_size = std::max<int32_t>(SUPER_APP_LAUNCH_FINAL_ICON_SIZE, 1);
    const auto width = std::max(environment.width_px, icon_size);
    const auto height = std::max(environment.height_px, icon_size);
    return gui::ViewFrame{
        .x = (width - icon_size) / 2,
        .y = (height - icon_size) / 2,
        .width = icon_size,
        .height = icon_size,
    };
}

int32_t get_collapsed_status_bar_y(core::AppContext &context)
{
    auto frame = context.gui().get_view_frame(SUPER_STATUS_BAR_PATH);
    if (frame && frame->height > 0) {
        return -std::max(std::abs(SUPER_STATUS_BAR_COLLAPSED_Y), frame->height * 2);
    }
    return SUPER_STATUS_BAR_COLLAPSED_Y;
}

bool is_supported_keyboard_mode(std::string_view mode)
{
    return mode == "text" || mode == "upper" || mode == "number" || mode == "special";
}

std::vector<std::string> default_keyboard_modes()
{
    return {"text", "upper", "number", "special"};
}

std::string join_keyboard_modes(const std::vector<std::string> &modes)
{
    std::string result;
    for (size_t i = 0; i < modes.size(); ++i) {
        if (i > 0) {
            result.push_back(',');
        }
        result += modes[i];
    }
    return result;
}

std::expected<core::KeyboardRequestOptions, std::string> normalize_keyboard_options(
    core::KeyboardRequestOptions options)
{
    if (!is_supported_keyboard_mode(options.mode)) {
        return std::unexpected("Keyboard mode must be one of: text, upper, number, special");
    }
    if (options.allowed_modes.empty()) {
        options.allowed_modes = default_keyboard_modes();
    }
    for (const auto &mode : options.allowed_modes) {
        if (!is_supported_keyboard_mode(mode)) {
            return std::unexpected("Keyboard allowed mode must be one of: text, upper, number, special");
        }
        if (std::count(options.allowed_modes.begin(), options.allowed_modes.end(), mode) > 1) {
            return std::unexpected("Keyboard allowed modes must not contain duplicates");
        }
    }
    if (std::find(options.allowed_modes.begin(), options.allowed_modes.end(), options.mode) ==
            options.allowed_modes.end()) {
        options.mode = options.allowed_modes.front();
    }
    return options;
}

} // namespace

std::expected<void, std::string> ShellApp::mount_background(core::AppContext &context)
{
    (void)context;
    background_mounted_ = true;
    return {};
}

void ShellApp::unmount_background()
{
    if (context_ == nullptr || !background_mounted_) {
        return;
    }
    background_mounted_ = false;
}

std::expected<void, std::string> ShellApp::mount_overlay(core::AppContext &context)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    (void)context;
    overlay_mounted_ = true;
    system_ui_expanded_ = true;
    foreground_is_shell_ = true;

    if (overlay_action_connections_.empty()) {
        auto subscribe_mask_action = [this, &context](std::string_view action) -> std::expected<void, std::string> {
            auto connection = context.gui().subscribe_action(action, [](const gui::Event &)
            {
            });
            if (!connection.connected())
            {
                return std::unexpected("Failed to subscribe Shell system UI mask action");
            }
            overlay_action_connections_.push_back(std::move(connection));
            return {};
        };
        if (auto result = subscribe_mask_action(SUPER_ACTION_SYSTEM_UI_MASK_CLICK); !result) {
            return result;
        }
        if (auto result = subscribe_mask_action(SUPER_ACTION_SYSTEM_UI_MASK_GESTURE); !result) {
            return result;
        }
    }

    refresh_status_clock();
    schedule_status_clock_timer();
    refresh_wifi_status();
    subscribe_sntp_events();
    (void)configure_display_gesture();
    apply_debug_config(get_debug_config_snapshot());
    return refresh_system_ui_bindings();
}

void ShellApp::unmount_overlay()
{
    stop_debug_runtime();
    disconnect_overlay_actions();
    disconnect_display_gesture();
    release_sntp_service_binding();
    release_wifi_service_binding();
    cancel_gesture_exit_hold_timer();
    cancel_status_peek_auto_hide_timer();
    stop_status_clock_timer();
    if (context_ == nullptr) {
        return;
    }
    if (status_bar_animation_id_ != 0) {
        context_->gui().stop_animation(status_bar_animation_id_);
        status_bar_animation_id_ = 0;
    }
    if (gesture_indicator_animation_id_ != 0) {
        context_->gui().stop_animation(gesture_indicator_animation_id_);
        gesture_indicator_animation_id_ = 0;
    }
    if (gesture_indicator_x_animation_id_ != 0) {
        context_->gui().stop_animation(gesture_indicator_x_animation_id_);
        gesture_indicator_x_animation_id_ = 0;
    }
    loading_owners_.clear();
    launch_overlay_active_ = false;
    overlay_mounted_ = false;
}

bool ShellApp::ensure_display_service_binding()
{
    if (display_service_binding_.is_valid()) {
        return true;
    }
    if (!service::Display::Helper::is_available()) {
        BROOKESIA_LOGW("Display service is not available for Shell active source control");
        return false;
    }
    display_service_binding_ = service::ServiceManager::get_instance().bind(
                                   service::Display::Helper::get_name().data()
                               );
    if (!display_service_binding_.is_valid()) {
        BROOKESIA_LOGW("Failed to bind Display service for Shell active source control");
        return false;
    }
    return true;
}

void ShellApp::release_display_service_binding()
{
    display_source_restore_records_.clear();
    display_service_binding_.release();
}

bool ShellApp::configure_display_gesture()
{
    disconnect_display_gesture();
    if (!ensure_display_service_binding()) {
        return false;
    }

    auto gui_output_names = get_gui_display_output_names();
    auto &display = service::Display::get_instance();
    auto outputs = display.get_outputs();
    bool configured = false;
    for (const auto &output : outputs) {
        if (!output.touch.has_value()) {
            continue;
        }
        if (!gui_output_names.empty() &&
                std::find(gui_output_names.begin(), gui_output_names.end(), output.name) == gui_output_names.end()) {
            continue;
        }

        DisplayHelper::TouchGestureConfig config;
        config.enabled = true;
        config.detect_period_ms = 20;
        config.direction_lock_enabled = true;
        config.release_debounce_ms = 40;
        config.threshold.horizontal_edge = get_gesture_horizontal_edge_px(output.width);
        config.threshold.vertical_edge = get_gesture_vertical_edge_px(output.height);
        auto config_json = BROOKESIA_DESCRIBE_TO_JSON(config).as_object();
        auto result = DisplayHelper::call_function_sync(
                          DisplayHelper::FunctionId::SetTouchGestureConfig,
                          static_cast<double>(output.id),
                          config_json,
                          service::helper::Timeout(500)
                      );
        if (!result) {
            BROOKESIA_LOGW(
                "Failed to configure Shell bottom gesture for Display output %1%: %2%",
                output.name, result.error()
            );
            continue;
        }
        configured = true;
    }

    if (!configured) {
        BROOKESIA_LOGW("No touch-capable Display output is available for Shell bottom gesture");
        return false;
    }

    const auto gesture_slot = [this](const std::string &, const boost::json::object & info_json) {
        handle_display_touch_gesture(info_json);
    };
    display_gesture_connection_ = DisplayHelper::subscribe_event(DisplayHelper::EventId::TouchGesture, gesture_slot);
    if (!display_gesture_connection_.connected()) {
        BROOKESIA_LOGW("Failed to subscribe Display touch gesture event for Shell");
        return false;
    }
    return true;
}

void ShellApp::disconnect_display_gesture()
{
    display_gesture_connection_.disconnect();
    reset_gesture_indicator();
}

bool ShellApp::ensure_wifi_service_binding()
{
    if (wifi_service_binding_.is_valid()) {
        if (WifiHelper::is_running()) {
            return true;
        }
        ++wifi_status_generation_;
        set_status_wifi_state(false, false);
        return false;
    }
    if (!WifiHelper::is_available()) {
        ++wifi_status_generation_;
        set_status_wifi_state(false, false);
        return false;
    }
    wifi_service_binding_ = service::ServiceManager::get_instance().bind(WifiHelper::get_name().data());
    if (!wifi_service_binding_.is_valid()) {
        BROOKESIA_LOGW("Failed to bind Wi-Fi service for Shell status");
        ++wifi_status_generation_;
        set_status_wifi_state(false, false);
        return false;
    }
    if (!WifiHelper::is_running()) {
        ++wifi_status_generation_;
        wifi_service_binding_.release();
        set_status_wifi_state(false, false);
        return false;
    }

    if (!wifi_event_connection_.connected()) {
        const auto general_callback = [this](const std::string &, const std::string & event, bool unexpected) {
            if (unexpected) {
                BROOKESIA_LOGW("Wi-Fi service reported unexpected event for Shell status: %1%", event);
            }
            ++wifi_status_generation_;
            const auto status = get_wifi_status_from_event(event);
            set_status_wifi_state(status.visible, status.connected);
        };
        wifi_event_connection_ = WifiHelper::subscribe_event(
                                     WifiHelper::EventId::GeneralEventHappened,
                                     general_callback
                                 );
        if (!wifi_event_connection_.connected()) {
            BROOKESIA_LOGW("Failed to subscribe Wi-Fi general event for Shell status");
        }
    }
    return true;
}

void ShellApp::release_wifi_service_binding()
{
    ++wifi_status_generation_;
    wifi_event_connection_.disconnect();
    wifi_service_binding_.release();
    wifi_connected_ = false;
    set_status_wifi_state(false, false);
}

void ShellApp::refresh_wifi_status()
{
    if (!ensure_wifi_service_binding()) {
        return;
    }

    const auto generation = ++wifi_status_generation_;
    const auto state_handler = [this, generation](service::FunctionResult && result) {
        if (context_ == nullptr || generation != wifi_status_generation_ || !wifi_service_binding_.is_valid()) {
            return;
        }
        if (!result.success || !result.has_data()) {
            set_status_wifi_state(true, false);
            return;
        }
        const auto &state = result.get_data<std::string>();
        const auto status = get_wifi_status_from_state(state);
        set_status_wifi_state(status.visible, status.connected);
    };
    if (!WifiHelper::call_function_async(WifiHelper::FunctionId::GetGeneralState, state_handler)) {
        BROOKESIA_LOGW("Failed to submit Wi-Fi state request for Shell status");
        set_status_wifi_state(true, false);
    }
}

void ShellApp::set_status_wifi_state(bool visible, bool connected)
{
    wifi_connected_ = connected;
    if (context_ == nullptr) {
        return;
    }
    std::vector<gui::BindingValueUpdate> updates;
    updates.push_back(gui::BindingValueUpdate{
        .absolute_path = SUPER_STATUS_WIFI_PATH,
        .key = "wifi_hidden",
        .value = bool_to_binding(!visible),
    });
    updates.push_back(gui::BindingValueUpdate{
        .absolute_path = SUPER_STATUS_WIFI_PATH,
        .key = "wifi_bg",
        .value = connected ? "${color.success.fill}" : "${color.border.strong}",
    });
    updates.push_back(gui::BindingValueUpdate{
        .absolute_path = std::string(SUPER_STATUS_WIFI_PATH) + "/label",
        .key = "wifi_text",
        .value = connected ? "${color.success.on}" : "${color.text.inverse}",
    });
    auto result = context_->gui().set_binding_values(updates);
    if (!result) {
        BROOKESIA_LOGW("Failed to refresh Shell Wi-Fi status icon: %1%", result.error());
    }
}

bool ShellApp::ensure_sntp_service_binding()
{
    if (sntp_service_binding_.is_valid()) {
        return true;
    }
    if (!SNTPHelper::is_available()) {
        BROOKESIA_LOGD("SNTP service is not available for Shell status clock updates");
        return false;
    }

    sntp_service_binding_ = service::ServiceManager::get_instance().bind(SNTPHelper::get_name().data());
    if (!sntp_service_binding_.is_valid()) {
        BROOKESIA_LOGW("Failed to bind SNTP service for Shell status clock updates");
        return false;
    }
    return true;
}

void ShellApp::release_sntp_service_binding()
{
    disconnect_sntp_events();
    sntp_service_binding_.release();
}

void ShellApp::subscribe_sntp_events()
{
    disconnect_sntp_events();
    if (!ensure_sntp_service_binding()) {
        return;
    }

    const auto timezone_callback = [this](const std::string &, const service::EventItemMap &) {
        BROOKESIA_LOGI("SNTP timezone changed, refresh Shell status clock");
        refresh_status_clock();
        stop_status_clock_timer();
        schedule_status_clock_timer();
    };
    sntp_event_connection_ = SNTPHelper::subscribe_event(
                                 SNTPHelper::EventId::TimezoneChanged,
                                 timezone_callback
                             );
    if (!sntp_event_connection_.connected()) {
        BROOKESIA_LOGW("Failed to subscribe SNTP timezone event for Shell status clock");
    }
}

void ShellApp::disconnect_sntp_events()
{
    sntp_event_connection_.disconnect();
}

void ShellApp::refresh_status_clock()
{
    if (context_ == nullptr) {
        return;
    }
    auto result = context_->gui().set_binding_value(SUPER_STATUS_CLOCK_PATH, "clock_text", make_clock_text());
    if (!result) {
        BROOKESIA_LOGW("Failed to refresh Shell status clock: %1%", result.error());
    }
}

void ShellApp::schedule_status_clock_timer()
{
    if (context_ == nullptr || status_clock_timer_id_ != core::INVALID_TIMER_ID) {
        return;
    }
    auto timer = context_->timer().start_delayed(SUPER_STATUS_CLOCK_TIMER_NAME, 1000);
    if (!timer) {
        BROOKESIA_LOGW("Failed to start Shell status clock timer: %1%", timer.error());
        return;
    }
    status_clock_timer_id_ = *timer;
}

void ShellApp::stop_status_clock_timer()
{
    if (context_ != nullptr && status_clock_timer_id_ != core::INVALID_TIMER_ID) {
        (void)context_->timer().stop(status_clock_timer_id_);
    }
    status_clock_timer_id_ = core::INVALID_TIMER_ID;
}

std::vector<std::string> ShellApp::get_gui_display_output_names() const
{
    auto &display = service::Display::get_instance();
    std::vector<std::string> output_names;
    auto sources = display.get_sources();
    auto source_it = std::find_if(sources.begin(), sources.end(), [](const auto & source) {
        return source.role == SUPER_GUI_DISPLAY_SOURCE_ROLE;
    });
    if (source_it != sources.end()) {
        output_names = source_it->preferred_outputs;
    }

    if (output_names.empty()) {
        auto outputs = display.get_outputs();
        if (!outputs.empty()) {
            output_names.push_back(outputs.front().name);
        }
    }
    return output_names;
}

bool ShellApp::is_display_source_available(std::string_view source_name) const
{
    auto sources = service::Display::get_instance().get_sources();
    return std::find_if(sources.begin(), sources.end(), [source_name](const auto & source) {
        return source.name == source_name;
    }) != sources.end();
}

bool ShellApp::is_display_source_role_available(std::string_view role) const
{
    auto sources = service::Display::get_instance().get_sources();
    return std::find_if(sources.begin(), sources.end(), [role](const auto & source) {
        return source.role == role;
    }) != sources.end();
}

void ShellApp::switch_display_to_gui_for_system_ui()
{
    if (!ensure_display_service_binding()) {
        return;
    }
    if (!is_display_source_role_available(SUPER_GUI_DISPLAY_SOURCE_ROLE)) {
        BROOKESIA_LOGW(
            "Skip switching Display to GUI: source role %1% is not registered", SUPER_GUI_DISPLAY_SOURCE_ROLE
        );
        return;
    }

    display_source_restore_records_.clear();
    auto output_names = get_gui_display_output_names();
    if (output_names.empty()) {
        BROOKESIA_LOGW("Skip switching Display to GUI: no output is available");
        return;
    }

    auto &display = service::Display::get_instance();
    for (const auto &output_name : output_names) {
        auto active_source = display.get_active_source(output_name);
        if (!active_source) {
            BROOKESIA_LOGW(
                "Failed to read active Display source before showing system UI: output(%1%), error(%2%)",
                output_name, active_source.error()
            );
            continue;
        }
        auto active_role = display.get_active_role(output_name);
        if (!active_role) {
            BROOKESIA_LOGW(
                "Failed to read active Display role before showing system UI: output(%1%), error(%2%)",
                output_name, active_role.error()
            );
            continue;
        }

        if (!foreground_is_shell_ && !active_source->empty() && (*active_role != SUPER_GUI_DISPLAY_SOURCE_ROLE)) {
            display_source_restore_records_.push_back(DisplaySourceRestoreRecord{
                .output_name = output_name,
                .source_name = *active_source,
            });
        }

        if (*active_role == SUPER_GUI_DISPLAY_SOURCE_ROLE) {
            continue;
        }

        auto result = display.set_active_source_role(output_name, SUPER_GUI_DISPLAY_SOURCE_ROLE);
        if (!result) {
            BROOKESIA_LOGW(
                "Failed to switch Display output %1% active source role to %2% for system UI: %3%",
                output_name, SUPER_GUI_DISPLAY_SOURCE_ROLE, result.error()
            );
            continue;
        }
        BROOKESIA_LOGI(
            "Switch Display output %1% active source role to %2% for system UI",
            output_name, SUPER_GUI_DISPLAY_SOURCE_ROLE
        );
    }
}

void ShellApp::restore_display_after_system_ui()
{
    if (display_source_restore_records_.empty()) {
        return;
    }
    auto restore_records = std::move(display_source_restore_records_);
    display_source_restore_records_.clear();
    if (!ensure_display_service_binding()) {
        return;
    }

    auto &display = service::Display::get_instance();
    for (const auto &record : restore_records) {
        if (!is_display_source_available(record.source_name)) {
            BROOKESIA_LOGW(
                "Skip restoring Display output %1% source %2%: source is not registered",
                record.output_name, record.source_name
            );
            continue;
        }

        auto result = display.set_active_source(record.output_name, record.source_name);
        if (!result) {
            BROOKESIA_LOGW(
                "Failed to restore Display output %1% active source to %2%: %3%",
                record.output_name, record.source_name, result.error()
            );
            continue;
        }
        BROOKESIA_LOGI(
            "Restore Display output %1% active source to %2%",
            record.output_name, record.source_name
        );
    }
}

void ShellApp::clear_display_source_restore_records()
{
    display_source_restore_records_.clear();
}

void ShellApp::handle_display_touch_gesture(const boost::json::object &info_json)
{
    if (context_ == nullptr || foreground_is_shell_) {
        return;
    }
    if (message_dialog_mounted_ || message_dialog_closing_) {
        if (gesture_tracking_ || gesture_exit_armed_ || gesture_exit_triggered_ || status_peek_tracking_) {
            reset_gesture_indicator();
            status_peek_tracking_ = false;
            (void)refresh_system_ui_state_bindings();
        }
        return;
    }

    DisplayHelper::TouchGestureInfo info;
    if (!BROOKESIA_DESCRIBE_FROM_JSON(boost::json::value(info_json), info)) {
        BROOKESIA_LOGW("Failed to parse Display touch gesture event for Shell");
        return;
    }

    const bool started_from_bottom = has_gesture_area(info.start_area, DisplayHelper::TouchGestureArea::BottomEdge);
    const bool started_from_top = has_gesture_area(info.start_area, DisplayHelper::TouchGestureArea::TopEdge);
    if (info.event_type == DisplayHelper::TouchGestureEventType::Press) {
        if (started_from_top && can_start_status_peek()) {
            ++status_peek_generation_;
            status_peek_tracking_ = true;
            (void)refresh_system_ui_state_bindings();
            if (!started_from_bottom) {
                return;
            }
        }
        if (!started_from_bottom) {
            return;
        }
        reset_status_peek_session(false, true, "before bottom gesture");
        ++gesture_generation_;
        gesture_tracking_ = true;
        gesture_exit_armed_ = false;
        gesture_exit_triggered_ = false;
        (void)refresh_system_ui_state_bindings();
        cancel_gesture_exit_hold_timer();
        switch_display_to_gui_for_system_ui();
        if (gesture_indicator_max_width_ <= 0) {
            gesture_indicator_max_width_ = get_fallback_gesture_indicator_width(owner_.get_environment());
        }
        update_gesture_indicator_width(std::max<int32_t>(gesture_indicator_max_width_, 1));
        return;
    }

    if (status_peek_tracking_) {
        if (gesture_tracking_ || gesture_exit_armed_ || gesture_exit_triggered_) {
            status_peek_tracking_ = false;
            (void)refresh_system_ui_state_bindings();
            return;
        }
        if (info.event_type == DisplayHelper::TouchGestureEventType::Release) {
            status_peek_tracking_ = false;
            (void)refresh_system_ui_state_bindings();
            return;
        }
        if (info.event_type != DisplayHelper::TouchGestureEventType::Pressing || !started_from_top) {
            status_peek_tracking_ = false;
            (void)refresh_system_ui_state_bindings();
            return;
        }
        const auto downward_delta = std::max(0, info.stop_y - info.start_y);
        const auto peek_distance = std::max<int32_t>(get_status_peek_distance_px(owner_.get_environment()), 1);
        if (downward_delta >= peek_distance &&
                (info.direction == DisplayHelper::TouchGestureDirection::Down ||
                 info.direction == DisplayHelper::TouchGestureDirection::None)) {
            trigger_status_peek();
        }
        return;
    }

    if (!gesture_tracking_) {
        return;
    }

    if (info.event_type == DisplayHelper::TouchGestureEventType::Release) {
        if (!gesture_exit_triggered_) {
            cancel_gesture_exit_hold_timer();
            animate_gesture_indicator_rebound();
        }
        gesture_tracking_ = false;
        (void)refresh_system_ui_state_bindings();
        return;
    }

    if (info.event_type != DisplayHelper::TouchGestureEventType::Pressing) {
        return;
    }
    if (!started_from_bottom) {
        animate_gesture_indicator_rebound();
        gesture_tracking_ = false;
        (void)refresh_system_ui_state_bindings();
        return;
    }

    const auto upward_delta = std::max(0, info.start_y - info.stop_y);
    if (upward_delta <= 0 && info.direction != DisplayHelper::TouchGestureDirection::Up) {
        update_gesture_indicator_width(std::max<int32_t>(gesture_indicator_max_width_, 1));
        return;
    }

    const auto exit_distance = std::max<int32_t>(get_gesture_exit_distance_px(owner_.get_environment()), 1);
    const auto max_width = std::max<int32_t>(gesture_indicator_max_width_, 1);
    const auto progress = std::clamp(static_cast<float>(upward_delta) / static_cast<float>(exit_distance), 0.0F, 1.0F);
    const auto width = static_cast<int32_t>(static_cast<float>(max_width) * (1.0F - progress));
    if (progress < 1.0F) {
        gesture_exit_armed_ = false;
        cancel_gesture_exit_hold_timer();
        (void)refresh_system_ui_state_bindings();
        update_gesture_indicator_width(std::max<int32_t>(width, 1));
        return;
    }

    update_gesture_indicator_width(1);
    std::vector<gui::BindingValueUpdate> updates;
    add_gesture_indicator_binding_updates(
        updates,
        true,
        std::max<int32_t>(gesture_indicator_max_width_, 1),
        1
    );
    auto result = context_->gui().set_binding_values(updates);
    if (!result) {
        BROOKESIA_LOGW("Failed to hide Shell gesture indicator: %1%", result.error());
    }
    if (gesture_exit_armed_ || gesture_exit_hold_timer_id_ != core::INVALID_TIMER_ID) {
        return;
    }
    gesture_exit_armed_ = true;
    (void)refresh_system_ui_state_bindings();
    auto timer = context_->timer().start_delayed(SUPER_GESTURE_EXIT_HOLD_TIMER_NAME, SUPER_GESTURE_EXIT_HOLD_MS);
    if (!timer) {
        BROOKESIA_LOGW("Failed to start Shell gesture exit hold timer: %1%", timer.error());
        animate_gesture_indicator_rebound();
        return;
    }
    gesture_exit_hold_timer_id_ = *timer;
}

void ShellApp::reset_gesture_indicator()
{
    ++gesture_generation_;
    gesture_tracking_ = false;
    gesture_exit_armed_ = false;
    gesture_exit_triggered_ = false;
    cancel_gesture_exit_hold_timer();
    if (context_ == nullptr) {
        gesture_indicator_max_width_ = 0;
        return;
    }
    if (gesture_indicator_animation_id_ != 0) {
        context_->gui().stop_animation(gesture_indicator_animation_id_);
        gesture_indicator_animation_id_ = 0;
    }
    if (gesture_indicator_x_animation_id_ != 0) {
        context_->gui().stop_animation(gesture_indicator_x_animation_id_);
        gesture_indicator_x_animation_id_ = 0;
    }
    if (gesture_indicator_max_width_ <= 0) {
        gesture_indicator_max_width_ = get_fallback_gesture_indicator_width(owner_.get_environment());
    }
    const auto max_width = std::max<int32_t>(gesture_indicator_max_width_, 1);
    std::vector<gui::BindingValueUpdate> updates;
    add_gesture_indicator_binding_updates(updates, true, max_width, max_width);
    auto result = context_->gui().set_binding_values(updates);
    if (!result) {
        BROOKESIA_LOGW("Failed to reset Shell gesture indicator: %1%", result.error());
    }
    (void)refresh_system_ui_state_bindings();
}

void ShellApp::cancel_gesture_exit_hold_timer()
{
    if (context_ != nullptr && gesture_exit_hold_timer_id_ != core::INVALID_TIMER_ID) {
        (void)context_->timer().stop(gesture_exit_hold_timer_id_);
    }
    gesture_exit_hold_timer_id_ = core::INVALID_TIMER_ID;
}

void ShellApp::cancel_status_peek_auto_hide_timer()
{
    if (context_ != nullptr && status_peek_auto_hide_timer_id_ != core::INVALID_TIMER_ID) {
        (void)context_->timer().stop(status_peek_auto_hide_timer_id_);
    }
    status_peek_auto_hide_timer_id_ = core::INVALID_TIMER_ID;
}

bool ShellApp::can_start_status_peek() const
{
    return context_ != nullptr && !foreground_is_shell_ && !launch_overlay_active_ && !message_dialog_mounted_ &&
           !message_dialog_closing_ && !gesture_tracking_ && !gesture_exit_armed_ && !gesture_exit_triggered_;
}

void ShellApp::trigger_status_peek()
{
    if (!can_start_status_peek()) {
        return;
    }
    BROOKESIA_LOGI(
        "Shell top status peek triggered: foreground_is_shell(%1%), tracking(%2%), visible(%3%)",
        foreground_is_shell_, status_peek_tracking_, status_peek_visible_
    );
    status_peek_tracking_ = false;
    status_peek_visible_ = true;
    ++status_peek_generation_;
    cancel_status_peek_auto_hide_timer();

    auto expand_result = set_system_ui_expanded(true);
    if (!expand_result) {
        BROOKESIA_LOGW("Failed to show Shell status peek: %1%", expand_result.error());
        status_peek_visible_ = false;
        return;
    }

    auto timer = context_->timer().start_delayed(
                     SUPER_STATUS_PEEK_AUTO_HIDE_TIMER_NAME,
                     SUPER_STATUS_PEEK_HOLD_MS
                 );
    if (!timer) {
        BROOKESIA_LOGW("Failed to start Shell status peek auto-hide timer: %1%", timer.error());
        return;
    }
    status_peek_auto_hide_timer_id_ = *timer;
}

void ShellApp::collapse_status_peek(bool restore_display)
{
    ++status_peek_generation_;
    status_peek_tracking_ = false;
    status_peek_visible_ = false;
    cancel_status_peek_auto_hide_timer();
    if (context_ == nullptr) {
        return;
    }

    if (status_bar_animation_id_ != 0) {
        context_->gui().stop_animation(status_bar_animation_id_);
        status_bar_animation_id_ = 0;
    }
    system_ui_expanded_ = false;
    auto state_result = refresh_system_ui_state_bindings();
    if (!state_result) {
        BROOKESIA_LOGW("Failed to update Shell status peek mask: %1%", state_result.error());
    }
    auto result = refresh_system_ui_position_bindings();
    if (!result) {
        BROOKESIA_LOGW("Failed to collapse Shell status peek: %1%", result.error());
    }
    if (restore_display) {
        restore_display_after_system_ui();
    }
}

void ShellApp::reset_status_peek_session(bool restore_display, bool collapse_immediately, const char *reason)
{
    const bool should_reset = status_peek_tracking_ || status_peek_visible_ ||
                              status_peek_auto_hide_timer_id_ != core::INVALID_TIMER_ID ||
                              (!foreground_is_shell_ && system_ui_expanded_) ||
                              !display_source_restore_records_.empty();
    if (!should_reset) {
        return;
    }

    BROOKESIA_LOGI(
        "Reset Shell status peek: reason(%1%), foreground_is_shell(%2%), tracking(%3%), visible(%4%), expanded(%5%), restore_display(%6%), immediate(%7%)",
        reason != nullptr ? reason : "unknown",
        foreground_is_shell_,
        status_peek_tracking_,
        status_peek_visible_,
        system_ui_expanded_,
        restore_display,
        collapse_immediately
    );

    ++status_peek_generation_;
    status_peek_tracking_ = false;
    status_peek_visible_ = false;
    cancel_status_peek_auto_hide_timer();

    if (context_ == nullptr) {
        if (restore_display) {
            BROOKESIA_LOGI("Restore display source while resetting Shell status peek without GUI context");
            restore_display_after_system_ui();
        }
        return;
    }

    if (collapse_immediately) {
        if (status_bar_animation_id_ != 0) {
            context_->gui().stop_animation(status_bar_animation_id_);
            status_bar_animation_id_ = 0;
        }
        system_ui_expanded_ = false;
        auto result = refresh_system_ui_position_bindings();
        if (!result) {
            BROOKESIA_LOGW("Failed to immediately reset Shell status peek: %1%", result.error());
        }
        if (restore_display) {
            BROOKESIA_LOGI("Restore display source while immediately resetting Shell status peek");
            restore_display_after_system_ui();
        }
        return;
    }

    if (!system_ui_expanded_) {
        if (restore_display) {
            BROOKESIA_LOGI("Restore display source after Shell status peek reset without collapse animation");
            restore_display_after_system_ui();
        }
        return;
    }

    auto collapse_result = set_system_ui_expanded(false);
    if (!collapse_result) {
        BROOKESIA_LOGW("Failed to animate Shell status peek reset: %1%", collapse_result.error());
        if (restore_display) {
            restore_display_after_system_ui();
        }
    }
}

void ShellApp::update_gesture_indicator_width(int32_t width)
{
    if (context_ == nullptr) {
        return;
    }
    if (gesture_indicator_animation_id_ != 0) {
        context_->gui().stop_animation(gesture_indicator_animation_id_);
        gesture_indicator_animation_id_ = 0;
    }
    if (gesture_indicator_x_animation_id_ != 0) {
        context_->gui().stop_animation(gesture_indicator_x_animation_id_);
        gesture_indicator_x_animation_id_ = 0;
    }
    if (gesture_indicator_max_width_ <= 0) {
        gesture_indicator_max_width_ = get_fallback_gesture_indicator_width(owner_.get_environment());
    }
    const auto clamped_width = std::clamp<int32_t>(width, 1, std::max<int32_t>(gesture_indicator_max_width_, 1));
    std::vector<gui::BindingValueUpdate> updates;
    add_gesture_indicator_binding_updates(
        updates,
        false,
        std::max<int32_t>(gesture_indicator_max_width_, 1),
        clamped_width
    );
    auto result = context_->gui().set_binding_values(updates);
    if (!result) {
        BROOKESIA_LOGW("Failed to update Shell gesture indicator: %1%", result.error());
    }
}

void ShellApp::animate_gesture_indicator_rebound()
{
    if (context_ == nullptr) {
        restore_display_after_system_ui();
        return;
    }
    if (gesture_indicator_animation_id_ != 0) {
        context_->gui().stop_animation(gesture_indicator_animation_id_);
        gesture_indicator_animation_id_ = 0;
    }
    if (gesture_indicator_x_animation_id_ != 0) {
        context_->gui().stop_animation(gesture_indicator_x_animation_id_);
        gesture_indicator_x_animation_id_ = 0;
    }
    if (gesture_indicator_max_width_ <= 0) {
        gesture_indicator_max_width_ = get_fallback_gesture_indicator_width(owner_.get_environment());
    }

    std::vector<gui::BindingValueUpdate> updates;
    add_binding_update(updates, SUPER_GESTURE_INDICATOR_PATH, "gesture_indicator_hidden", "false");
    auto binding_result = context_->gui().set_binding_values(updates);
    if (!binding_result) {
        BROOKESIA_LOGW("Failed to show Shell gesture indicator before rebound: %1%", binding_result.error());
        reset_gesture_indicator();
        restore_display_after_system_ui();
        return;
    }

    const auto generation = ++gesture_generation_;
    auto x_animation = make_timed_animation(gui::AnimationProperty::X, 0, SUPER_GESTURE_REBOUND_MS);
    auto x_animation_result = context_->gui().start_view_animation_with_result(
                                  SUPER_GESTURE_INDICATOR_BAR_PATH,
                                  x_animation,
    [this, generation]() {
        if (context_ == nullptr || generation != gesture_generation_) {
            return;
        }
        gesture_indicator_x_animation_id_ = 0;
    }
                              );
    if (!x_animation_result) {
        BROOKESIA_LOGW("Failed to start Shell gesture indicator x rebound: %1%", x_animation_result.error());
    } else {
        gesture_indicator_x_animation_id_ = x_animation_result->subscription_id;
    }

    auto animation = make_timed_animation(
                         gui::AnimationProperty::Width,
                         std::max<int32_t>(gesture_indicator_max_width_, 1),
                         SUPER_GESTURE_REBOUND_MS
                     );
    auto animation_result = context_->gui().start_view_animation_with_result(
                                SUPER_GESTURE_INDICATOR_BAR_PATH,
                                animation,
    [this, generation]() {
        if (context_ == nullptr || generation != gesture_generation_) {
            return;
        }
        gesture_indicator_animation_id_ = 0;
        gesture_indicator_x_animation_id_ = 0;
        reset_gesture_indicator();
        restore_display_after_system_ui();
    }
                            );
    if (!animation_result) {
        BROOKESIA_LOGW("Failed to start Shell gesture indicator rebound: %1%", animation_result.error());
        reset_gesture_indicator();
        restore_display_after_system_ui();
        return;
    }
    gesture_indicator_animation_id_ = animation_result->subscription_id;
}

void ShellApp::trigger_gesture_exit()
{
    if (context_ == nullptr || gesture_exit_triggered_) {
        return;
    }
    gesture_exit_triggered_ = true;
    gesture_tracking_ = false;
    gesture_exit_armed_ = false;
    status_peek_tracking_ = false;
    status_peek_visible_ = false;
    cancel_status_peek_auto_hide_timer();
    if (gesture_indicator_animation_id_ != 0) {
        context_->gui().stop_animation(gesture_indicator_animation_id_);
        gesture_indicator_animation_id_ = 0;
    }
    if (gesture_indicator_x_animation_id_ != 0) {
        context_->gui().stop_animation(gesture_indicator_x_animation_id_);
        gesture_indicator_x_animation_id_ = 0;
    }
    if (gesture_indicator_max_width_ <= 0) {
        gesture_indicator_max_width_ = get_fallback_gesture_indicator_width(owner_.get_environment());
    }
    const auto max_width = std::max<int32_t>(gesture_indicator_max_width_, 1);
    std::vector<gui::BindingValueUpdate> updates;
    add_gesture_indicator_binding_updates(updates, true, max_width, max_width);
    auto binding_result = context_->gui().set_binding_values(updates);
    if (!binding_result) {
        BROOKESIA_LOGW("Failed to hide Shell gesture indicator after exit trigger: %1%", binding_result.error());
    }
    auto result = owner_.close_active_app();
    if (!result) {
        BROOKESIA_LOGW("Failed to close active app from Shell bottom gesture: %1%", result.error());
        gesture_exit_triggered_ = false;
        (void)refresh_system_ui_state_bindings();
        restore_display_after_system_ui();
        return;
    }
    clear_display_source_restore_records();
}

std::expected<void, std::string> ShellApp::set_foreground_app(const std::optional<core::AppInfo> &app)
{
    const bool next_foreground_is_shell = !app.has_value() || (app->app_id == owner_.shell_app_id_);
    const bool should_reset_gesture = gesture_tracking_ || gesture_exit_armed_ || gesture_exit_triggered_ ||
                                      gesture_exit_hold_timer_id_ != core::INVALID_TIMER_ID ||
                                      gesture_indicator_animation_id_ != 0 ||
                                      gesture_indicator_x_animation_id_ != 0;
    if (next_foreground_is_shell && foreground_is_shell_ && system_ui_expanded_ && !should_reset_gesture) {
        return {};
    }

    if (!foreground_is_shell_) {
        reset_status_peek_session(true, true, "foreground app change");
    }
    if (should_reset_gesture) {
        BROOKESIA_LOGI(
            "Reset Shell bottom gesture: reason(foreground app change), tracking(%1%), armed(%2%), triggered(%3%)",
            gesture_tracking_,
            gesture_exit_armed_,
            gesture_exit_triggered_
        );
        reset_gesture_indicator();
    }
    foreground_is_shell_ = next_foreground_is_shell;
    if (foreground_is_shell_) {
        clear_display_source_restore_records();
    }
    if (auto state_result = refresh_system_ui_state_bindings(); !state_result) {
        BROOKESIA_LOGW("Failed to refresh Shell system UI mask after foreground change: %1%", state_result.error());
    }
    auto background_result = context_->gui().trigger_screen_flow(
                                 SUPER_BACKGROUND_FLOW_ID,
                                 foreground_is_shell_ ? SUPER_ACTION_BACKGROUND_SHELL : SUPER_ACTION_BACKGROUND_APP
                             );
    if (!background_result) {
        return background_result;
    }
    return set_system_ui_expanded(foreground_is_shell_);
}

std::expected<void, std::string> ShellApp::refresh_system_ui_bindings()
{
    auto result = refresh_system_ui_state_bindings();
    if (!result) {
        return result;
    }
    return refresh_system_ui_position_bindings();
}

std::expected<void, std::string> ShellApp::refresh_system_ui_position_bindings()
{
    if (context_ == nullptr) {
        return {};
    }

    const auto status_bar_target_y = system_ui_expanded_ ?
                                     SUPER_STATUS_BAR_EXPANDED_Y :
                                     get_collapsed_status_bar_y(*context_);
    std::vector<gui::BindingValueUpdate> updates;
    add_binding_update(
        updates,
        SUPER_STATUS_BAR_PATH,
        "system_ui_status_y",
        std::to_string(status_bar_target_y)
    );
    return context_->gui().set_binding_values(updates);
}

std::expected<void, std::string> ShellApp::refresh_system_ui_state_bindings()
{
    if (context_ == nullptr) {
        return {};
    }

    const bool mask_visible = !foreground_is_shell_ &&
                              (system_ui_expanded_ || status_peek_tracking_ || status_peek_visible_ ||
                               gesture_tracking_ || gesture_exit_armed_ || gesture_exit_triggered_ ||
                               message_dialog_mounted_);
    std::vector<gui::BindingValueUpdate> updates;
    add_binding_update(
        updates,
        SUPER_SYSTEM_UI_MASK_PATH,
        "hidden",
        bool_to_binding(!mask_visible)
    );
    return context_->gui().set_binding_values(updates);
}

std::expected<void, std::string> ShellApp::show_launch_overlay(
    const core::AppInfo &app,
    const std::optional<gui::ViewFrame> &origin,
    std::function<void()> completed_handler
)
{
    if (context_ == nullptr) {
        return std::unexpected("Shell app is not running");
    }
    if (launch_overlay_active_) {
        return std::unexpected("App launch overlay is already active");
    }

    const auto environment = owner_.get_environment();
    const auto start_frame = origin.value_or(make_fallback_origin(environment));
    const auto screen_width = std::max<int32_t>(environment.width_px, SUPER_APP_LAUNCH_FINAL_ICON_SIZE);
    const auto screen_height = std::max<int32_t>(environment.height_px, SUPER_APP_LAUNCH_FINAL_ICON_SIZE);
    const auto final_icon_size = std::max<int32_t>(SUPER_APP_LAUNCH_FINAL_ICON_SIZE, 1);
    const auto final_icon_x = (screen_width - final_icon_size) / 2;
    const auto final_icon_y = (screen_height - final_icon_size) / 2;
    const bool has_icon = core::has_app_icon_image(app.manifest);
    const auto display_name = get_app_display_name(app);
    const auto initial_surface_x = SUPER_APP_LAUNCH_ANIMATION_ENABLED ? start_frame.x : 0;
    const auto initial_surface_y = SUPER_APP_LAUNCH_ANIMATION_ENABLED ? start_frame.y : 0;
    const auto initial_surface_width = SUPER_APP_LAUNCH_ANIMATION_ENABLED ? start_frame.width : screen_width;
    const auto initial_surface_height = SUPER_APP_LAUNCH_ANIMATION_ENABLED ? start_frame.height : screen_height;
    const auto initial_icon_x = SUPER_APP_LAUNCH_ANIMATION_ENABLED ? start_frame.x : final_icon_x;
    const auto initial_icon_y = SUPER_APP_LAUNCH_ANIMATION_ENABLED ? start_frame.y : final_icon_y;
    const auto initial_icon_width = SUPER_APP_LAUNCH_ANIMATION_ENABLED ? start_frame.width : final_icon_size;
    const auto initial_icon_height = SUPER_APP_LAUNCH_ANIMATION_ENABLED ? start_frame.height : final_icon_size;

    std::vector<gui::BindingValueUpdate> updates;
    add_binding_update(updates, SUPER_APP_MODAL_PATH, "hidden", "false");
    add_binding_update(updates, SUPER_APP_MODAL_LAUNCH_PATH, "hidden", "false");
    add_binding_update(updates, SUPER_APP_MODAL_LOADING_PATH, "hidden", "true");
    add_binding_update(updates, SUPER_APP_MODAL_LAUNCH_SURFACE_PATH, "x", int_to_binding(initial_surface_x));
    add_binding_update(updates, SUPER_APP_MODAL_LAUNCH_SURFACE_PATH, "y", int_to_binding(initial_surface_y));
    add_binding_update(updates, SUPER_APP_MODAL_LAUNCH_SURFACE_PATH, "width", int_to_binding(initial_surface_width));
    add_binding_update(updates, SUPER_APP_MODAL_LAUNCH_SURFACE_PATH, "height", int_to_binding(initial_surface_height));
    add_binding_update(updates, SUPER_APP_MODAL_LAUNCH_ICON_BOX_PATH, "hidden", "false");
    add_binding_update(updates, SUPER_APP_MODAL_LAUNCH_ICON_BOX_PATH, "x", int_to_binding(initial_icon_x));
    add_binding_update(updates, SUPER_APP_MODAL_LAUNCH_ICON_BOX_PATH, "y", int_to_binding(initial_icon_y));
    add_binding_update(updates, SUPER_APP_MODAL_LAUNCH_ICON_BOX_PATH, "width", int_to_binding(initial_icon_width));
    add_binding_update(updates, SUPER_APP_MODAL_LAUNCH_ICON_BOX_PATH, "height", int_to_binding(initial_icon_height));
    add_binding_update(
        updates,
        SUPER_APP_MODAL_LAUNCH_ICON_IMAGE_PATH,
        "src",
        has_icon ? core::resolve_app_icon_resource_id(app.manifest) : SUPER_DEFAULT_IMAGE_ID
    );
    add_binding_update(updates, SUPER_APP_MODAL_LAUNCH_ICON_IMAGE_PATH, "hidden", bool_to_binding(!has_icon));
    add_binding_update(
        updates,
        SUPER_APP_MODAL_LAUNCH_FALLBACK_LABEL_PATH,
        "text",
        get_app_icon_text(display_name)
    );
    add_binding_update(
        updates,
        SUPER_APP_MODAL_LAUNCH_FALLBACK_LABEL_PATH,
        "hidden",
        bool_to_binding(has_icon)
    );
    auto binding_result = context_->gui().set_binding_values(updates);
    if (!binding_result) {
        return binding_result;
    }

    launch_overlay_active_ = true;
    if (!SUPER_APP_LAUNCH_ANIMATION_ENABLED) {
        if (completed_handler) {
            completed_handler();
        }
        return {};
    }

    const std::array<std::pair<const char *, gui::Animation>, 8> animations = {{
            {SUPER_APP_MODAL_LAUNCH_SURFACE_PATH, make_modal_animation(gui::AnimationProperty::X, 0)},
            {SUPER_APP_MODAL_LAUNCH_SURFACE_PATH, make_modal_animation(gui::AnimationProperty::Y, 0)},
            {SUPER_APP_MODAL_LAUNCH_SURFACE_PATH, make_modal_animation(gui::AnimationProperty::Width, screen_width)},
            {SUPER_APP_MODAL_LAUNCH_SURFACE_PATH, make_modal_animation(gui::AnimationProperty::Height, screen_height)},
            {SUPER_APP_MODAL_LAUNCH_ICON_BOX_PATH, make_modal_animation(gui::AnimationProperty::X, final_icon_x)},
            {SUPER_APP_MODAL_LAUNCH_ICON_BOX_PATH, make_modal_animation(gui::AnimationProperty::Y, final_icon_y)},
            {SUPER_APP_MODAL_LAUNCH_ICON_BOX_PATH, make_modal_animation(gui::AnimationProperty::Width, final_icon_size)},
            {SUPER_APP_MODAL_LAUNCH_ICON_BOX_PATH, make_modal_animation(gui::AnimationProperty::Height, final_icon_size)},
        }
    };
    auto barrier = std::make_shared<AnimationCompletionBarrier>(std::move(completed_handler));
    int32_t started_count = 0;
    for (const auto &[path, animation] : animations) {
        barrier->add();
        auto animation_result = context_->gui().start_view_animation_with_result(
                                    path,
                                    animation,
        [barrier]() {
            barrier->complete();
        }
                                );
        if (animation_result) {
            ++started_count;
            continue;
        }
        BROOKESIA_LOGW("Failed to start launch animation: path(%1%), error(%2%)", path, animation_result.error());
        barrier->complete();
    }
    if (started_count == 0) {
        launch_overlay_active_ = false;
        auto refresh_result = refresh_app_modal_for_loading();
        if (!refresh_result) {
            BROOKESIA_LOGW("Failed to restore loading modal after launch animation failure: %1%", refresh_result.error());
        }
        return std::unexpected("No app launch animations started");
    }
    barrier->arm();
    return {};
}

void ShellApp::finish_launch_overlay()
{
    launch_overlay_active_ = false;
    cancel_pending_launch();
    auto result = apply_app_modal_after_launch();
    if (!result) {
        BROOKESIA_LOGW("Failed to finish launch overlay: %1%", result.error());
    }
}

void ShellApp::cancel_pending_launch()
{
    if (context_ != nullptr && launch_hold_timer_id_ != core::INVALID_TIMER_ID) {
        (void)context_->timer().stop(launch_hold_timer_id_);
    }
    launch_hold_timer_id_ = core::INVALID_TIMER_ID;
    pending_launch_app_id_.reset();
    launch_request_started_at_.reset();
}

void ShellApp::start_app_after_launch(core::AppId app_id)
{
    const auto request_started_at = launch_request_started_at_;
    pending_launch_app_id_.reset();
    launch_hold_timer_id_ = core::INVALID_TIMER_ID;
    if (!launch_overlay_active_) {
        BROOKESIA_LOGW("Skip app launch after overlay completion: app_id(%1%)", app_id);
        return;
    }
    if (context_ == nullptr) {
        BROOKESIA_LOGW("Skip app launch after overlay completion because Shell is not running: app_id(%1%)", app_id);
        finish_launch_overlay();
        return;
    }

    auto modal_result = prepare_app_modal_for_app_start();
    if (!modal_result) {
        BROOKESIA_LOGW("Failed to prepare app modal before app start: %1%", modal_result.error());
    }

    const auto app_start_started_at = SteadyClock::now();
    auto result = owner_.start_app(app_id, core::System::AppStartOptions{});
    const auto app_start_ended_at = SteadyClock::now();
    finish_launch_overlay();
    SYSTEM_SUPER_PROFILE_LOGI(
        "Shell app launch profile: app_id(%1%), pre_start_ms(%2%), start_app_ms(%3%), total_ms(%4%)",
        app_id,
        request_started_at.has_value() ? elapsed_ms_since(*request_started_at, app_start_started_at) : 0,
        elapsed_ms_since(app_start_started_at, app_start_ended_at),
        request_started_at.has_value() ? elapsed_ms_since(*request_started_at, app_start_ended_at) : 0
    );
    if (!result) {
        BROOKESIA_LOGW("Failed to launch app: app_id(%1%), error(%2%)", app_id, result.error());
    }
}

std::expected<void, std::string> ShellApp::prepare_app_modal_for_app_start()
{
    const auto environment = owner_.get_environment();
    const auto final_icon_size = std::max<int32_t>(SUPER_APP_LAUNCH_FINAL_ICON_SIZE, 1);
    const auto screen_width = std::max<int32_t>(environment.width_px, final_icon_size);
    const auto screen_height = std::max<int32_t>(environment.height_px, final_icon_size);
    const auto final_icon_x = (screen_width - final_icon_size) / 2;
    const auto final_icon_y = (screen_height - final_icon_size) / 2;

    std::vector<gui::BindingValueUpdate> updates;
    add_binding_update(updates, SUPER_APP_MODAL_PATH, "hidden", "false");
    add_binding_update(updates, SUPER_APP_MODAL_LAUNCH_PATH, "hidden", "false");
    add_binding_update(updates, SUPER_APP_MODAL_LOADING_PATH, "hidden", "true");
    add_binding_update(updates, SUPER_APP_MODAL_LAUNCH_SURFACE_PATH, "x", "0");
    add_binding_update(updates, SUPER_APP_MODAL_LAUNCH_SURFACE_PATH, "y", "0");
    add_binding_update(updates, SUPER_APP_MODAL_LAUNCH_SURFACE_PATH, "width", int_to_binding(screen_width));
    add_binding_update(updates, SUPER_APP_MODAL_LAUNCH_SURFACE_PATH, "height", int_to_binding(screen_height));
    add_binding_update(updates, SUPER_APP_MODAL_LAUNCH_ICON_BOX_PATH, "hidden", "false");
    add_binding_update(updates, SUPER_APP_MODAL_LAUNCH_ICON_BOX_PATH, "x", int_to_binding(final_icon_x));
    add_binding_update(updates, SUPER_APP_MODAL_LAUNCH_ICON_BOX_PATH, "y", int_to_binding(final_icon_y));
    add_binding_update(updates, SUPER_APP_MODAL_LAUNCH_ICON_BOX_PATH, "width", int_to_binding(final_icon_size));
    add_binding_update(updates, SUPER_APP_MODAL_LAUNCH_ICON_BOX_PATH, "height", int_to_binding(final_icon_size));

    core::GuiBatchCommand command;
    command.type = core::GuiBatchCommand::Type::SetBindings;
    command.binding_updates = std::move(updates);
    auto result = owner_.gui_execute_batch(owner_.shell_app_id_, {std::move(command)});
    if (!result) {
        return std::unexpected(result.error());
    }
    if (!result->success) {
        return std::unexpected("Failed to prepare app modal for app start batch");
    }
    return {};
}

std::expected<void, std::string> ShellApp::apply_app_modal_after_launch()
{
    const bool show_loading = !loading_owners_.empty();
    std::vector<gui::BindingValueUpdate> updates;
    add_binding_update(updates, SUPER_APP_MODAL_PATH, "hidden", bool_to_binding(!show_loading));
    add_binding_update(updates, SUPER_APP_MODAL_LAUNCH_PATH, "hidden", "true");
    add_binding_update(updates, SUPER_APP_MODAL_LOADING_PATH, "hidden", bool_to_binding(!show_loading));
    add_binding_update(updates, SUPER_APP_MODAL_LAUNCH_ICON_BOX_PATH, "hidden", "true");
    add_binding_update(updates, SUPER_APP_MODAL_LAUNCH_ICON_IMAGE_PATH, "hidden", "true");
    add_binding_update(updates, SUPER_APP_MODAL_LAUNCH_FALLBACK_LABEL_PATH, "hidden", "true");

    core::GuiBatchCommand command;
    command.type = core::GuiBatchCommand::Type::SetBindings;
    command.binding_updates = std::move(updates);
    auto result = owner_.gui_execute_batch(owner_.shell_app_id_, {std::move(command)});
    if (!result) {
        return std::unexpected(result.error());
    }
    if (!result->success) {
        return std::unexpected("Failed to apply app modal after launch batch");
    }
    return {};
}

void ShellApp::schedule_app_start_after_launch(core::AppId app_id)
{
    const auto hold_ms = SUPER_APP_LAUNCH_ANIMATION_ENABLED ? SUPER_APP_LAUNCH_POST_COMPLETE_HOLD_MS :
                         std::max(SUPER_APP_LAUNCH_POST_COMPLETE_HOLD_MS, SUPER_APP_LAUNCH_NO_ANIMATION_MIN_HOLD_MS);
    if (!launch_request_started_at_.has_value()) {
        launch_request_started_at_ = SteadyClock::now();
    }
    if (hold_ms <= 0 || context_ == nullptr) {
        start_app_after_launch(app_id);
        return;
    }

    if (launch_hold_timer_id_ != core::INVALID_TIMER_ID) {
        (void)context_->timer().stop(launch_hold_timer_id_);
    }
    launch_hold_timer_id_ = core::INVALID_TIMER_ID;
    pending_launch_app_id_.reset();
    pending_launch_app_id_ = app_id;
    auto timer = context_->timer().start_delayed(
                     SUPER_APP_LAUNCH_HOLD_TIMER_NAME,
                     hold_ms
                 );
    if (!timer) {
        BROOKESIA_LOGW("Failed to start app launch hold timer: %1%", timer.error());
        start_app_after_launch(app_id);
        return;
    }
    launch_hold_timer_id_ = *timer;
    SYSTEM_SUPER_PROFILE_LOGI(
        "Shell app launch profile: app_id(%1%), wait_before_start_ms(%2%), hold_ms(%3%)",
        app_id,
        launch_request_started_at_.has_value() ? elapsed_ms_since(*launch_request_started_at_) : 0,
        hold_ms
    );
}

std::expected<void, std::string> ShellApp::show_loading(core::AppId app_id)
{
    auto it = std::remove(loading_owners_.begin(), loading_owners_.end(), app_id);
    loading_owners_.erase(it, loading_owners_.end());
    loading_owners_.push_back(app_id);
    if (launch_overlay_active_) {
        return {};
    }
    return show_loading_overlay();
}

void ShellApp::hide_loading(core::AppId app_id)
{
    auto it = std::remove(loading_owners_.begin(), loading_owners_.end(), app_id);
    loading_owners_.erase(it, loading_owners_.end());
    if (launch_overlay_active_) {
        return;
    }
    auto result = refresh_app_modal_for_loading();
    if (!result) {
        BROOKESIA_LOGW("Failed to hide loading overlay: %1%", result.error());
    }
}

std::expected<void, std::string> ShellApp::show_loading_overlay()
{
    if (context_ == nullptr) {
        return std::unexpected("Shell app is not running");
    }

    std::vector<gui::BindingValueUpdate> updates;
    add_binding_update(updates, SUPER_APP_MODAL_PATH, "hidden", "false");
    add_binding_update(updates, SUPER_APP_MODAL_LAUNCH_PATH, "hidden", "true");
    add_binding_update(updates, SUPER_APP_MODAL_LOADING_PATH, "hidden", "false");
    return context_->gui().set_binding_values(updates);
}

std::expected<void, std::string> ShellApp::hide_app_modal()
{
    if (context_ == nullptr) {
        return {};
    }

    std::vector<gui::BindingValueUpdate> updates;
    add_binding_update(updates, SUPER_APP_MODAL_PATH, "hidden", "true");
    add_binding_update(updates, SUPER_APP_MODAL_LAUNCH_PATH, "hidden", "true");
    add_binding_update(updates, SUPER_APP_MODAL_LOADING_PATH, "hidden", "true");
    return context_->gui().set_binding_values(updates);
}

std::expected<void, std::string> ShellApp::refresh_app_modal_for_loading()
{
    if (!loading_owners_.empty()) {
        return show_loading_overlay();
    }
    return hide_app_modal();
}

std::expected<void, std::string> ShellApp::mount_keyboard()
{
    if (context_ == nullptr) {
        return std::unexpected("Shell app is not running");
    }
    if (keyboard_mounted_) {
        return {};
    }

    if (keyboard_action_connections_.empty()) {
        auto subscribe_keyboard_action = [this](const char *action, bool confirmed) {
            keyboard_action_connections_.push_back(context_->gui().subscribe_action(
                    action,
            [this, confirmed](const gui::Event &) {
                if (keyboard_closing_ || !keyboard_accepting_actions_) {
                    return;
                }
                finish_keyboard(confirmed);
            }
                                                   ));
        };
        keyboard_action_connections_.push_back(context_->gui().subscribe_action(
                SUPER_ACTION_KEYBOARD_TEXT_CHANGED,
        [this](const gui::Event & event) {
            if (keyboard_closing_) {
                return;
            }
            auto text = event.get_string("text");
            if (text.has_value()) {
                active_keyboard_text_ = std::string(*text);
            }
        }
                                               ));
        keyboard_action_connections_.push_back(context_->gui().subscribe_action(
                SUPER_ACTION_KEYBOARD_TOGGLE_PASSWORD,
        [this](const gui::Event &) {
            if (keyboard_closing_ || !active_keyboard_request_id_.has_value() ||
                    !active_keyboard_password_requested_) {
                return;
            }
            active_keyboard_password_hidden_ = !active_keyboard_password_hidden_;
            auto result = refresh_keyboard_password_bindings();
            if (!result) {
                BROOKESIA_LOGW("Failed to toggle keyboard password visibility: %1%", result.error());
            }
        }
                                               ));
        subscribe_keyboard_action(SUPER_ACTION_KEYBOARD_SUBMIT_INPUT, true);
        subscribe_keyboard_action(SUPER_ACTION_KEYBOARD_SUBMIT_KEYBOARD, true);
        subscribe_keyboard_action(SUPER_ACTION_KEYBOARD_CANCEL_INPUT, false);
        subscribe_keyboard_action(SUPER_ACTION_KEYBOARD_CANCEL_KEYBOARD, false);
        subscribe_keyboard_action(SUPER_ACTION_KEYBOARD_CANCEL_BACKDROP, false);
    }

    auto flow_result = context_->gui().trigger_screen_flow(SUPER_KEYBOARD_FLOW_ID, SUPER_ACTION_KEYBOARD_SHOW);
    if (!flow_result) {
        return std::unexpected(flow_result.error());
    }
    keyboard_mounted_ = true;
    keyboard_closing_ = false;
    keyboard_accepting_actions_ = false;
    return {};
}

void ShellApp::unmount_keyboard()
{
    keyboard_action_connections_.clear();
    stop_keyboard_animations();
    if (context_ != nullptr && keyboard_mounted_) {
        std::vector<gui::BindingValueUpdate> updates;
        add_binding_update(updates, SUPER_KEYBOARD_INPUT_BAR_PATH, "inputHidden", "true");
        add_binding_update(updates, SUPER_KEYBOARD_INPUT_KEYBOARD_PANEL_PATH, "keyboardHidden", "true");
        core::GuiBatchCommand command;
        command.type = core::GuiBatchCommand::Type::SetBindings;
        command.binding_updates = std::move(updates);
        auto result = owner_.gui_execute_batch(owner_.shell_app_id_, {std::move(command)});
        if (!result || !result->success) {
            BROOKESIA_LOGW("Failed to reset keyboard hidden state before unmount");
        }
        auto flow_result = context_->gui().trigger_screen_flow(SUPER_KEYBOARD_FLOW_ID, SUPER_ACTION_KEYBOARD_HIDE);
        if (!flow_result) {
            BROOKESIA_LOGW("Failed to hide keyboard screen: %1%", flow_result.error());
        }
    }
    keyboard_mounted_ = false;
    reset_keyboard_state();
}

std::expected<void, std::string> ShellApp::refresh_keyboard_bindings(
    const core::KeyboardRequestOptions &options
)
{
    if (context_ == nullptr) {
        return std::unexpected("Shell app is not running");
    }

    active_keyboard_password_requested_ = options.password;
    active_keyboard_password_hidden_ = options.password;

    std::vector<gui::BindingValueUpdate> updates;
    add_binding_update(updates, SUPER_KEYBOARD_INPUT_TEXT_PATH, "text", options.initial_text);
    add_binding_update(updates, SUPER_KEYBOARD_INPUT_TEXT_PATH, "placeholder", options.placeholder);
    add_binding_update(updates, SUPER_KEYBOARD_INPUT_TEXT_PATH, "password", bool_to_binding(active_keyboard_password_hidden_));
    add_binding_update(updates, SUPER_KEYBOARD_INPUT_TEXT_PATH, "maxLength", int_to_binding(options.max_length));
    add_binding_update(
        updates,
        SUPER_KEYBOARD_PASSWORD_TOGGLE_PATH,
        "passwordToggleHidden",
        bool_to_binding(!active_keyboard_password_requested_)
    );
    add_binding_update(
        updates,
        SUPER_KEYBOARD_PASSWORD_TOGGLE_ICON_PATH,
        "passwordIcon",
        active_keyboard_password_hidden_ ? SUPER_KEYBOARD_EYE_HIDE_IMAGE_ID : SUPER_KEYBOARD_EYE_SHOW_IMAGE_ID
    );
    add_binding_update(updates, SUPER_KEYBOARD_INPUT_KEYBOARD_PATH, "mode", options.mode);
    add_binding_update(
        updates,
        SUPER_KEYBOARD_INPUT_KEYBOARD_PATH,
        "allowedModes",
        join_keyboard_modes(options.allowed_modes.empty() ? default_keyboard_modes() : options.allowed_modes)
    );
    return context_->gui().set_binding_values(updates);
}

std::expected<void, std::string> ShellApp::refresh_keyboard_password_bindings()
{
    if (context_ == nullptr) {
        return std::unexpected("Shell app is not running");
    }

    std::vector<gui::BindingValueUpdate> updates;
    add_binding_update(
        updates,
        SUPER_KEYBOARD_INPUT_TEXT_PATH,
        "password",
        bool_to_binding(active_keyboard_password_hidden_)
    );
    add_binding_update(
        updates,
        SUPER_KEYBOARD_PASSWORD_TOGGLE_PATH,
        "passwordToggleHidden",
        bool_to_binding(!active_keyboard_password_requested_)
    );
    add_binding_update(
        updates,
        SUPER_KEYBOARD_PASSWORD_TOGGLE_ICON_PATH,
        "passwordIcon",
        active_keyboard_password_hidden_ ? SUPER_KEYBOARD_EYE_HIDE_IMAGE_ID : SUPER_KEYBOARD_EYE_SHOW_IMAGE_ID
    );
    return context_->gui().set_binding_values(updates);
}

std::expected<void, std::string> ShellApp::start_keyboard_show_animation()
{
    if (context_ == nullptr || !keyboard_mounted_) {
        return {};
    }

    stop_keyboard_animations();
    const auto input_frame = context_->gui().get_view_frame(SUPER_KEYBOARD_INPUT_BAR_PATH);
    const auto keyboard_frame = context_->gui().get_view_frame(SUPER_KEYBOARD_INPUT_KEYBOARD_PANEL_PATH);
    if (!input_frame || !keyboard_frame) {
        return std::unexpected("Keyboard views are not mounted");
    }

    const auto final_input_y = input_frame->y;
    const auto final_keyboard_y = keyboard_frame->y;
    const auto hidden_input_y = -std::max<int32_t>(input_frame->height, 1) - 2;
    const auto hidden_keyboard_y = owner_.get_environment().height_px + std::max<int32_t>(keyboard_frame->height, 1);

    const auto generation = keyboard_animation_generation_;
    auto barrier = std::make_shared<AnimationCompletionBarrier>([this, generation]() {
        if (context_ == nullptr || generation != keyboard_animation_generation_) {
            return;
        }
        keyboard_input_animation_id_ = 0;
        keyboard_body_animation_id_ = 0;
        keyboard_accepting_actions_ = true;
    });

    barrier->add();
    auto input_animation = context_->gui().start_view_animation_with_result(
                               SUPER_KEYBOARD_INPUT_BAR_PATH,
                               make_keyboard_animation(gui::AnimationProperty::Y, hidden_input_y, final_input_y),
    [barrier]() {
        barrier->complete();
    }
                           );
    if (!input_animation) {
        return std::unexpected(input_animation.error());
    }

    barrier->add();
    auto keyboard_animation = context_->gui().start_view_animation_with_result(
                                  SUPER_KEYBOARD_INPUT_KEYBOARD_PANEL_PATH,
                                  make_keyboard_animation(gui::AnimationProperty::Y, hidden_keyboard_y, final_keyboard_y),
    [barrier]() {
        barrier->complete();
    }
                              );
    if (!keyboard_animation) {
        context_->gui().stop_animation(input_animation->subscription_id);
        return std::unexpected(keyboard_animation.error());
    }

    std::vector<gui::BindingValueUpdate> visibility_updates;
    add_binding_update(visibility_updates, SUPER_KEYBOARD_INPUT_BAR_PATH, "inputHidden", "false");
    add_binding_update(visibility_updates, SUPER_KEYBOARD_INPUT_KEYBOARD_PANEL_PATH, "keyboardHidden", "false");
    auto visibility_result = context_->gui().set_binding_values(visibility_updates);
    if (!visibility_result) {
        context_->gui().stop_animation(input_animation->subscription_id);
        context_->gui().stop_animation(keyboard_animation->subscription_id);
        return visibility_result;
    }

    keyboard_input_animation_id_ = input_animation->subscription_id;
    keyboard_body_animation_id_ = keyboard_animation->subscription_id;
    barrier->arm();
    return {};
}

void ShellApp::start_keyboard_hide_animation(std::function<void()> completed_handler)
{
    if (context_ == nullptr || !keyboard_mounted_) {
        if (completed_handler) {
            completed_handler();
        }
        return;
    }

    keyboard_closing_ = true;
    stop_keyboard_animations();
    const auto input_frame = context_->gui().get_view_frame(SUPER_KEYBOARD_INPUT_BAR_PATH);
    const auto keyboard_frame = context_->gui().get_view_frame(SUPER_KEYBOARD_INPUT_KEYBOARD_PANEL_PATH);
    if (!input_frame || !keyboard_frame) {
        unmount_keyboard();
        if (completed_handler) {
            completed_handler();
        }
        return;
    }

    auto completed_handler_ptr = std::make_shared<std::function<void()>>(std::move(completed_handler));
    const auto hidden_input_y = -std::max<int32_t>(input_frame->height, 1) - 2;
    const auto hidden_keyboard_y = owner_.get_environment().height_px + std::max<int32_t>(keyboard_frame->height, 1);
    const auto generation = keyboard_animation_generation_;
    auto barrier = std::make_shared<AnimationCompletionBarrier>(
    [this, generation, completed_handler_ptr]() {
        if (context_ == nullptr || generation != keyboard_animation_generation_) {
            return;
        }
        keyboard_input_animation_id_ = 0;
        keyboard_body_animation_id_ = 0;
        unmount_keyboard();
        if (*completed_handler_ptr) {
            (*completed_handler_ptr)();
        }
    }
                   );

    barrier->add();
    auto input_animation = context_->gui().start_view_animation_with_result(
                               SUPER_KEYBOARD_INPUT_BAR_PATH,
                               make_keyboard_animation(gui::AnimationProperty::Y, hidden_input_y),
    [barrier]() {
        barrier->complete();
    }
                           );
    if (!input_animation) {
        unmount_keyboard();
        if (*completed_handler_ptr) {
            (*completed_handler_ptr)();
        }
        return;
    }

    barrier->add();
    auto keyboard_animation = context_->gui().start_view_animation_with_result(
                                  SUPER_KEYBOARD_INPUT_KEYBOARD_PANEL_PATH,
                                  make_keyboard_animation(gui::AnimationProperty::Y, hidden_keyboard_y),
    [barrier]() {
        barrier->complete();
    }
                              );
    if (!keyboard_animation) {
        context_->gui().stop_animation(input_animation->subscription_id);
        unmount_keyboard();
        if (*completed_handler_ptr) {
            (*completed_handler_ptr)();
        }
        return;
    }

    keyboard_input_animation_id_ = input_animation->subscription_id;
    keyboard_body_animation_id_ = keyboard_animation->subscription_id;
    barrier->arm();
}

void ShellApp::stop_keyboard_animations()
{
    ++keyboard_animation_generation_;
    if (context_ != nullptr && keyboard_input_animation_id_ != 0) {
        context_->gui().stop_animation(keyboard_input_animation_id_);
    }
    if (context_ != nullptr && keyboard_body_animation_id_ != 0) {
        context_->gui().stop_animation(keyboard_body_animation_id_);
    }
    keyboard_input_animation_id_ = 0;
    keyboard_body_animation_id_ = 0;
}

void ShellApp::reset_keyboard_state()
{
    active_keyboard_request_id_.reset();
    active_keyboard_owner_ = core::INVALID_APP_ID;
    active_keyboard_text_.clear();
    keyboard_accepting_actions_ = false;
    active_keyboard_password_requested_ = false;
    active_keyboard_password_hidden_ = false;
    keyboard_closing_ = false;
}

std::expected<void, std::string> ShellApp::show_keyboard(
    core::AppId app_id,
    core::KeyboardRequestId request_id,
    const core::KeyboardRequestOptions &options
)
{
    if (active_keyboard_request_id_.has_value()) {
        return std::unexpected("Keyboard input is already active");
    }
    if (context_ == nullptr) {
        return std::unexpected("Shell app is not running");
    }
    auto normalized_options = normalize_keyboard_options(options);
    if (!normalized_options) {
        return std::unexpected(normalized_options.error());
    }

    active_keyboard_request_id_ = request_id;
    active_keyboard_owner_ = app_id;
    active_keyboard_text_ = normalized_options->initial_text;

    auto mount_result = mount_keyboard();
    if (!mount_result) {
        reset_keyboard_state();
        return mount_result;
    }

    auto binding_result = refresh_keyboard_bindings(*normalized_options);
    if (!binding_result) {
        unmount_keyboard();
        return binding_result;
    }

    auto animation_result = start_keyboard_show_animation();
    if (!animation_result) {
        BROOKESIA_LOGW("Failed to start keyboard show animation: %1%", animation_result.error());
        keyboard_accepting_actions_ = true;
    }
    return {};
}

void ShellApp::hide_keyboard(core::AppId app_id, core::KeyboardRequestId request_id)
{
    if (!active_keyboard_request_id_.has_value() || *active_keyboard_request_id_ != request_id ||
            active_keyboard_owner_ != app_id) {
        return;
    }
    start_keyboard_hide_animation({});
}

void ShellApp::finish_keyboard(bool confirmed)
{
    if (!active_keyboard_request_id_.has_value()) {
        return;
    }

    const auto request_id = *active_keyboard_request_id_;
    const auto app_id = active_keyboard_owner_;
    auto text = confirmed ? active_keyboard_text_ : std::string();
    start_keyboard_hide_animation([this, app_id, request_id, confirmed, text = std::move(text)]() mutable {
        auto result = owner_.complete_app_keyboard(app_id, request_id, confirmed, std::move(text));
        if (!result)
        {
            BROOKESIA_LOGW("Failed to complete keyboard request: %1%", result.error());
        }
    });
}

std::expected<void, std::string> ShellApp::mount_message_dialog()
{
    if (context_ == nullptr) {
        return std::unexpected("Shell app is not running");
    }

    if (message_dialog_action_connections_.empty()) {
        auto subscribe_button = [this](const char *action, int32_t button_index) {
            message_dialog_action_connections_.push_back(context_->gui().subscribe_action(
                        action,
            [this, button_index](const gui::Event &) {
                if (message_dialog_closing_) {
                    return;
                }
                if (button_index < 0 ||
                        static_cast<size_t>(button_index) >= active_message_dialog_options_.buttons.size()) {
                    return;
                }
                finish_message_dialog(button_index, core::MessageDialogCloseReason::Button);
            }
                    ));
        };
        subscribe_button(SUPER_ACTION_MESSAGE_DIALOG_BUTTON0, 0);
        subscribe_button(SUPER_ACTION_MESSAGE_DIALOG_BUTTON1, 1);
        subscribe_button(SUPER_ACTION_MESSAGE_DIALOG_BUTTON2, 2);
    }

    auto flow_result = context_->gui().trigger_screen_flow(
                           SUPER_MESSAGE_DIALOG_FLOW_ID,
                           SUPER_ACTION_MESSAGE_DIALOG_SHOW
                       );
    if (!flow_result) {
        return std::unexpected(flow_result.error());
    }
    message_dialog_mounted_ = true;
    message_dialog_closing_ = false;
    if (auto state_result = refresh_system_ui_state_bindings(); !state_result) {
        BROOKESIA_LOGW("Failed to refresh system UI mask after message dialog mount: %1%", state_result.error());
    }
    return {};
}

void ShellApp::unmount_message_dialog()
{
    message_dialog_action_connections_.clear();
    stop_message_dialog_auto_close_timer();
    if (context_ != nullptr && message_dialog_mounted_) {
        auto flow_result = context_->gui().trigger_screen_flow(
                               SUPER_MESSAGE_DIALOG_FLOW_ID,
                               SUPER_ACTION_MESSAGE_DIALOG_HIDE
                           );
        if (!flow_result) {
            BROOKESIA_LOGW("Failed to hide message dialog screen: %1%", flow_result.error());
        }
    }
    message_dialog_mounted_ = false;
    message_dialog_needs_reset_ = context_ != nullptr;
    if (context_ != nullptr) {
        if (auto state_result = refresh_system_ui_state_bindings(); !state_result) {
            BROOKESIA_LOGW("Failed to refresh system UI mask after message dialog unmount: %1%", state_result.error());
        }
    }
    reset_message_dialog_state();
}

std::expected<void, std::string> ShellApp::refresh_message_dialog_bindings(
    const core::MessageDialogOptions &options
)
{
    if (context_ == nullptr) {
        return std::unexpected("Shell app is not running");
    }

    std::vector<gui::BindingValueUpdate> updates;
    const auto icon_src = message_dialog_icon_src(options.icon);
    const auto icon_recolor = message_dialog_icon_recolor(options.icon);
    add_binding_update(updates, SUPER_MESSAGE_DIALOG_ICON_PATH, "src", icon_src);
    add_binding_update(updates, SUPER_MESSAGE_DIALOG_ICON_PATH, "recolor", icon_recolor);
    add_binding_update(updates, SUPER_MESSAGE_DIALOG_ICON_PATH, "recolorOpacity", int_to_binding(255));
    add_binding_update(updates, SUPER_MESSAGE_DIALOG_ICON_PATH, "hidden", bool_to_binding(icon_src.empty()));
    add_binding_update(updates, SUPER_MESSAGE_DIALOG_TEXT_PATH, "text", options.text);
    add_binding_update(
        updates,
        SUPER_MESSAGE_DIALOG_INFORMATIVE_TEXT_PATH,
        "text",
        options.informative_text
    );
    add_binding_update(
        updates,
        SUPER_MESSAGE_DIALOG_INFORMATIVE_TEXT_PATH,
        "hidden",
        bool_to_binding(options.informative_text.empty())
    );
    add_binding_update(updates, SUPER_MESSAGE_DIALOG_ACTIONS_PATH, "hidden", bool_to_binding(options.buttons.empty()));

    const std::array<const char *, 3> button_paths = {
        SUPER_MESSAGE_DIALOG_BUTTON0_PATH,
        SUPER_MESSAGE_DIALOG_BUTTON1_PATH,
        SUPER_MESSAGE_DIALOG_BUTTON2_PATH,
    };
    const std::array<const char *, 3> label_paths = {
        SUPER_MESSAGE_DIALOG_BUTTON0_LABEL_PATH,
        SUPER_MESSAGE_DIALOG_BUTTON1_LABEL_PATH,
        SUPER_MESSAGE_DIALOG_BUTTON2_LABEL_PATH,
    };
    for (size_t i = 0; i < button_paths.size(); ++i) {
        const bool visible = i < options.buttons.size();
        add_binding_update(updates, button_paths[i], "hidden", bool_to_binding(!visible));
        add_binding_update(updates, label_paths[i], "text", visible ? options.buttons[i].text : std::string());
    }
    return context_->gui().set_binding_values(updates);
}

std::expected<void, std::string> ShellApp::clear_message_dialog_bindings()
{
    if (context_ == nullptr) {
        return std::unexpected("Shell app is not running");
    }

    std::vector<gui::BindingValueUpdate> updates;
    add_binding_update(updates, SUPER_MESSAGE_DIALOG_ICON_PATH, "src", "");
    add_binding_update(updates, SUPER_MESSAGE_DIALOG_ICON_PATH, "recolor", "");
    add_binding_update(updates, SUPER_MESSAGE_DIALOG_ICON_PATH, "recolorOpacity", int_to_binding(255));
    add_binding_update(updates, SUPER_MESSAGE_DIALOG_ICON_PATH, "hidden", "true");
    add_binding_update(updates, SUPER_MESSAGE_DIALOG_TEXT_PATH, "text", "");
    add_binding_update(updates, SUPER_MESSAGE_DIALOG_INFORMATIVE_TEXT_PATH, "text", "");
    add_binding_update(updates, SUPER_MESSAGE_DIALOG_INFORMATIVE_TEXT_PATH, "hidden", "true");
    add_binding_update(updates, SUPER_MESSAGE_DIALOG_ACTIONS_PATH, "hidden", "true");

    const std::array<const char *, 3> button_paths = {
        SUPER_MESSAGE_DIALOG_BUTTON0_PATH,
        SUPER_MESSAGE_DIALOG_BUTTON1_PATH,
        SUPER_MESSAGE_DIALOG_BUTTON2_PATH,
    };
    const std::array<const char *, 3> label_paths = {
        SUPER_MESSAGE_DIALOG_BUTTON0_LABEL_PATH,
        SUPER_MESSAGE_DIALOG_BUTTON1_LABEL_PATH,
        SUPER_MESSAGE_DIALOG_BUTTON2_LABEL_PATH,
    };
    for (size_t i = 0; i < button_paths.size(); ++i) {
        add_binding_update(updates, button_paths[i], "hidden", "true");
        add_binding_update(updates, label_paths[i], "text", "");
    }
    return context_->gui().set_binding_values(updates);
}

void ShellApp::start_message_dialog_auto_close_timer(int32_t auto_close_ms)
{
    stop_message_dialog_auto_close_timer();
    if (context_ == nullptr || auto_close_ms <= 0) {
        return;
    }

    auto timer = context_->timer().start_delayed(SUPER_MESSAGE_DIALOG_TIMEOUT_TIMER_NAME, auto_close_ms);
    if (!timer) {
        BROOKESIA_LOGW("Failed to start message dialog auto-close timer: %1%", timer.error());
        return;
    }
    message_dialog_auto_close_timer_id_ = *timer;
}

void ShellApp::stop_message_dialog_auto_close_timer()
{
    if (context_ != nullptr && message_dialog_auto_close_timer_id_ != core::INVALID_TIMER_ID) {
        (void)context_->timer().stop(message_dialog_auto_close_timer_id_);
    }
    message_dialog_auto_close_timer_id_ = core::INVALID_TIMER_ID;
}

void ShellApp::reset_message_dialog_state()
{
    active_message_dialog_request_id_.reset();
    active_message_dialog_owner_ = core::INVALID_APP_ID;
    active_message_dialog_options_ = {};
    message_dialog_closing_ = false;
}

std::expected<void, std::string> ShellApp::show_message_dialog(
    core::AppId app_id,
    core::MessageDialogRequestId request_id,
    const core::MessageDialogOptions &options
)
{
    if (active_message_dialog_request_id_.has_value()) {
        return std::unexpected("Message dialog is already active");
    }
    if (context_ == nullptr) {
        return std::unexpected("Shell app is not running");
    }

    reset_status_peek_session(false, true, "message dialog");
    if (gesture_tracking_ || gesture_exit_armed_ || gesture_exit_triggered_) {
        reset_gesture_indicator();
    }
    if (!system_ui_expanded_) {
        switch_display_to_gui_for_system_ui();
    }

    active_message_dialog_request_id_ = request_id;
    active_message_dialog_owner_ = app_id;
    active_message_dialog_options_ = options;
    message_dialog_needs_reset_ = false;

    auto mount_result = mount_message_dialog();
    if (!mount_result) {
        reset_message_dialog_state();
        return mount_result;
    }

    auto binding_result = refresh_message_dialog_bindings(options);
    if (!binding_result) {
        unmount_message_dialog();
        return binding_result;
    }

    start_message_dialog_auto_close_timer(options.auto_close_ms);
    return {};
}

std::expected<void, std::string> ShellApp::update_message_dialog(
    core::AppId app_id,
    core::MessageDialogRequestId request_id,
    const core::MessageDialogOptions &options
)
{
    if (!active_message_dialog_request_id_.has_value() || *active_message_dialog_request_id_ != request_id ||
            active_message_dialog_owner_ != app_id) {
        return std::unexpected("Message dialog is not active");
    }
    if (context_ == nullptr) {
        return std::unexpected("Shell app is not running");
    }

    active_message_dialog_options_ = options;
    auto binding_result = refresh_message_dialog_bindings(options);
    if (!binding_result) {
        return binding_result;
    }
    start_message_dialog_auto_close_timer(options.auto_close_ms);
    return {};
}

void ShellApp::hide_message_dialog(core::AppId app_id, core::MessageDialogRequestId request_id)
{
    if (!active_message_dialog_request_id_.has_value() || *active_message_dialog_request_id_ != request_id ||
            active_message_dialog_owner_ != app_id) {
        return;
    }

    message_dialog_closing_ = true;
    stop_message_dialog_auto_close_timer();
    if (context_ != nullptr && message_dialog_mounted_) {
        auto flow_result = context_->gui().trigger_screen_flow(
                               SUPER_MESSAGE_DIALOG_FLOW_ID,
                               SUPER_ACTION_MESSAGE_DIALOG_HIDE
                           );
        if (!flow_result) {
            BROOKESIA_LOGW("Failed to hide message dialog screen: %1%", flow_result.error());
        }
    }
    message_dialog_mounted_ = false;
    message_dialog_needs_reset_ = true;
    if (context_ != nullptr) {
        if (auto state_result = refresh_system_ui_state_bindings(); !state_result) {
            BROOKESIA_LOGW("Failed to refresh system UI mask after message dialog hide: %1%", state_result.error());
        }
    }
    reset_message_dialog_state();
}

void ShellApp::handle_message_dialog_idle()
{
    if (message_dialog_needs_reset_) {
        auto clear_result = clear_message_dialog_bindings();
        if (!clear_result) {
            BROOKESIA_LOGW("Failed to clear message dialog bindings: %1%", clear_result.error());
        }
        message_dialog_needs_reset_ = false;
    }
    if (!system_ui_expanded_) {
        restore_display_after_system_ui();
    }
}

void ShellApp::finish_message_dialog(int32_t button_index, core::MessageDialogCloseReason reason)
{
    if (!active_message_dialog_request_id_.has_value()) {
        return;
    }
    if (reason == core::MessageDialogCloseReason::Button &&
            (button_index < 0 || static_cast<size_t>(button_index) >= active_message_dialog_options_.buttons.size())) {
        return;
    }

    const auto request_id = *active_message_dialog_request_id_;
    const auto app_id = active_message_dialog_owner_;
    message_dialog_closing_ = true;
    stop_message_dialog_auto_close_timer();
    if (context_ != nullptr && message_dialog_mounted_) {
        auto flow_result = context_->gui().trigger_screen_flow(
                               SUPER_MESSAGE_DIALOG_FLOW_ID,
                               SUPER_ACTION_MESSAGE_DIALOG_HIDE
                           );
        if (!flow_result) {
            BROOKESIA_LOGW("Failed to hide message dialog screen: %1%", flow_result.error());
        }
    }
    message_dialog_mounted_ = false;
    message_dialog_needs_reset_ = true;
    if (context_ != nullptr) {
        if (auto state_result = refresh_system_ui_state_bindings(); !state_result) {
            BROOKESIA_LOGW("Failed to refresh system UI mask after message dialog finish: %1%", state_result.error());
        }
    }
    reset_message_dialog_state();

    auto result = owner_.complete_app_message_dialog(app_id, request_id, button_index, reason);
    if (!result) {
        BROOKESIA_LOGW("Failed to complete message dialog request: %1%", result.error());
    }
}

std::expected<void, std::string> ShellApp::set_system_ui_expanded(bool expanded)
{
    if (context_ == nullptr) {
        return {};
    }
    ++system_ui_animation_generation_;
    if (status_bar_animation_id_ != 0) {
        context_->gui().stop_animation(status_bar_animation_id_);
        status_bar_animation_id_ = 0;
    }

    if (expanded) {
        switch_display_to_gui_for_system_ui();
    }

    if (expanded == system_ui_expanded_) {
        auto state_result = refresh_system_ui_state_bindings();
        if (!state_result) {
            return state_result;
        }
        auto position_result = refresh_system_ui_position_bindings();
        if (!expanded) {
            restore_display_after_system_ui();
        }
        return position_result;
    }

    system_ui_expanded_ = expanded;
    auto binding_result = refresh_system_ui_state_bindings();
    if (!binding_result) {
        return binding_result;
    }

    const auto status_bar_target_y = expanded ?
                                     SUPER_STATUS_BAR_EXPANDED_Y :
                                     get_collapsed_status_bar_y(*context_);
    const auto generation = system_ui_animation_generation_;
    auto barrier = std::make_shared<AnimationCompletionBarrier>([this, generation, expanded]() {
        if (context_ == nullptr || generation != system_ui_animation_generation_) {
            return;
        }
        status_bar_animation_id_ = 0;
        auto result = refresh_system_ui_position_bindings();
        if (!result) {
            BROOKESIA_LOGW("Failed to finalize system UI animation bindings: %1%", result.error());
        }
        if (!expanded) {
            restore_display_after_system_ui();
        }
    });
    barrier->add();
    auto status_animation_result = context_->gui().start_view_animation_with_result(
                                       SUPER_STATUS_BAR_PATH,
                                       make_position_animation(gui::AnimationProperty::Y, status_bar_target_y),
    [barrier]() {
        barrier->complete();
    }
                                   );
    if (!status_animation_result) {
        (void)refresh_system_ui_position_bindings();
        if (!expanded) {
            restore_display_after_system_ui();
        }
        return std::unexpected(status_animation_result.error());
    }
    status_bar_animation_id_ = status_animation_result->subscription_id;
    barrier->arm();
    return {};
}

void ShellApp::disconnect_overlay_actions()
{
    for (auto &connection : overlay_action_connections_) {
        connection.disconnect();
    }
    overlay_action_connections_.clear();
}

} // namespace esp_brookesia::system::super
