/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <cstdint>
#include <string>
#include <vector>
#include "boost/json/array.hpp"
#include "boost/json/object.hpp"
#include "boost/json/value.hpp"
#include "esp_lv_adapter.h"
#include "private/utils.hpp"
#include "brookesia/lib_utils.hpp"
#include "brookesia/service_helper.hpp"
#include "brookesia/expression_emote.hpp"
#include "brookesia/gui_lvgl.hpp"
#include "screens/settings.hpp"
#include "screens/emote.hpp"
#include "display.hpp"

using namespace esp_brookesia;
using EmoteHelper = service::helper::ExpressionEmote;
using DisplayHelper = service::helper::Display;
using VideoHelper = service::helper::Video;
using LvglDisplaySource = gui::lvgl::DisplaySource;

namespace {

constexpr uint32_t BACKLIGHT_ON_DELAY_MS = 1000;
constexpr uint32_t LOAD_ASSETS_TIMEOUT_MS = 10000;
constexpr uint32_t DISPLAY_SERVICE_TIMEOUT_MS = 1000;

} // namespace

bool Display::start(const Config &config)
{
    BROOKESIA_LOG_TRACE_GUARD();

    BROOKESIA_CHECK_NULL_RETURN(config.task_scheduler, false, "Task scheduler is null");

    task_scheduler_ = config.task_scheduler;
    gesture_data_ = config.gesture_data;

    BROOKESIA_CHECK_FALSE_RETURN(start_display_service(), false, "Failed to start display service");

    // Start LVGL and expression emote first
    BROOKESIA_CHECK_FALSE_RETURN(
        start_lvgl_display_source(), false, "Failed to start LVGL"
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        start_expression_emote_assets(), false, "Failed to start expression emote"
    );
    BROOKESIA_CHECK_FALSE_RETURN(set_active_source_role(DrawSource::Lvgl), false, "Failed to activate LVGL source");
    BROOKESIA_CHECK_FALSE_RETURN(start_ui_state_machine(), false, "Failed to start UI state machine");

    // Start gesture detection
    BROOKESIA_CHECK_FALSE_RETURN(start_gesture(), false, "Failed to start gesture");

    auto delayed_task = []() {
        auto result = DisplayHelper::call_function_async(
                          DisplayHelper::FunctionId::SetBacklightOnOff,
                          Display::get_instance().display_output_id_,
                          true
                      );
        BROOKESIA_CHECK_FALSE_EXIT(result, "Failed to set backlight on");
    };
    auto post_delayed_ret = task_scheduler_->post_delayed(delayed_task, BACKLIGHT_ON_DELAY_MS);
    BROOKESIA_CHECK_FALSE_RETURN(post_delayed_ret, false, "Failed to post delayed task");

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
    BROOKESIA_LOGI("Using display output %1% (%2%x%3%)", display_output_name_, display_width_, display_height_);

    return true;
}

bool Display::start_lvgl_display_source()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    gui::lvgl::DisplaySourceConfig config{};
    config.output_name = "";
    config.task_core_id = CONFIG_BROOKESIA_HAL_ADAPTOR_DISPLAY_LCD_PANEL_INIT_THREAD_CORE_ID;

    auto &source = LvglDisplaySource::get_instance();
    BROOKESIA_CHECK_FALSE_RETURN(source.start(config), false, "Failed to start LVGL display source");

    return true;
}

bool Display::set_active_source_role(DrawSource source)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    std::string source_role;
    switch (source) {
    case DrawSource::Lvgl:
        source_role = gui::lvgl::DISPLAY_SOURCE_ROLE;
        break;
    case DrawSource::Emote:
        source_role = expression::Emote::DISPLAY_SOURCE_ROLE;
        break;
    case DrawSource::Video:
        source_role = std::string(VideoHelper::DISPLAY_SOURCE_ROLE);
        break;
    default:
        return false;
    }

    BROOKESIA_CHECK_FALSE_RETURN(!source_role.empty(), false, "Display source role is not initialized");

    auto result_handler = [](service::FunctionResult && result) {
        if (!result.success) {
            BROOKESIA_LOGE("Failed to set active display source role: %1%", result.error_message);
        }
    };
    auto dispatched = DisplayHelper::call_function_async(
                          DisplayHelper::FunctionId::SetActiveSourceRole,
                          std::string(),
                          source_role,
                          result_handler
                      );
    BROOKESIA_CHECK_FALSE_RETURN(dispatched, false, "Failed to dispatch active display source role switch");

    return true;
}

