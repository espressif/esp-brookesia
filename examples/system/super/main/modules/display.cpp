/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <string>
#include <utility>
#include <vector>

#include "boost/json/array.hpp"
#include "boost/json/object.hpp"
#include "boost/json/value.hpp"

#include "private/utils.hpp"
#include "brookesia/gui_lvgl.hpp"
#include "brookesia/lib_utils.hpp"
#include "brookesia/service_helper.hpp"
#include "display.hpp"

using namespace esp_brookesia;
using DisplayHelper = service::helper::Display;
using LvglDisplaySource = gui::lvgl::DisplaySource;

namespace {

constexpr uint32_t DISPLAY_SERVICE_TIMEOUT_MS = 1000;

} // namespace

bool Display::start(const Config &config)
{
    BROOKESIA_LOG_TRACE_GUARD();

    BROOKESIA_CHECK_NULL_RETURN(config.task_scheduler, false, "Task scheduler is null");

    task_scheduler_ = config.task_scheduler;
    gesture_data_ = config.gesture_data;

    BROOKESIA_CHECK_FALSE_RETURN(start_display_service(), false, "Failed to start display service");
    BROOKESIA_CHECK_FALSE_RETURN(start_lvgl_display_source(config.lvgl_task_core), false, "Failed to start LVGL");
    BROOKESIA_CHECK_FALSE_RETURN(set_active_lvgl_source(), false, "Failed to activate LVGL source");
    BROOKESIA_CHECK_FALSE_RETURN(start_gesture(), false, "Failed to start display gesture");

    if (display_backlight_on_off_supported_) {
        auto result = DisplayHelper::call_function_async(
                          DisplayHelper::FunctionId::SetBacklightOnOff, static_cast<double>(display_output_id_), true
                      );
        BROOKESIA_CHECK_FALSE_RETURN(result, false, "Failed to set backlight on");
    } else {
        BROOKESIA_LOGI("Display output %1% has no on/off backlight control; skip turning backlight on",
                       display_output_name_);
    }

    return true;
}

bool Display::start_display_service()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_FALSE_RETURN(DisplayHelper::is_available(), false, "Display service is not available");

    display_service_binding_ = service::ServiceManager::get_instance().bind(DisplayHelper::get_name().data());
    BROOKESIA_CHECK_FALSE_RETURN(display_service_binding_.is_valid(), false, "Failed to bind Display service");

    auto outputs_json = DisplayHelper::call_function_sync<boost::json::array>(
                            DisplayHelper::FunctionId::GetOutputs,
                            service::helper::Timeout(DISPLAY_SERVICE_TIMEOUT_MS)
                        );
    BROOKESIA_CHECK_FALSE_RETURN(
        outputs_json.has_value(), false, "Failed to get Display outputs: %1%", outputs_json.error()
    );

    std::vector<DisplayHelper::OutputInfo> outputs;
    BROOKESIA_CHECK_FALSE_RETURN(
        BROOKESIA_DESCRIBE_FROM_JSON(boost::json::value(outputs_json.value()), outputs), false,
        "Failed to parse Display outputs"
    );
    BROOKESIA_CHECK_FALSE_RETURN(!outputs.empty(), false, "No Display output is available");

    const auto &main_output = outputs.front();
    BROOKESIA_CHECK_FALSE_RETURN(
        (main_output.width > 0) && (main_output.height > 0), false,
        "Invalid Display output size: %1%x%2%", main_output.width, main_output.height
    );

    display_output_name_ = main_output.name;
    display_output_id_ = main_output.id;
    display_width_ = main_output.width;
    display_height_ = main_output.height;
    display_backlight_on_off_supported_ =
        main_output.backlight.has_value() && main_output.backlight->on_off_supported;
    BROOKESIA_LOGI("Using Display output %1%", main_output);

    return true;
}

bool Display::start_lvgl_display_source(int core_id)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    gui::lvgl::DisplaySourceConfig config{};
    config.output_name = "";
    config.task_core_id = core_id;
    config.task_stack_size = 40 * 1024;

    auto &source = LvglDisplaySource::get_instance();
    BROOKESIA_CHECK_FALSE_RETURN(source.start(config), false, "Failed to start LVGL Display source");

    return true;
}

bool Display::set_active_lvgl_source()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto result = DisplayHelper::call_function_sync(
                      DisplayHelper::FunctionId::SetActiveSourceRole,
                      std::string(),
                      std::string(gui::lvgl::DISPLAY_SOURCE_ROLE),
                      service::helper::Timeout(DISPLAY_SERVICE_TIMEOUT_MS)
                  );
    BROOKESIA_CHECK_FALSE_RETURN(result.has_value(), false, "Failed to activate LVGL source: %1%", result.error());

    return true;
}

bool Display::start_gesture()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto config = gesture_data_;
    config.enabled = true;
    auto config_json = BROOKESIA_DESCRIBE_TO_JSON(config).as_object();
    auto result = DisplayHelper::call_function_sync(
                      DisplayHelper::FunctionId::SetTouchGestureConfig,
                      static_cast<double>(display_output_id_),
                      config_json,
                      service::helper::Timeout(DISPLAY_SERVICE_TIMEOUT_MS)
                  );
    BROOKESIA_CHECK_FALSE_RETURN(
        result.has_value(), false, "Failed to configure Display touch gesture: %1%", result.error()
    );

    auto gesture_slot = [this](const std::string & event_name, const boost::json::object & info_json) {
        GestureInfo info;
        if (!BROOKESIA_DESCRIBE_FROM_JSON(boost::json::value(info_json), info)) {
            BROOKESIA_LOGE("Failed to parse Display touch gesture event");
            return;
        }
        handle_touch_gesture_event(event_name, info);
    };
    gesture_event_connection_ = DisplayHelper::subscribe_event(DisplayHelper::EventId::TouchGesture, gesture_slot);
    BROOKESIA_CHECK_FALSE_RETURN(
        gesture_event_connection_.connected(), false, "Failed to subscribe Display touch gesture event"
    );

    BROOKESIA_LOGI("Display touch gesture enabled for output %1%", display_output_name_);
    return true;
}

void Display::handle_touch_gesture_event(const std::string &event_name, const GestureInfo &info)
{
    if (info.output_name != display_output_name_) {
        return;
    }

    // BROOKESIA_LOGI(
    //     "Display touch gesture placeholder: event(%1%), output(%2%), type(%3%), direction(%4%), "
    //     "start_area(%5%), stop_area(%6%), start(%7%,%8%), stop(%9%,%10%), duration(%11%ms), "
    //     "speed(%12%px/ms), distance(%13%px), slow(%14%), short(%15%)",
    //     event_name, info.output_name, info.event_type, info.direction,
    //     static_cast<int>(info.start_area), static_cast<int>(info.stop_area),
    //     info.start_x, info.start_y, info.stop_x, info.stop_y, info.duration_ms,
    //     info.speed_px_per_ms, info.distance_px, info.flags_slow_speed, info.flags_short_duration
    // );
}
