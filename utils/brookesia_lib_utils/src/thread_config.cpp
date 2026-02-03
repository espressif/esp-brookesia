/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#if defined(ESP_PLATFORM)
#   include "freertos/FreeRTOS.h"
#   include "freertos/task.h"
#   include "esp_idf_version.h"
#   include "esp_pthread.h"
#   include "esp_heap_caps.h"
#endif
#include "brookesia/lib_utils/macro_configs.h"
#if !BROOKESIA_UTILS_THREAD_CONFIG_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/lib_utils/log.hpp"
#include "brookesia/lib_utils/check.hpp"
#include "brookesia/lib_utils/thread_config.hpp"

namespace esp_brookesia::lib_utils {

void ThreadConfig::from_pthread_cfg(const void *cfg)
{
    BROOKESIA_CHECK_NULL_EXIT(cfg, "Invalid argument");

    const esp_pthread_cfg_t *pthread_cfg = static_cast<const esp_pthread_cfg_t *>(cfg);
    name = (pthread_cfg->thread_name != nullptr) ? pthread_cfg->thread_name : CONFIG_PTHREAD_TASK_NAME_DEFAULT;
    core_id = (pthread_cfg->pin_to_core == tskNO_AFFINITY) ? -1 : pthread_cfg->pin_to_core;
    priority = static_cast<size_t>(pthread_cfg->prio);
    stack_size = static_cast<size_t>(pthread_cfg->stack_size);
    stack_in_ext = (pthread_cfg->stack_alloc_caps & MALLOC_CAP_SPIRAM) != 0;
}

void ThreadConfig::to_pthread_cfg(void *cfg) const
{
    BROOKESIA_CHECK_NULL_EXIT(cfg, "Invalid argument");

    esp_pthread_cfg_t *pthread_cfg = static_cast<esp_pthread_cfg_t *>(cfg);
    pthread_cfg->thread_name = (name.empty()) ? CONFIG_PTHREAD_TASK_NAME_DEFAULT : name.c_str();
    pthread_cfg->pin_to_core = (core_id < 0) ? tskNO_AFFINITY : core_id;
    pthread_cfg->prio = static_cast<int>(priority);
    pthread_cfg->stack_size = static_cast<size_t>(stack_size);
    pthread_cfg->stack_alloc_caps = static_cast<uint32_t>(stack_in_ext ? MALLOC_CAP_SPIRAM : MALLOC_CAP_INTERNAL) | MALLOC_CAP_8BIT;
}

void ThreadConfig::apply() const
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

#if defined(ESP_PLATFORM)
    esp_pthread_cfg_t new_cfg;
    to_pthread_cfg(&new_cfg);
    BROOKESIA_CHECK_ESP_ERR_EXIT(esp_pthread_set_cfg(&new_cfg), "Failed to set thread configuration");
#else
    BROOKESIA_LOGW("Not supported on non-ESP platforms");
#endif
}

ThreadConfig ThreadConfig::get_system_default_config()
{
#if defined(ESP_PLATFORM)
    esp_pthread_cfg_t default_cfg = esp_pthread_get_default_config();
    ThreadConfig config;
    config.from_pthread_cfg(&default_cfg);
    return config;
#else
    return ThreadConfig();
#endif
}

ThreadConfig ThreadConfig::get_applied_config()
{
#if defined(ESP_PLATFORM)
    esp_pthread_cfg_t current_cfg;
    esp_err_t err = esp_pthread_get_cfg(&current_cfg);
    if (err == ESP_ERR_NOT_FOUND) {
        return ThreadConfig::get_system_default_config();
    }

    BROOKESIA_CHECK_ESP_ERR_RETURN(err, ThreadConfig::get_system_default_config(), "Failed to get thread configuration");

    ThreadConfig config;
    config.from_pthread_cfg(&current_cfg);
    return config;
#else
    return ThreadConfig();
#endif
}

ThreadConfig ThreadConfig::get_current_config()
{
#if defined(ESP_PLATFORM)
    return {
        .name = pcTaskGetName(nullptr),
        .core_id = xPortGetCoreID(),
        .priority = uxTaskPriorityGet(nullptr),
    };
#else
    return ThreadConfig();
#endif
}

ThreadConfigGuard::ThreadConfigGuard(const ThreadConfig &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Param: config(%1%)", BROOKESIA_DESCRIBE_TO_STR(config));

    original_config_ = ThreadConfig::get_applied_config();
    config.apply();
}

ThreadConfigGuard::~ThreadConfigGuard()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    original_config_.apply();
}

}; // namespace esp_brookesia::lib_utils
