/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "brookesia/lib_utils/macro_configs.h"
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/lib_utils/macro_configs.h"

namespace esp_brookesia::lib_utils {

/**
 * @brief Thread configuration structure
 *
 * This structure holds configuration parameters for thread creation,
 * including name, core affinity, priority, stack size, and stack location.
 */
struct ThreadConfig {
    std::string  name = BROOKESIA_UTILS_THREAD_CONFIG_NAME;  ///< Thread name
    int       core_id = BROOKESIA_UTILS_THREAD_CONFIG_CORE_ID;  ///< CPU core ID for affinity (-1 for no affinity)
    size_t   priority = BROOKESIA_UTILS_THREAD_CONFIG_PRIORITY;  ///< Thread priority
    size_t stack_size = BROOKESIA_UTILS_THREAD_CONFIG_STACK_SIZE;  ///< Stack size in bytes
    bool stack_in_ext = BROOKESIA_UTILS_THREAD_CONFIG_STACK_IN_EXT;  ///< Whether stack should be in external memory

    bool operator==(const ThreadConfig &other) const
    {
        return (name == other.name) && (core_id == other.core_id) && (priority == other.priority) &&
               (stack_size == other.stack_size) && (stack_in_ext == other.stack_in_ext);
    }

    bool operator!=(const ThreadConfig &other) const
    {
        return !(*this == other);
    }

    /**
     * @brief Populate this object from an ESP-IDF pthread configuration.
     *
     * @param[in] cfg Pointer to an `esp_pthread_cfg_t`-compatible object.
     */
    void from_pthread_cfg(const void *cfg);

    /**
     * @brief Apply this configuration as the current pthread default.
     *
     * @note The new settings affect threads created after the call through the supported pthread APIs.
     */
    void apply() const;

    /**
     * @brief Get the platform default pthread configuration.
     *
     * @return Default `ThreadConfig` resolved from the runtime.
     */
    static ThreadConfig get_system_default_config();

    /**
     * @brief Get the pthread configuration currently applied to new threads.
     *
     * @return Currently active `ThreadConfig`.
     */
    static ThreadConfig get_applied_config();

    /**
     * @brief Query the runtime configuration of the current thread or task.
     *
     * @note The returned stack size is the minimum free stack space ever observed,
     *       not the original configured stack capacity.
     *
     * @return Runtime `ThreadConfig` for the current thread or task.
     */
    static ThreadConfig get_current_config();
};

/**
 * @brief RAII guard for thread configuration
 *
 * This class provides automatic restoration of thread configuration.
 * When constructed, it applies the new configuration and saves the original.
 * When destroyed, it restores the original configuration.
 */
class ThreadConfigGuard {
public:
    /**
     * @brief Apply a new thread configuration and remember the previous one.
     *
     * @param[in] config Thread configuration to apply for the lifetime of this guard.
     */
    ThreadConfigGuard(const ThreadConfig &config);

    /**
     * @brief Restore the thread configuration that was active before construction.
     */
    ~ThreadConfigGuard();

private:
    ThreadConfig original_config_;  ///< Original configuration to restore on destruction
};

BROOKESIA_DESCRIBE_STRUCT(ThreadConfig, (), (name, core_id, priority, stack_size, stack_in_ext))

}; // namespace esp_brookesia::lib_utils

#define _BROOKESIA_THREAD_CONFIG_CONCAT(a, b) a##b
#define BROOKESIA_THREAD_CONFIG_CONCAT(a, b) _BROOKESIA_THREAD_CONFIG_CONCAT(a, b)
/**
 * @brief Apply a temporary thread configuration for the current scope.
 *
 * This macro creates a `ThreadConfigGuard` instance that restores the previous
 * configuration automatically when the surrounding scope exits.
 *
 * @param ... Arguments forwarded to the `ThreadConfigGuard` constructor.
 *
 * @code{.cpp}
 * {
 *     BROOKESIA_THREAD_CONFIG_GUARD({
 *          .stack_size = 10 * 1024,
 *     });
 *     boost::thread([&]() {
 *         // Thread will be created with 10KB stack size
 *     });
 * }  // Original configuration is restored here
 * @endcode
 */
#define BROOKESIA_THREAD_CONFIG_GUARD(...) \
    esp_brookesia::lib_utils::ThreadConfigGuard \
    BROOKESIA_THREAD_CONFIG_CONCAT(thread_config_guard_, __LINE__)(__VA_ARGS__);

/**
 * @brief Query the runtime configuration of the current task.
 *
 * @note This returns the actual runtime configuration of the current FreeRTOS task,
 *       which may differ from the applied pthread configuration if the task was
 *       created directly via FreeRTOS APIs rather than pthread APIs.
 *
 * @return `ThreadConfig` containing the current task's runtime configuration.
 *
 * @code{.cpp}
 * BROOKESIA_LOGI("Current task: %1%", BROOKESIA_THREAD_GET_CURRENT_CONFIG());
 * @endcode
 */
#define BROOKESIA_THREAD_GET_CURRENT_CONFIG() esp_brookesia::lib_utils::ThreadConfig::get_current_config()

/**
 * @brief Query the pthread configuration currently applied to newly created threads.
 *
 * @note This returns the applied pthread configuration, which may differ from
 *       the actual runtime configuration of existing tasks. Use
 *       BROOKESIA_THREAD_GET_CURRENT_CONFIG() to get the actual runtime state of
 *       the current task.
 *
 * @return `ThreadConfig` containing the active pthread defaults.
 *         When no override was applied, the system default configuration is returned.
 *
 * @code{.cpp}
 * BROOKESIA_LOGI("Applied task: %1%", BROOKESIA_THREAD_GET_APPLIED_CONFIG());
 * @endcode
 */
#define BROOKESIA_THREAD_GET_APPLIED_CONFIG() esp_brookesia::lib_utils::ThreadConfig::get_applied_config()
