/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <limits>
#include "esp_lcd_touch.h"
#include "esp_lv_adapter.h"
#include "private/utils.hpp"
#include "brookesia/lib_utils.hpp"
#include "brookesia/service_helper.hpp"
#include "brookesia/expression_emote.hpp"
#include "brookesia/hal_interface.hpp"
#include "screens/settings.hpp"
#include "screens/emote.hpp"
#include "display.hpp"

using namespace esp_brookesia;
using EmoteHelper = service::helper::ExpressionEmote;
using NVSHelper = service::helper::NVS;

namespace {
constexpr uint32_t LOAD_ASSETS_TIMEOUT_MS = 10000;
constexpr float PI = 3.14159265358979323846F;
constexpr size_t TOUCH_READ_MAX_POINTS = 1;
constexpr auto NVS_NAMESPACE = "Settings";
constexpr auto NVS_KEY_BRIGHTNESS = "brightness";

constexpr uint8_t to_area_mask(Display::GestureArea area)
{
    return static_cast<uint8_t>(area);
}

uint64_t get_current_time_ms()
{
    return static_cast<uint64_t>(
               std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now().time_since_epoch()
               ).count()
           );
}
} // namespace

bool Display::start(const Config &config)
{
    BROOKESIA_LOG_TRACE_GUARD();

    BROOKESIA_CHECK_NULL_RETURN(config.task_scheduler, false, "Task scheduler is null");

    auto [display_name, display_iface] = hal::get_first_interface<hal::DisplayPanelIface>();
    BROOKESIA_CHECK_NULL_RETURN(display_iface, false, "Failed to get display interface");
    display_iface_ = display_iface;

    auto [touch_name, touch_iface] = hal::get_first_interface<hal::DisplayTouchIface>();
    BROOKESIA_CHECK_NULL_RETURN(touch_iface, false, "Failed to get touch interface");
    touch_iface_ = touch_iface;

    auto [backlight_name, backlight_iface] = hal::get_first_interface<hal::DisplayBacklightIface>();
    if (backlight_iface) {
        backlight_iface_ = backlight_iface;
    }

    task_scheduler_ = config.task_scheduler;
    gesture_data_ = config.gesture_data;

    // Start LVGL and expression emote first
    BROOKESIA_CHECK_FALSE_RETURN(
        start_lvgl(config.lvgl_task_core), false, "Failed to start LVGL"
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        start_expression_emote(config.emote_task_core), false, "Failed to start expression emote"
    );
    BROOKESIA_CHECK_FALSE_RETURN(start_ui_state_machine(), false, "Failed to start UI state machine");

    // Start gesture detection
    BROOKESIA_CHECK_FALSE_RETURN(start_gesture(config.gesture_thread_config), false, "Failed to start gesture");
    // Monitor pressing event
    auto pressing_slot = [this](const GestureInfo & info) {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

        if (is_ui_state_action_triggered_) {
            return;
        }

        // BROOKESIA_LOGI("Gesture pressing: %1%", info);

        if (info.direction != GestureDirection::None) {
            auto action = get_ui_action_from_gesture(info);
            if (action == DisplayAction::Max) {
                return;
            }

            auto action_ret = ui_state_machine_->trigger_action(BROOKESIA_DESCRIBE_TO_STR(action));
            BROOKESIA_CHECK_FALSE_EXIT(action_ret, "Failed to trigger action");

            is_ui_state_action_triggered_ = true;
        }
    };
    ui_state_gesture_pressing_connection_ = connect_gesture_pressing_signal(pressing_slot);
    BROOKESIA_CHECK_FALSE_RETURN(
        ui_state_gesture_pressing_connection_.connected(), false, "Failed to connect gesture pressing signal"
    );
    // Monitor release event
    auto release_slot = [this](const GestureInfo & info) {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

        // BROOKESIA_LOGI("Gesture release: %1%", info);

        is_ui_state_action_triggered_ = false;
    };
    ui_state_gesture_release_connection_ = connect_gesture_release_signal(release_slot);
    BROOKESIA_CHECK_FALSE_RETURN(
        ui_state_gesture_release_connection_.connected(), false, "Failed to connect gesture release signal"
    );

    try_load_data_from_nvs();

    return true;
}

