/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <utility>

#include "esp_log.h"
#include "private/utils.hpp"
#include "brookesia/hal_interface.hpp"
#include "brookesia/hal_adaptor.hpp"
#include "brookesia/service_manager.hpp"
#include "modules/console.hpp"
#include "modules/emote.hpp"
#include "modules/profiler.hpp"

using namespace esp_brookesia;

namespace {

bool configure_audio_processor()
{
    hal::AudioProcessorConfig processor_config{
        .playback = {
            .player_task = {
                .core_id = 0,
                .priority = 5,
                .stack_size = 4 * 1024,
#if CONFIG_SPIRAM_XIP_FROM_PSRAM
                .stack_in_ext = true,
#else
                .stack_in_ext = false,
#endif
            },
        },
        .encoder = {},
        .decoder = {},
        .afe = {},
    };

    return hal::AudioDevice::get_instance().set_processor_config(std::move(processor_config));
}

} // namespace

extern "C" void app_main(void)
{
    BROOKESIA_LOGI("\n\n=== Console Service Example ===\n");

    // Set log level to warning for some components to reduce log output
    esp_log_level_set("ESP_GMF_TASK", ESP_LOG_WARN);
    esp_log_level_set("ESP_GMF_FILE", ESP_LOG_WARN);
    esp_log_level_set("ESP_GMF_PORT", ESP_LOG_WARN);
    esp_log_level_set("ESP_GMF_POOL", ESP_LOG_WARN);
    esp_log_level_set("ESP_ES_PARSER", ESP_LOG_WARN);
    esp_log_level_set("GMF_AFE", ESP_LOG_WARN);
    esp_log_level_set("ESP_GMF_AENC", ESP_LOG_WARN);
    esp_log_level_set("AFE_MANAGER", ESP_LOG_WARN);
    esp_log_level_set("AUD_RENDER", ESP_LOG_WARN);
    esp_log_level_set("AUDIO_PROCESSOR", ESP_LOG_WARN);
    esp_log_level_set("AUD_SIMP_PLAYER", ESP_LOG_WARN);
    esp_log_level_set("AUD_SDEC", ESP_LOG_WARN);
    esp_log_level_set("AFE_CONFIG", ESP_LOG_WARN);
    esp_log_level_set("NEW_DATA_BUS", ESP_LOG_WARN);

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
            },
            {
                .name = "BackendWorker2",
                .core_id = 1,
                .priority = 1,
                .stack_size = 10 * 1024,
            },
        }
    });
    BROOKESIA_CHECK_FALSE_EXIT(start_result, "Failed to start task scheduler");

    auto setup_task = [backend_scheduler]() {
        BROOKESIA_CHECK_FALSE_EXIT(configure_audio_processor(), "Failed to configure audio processor");

        /* Start ServiceManager */
        auto &service_manager = service::ServiceManager::get_instance();
        BROOKESIA_CHECK_FALSE_EXIT(service_manager.start(), "Failed to start ServiceManager");

        /* Start emote */
        Emote::get_instance().init();

        /* Start profiler */
        Profiler::get_instance().init({
            .task_scheduler = backend_scheduler,
            .thread_idle_cpu_usage_threshold = 1,
            .thread_stack_usage_threshold = 128,
            .mem_internal_largest_free_threshold = 10 * 1024,
            .mem_internal_free_percent_threshold = 10,
            .mem_external_largest_free_threshold = 500 * 1024,
            .mem_external_free_percent_threshold = 20,
        });
        Profiler::get_instance().start_thread_profiler(false);
        Profiler::get_instance().start_memory_profiler(false);

        /* Start console */
        Console::get_instance().start();
    };
    auto post_result = backend_scheduler->post(std::move(setup_task));
    BROOKESIA_CHECK_FALSE_EXIT(post_result, "Failed to post setup task");
}
