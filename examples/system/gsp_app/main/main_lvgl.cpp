/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#define BROOKESIA_LOG_TAG "LvglApp"
#include "brookesia/lib_utils/log.hpp"

#include <exception>
#include <memory>

#include "brookesia/app_settings/settings_app.hpp"
#include "brookesia/gui_lvgl.hpp"
#include "brookesia/system_super.hpp"
#include "modules/display.hpp"
#include "modules/general_services.hpp"

namespace {

void run_main()
{
    if (!GeneralServices::get_instance().init()) {
        BROOKESIA_LOGE("Failed to initialize general services");
        return;
    }

    auto &display = Display::get_instance();
    if (!display.start({})) {
        BROOKESIA_LOGE("Failed to start the LVGL display");
        return;
    }
    if (!GeneralServices::get_instance().start_audio_services()) {
        BROOKESIA_LOGE("Failed to start audio services");
        return;
    }

    static std::unique_ptr<esp_brookesia::system::super::System> system_instance;
    system_instance = std::make_unique<esp_brookesia::system::super::System>();
    esp_brookesia::system::super::System::Config config;
    config.core_config.gui_backend = std::make_unique<esp_brookesia::gui::lvgl::Backend>();
    config.core_config.environment.width_px = static_cast<int32_t>(display.width());
    config.core_config.environment.height_px = static_cast<int32_t>(display.height());
    config.core_config.install_package_apps = false;
    auto init_result = system_instance->init(std::move(config));
    if (!init_result) {
        BROOKESIA_LOGE("LVGL system init failed: %1%", init_result.error());
        return;
    }
    auto start_result = system_instance->start();
    if (!start_result) {
        BROOKESIA_LOGE("LVGL system start failed: %1%", start_result.error());
        return;
    }
    BROOKESIA_LOGI("LVGL system started");
}

} // namespace

extern "C" void app_main(void)
{
    try {
        run_main();
    } catch (const std::exception &exception) {
        BROOKESIA_LOGE("Unhandled exception: %1%", exception.what());
    } catch (...) {
        BROOKESIA_LOGE("Unhandled unknown exception");
    }
}
