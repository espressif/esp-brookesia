/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "private/utils.hpp"
#include "brookesia/lib_utils.hpp"
#include "brookesia/gui_lvgl.hpp"
#include "brookesia/system_super.hpp"
#include "brookesia/app_matter_controller.hpp"
#include "modules/general_services.hpp"
#include "modules/display.hpp"
#include "esp_heap_caps.h"
#include "esp_log.h"

using namespace esp_brookesia;

namespace {

void heap_allocation_failed_hook(size_t requested_size, uint32_t caps, const char *function_name)
{
    // Keep this callback allocation-free: it runs precisely when the allocator
    // cannot satisfy a request. The surrounding [MatterHeap] checkpoints show
    // total/largest blocks; this line identifies the failed request itself.
    ESP_EARLY_LOGE(
        "HeapAlloc",
        "allocation_failed size=%u caps=0x%08x function=%s",
        static_cast<unsigned>(requested_size), static_cast<unsigned>(caps),
        function_name != nullptr ? function_name : "<unknown>"
    );
}

} // namespace

extern "C" void app_main(void)
{
    BROOKESIA_LOGI("\n\n=== System Example ===\n");

    const auto heap_hook_result = heap_caps_register_failed_alloc_callback(heap_allocation_failed_hook);
    if (heap_hook_result != ESP_OK) {
        ESP_LOGW("HeapAlloc", "Failed to register allocation hook: %s", esp_err_to_name(heap_hook_result));
    }

    /* Create a task scheduler for backend usage */
    std::shared_ptr<lib_utils::TaskScheduler> backend_scheduler;
    BROOKESIA_CHECK_EXCEPTION_EXIT(
        backend_scheduler = std::make_shared<lib_utils::TaskScheduler>(), "Failed to create task scheduler"
    );
    auto start_result = backend_scheduler->start({
        .worker_configs = {
            {
                .name = "BackendWorker1",
                .core_id = 0,
                .priority = 1,
                .stack_size = 10 * 1024,
                .stack_in_ext = false,
            },
            {
                .name = "BackendWorker2",
                .core_id = 1,
                .priority = 1,
                .stack_size = 4 * 1024,
                .stack_in_ext = false,
            },
        }
    });
    BROOKESIA_CHECK_FALSE_EXIT(start_result, "Failed to start task scheduler");

    auto setup_task = [backend_scheduler]() {
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

        /* Start audio services */
        BROOKESIA_CHECK_FALSE_EXIT(
            GeneralServices::get_instance().start_audio_services(), "Failed to start audio services"
        );

        auto matter_runtime_task = []() {
            auto matter_runtime_result = app::matter_controller::ensure_matter_runtime_started();
            if (!matter_runtime_result) {
                BROOKESIA_LOGE("Matter controller runtime init failed: %1%", matter_runtime_result.error());
            }
        };
        {
            BROOKESIA_THREAD_CONFIG_GUARD({
                .stack_size = 12 * 1024,
                .stack_in_ext = false,
            });
            boost::thread(matter_runtime_task).detach();
        }

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

        BROOKESIA_LOGI("=== System Example Completed ===");
    };
    {
        /* Setup task in a high stack size thread to avoid stack overflow */
        BROOKESIA_THREAD_CONFIG_GUARD({
            .stack_size = 40 * 1024,
            .stack_in_ext = true,
        });
        boost::thread(setup_task).detach();
    }
}
