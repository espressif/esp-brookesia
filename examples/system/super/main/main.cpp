/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_log.h"
#include "private/utils.hpp"
#include "brookesia/lib_utils.hpp"
#include "brookesia/hal_adaptor.hpp"
#include "brookesia/gui_lvgl.hpp"
#include "brookesia/system_super.hpp"
#include "modules/general_services.hpp"
#include "modules/profiler.hpp"
#include "modules/display.hpp"

#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

using namespace esp_brookesia;

extern "C" void app_main(void)
{
    BROOKESIA_LOGI("\n\n=== System Example ===\n");

    /* Create a task scheduler for backend usage */
    std::shared_ptr<lib_utils::TaskScheduler> backend_scheduler;
    BROOKESIA_CHECK_EXCEPTION_EXIT(
        backend_scheduler = std::make_shared<lib_utils::TaskScheduler>(), "Failed to create task scheduler"
    );
    auto start_result = backend_scheduler->start({
        .worker_configs = {
            {
                .name = "BackendWorker",
                .core_id = 0,
                .priority = 1,
                .stack_size = 40 * 1024,
            }
        }
    });
    BROOKESIA_CHECK_FALSE_EXIT(start_result, "Failed to start task scheduler");

    auto setup_task = [backend_scheduler]() {
        /* Initialize general services */
        GeneralServices::get_instance().init(backend_scheduler);
        GeneralServices::get_instance().init_audio();

        /* Start display UI */
        auto &display = Display::get_instance();
        auto display_start_ret = display.start({
            .task_scheduler = backend_scheduler,
        });
        BROOKESIA_CHECK_FALSE_EXIT(display_start_ret, "Failed to start display");

        static std::unique_ptr<system::super::System> system_instance;
        system_instance = std::make_unique<system::super::System>();

        system::super::System::Config config;
        config.core_config.gui_backend = std::make_unique<gui::lvgl::Backend>();
        config.core_config.environment = {
            .width_px = static_cast<int32_t>(display.width()),
            .height_px = static_cast<int32_t>(display.height()),
            .density = 1.0F,
            .font_scale = 1.0F,
            // .language = "zh_CN",
            // .theme_id = "default",
        };
        // config.core_config.enable_gui_view_debug = true;

        auto init_result = system_instance->init(std::move(config));
        BROOKESIA_CHECK_FALSE_EXIT(init_result, "System init failed: %1%", init_result.error());
        auto start_result = system_instance->start();
        BROOKESIA_CHECK_FALSE_EXIT(start_result, "System start failed: %1%", start_result.error());

        // /* Start profiler */
        // Profiler::get_instance().init({
        //     .task_scheduler = backend_scheduler,
        //     .thread_idle_cpu_usage_threshold = 1,
        //     .thread_stack_usage_threshold = 128,
        //     .mem_internal_largest_free_threshold = 10 * 1024,
        //     .mem_internal_free_percent_threshold = 10,
        //     .mem_external_largest_free_threshold = 500 * 1024,
        //     .mem_external_free_percent_threshold = 20,
        // });
        // Profiler::get_instance().start_thread_profiler(true);
        // Profiler::get_instance().start_memory_profiler(true);

        auto complete_log_task = []() {
            BROOKESIA_LOGI("\n\n=== System Example Completed ===\n");
        };
        auto result = backend_scheduler->post_delayed(std::move(complete_log_task), 10000);
        BROOKESIA_CHECK_FALSE_EXIT(result, "Failed to post delayed complete log task");
    };
    auto post_result = backend_scheduler->post(std::move(setup_task));
    BROOKESIA_CHECK_FALSE_EXIT(post_result, "Failed to post setup task");
}