void Display::try_load_data_from_nvs()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!NVSHelper::is_available()) {
        BROOKESIA_LOGW("NVS is not available, skip");
        return;
    }

    auto binding = service::ServiceManager::get_instance().bind(NVSHelper::get_name().data());
    BROOKESIA_CHECK_FALSE_EXIT(binding.is_valid(), "Failed to bind NVS service");

    if (is_backlight_available()) {
        auto result = NVSHelper::get_key_value<uint8_t>(NVS_NAMESPACE, NVS_KEY_BRIGHTNESS);
        if (!result) {
            BROOKESIA_LOGW("Failed to get brightness from NVS: %1%", result.error());
            BROOKESIA_CHECK_FALSE_EXECUTE(backlight_iface_->turn_on(), {}, {
                BROOKESIA_LOGE("Failed to turn on backlight");
            });
        } else {
            BROOKESIA_CHECK_FALSE_EXECUTE(set_brightness(result.value()), {}, {
                BROOKESIA_LOGE("Failed to set brightness");
            });
        }
    }
}

void Display::try_reset_data()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!NVSHelper::is_available()) {
        BROOKESIA_LOGW("NVS is not available, skip");
        return;
    }

    auto binding = service::ServiceManager::get_instance().bind(NVSHelper::get_name().data());
    BROOKESIA_CHECK_FALSE_EXIT(binding.is_valid(), "Failed to bind NVS service");

    auto result = NVSHelper::erase_keys(NVS_NAMESPACE, {});
    BROOKESIA_CHECK_FALSE_EXIT(result, "Failed to erase keys from NVS: %1%", result.error());
}

bool Display::set_brightness(uint8_t brightness)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_backlight_available()) {
        BROOKESIA_LOGW("Backlight interface is not available, skip");
        return false;
    }

    uint8_t original_brightness = 0;
    bool get_original_ret = get_brightness(original_brightness);
    BROOKESIA_CHECK_FALSE_RETURN(get_original_ret, false, "Failed to get original brightness");

    bool set_ret = backlight_iface_->set_brightness(brightness);
    BROOKESIA_CHECK_FALSE_RETURN(set_ret, false, "Failed to set brightness");

    uint8_t new_brightness = 0;
    bool get_new_ret = get_brightness(new_brightness);
    BROOKESIA_CHECK_FALSE_RETURN(get_new_ret, false, "Failed to get new brightness");

    if ((original_brightness != new_brightness) && NVSHelper::is_available()) {
        auto binding = service::ServiceManager::get_instance().bind(NVSHelper::get_name().data());
        BROOKESIA_CHECK_FALSE_RETURN(binding.is_valid(), false, "Failed to bind NVS service");

        auto result = NVSHelper::save_key_value(NVS_NAMESPACE, NVS_KEY_BRIGHTNESS, new_brightness);
        BROOKESIA_CHECK_FALSE_RETURN(result, false, "Failed to set brightness in NVS: %1%", result.error());
    }

    return true;
}

bool Display::get_brightness(uint8_t &brightness)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_backlight_available()) {
        BROOKESIA_LOGW("Backlight interface is not available, skip");
        return false;
    }

    return backlight_iface_->get_brightness(brightness);
}

bool Display::start_lvgl(int core_id)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    // init esp lvgl adapter
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    esp_lv_adapter_config_t adapter_config = ESP_LV_ADAPTER_DEFAULT_CONFIG();
#pragma GCC diagnostic pop
    adapter_config.task_priority = 6;
    adapter_config.task_core_id = core_id;
    adapter_config.tick_period_ms = 5;
    adapter_config.task_min_delay_ms = 10;
    adapter_config.task_max_delay_ms = 100;
    adapter_config.stack_in_psram = true;
    BROOKESIA_CHECK_ESP_ERR_RETURN(esp_lv_adapter_init(&adapter_config), false, "Failed to initialize LVGL adapter");

    hal::DisplayPanelIface::DriverSpecific driver_specific;
    BROOKESIA_CHECK_FALSE_RETURN(
        display_iface_->get_driver_specific(driver_specific), false, "Failed to get driver specific"
    );
    auto &display_info = display_iface_->get_info();
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    esp_lv_adapter_display_config_t display_config {0};
    switch (driver_specific.bus_type) {
    case hal::DisplayPanelIface::BusType::Generic:
        display_config = ESP_LV_ADAPTER_DISPLAY_SPI_WITHOUT_PSRAM_DEFAULT_CONFIG(
                             reinterpret_cast<esp_lcd_panel_handle_t>(driver_specific.panel_handle),
                             reinterpret_cast<esp_lcd_panel_io_handle_t>(driver_specific.io_handle),
                             static_cast<uint16_t>(display_info.h_res),
                             static_cast<uint16_t>(display_info.v_res),
                             ESP_LV_ADAPTER_ROTATE_0
                         );
        break;
    case hal::DisplayPanelIface::BusType::MIPI:
        display_config = ESP_LV_ADAPTER_DISPLAY_MIPI_DEFAULT_CONFIG(
                             reinterpret_cast<esp_lcd_panel_handle_t>(driver_specific.panel_handle),
                             reinterpret_cast<esp_lcd_panel_io_handle_t>(driver_specific.io_handle),
                             static_cast<uint16_t>(display_info.h_res),
                             static_cast<uint16_t>(display_info.v_res),
                             ESP_LV_ADAPTER_ROTATE_0
                         );
        break;
    default:
        BROOKESIA_LOGE("Unsupported bus type: %1%", driver_specific.bus_type);
        return false;
    }
