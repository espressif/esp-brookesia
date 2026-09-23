/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#define BROOKESIA_LOG_TAG "GspBasic"
#include "brookesia/lib_utils/log.hpp"

#include <exception>
#include "brookesia/hal_adaptor/display/device.hpp"
#include "brookesia/hal_interface/interface.hpp"
#include "brookesia/hal_interface/interfaces/display/backlight.hpp"
#include "demo_ui.hpp"

namespace esp_brookesia::gsp_basic {

namespace {

constexpr uint16_t EXPECTED_H_RES = 1024;
constexpr uint16_t EXPECTED_V_RES = 600;

void run_main()
{
    BROOKESIA_LOGI("Brookesia GSP basic demo");

    static auto panel = hal::acquire_interface<hal::display::PanelIface>(hal::DisplayDevice::LCD_PANEL_IMPL_NAME);
    static auto touch = hal::acquire_interface<hal::display::TouchIface>(hal::DisplayDevice::LCD_TOUCH_IMPL_NAME);
    static auto backlight = hal::acquire_interface<hal::display::BacklightIface>(
                                hal::DisplayDevice::LEDC_BACKLIGHT_IMPL_NAME
                            );
    if (!panel || !touch || !backlight) {
        BROOKESIA_LOGE("Failed to acquire panel, touch, or backlight");
        return;
    }

    const auto &info = panel->get_info();
    if ((info.h_res != EXPECTED_H_RES) || (info.v_res != EXPECTED_V_RES)) {
        BROOKESIA_LOGE("Panel is %1%x%2%, demo scene is 1024x600", info.h_res, info.v_res);
        return;
    }
    if (!backlight->set_brightness(100)) {
        BROOKESIA_LOGE("Failed to set backlight");
        return;
    }

    // The HAL objects stay in static storage so their destructors do not release
    // the panel while the GSP render task still borrows it.
    const gui::gsp::DisplayLease display{
        .panel = & *panel,
        .touch = & *touch,
    };
    static gui::gsp::Session session;
    if (!start_demo(session, display)) {
        BROOKESIA_LOGE("Failed to start demo");
        return;
    }
    if (!restart_demo(session, display)) {
        BROOKESIA_LOGE("Failed to restart demo");
        return;
    }
    keep_alive();
}

} // namespace

} // namespace esp_brookesia::gsp_basic

extern "C" void app_main(void)
{
    try {
        esp_brookesia::gsp_basic::run_main();
    } catch (const std::exception &exception) {
        BROOKESIA_LOGE("Unhandled exception: %1%", exception.what());
    } catch (...) {
        BROOKESIA_LOGE("Unhandled unknown exception");
    }
}
