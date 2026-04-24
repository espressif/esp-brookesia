/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_log.h"
#include "private/utils.hpp"
#include "brookesia/lib_utils.hpp"
#include "brookesia/service_manager.hpp"
#include "brookesia/hal_interface.hpp"
#include "brookesia/hal_adaptor.hpp"
#include "modules/wifi_provisioning.hpp"
#include "modules/ai_agents.hpp"
#include "modules/general_services.hpp"
#include "modules/profiler.hpp"
#include "modules/display/display.hpp"

using namespace esp_brookesia;

extern "C" void app_main(void)
{
    BROOKESIA_LOGI("\n\n=== Agent Chatbot Example ===\n");

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
    esp_log_level_set("ESP_XIAOZHI_MQTT", ESP_LOG_WARN);
    esp_log_level_set("ESP_XIAOZHI_CHAT", ESP_LOG_WARN);
    esp_log_level_set("AFE", ESP_LOG_ERROR);

    /* Initialize all devices from HAL adaptor */
#if CONFIG_SOC_CPU_CORES_NUM > 1
    {
        // For SPI LCDs, bus initialization and data transmission operations must be performed on the same core;
        // otherwise, a crash may occur.
        BROOKESIA_THREAD_CONFIG_GUARD({
            .core_id = 1,
        });
        std::thread([&]() {
            hal::init_device(hal::DisplayDevice::DEVICE_NAME);
        }).join();
    }
#endif
    hal::init_all_devices();

    /* Start ServiceManager */
    auto &service_manager = service::ServiceManager::get_instance();
    BROOKESIA_CHECK_FALSE_EXIT(service_manager.start(), "Failed to start ServiceManager");

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
        /* Initialize general services */
        GeneralServices::get_instance().init(backend_scheduler);
        GeneralServices::get_instance().init_audio();
        GeneralServices::get_instance().start_nvs();
        GeneralServices::get_instance().start_sntp();

        /* Initialize AI agents */
        AI_Agents::get_instance().init({
            .task_scheduler = backend_scheduler,
        });
        AI_Agents::get_instance().init_coze();
        AI_Agents::get_instance().init_openai();
        AI_Agents::get_instance().init_xiaozhi();

        /* Start display UI */
        Display::get_instance().start({
            .task_scheduler = backend_scheduler,
#if CONFIG_SOC_CPU_CORES_NUM > 1
            .lvgl_task_core = 1,
            .emote_task_core = 1,
#endif
        });

        /* Start WiFi provisioning module */
        WifiProvisioning::get_instance().init({
            .task_scheduler = backend_scheduler,
        });
        WifiProvisioning::get_instance().start();

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
    };
    auto post_result = backend_scheduler->post(std::move(setup_task));
    BROOKESIA_CHECK_FALSE_EXIT(post_result, "Failed to post setup task");
}
