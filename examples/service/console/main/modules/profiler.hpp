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
        std::shared_ptr<esp_brookesia::lib_utils::TaskScheduler> task_scheduler;

        // Thread profiler thresholds
        size_t thread_idle_cpu_usage_threshold;
        size_t thread_stack_usage_threshold;

        // Memory profiler thresholds
        size_t mem_internal_largest_free_threshold;
        size_t mem_internal_free_percent_threshold;
        size_t mem_external_largest_free_threshold;
        size_t mem_external_free_percent_threshold;
    };

    static Profiler &get_instance()
    {
        static Profiler instance;
        return instance;
    }

    bool init(const Config &config);

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
