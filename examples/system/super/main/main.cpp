/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "private/utils.hpp"
#include "brookesia/lib_utils.hpp"
#include "brookesia/gui_lvgl.hpp"
#include "brookesia/system_super.hpp"
#include "modules/general_services.hpp"
#include "modules/display.hpp"

using namespace esp_brookesia;

extern "C" void app_main(void)
{
    BROOKESIA_LOGI("\n\n=== System Example ===\n");

    auto setup_task = []() {
        /* Initialize general services */
        BROOKESIA_CHECK_FALSE_EXIT(
            GeneralServices::get_instance().init(), "Failed to initialize general services"
        );

        /* Start display UI */
        auto &display = Display::get_instance();
        auto display_start_ret = display.start({});
        BROOKESIA_CHECK_FALSE_EXIT(display_start_ret, "Failed to start display");
        /* Start audio services */
        BROOKESIA_CHECK_FALSE_EXIT(
            GeneralServices::get_instance().start_audio_services(), "Failed to start audio services"
        );

        /* Create system instance */
        static std::unique_ptr<system::super::System> system_instance;
        system_instance = std::make_unique<system::super::System>();

        /* Configure system */
        system::super::System::Config config;
        config.core_config.gui_backend = std::make_unique<gui::lvgl::Backend>();
        config.core_config.environment = {
            .width_px = static_cast<int32_t>(display.width()),
            .height_px = static_cast<int32_t>(display.height()),
            .density = 1.0F,
            .font_scale = 1.0F,
            // .language = "zh_CN",
            // .theme_id = "dark",
        };
        // config.core_config.enable_gui_view_debug = true;

        /* Initialize and start system */
        auto init_result = system_instance->init(std::move(config));
        BROOKESIA_CHECK_FALSE_EXIT(init_result, "System init failed: %1%", init_result.error());
        auto start_result = system_instance->start();
        BROOKESIA_CHECK_FALSE_EXIT(start_result, "System start failed: %1%", start_result.error());

        boost::this_thread::sleep_for(boost::chrono::seconds(10));

        BROOKESIA_LOGI("=== System Example Completed ===");
    };
    {
        /* Setup task in a high stack size thread to avoid stack overflow */
        BROOKESIA_THREAD_CONFIG_GUARD({
            .stack_size = 40 * 1024,
        });
        boost::thread(setup_task).detach();
    }
}