#pragma GCC diagnostic pop
    display_config.profile.require_double_buffer = false;
    display_config.profile.buffer_height = 20;

    lv_display_t *lv_display = esp_lv_adapter_register_display(&display_config);
    if (!lv_display) {
        BROOKESIA_LOGE("Failed to register display");
        return false;
    }

    hal::DisplayTouchIface::DriverSpecific touch_driver_specific;
    BROOKESIA_CHECK_FALSE_RETURN(
        touch_iface_->get_driver_specific(touch_driver_specific), false, "Failed to get driver specific"
    );
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    esp_lv_adapter_touch_config_t touch_config = ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(
                lv_display, reinterpret_cast<esp_lcd_touch_handle_t>(touch_driver_specific.touch_handle)
            );
#pragma GCC diagnostic pop
    lv_indev_t *touch = esp_lv_adapter_register_touch(&touch_config);
    if (!touch) {
        BROOKESIA_LOGE("Failed to register touch");
        return false;
    }

    auto start_ret = esp_lv_adapter_start();
    BROOKESIA_CHECK_ESP_ERR_RETURN(start_ret, false, "Failed to start LVGL adapter");

    return true;
}

bool Display::start_expression_emote(int core_id)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!EmoteHelper::is_available()) {
        BROOKESIA_LOGW("Emote is not available, skip initialization");
        return true;
    }

    BROOKESIA_LOGI("Initializing emote...");

    hal::DisplayPanelIface::DriverSpecific driver_specific;
    BROOKESIA_CHECK_FALSE_RETURN(
        display_iface_->get_driver_specific(driver_specific), false, "Failed to get driver specific"
    );
    auto &display_info = display_iface_->get_info();
    // Set emote config
    EmoteHelper::Config config{
        .h_res = display_info.h_res,
        .v_res = display_info.v_res,
        .buf_pixels = static_cast<size_t>(display_info.h_res * 16),
        .fps = 30,
        .task_priority = 5,
        .task_stack = 10 * 1024,
        .task_affinity = core_id,
        .task_stack_in_ext = true,
        .flag_swap_color_bytes = (driver_specific.bus_type == hal::DisplayPanelIface::BusType::Generic),
        .flag_double_buffer = true,
        .flag_buff_dma = true,
    };
    auto result = EmoteHelper::call_function_sync(
                      EmoteHelper::FunctionId::SetConfig, BROOKESIA_DESCRIBE_TO_JSON(config).as_object()
                  );
    BROOKESIA_CHECK_FALSE_RETURN(result.has_value(), false, "Failed to set emote config: %1%", result.error());

    // Subscribe to flush ready event
    auto flush_ready_event_slot = [&](const std::string & event_name, const boost::json::object & param_json) {
        lib_utils::FunctionGuard notify_guard([]() {
            // BROOKESIA_LOG_TRACE_GUARD();
            expression::Emote::get_instance().native_notify_flush_finished();
        });

        if (!esp_lv_adapter_get_dummy_draw_enabled(lv_display_get_default())) {
            return;
        }

        EmoteHelper::FlushReadyEventParam param;
        auto success = BROOKESIA_DESCRIBE_FROM_JSON(param_json, param);
        BROOKESIA_CHECK_FALSE_EXIT(success, "Failed to parse flush ready event param: %1%");

        auto ret = esp_lv_adapter_dummy_draw_blit(
                       lv_display_get_default(), param.x_start, param.y_start, param.x_end, param.y_end, param.data, true
                   );
        BROOKESIA_CHECK_ESP_ERR_EXIT(ret, "Failed to draw bitmap");
    };
    static auto connection = EmoteHelper::subscribe_event(EmoteHelper::EventId::FlushReady, flush_ready_event_slot);
    BROOKESIA_CHECK_FALSE_RETURN(connection.connected(), false, "Failed to subscribe to flush ready event");

    static auto binding = service::ServiceManager::get_instance().bind(EmoteHelper::get_name().data());
    BROOKESIA_CHECK_FALSE_RETURN(binding.is_valid(), false, "Failed to bind Emote service");

    // Load emote assets
    {
        EmoteHelper::AssetSource source{
            .source = ASSETS_PARTITION_NAME,
            .type = EmoteHelper::AssetSourceType::PartitionLabel,
            .flag_enable_mmap = false,
        };
        auto result = EmoteHelper::call_function_sync(
                          EmoteHelper::FunctionId::LoadAssetsSource, BROOKESIA_DESCRIBE_TO_JSON(source).as_object(),
                          service::helper::Timeout(LOAD_ASSETS_TIMEOUT_MS)
                      );
        BROOKESIA_CHECK_FALSE_RETURN(result.has_value(), false, "Failed to load emote assets: %1%", result.error());
    }

    // Set idle event message
    {
        auto result = EmoteHelper::call_function_sync(
                          EmoteHelper::FunctionId::SetEventMessage, BROOKESIA_DESCRIBE_TO_STR(EmoteHelper::EventMessageType::Idle)
                      );
        BROOKESIA_CHECK_FALSE_RETURN(result.has_value(), false, "Failed to set emote event message: %1%", result.error());
    }

    return true;
}

