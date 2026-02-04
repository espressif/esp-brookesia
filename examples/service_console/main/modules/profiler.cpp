/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "profiler.hpp"
#include "private/utils.hpp"

using namespace esp_brookesia;

constexpr const char *WORKER_NAME = "ProfilerWorker";
constexpr size_t WORKER_POLL_INTERVAL_MS = 100;

bool Profiler::init(const Config &config)
{
    if (is_initialized()) {
        BROOKESIA_LOGW("Profiler is already initialized");
        return true;
    }

    config_ = config;

    // Create and start task scheduler
    scheduler_ = std::make_shared<lib_utils::TaskScheduler>();
    auto start_config = lib_utils::TaskScheduler::StartConfig{
        .worker_configs = {
            {
                .name = WORKER_NAME,
                .core_id = config_.worker_core_id,
                .priority = config_.worker_priority,
                .stack_size = config_.worker_stack_size,
            },
        },
        .worker_poll_interval_ms = WORKER_POLL_INTERVAL_MS,
    };
    BROOKESIA_CHECK_FALSE_RETURN(scheduler_->start(start_config), false, "Failed to start profiler task scheduler");

    is_initialized_.store(true);

    BROOKESIA_LOGI("Profiler started");

    return true;
}

void Profiler::start_thread_profiler(bool enable_auto_logging)
{
    BROOKESIA_CHECK_FALSE_EXIT(is_initialized(), "Profiler is not initialized");

    auto &thread_profiler = lib_utils::ThreadProfiler::get_instance();
    thread_profiler.configure_profiling({
        .enable_auto_logging = enable_auto_logging,
    });
    BROOKESIA_CHECK_FALSE_EXIT(thread_profiler.start_profiling(scheduler_), "Failed to start thread profiler");

    // Monitor idle tasks CPU usage
    auto profiling_slot = [this](const lib_utils::ThreadProfiler::ProfileSnapshot & snapshot) {
        std::vector<std::string> idle_task_names = {
            "IDLE0",
#if CONFIG_SOC_CPU_CORES_NUM > 1
            "IDLE1",
#endif
        };
        bool need_print_snapshot = false;
        for (auto &name : idle_task_names) {
            lib_utils::ThreadProfiler::TaskInfo task_info;
            BROOKESIA_CHECK_FALSE_EXIT(
                lib_utils::ThreadProfiler::get_task_by_name(snapshot, name, task_info),
                "Failed to get idle task `%1%`", name
            );
            if (task_info.cpu_percent < config_.thread_idle_cpu_usage_threshold) {
                need_print_snapshot = true;
                BROOKESIA_LOGW(
                    "The CPU usage of the idle task `%1%` is less than %2%%%:", name,
                    config_.thread_idle_cpu_usage_threshold
                );
            }
        }
        if (need_print_snapshot) {
            lib_utils::ThreadProfiler::print_snapshot(snapshot);
        }
    };
    auto profiling_connection = thread_profiler.connect_profiling_signal(profiling_slot);
    if (profiling_connection.connected()) {
        thread_profiler_connections_.push_back(std::move(profiling_connection));
    } else {
        BROOKESIA_LOGE("Failed to connect to thread profiler profiling signal");
    }

    // Monitor tasks with high stack usage
    auto threshold_slot = [](const std::vector<lib_utils::ThreadProfiler::TaskInfo> &tasks) {
        BROOKESIA_LOGW("The following tasks have high stack usage:");
        lib_utils::ThreadProfiler::print_snapshot({.tasks = tasks});
    };
    auto stack_usage_connection = thread_profiler.connect_threshold_signal(
                                      lib_utils::ThreadProfiler::ThresholdType::StackUsage,
                                      config_.thread_stack_usage_threshold, threshold_slot
                                  );
    if (stack_usage_connection.connected()) {
        thread_profiler_connections_.push_back(std::move(stack_usage_connection));
    } else {
        BROOKESIA_LOGE("Failed to connect to thread profiler stack usage threshold signal");
    }
}

