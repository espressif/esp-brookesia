/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#define BROOKESIA_LOG_TAG "GspApp"
#include "brookesia/lib_utils/log.hpp"

#include <chrono>
#include <exception>
#include <memory>
#include <thread>

#include "brookesia/hal_adaptor/display/device.hpp"
#include "brookesia/hal_interface/interface.hpp"
#include "brookesia/hal_interface/interfaces/display/panel.hpp"
#include "brookesia/hal_interface/interfaces/display/touch.hpp"
#include "brookesia/system_core.hpp"
#include "showcase_app.hpp"

namespace esp_brookesia::gsp_app {

namespace {

constexpr uint16_t EXPECTED_H_RES = 1024;
constexpr uint16_t EXPECTED_V_RES = 600;

void run_main()
{
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
        BROOKESIA_LOGE("Panel is %1%x%2%, showcase is 1024x600", info.h_res, info.v_res);
        return;
    }
    if (!backlight->set_brightness(72)) {
        BROOKESIA_LOGE("Failed to set the initial backlight");
        return;
    }

    static system::core::System system_instance;
    system::core::System::Config config;
    config.gsp_panel = & *panel;
    config.gsp_touch = & *touch;
    config.environment.width_px = EXPECTED_H_RES;
    config.environment.height_px = EXPECTED_V_RES;
    config.start_service_manager = true;
    config.install_registered_apps = false;
    config.install_package_apps = false;
    auto init_result = system_instance.init(std::move(config));
    if (!init_result) {
        BROOKESIA_LOGE("System init failed: %1%", init_result.error());
        return;
    }
    auto start_result = system_instance.start();
    if (!start_result) {
        BROOKESIA_LOGE("System start failed: %1%", start_result.error());
        return;
    }

    static auto app = std::make_shared<ShowcaseApp>(& *backlight);
    auto app_id = system_instance.install_app(app);
    if (!app_id) {
        BROOKESIA_LOGE("Failed to install showcase: %1%", app_id.error());
        return;
    }

    for (int cycle = 1; ; ++cycle) {
        if (!backlight->set_brightness(72)) {
            BROOKESIA_LOGE("Failed to restore backlight");
            return;
        }
        auto started = system_instance.start_app(app_id.value());
        if (!started) {
            BROOKESIA_LOGE("Failed to start showcase cycle %1%: %2%", cycle, started.error());
            return;
        }
        BROOKESIA_LOGI("Showcase cycle %1% running", cycle);
        while (!ShowcaseApp::exit_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
        BROOKESIA_LOGI("Showcase cycle %1% stopped; screen off, then start again", cycle);
        if (!backlight->set_brightness(0)) {
            BROOKESIA_LOGE("Failed to blank backlight");
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    }
}

} // namespace

} // namespace esp_brookesia::gsp_app

extern "C" void app_main(void)
{
    try {
        esp_brookesia::gsp_app::run_main();
    } catch (const std::exception &exception) {
        BROOKESIA_LOGE("Unhandled exception: %1%", exception.what());
    } catch (...) {
        BROOKESIA_LOGE("Unhandled unknown exception");
    }
}