bool Display::start_gesture(const esp_brookesia::lib_utils::ThreadConfig &thread_config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!check_gesture_data(gesture_data_)) {
        auto &display_info = display_iface_->get_info();
        gesture_data_ = build_default_gesture_data(display_info.h_res, display_info.v_res);
    }
    BROOKESIA_CHECK_FALSE_RETURN(check_gesture_data(gesture_data_), false, "Invalid gesture data");

    {
        boost::lock_guard<boost::mutex> lock(gesture_mutex_);
        gesture_direction_tan_threshold_ = std::tan((static_cast<float>(gesture_data_.threshold.direction_angle) * PI) / 180.0F);
        gesture_info_ = GestureInfo{};
        gesture_touch_start_time_ms_ = 0;
        gesture_detection_started_ = false;
    }

    {
        BROOKESIA_THREAD_CONFIG_GUARD(thread_config);
        gesture_thread_ = boost::thread([this]() {
            BROOKESIA_LOG_TRACE_GUARD();
            while (true) {
                process_gesture_tick();
                boost::this_thread::sleep_for(boost::chrono::milliseconds(gesture_data_.detect_period_ms));
            }
        });
    }

    BROOKESIA_LOGI("Gesture detection started(period=%1%ms)", gesture_data_.detect_period_ms);

    return true;
}

void Display::stop_gesture()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard<boost::mutex> lock(gesture_mutex_);
    gesture_info_ = GestureInfo{};
    gesture_touch_start_time_ms_ = 0;
    gesture_detection_started_ = false;
}

uint8_t Display::get_gesture_area(int x, int y) const
{
    uint8_t area = to_area_mask(GestureArea::Center);

    auto &gesture_data = gesture_data_;
    auto &display_info = display_iface_->get_info();
    area |= (y < gesture_data.threshold.vertical_edge) ? to_area_mask(GestureArea::TopEdge) : 0;
    area |= ((static_cast<int>(display_info.v_res) - y) < gesture_data.threshold.vertical_edge) ?
            to_area_mask(GestureArea::BottomEdge) : 0;
    area |= (x < gesture_data.threshold.horizontal_edge) ? to_area_mask(GestureArea::LeftEdge) : 0;
    area |= ((static_cast<int>(display_info.h_res) - x) < gesture_data.threshold.horizontal_edge) ?
            to_area_mask(GestureArea::RightEdge) : 0;

    return area;
}