bool Display::show_video()
{
    return set_active_source_role(DrawSource::Video);
}

bool Display::show_emote()
{
    return set_active_source_role(DrawSource::Emote);
}

bool Display::start_expression_emote_assets()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!EmoteHelper::is_available()) {
        BROOKESIA_LOGW("Emote is not available, skip initialization");
        return true;
    }

    BROOKESIA_LOGI("Initializing emote assets...");

    emote_service_binding_ = service::ServiceManager::get_instance().bind(EmoteHelper::get_name().data());
    BROOKESIA_CHECK_FALSE_RETURN(emote_service_binding_.is_valid(), false, "Failed to bind Emote service");

    {
        // Set emote config
        EmoteHelper::Config config{
            .task_priority = 6,
            .task_stack = 8 * 1024,
            .task_affinity = CONFIG_BROOKESIA_HAL_ADAPTOR_DISPLAY_LCD_PANEL_INIT_THREAD_CORE_ID,
            .task_stack_in_ext = false,
            .flag_buff_dma = true,
        };
        auto result = EmoteHelper::call_function_sync(
                          EmoteHelper::FunctionId::SetConfig, BROOKESIA_DESCRIBE_TO_JSON(config).as_object()
                      );
        BROOKESIA_CHECK_FALSE_RETURN(result.has_value(), false, "Failed to set emote config: %1%", result.error());
    }

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

    {
        auto result = EmoteHelper::call_function_sync(
                          EmoteHelper::FunctionId::SetEventMessage,
                          BROOKESIA_DESCRIBE_TO_STR(EmoteHelper::EventMessageType::Idle)
                      );
        BROOKESIA_CHECK_FALSE_RETURN(
            result.has_value(), false, "Failed to set emote event message: %1%", result.error()
        );
    }

    return true;
}

bool Display::start_gesture()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_FALSE_RETURN(!display_output_name_.empty(), false, "Display output is not initialized");

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
        BROOKESIA_LOGD("Display gesture event: %1%", event_name);

        GestureInfo info;
        if (!BROOKESIA_DESCRIBE_FROM_JSON(boost::json::value(info_json), info)) {
            BROOKESIA_LOGE("Failed to parse Display touch gesture event");
            return;
        }
        if (info.output_name != display_output_name_) {
            return;
        }

        if (info.event_type == GestureEventType::Release) {
            is_ui_state_action_triggered_ = false;
            return;
        }
        if ((info.event_type != GestureEventType::Pressing) || is_ui_state_action_triggered_) {
            return;
        }
        if (info.direction == GestureDirection::None) {
            return;
        }

        auto action = get_ui_action_from_gesture(info);
        if (action == DisplayAction::Max) {
            return;
        }

        auto action_ret = ui_state_machine_->trigger_action(BROOKESIA_DESCRIBE_TO_STR(action));
        BROOKESIA_CHECK_FALSE_EXIT(action_ret, "Failed to trigger action");
        is_ui_state_action_triggered_ = true;
    };
    gesture_event_connection_ = DisplayHelper::subscribe_event(DisplayHelper::EventId::TouchGesture, gesture_slot);
    BROOKESIA_CHECK_FALSE_RETURN(
        gesture_event_connection_.connected(), false, "Failed to subscribe Display touch gesture event"
    );

    BROOKESIA_LOGI("Display touch gesture enabled for output %1%", display_output_name_);
    return true;
}

void Display::stop_gesture()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    gesture_event_connection_.disconnect();
    is_ui_state_action_triggered_ = false;
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
