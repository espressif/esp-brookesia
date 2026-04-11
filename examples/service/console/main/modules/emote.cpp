/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "private/utils.hpp"
#include "brookesia/lib_utils.hpp"
#include "brookesia/expression_emote.hpp"
#include "brookesia/hal_interface.hpp"
#include "emote.hpp"

using namespace esp_brookesia;
using EmoteHelper = service::helper::ExpressionEmote;

bool Emote::init(int core_id)
{
    BROOKESIA_LOG_TRACE_GUARD();

    if (!EmoteHelper::is_available()) {
        BROOKESIA_LOGW("Emote is not available, skip initialization");
        return true;
    }

    auto [display_name, display_iface] = hal::get_first_interface<hal::DisplayPanelIface>();
    BROOKESIA_CHECK_NULL_RETURN(display_iface, false, "Failed to get display interface");

    auto [backlight_name, backlight_iface] = hal::get_first_interface<hal::DisplayBacklightIface>();
    if (backlight_iface) {
        BROOKESIA_CHECK_FALSE_EXECUTE(backlight_iface->turn_on(), {}, {
            BROOKESIA_LOGE("Failed to turn on backlight");
        });
    }

    hal::DisplayPanelIface::DriverSpecific driver_specific;
    BROOKESIA_CHECK_FALSE_RETURN(
        display_iface->get_driver_specific(driver_specific), false, "Failed to get driver specific"
    );
    auto &display_info = display_iface->get_info();
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
    auto flush_ready_event_slot = [display_iface](const std::string & event_name, const boost::json::object & param_json) {
        lib_utils::FunctionGuard notify_guard([]() {
            BROOKESIA_LOG_TRACE_GUARD();
            expression::Emote::get_instance().native_notify_flush_finished();
        });

        EmoteHelper::FlushReadyEventParam param;
        auto success = BROOKESIA_DESCRIBE_FROM_JSON(param_json, param);
        BROOKESIA_CHECK_FALSE_EXIT(success, "Failed to parse flush ready event param: %1%");

        auto ret = display_iface->draw_bitmap(
                       param.x_start, param.y_start, param.x_end, param.y_end, static_cast<const uint8_t *>(param.data)
                   );
        BROOKESIA_CHECK_FALSE_EXIT(ret, "Failed to draw bitmap");
    };
    static auto connection = EmoteHelper::subscribe_event(EmoteHelper::EventId::FlushReady, flush_ready_event_slot);
    BROOKESIA_CHECK_FALSE_RETURN(connection.connected(), false, "Failed to subscribe to flush ready event");

    return true;
}