void Profiler::start_memory_profiler(bool enable_auto_logging)
{
    BROOKESIA_CHECK_FALSE_EXIT(is_initialized(), "Profiler is not initialized");

    auto &memory_profiler = lib_utils::MemoryProfiler::get_instance();
    memory_profiler.configure_profiling({
        .enable_auto_logging = enable_auto_logging,
    });
    BROOKESIA_CHECK_FALSE_EXIT(memory_profiler.start_profiling(scheduler_), "Failed to start memory profiler");

    // Monitor largest free internal memory
    auto largest_free_internal_memory_threshold_slot = [](const lib_utils::MemoryProfiler::ProfileSnapshot & snapshot) {
        BROOKESIA_LOGW(
            "Largest free internal memory is too low: %1% KB (total: %2% KB, free: %3% KB)",
            snapshot.memory.internal.largest_free_block / 1024,
            snapshot.memory.internal.total_size / 1024, snapshot.memory.internal.free_size / 1024
        );
    };
    auto largest_free_internal_memory_connection = memory_profiler.connect_threshold_signal(
                lib_utils::MemoryProfiler::ThresholdType::InternalLargestFreeBlock,
                config_.mem_internal_largest_free_threshold, largest_free_internal_memory_threshold_slot
            );
    if (largest_free_internal_memory_connection.connected()) {
        memory_profiler_connections_.push_back(std::move(largest_free_internal_memory_connection));
    } else {
        BROOKESIA_LOGE("Failed to connect to memory profiler largest free internal memory threshold signal");
    }

    // Monitor free internal memory
    auto free_internal_memory_threshold_slot = [](const lib_utils::MemoryProfiler::ProfileSnapshot & snapshot) {
        BROOKESIA_LOGW(
            "Internal free memory is too low: %1%%% (%2% KB)",
            snapshot.memory.internal.free_percent, snapshot.memory.internal.free_size / 1024
        );
    };
    auto free_internal_memory_connection = memory_profiler.connect_threshold_signal(
            lib_utils::MemoryProfiler::ThresholdType::InternalFreePercent,
            config_.mem_internal_free_percent_threshold, free_internal_memory_threshold_slot
                                           );
    if (free_internal_memory_connection.connected()) {
        memory_profiler_connections_.push_back(std::move(free_internal_memory_connection));
    } else {
        BROOKESIA_LOGE("Failed to connect to memory profiler free internal memory threshold signal");
    }

#if CONFIG_SPIRAM
    // // Monitor largest free external memory
    // auto largest_free_external_memory_threshold_slot = [](const lib_utils::MemoryProfiler::ProfileSnapshot & snapshot) {
    //     BROOKESIA_LOGW(
    //         "Largest free external memory is too low: %1% KB (total: %2% KB, free: %3% KB)",
    //         snapshot.memory.external.largest_free_block / 1024,
    //         snapshot.memory.external.total_size / 1024, snapshot.memory.external.free_size / 1024
    //     );
    // };
    // auto largest_free_external_memory_connection = memory_profiler.connect_threshold_signal(
    //             lib_utils::MemoryProfiler::ThresholdType::ExternalLargestFreeBlock,
    //             config_.mem_external_largest_free_threshold, largest_free_external_memory_threshold_slot
    //         );
    // if (largest_free_external_memory_connection.connected()) {
    //     memory_profiler_connections_.push_back(std::move(largest_free_external_memory_connection));
    // } else {
    //     BROOKESIA_LOGE("Failed to connect to memory profiler largest free external memory threshold signal");
    // }

    // Monitor free external memory
    auto free_external_memory_threshold_slot = [](const lib_utils::MemoryProfiler::ProfileSnapshot & snapshot) {
        BROOKESIA_LOGW(
            "External free memory is too low: %1%%% (%2% KB)",
            snapshot.memory.external.free_percent, snapshot.memory.external.free_size / 1024
        );
    };
    auto free_external_memory_connection = memory_profiler.connect_threshold_signal(
            lib_utils::MemoryProfiler::ThresholdType::ExternalFreePercent,
            config_.mem_external_free_percent_threshold, free_external_memory_threshold_slot
                                           );
    if (free_external_memory_connection.connected()) {
        memory_profiler_connections_.push_back(std::move(free_external_memory_connection));
    } else {
        BROOKESIA_LOGE("Failed to connect to memory profiler free external memory threshold signal");
    }
#endif // CONFIG_SPIRAM
}
