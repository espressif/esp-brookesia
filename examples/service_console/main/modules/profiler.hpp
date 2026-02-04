/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <memory>
#include <vector>
#include "brookesia/lib_utils/task_scheduler.hpp"
#include "brookesia/lib_utils/thread_profiler.hpp"
#include "brookesia/lib_utils/memory_profiler.hpp"

class Profiler {
public:
    struct Config {
        // Worker configuration
        int worker_core_id = 0;
        size_t worker_priority = 1;
        size_t worker_stack_size = 6 * 1024;

        // Thread profiler thresholds
        size_t thread_idle_cpu_usage_threshold = 1;
        size_t thread_stack_usage_threshold = 128;

        // Memory profiler thresholds
        size_t mem_internal_largest_free_threshold = 10 * 1024;
        size_t mem_internal_free_percent_threshold = 10;
        size_t mem_external_largest_free_threshold = 500 * 1024;
        size_t mem_external_free_percent_threshold = 20;
    };

    static Profiler &get_instance()
    {
        static Profiler instance;
        return instance;
    }

    /**
     * @brief Init profiler with the given configuration
     * @param config Configuration for the profiler
     * @return true if initialization was successful, false otherwise
     */
    bool init(const Config &config);

    /**
     * @brief Init profiler with default configuration
     */
    bool init()
    {
        return init({});
    }

    /**
     * @brief Check if profiler is initialized
     * @return true if profiler is initialized, false otherwise
     */
    bool is_initialized() const
    {
        return is_initialized_.load();
    }

    void start_thread_profiler(bool enable_auto_logging = false);
    void start_memory_profiler(bool enable_auto_logging = false);

private:
    Profiler() = default;
    ~Profiler() = default;
    Profiler(const Profiler &) = delete;
    Profiler &operator=(const Profiler &) = delete;

    std::atomic<bool> is_initialized_{false};
    Config config_{};
    std::shared_ptr<esp_brookesia::lib_utils::TaskScheduler> scheduler_;

    // Store connections to prevent them from being destroyed
    std::vector<esp_brookesia::lib_utils::ThreadProfiler::SignalConnection> thread_profiler_connections_;
    std::vector<esp_brookesia::lib_utils::MemoryProfiler::SignalConnection> memory_profiler_connections_;
};