bool Display::process_gesture_tick()
{
    enum class GestureEventType {
        None,
        Press,
        Pressing,
        Release,
    };

    GestureEventType event_type = GestureEventType::None;
    GestureInfo event_info;

    std::vector<hal::DisplayTouchIface::Point> points;
    auto read_ret = touch_iface_->read_points(points);
    BROOKESIA_CHECK_FALSE_RETURN(read_ret, false, "Failed to read touch point");

    auto &gesture_data = gesture_data_;
    bool touched = !points.empty();
    {
        boost::lock_guard<boost::mutex> lock(gesture_mutex_);

        if (touched) {
            gesture_info_.stop_x = points[0].x;
            gesture_info_.stop_y = points[0].y;
            gesture_info_.stop_area = get_gesture_area(points[0].x, points[0].y);
        }

        if (!gesture_detection_started_ && !touched) {
            return true;
        }

        if (!gesture_detection_started_ && touched) {
            gesture_detection_started_ = true;
            gesture_touch_start_time_ms_ = get_current_time_ms();
            gesture_info_ = GestureInfo{};
            gesture_info_.start_x = points[0].x;
            gesture_info_.start_y = points[0].y;
            gesture_info_.stop_x = points[0].x;
            gesture_info_.stop_y = points[0].y;
            gesture_info_.start_area = get_gesture_area(points[0].x, points[0].y);
            gesture_info_.stop_area = gesture_info_.start_area;

            event_type = GestureEventType::Press;
            event_info = gesture_info_;
        } else {
            auto current_time_ms = get_current_time_ms();
            gesture_info_.duration_ms = static_cast<uint32_t>(current_time_ms - gesture_touch_start_time_ms_);
            gesture_info_.flags_short_duration = (gesture_info_.duration_ms < static_cast<uint32_t>(gesture_data.threshold.duration_short_ms));

            int distance_x = gesture_info_.stop_x - gesture_info_.start_x;
            int distance_y = gesture_info_.stop_y - gesture_info_.start_y;
            if ((distance_x != 0) || (distance_y != 0)) {
                gesture_info_.distance_px = std::sqrt(static_cast<float>((distance_x * distance_x) + (distance_y * distance_y)));
                gesture_info_.speed_px_per_ms = (gesture_info_.duration_ms > 0) ?
                                                (gesture_info_.distance_px / static_cast<float>(gesture_info_.duration_ms)) :
                                                std::numeric_limits<float>::infinity();
                gesture_info_.flags_slow_speed = (gesture_info_.speed_px_per_ms < gesture_data.threshold.speed_slow_px_per_ms);

                float distance_tan = (distance_x == 0) ?
                                     std::numeric_limits<float>::infinity() :
                                     (static_cast<float>(distance_y) / static_cast<float>(distance_x));
                if ((distance_tan == std::numeric_limits<float>::infinity()) ||
                        (distance_tan > gesture_direction_tan_threshold_) ||
                        (distance_tan < -gesture_direction_tan_threshold_)) {
                    if (distance_y > gesture_data.threshold.direction_vertical) {
                        gesture_info_.direction = GestureDirection::Down;
                    } else if (distance_y < -gesture_data.threshold.direction_vertical) {
                        gesture_info_.direction = GestureDirection::Up;
                    }
                } else {
                    if (distance_x > gesture_data.threshold.direction_horizon) {
                        gesture_info_.direction = GestureDirection::Right;
                    } else if (distance_x < -gesture_data.threshold.direction_horizon) {
                        gesture_info_.direction = GestureDirection::Left;
                    }
                }
            }

            event_type = touched ? GestureEventType::Pressing : GestureEventType::Release;
            event_info = gesture_info_;
            if (!touched) {
                gesture_info_ = GestureInfo{};
                gesture_touch_start_time_ms_ = 0;
                gesture_detection_started_ = false;
            }
        }
    }

    switch (event_type) {
    case GestureEventType::Press:
        gesture_press_signal_(event_info);
        break;
    case GestureEventType::Pressing:
        gesture_pressing_signal_(event_info);
        break;
    case GestureEventType::Release:
        gesture_release_signal_(event_info);
        break;
    case GestureEventType::None:
    default:
        break;
    }

    return true;
}

