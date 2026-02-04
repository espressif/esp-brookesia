/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

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

    /**
     * @brief Load configuration from pthread configuration structure
     *
     * @param[in] cfg Pointer to pthread configuration structure
     */
    void from_pthread_cfg(const void *cfg);

    /**
     * @brief Convert to pthread configuration structure
     *
     * @param[out] cfg Pointer to pthread configuration structure to fill
     */
    void to_pthread_cfg(void *cfg) const;

    /**
     * @brief Apply this configuration to the current thread
     */
    void apply() const;

    /**
     * @brief Get the system default thread configuration
     *
     * @return Default ThreadConfig with system defaults
     */
    static ThreadConfig get_system_default_config();

    /**
     * @brief Get the currently applied thread configuration
     *
     * @return Currently active ThreadConfig
     */
    static ThreadConfig get_applied_config();

    /**
     * @brief Get the current thread configuration
     *
     * @return Current ThreadConfig
     */
    static ThreadConfig get_current_config();
};

BROOKESIA_DESCRIBE_STRUCT(ThreadConfig, (), (name, core_id, priority, stack_size, stack_in_ext))

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
     * @brief Constructor that applies new thread configuration
     *
     * Saves the current thread configuration and applies the provided configuration.
     *
     * @param[in] config Thread configuration to apply
     */
    ThreadConfigGuard(const ThreadConfig &config);

    /**
     * @brief Destructor that restores original thread configuration
     *
     * Automatically restores the thread configuration that was active
     * when this guard was constructed.
     */
    ~ThreadConfigGuard();

private:
    ThreadConfig original_config_;  ///< Original configuration to restore on destruction
};

}; // namespace esp_brookesia::lib_utils

#define _BROOKESIA_THREAD_CONFIG_CONCAT(a, b) a##b
#define BROOKESIA_THREAD_CONFIG_CONCAT(a, b) _BROOKESIA_THREAD_CONFIG_CONCAT(a, b)
/**
 * @brief Set thread configuration and automatically restore the original configuration
 *
 * This macro creates a ThreadConfigGuard object that applies the specified
 * configuration and automatically restores the previous configuration when
 * the guard goes out of scope.
 *
 * @param config Thread configuration to apply
 *
 * @example
 * {
 *     BROOKESIA_THREAD_CONFIG_GUARD({
 *          .stack_size = 10 * 1024,
 *     });
 *     boost::thread([&]() {
 *         // Thread will be created with 10KB stack size
 *     });
 * }  // Original configuration is restored here
 */
#define BROOKESIA_THREAD_CONFIG_GUARD(...) \
    esp_brookesia::lib_utils::ThreadConfigGuard \
    BROOKESIA_THREAD_CONFIG_CONCAT(thread_config_guard_, __LINE__)(__VA_ARGS__);