bool Display::check_gesture_data(const GestureData &data) const
{
    if (data.detect_period_ms == 0) {
        return false;
    }
    if ((data.threshold.direction_vertical <= 0) || (data.threshold.direction_horizon <= 0)) {
        return false;
    }
    if ((data.threshold.direction_angle == 0) || (data.threshold.direction_angle >= 90)) {
        return false;
    }
    if ((data.threshold.horizontal_edge <= 0) || (data.threshold.vertical_edge <= 0)) {
        return false;
    }
    if (data.threshold.duration_short_ms <= 0) {
        return false;
    }
    if (data.threshold.speed_slow_px_per_ms <= 0) {
        return false;
    }
    auto &display_info = display_iface_->get_info();
    if ((data.threshold.direction_horizon > static_cast<int>(display_info.h_res)) ||
            (data.threshold.horizontal_edge > static_cast<int>(display_info.h_res))) {
        return false;
    }
    if ((data.threshold.direction_vertical > static_cast<int>(display_info.v_res)) ||
            (data.threshold.vertical_edge > static_cast<int>(display_info.v_res))) {
        return false;
    }

    return true;
}

Display::GestureData Display::build_default_gesture_data(uint32_t h_res, uint32_t v_res)
{
    GestureData data;
    data.detect_period_ms = 20;
    data.threshold.direction_horizon = std::max(24, static_cast<int>(h_res / 6));
    data.threshold.direction_vertical = std::max(24, static_cast<int>(v_res / 6));
    data.threshold.direction_angle = 45;
    data.threshold.horizontal_edge = std::max(12, static_cast<int>(h_res / 10));
    data.threshold.vertical_edge = std::max(12, static_cast<int>(v_res / 10));
    data.threshold.duration_short_ms = 220;
    data.threshold.speed_slow_px_per_ms = 0.6F;
    return data;
}

bool Display::start_ui_state_machine()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_EXCEPTION_RETURN(
        ui_state_machine_ = std::make_unique<lib_utils::StateMachine>(), false, "Failed to create state machine"
    );

    // Create states
    std::shared_ptr<ScreenSettings> screen_settings;
    std::shared_ptr<ScreenEmote> screen_emote;
    {
        // Lock LVGL adapter to ensure thread-safe operations
        esp_lv_adapter_lock(-1);
        lib_utils::FunctionGuard unlock_guard([this]() {
            esp_lv_adapter_unlock();
        });
        BROOKESIA_CHECK_EXCEPTION_RETURN(
            screen_settings = std::make_shared<ScreenSettings>(), false, "Failed to create state"
        );
        BROOKESIA_CHECK_EXCEPTION_RETURN(
            screen_emote = std::make_shared<ScreenEmote>(), false, "Failed to create state"
        );
    }

    // Add states to state machine
    auto add_settings_ret = ui_state_machine_->add_state(screen_settings);
    BROOKESIA_CHECK_FALSE_RETURN(add_settings_ret, false, "Failed to add state");
    auto add_emote_ret = ui_state_machine_->add_state(screen_emote);
    BROOKESIA_CHECK_FALSE_RETURN(add_emote_ret, false, "Failed to add state");

    // Add transitions between states
    auto action_scroll_left = BROOKESIA_DESCRIBE_TO_STR(DisplayAction::ScrollLeft);
    auto action_scroll_right = BROOKESIA_DESCRIBE_TO_STR(DisplayAction::ScrollRight);
    auto action_scroll_up = BROOKESIA_DESCRIBE_TO_STR(DisplayAction::ScrollUp);
    auto action_scroll_down = BROOKESIA_DESCRIBE_TO_STR(DisplayAction::ScrollDown);
    auto action_edge_scroll_left = BROOKESIA_DESCRIBE_TO_STR(DisplayAction::EdgeScrollLeft);
    auto action_edge_scroll_right = BROOKESIA_DESCRIBE_TO_STR(DisplayAction::EdgeScrollRight);
    auto action_edge_scroll_up = BROOKESIA_DESCRIBE_TO_STR(DisplayAction::EdgeScrollUp);
    auto action_edge_scroll_down = BROOKESIA_DESCRIBE_TO_STR(DisplayAction::EdgeScrollDown);
    // screen settings -> screen emote
    BROOKESIA_CHECK_FALSE_RETURN(
        ui_state_machine_->add_transition(
            screen_settings->get_name(), action_edge_scroll_left, screen_emote->get_name()
        ), false, "Failed to add transition"
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        ui_state_machine_->add_transition(
            screen_settings->get_name(), action_edge_scroll_right, screen_emote->get_name()
        ), false, "Failed to add transition"
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        ui_state_machine_->add_transition(
            screen_settings->get_name(), action_edge_scroll_up, screen_emote->get_name()
        ), false, "Failed to add transition"
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        ui_state_machine_->add_transition(
            screen_settings->get_name(), action_edge_scroll_down, screen_emote->get_name()
        ), false, "Failed to add transition"
    );
    // screen emote -> screen settings
    BROOKESIA_CHECK_FALSE_RETURN(
        ui_state_machine_->add_transition(
            screen_emote->get_name(), action_scroll_left, screen_settings->get_name()
        ), false, "Failed to add transition"
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        ui_state_machine_->add_transition(
            screen_emote->get_name(), action_scroll_right, screen_settings->get_name()
        ), false, "Failed to add transition"
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        ui_state_machine_->add_transition(
            screen_emote->get_name(), action_scroll_up, screen_settings->get_name()
        ), false, "Failed to add transition"
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        ui_state_machine_->add_transition(
            screen_emote->get_name(), action_scroll_down, screen_settings->get_name()
        ), false, "Failed to add transition"
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        ui_state_machine_->add_transition(
            screen_emote->get_name(), action_edge_scroll_left, screen_settings->get_name()
        ), false, "Failed to add transition"
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        ui_state_machine_->add_transition(
            screen_emote->get_name(), action_edge_scroll_right, screen_settings->get_name()
        ), false, "Failed to add transition"
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        ui_state_machine_->add_transition(
            screen_emote->get_name(), action_edge_scroll_up, screen_settings->get_name()
        ), false, "Failed to add transition"
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        ui_state_machine_->add_transition(
            screen_emote->get_name(), action_edge_scroll_down, screen_settings->get_name()
        ), false, "Failed to add transition"
    );

    // Start state machine
    auto pre_execute_callback =
        [this](const lib_utils::TaskScheduler::Group & group, lib_utils::TaskScheduler::TaskId id,
    lib_utils::TaskScheduler::TaskType type) {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        esp_lv_adapter_lock(-1);
    };
    auto post_execute_callback =
        [this](const lib_utils::TaskScheduler::Group & group, lib_utils::TaskScheduler::TaskId id,
    lib_utils::TaskScheduler::TaskType type, bool success) {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        esp_lv_adapter_unlock();
    };
    auto start_ret = ui_state_machine_->start({
        .task_scheduler = task_scheduler_,
        .task_group_name = TASK_GROUP_NAME,
        .task_group_config = {
            .pre_execute_callback = pre_execute_callback,
            .post_execute_callback = post_execute_callback,
        },
        .initial_state = screen_emote->get_name(),
    });
    BROOKESIA_CHECK_FALSE_RETURN(start_ret, false, "Failed to start state machine");

    return true;
}

DisplayAction Display::get_ui_action_from_gesture(const GestureInfo &info) const
{
    if ((info.start_area & static_cast<uint8_t>(GestureArea::TopEdge)) && (info.direction == GestureDirection::Down)) {
        return DisplayAction::EdgeScrollDown;
    } else if ((info.start_area & static_cast<uint8_t>(GestureArea::BottomEdge)) &&
               (info.direction == GestureDirection::Up)) {
        return DisplayAction::EdgeScrollUp;
    } else if ((info.start_area & static_cast<uint8_t>(GestureArea::LeftEdge)) &&
               (info.direction == GestureDirection::Right)) {
        return DisplayAction::EdgeScrollRight;
    } else if ((info.start_area & static_cast<uint8_t>(GestureArea::RightEdge)) &&
               (info.direction == GestureDirection::Left)) {
        return DisplayAction::EdgeScrollLeft;
    } else {
        if (info.direction == GestureDirection::Left) {
            return DisplayAction::ScrollLeft;
        } else if (info.direction == GestureDirection::Right) {
            return DisplayAction::ScrollRight;
        } else if (info.direction == GestureDirection::Up) {
            return DisplayAction::ScrollUp;
        } else if (info.direction == GestureDirection::Down) {
            return DisplayAction::ScrollDown;
        }
    }

    return DisplayAction::Max;
}

bool Display::send_display_task(esp_brookesia::lib_utils::TaskScheduler::OnceTask &&task)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_NULL_RETURN(task_scheduler_, false, "Task scheduler is null");

    auto result = task_scheduler_->post(std::move(task), nullptr, Display::TASK_GROUP_NAME);
    BROOKESIA_CHECK_FALSE_RETURN(result, false, "Failed to post task function");

    return true;
}
