/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <boost/signals2.hpp>
#include "boost/thread/mutex.hpp"
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/lib_utils/task_scheduler.hpp"

namespace esp_brookesia::lib_utils {

/**
 * @brief Memory profiler for monitoring ESP32 heap memory usage
 *
 * This class provides a C++ interface to monitor ESP32 heap memory usage,
 * including internal SRAM and external PSRAM. It integrates naturally with
 * TaskScheduler for periodic profiling.
 */
class MemoryProfiler {
public:
    /**
     * @brief Memory information for a specific heap type
     */
    struct HeapInfo {
        size_t total_size = 0;          // Total heap size (bytes)
        size_t free_size = 0;            // Used heap size (bytes)
        size_t largest_free_block = 0;   // Largest free block (bytes)
        size_t free_percent = 0;         // Free percentage
        size_t used_percent = 0;         // Used percentage

        HeapInfo() = default;
        HeapInfo(size_t total, size_t free, size_t largest)
            : total_size(total), free_size(free), largest_free_block(largest)
            , free_percent(total > 0 ? free * 100 / total : 0)
            , used_percent(total > 0 ? (100 - free_percent) : 0)
        {
        }
    };

    /**
     * @brief Memory information structure
     */
    struct MemoryInfo {
        HeapInfo internal;  // Internal SRAM (MALLOC_CAP_INTERNAL)
        HeapInfo external;  // External PSRAM (MALLOC_CAP_SPIRAM)
        size_t total_size = 0;              // Total heap size (bytes)
        size_t total_free = 0;              // Total free memory (bytes)
        size_t total_free_percent = 0;      // Total free percentage
        size_t total_largest_free_block = 0; // Total largest free block (bytes)
    };

    /**
     * @brief Statistics summary
     */
    struct Statistics {
        size_t sample_count = 0;                    // Number of samples taken
        size_t min_total_free = 0;                  // Minimum total free memory (bytes) observed
        size_t min_internal_free = 0;               // Minimum internal free memory (bytes) observed
        size_t min_external_free = 0;               // Minimum external free memory (bytes) observed
        size_t min_total_free_percent = 0;          // Minimum total free percentage observed
        size_t min_internal_free_percent = 0;       // Minimum internal free percentage observed
        size_t min_external_free_percent = 0;       // Minimum external free percentage observed
        size_t min_total_largest_free_block = 0;    // Minimum total largest free block (bytes)
        size_t min_internal_largest_free_block = 0; // Minimum internal largest free block (bytes)
        size_t min_external_largest_free_block = 0; // Minimum external largest free block (bytes)

        Statistics() = default;
        Statistics(const MemoryInfo &cur_memory, const Statistics &last_stats);
    };

    /**
     * @brief Snapshot of memory information at a point in time
     */
    struct ProfileSnapshot {
        std::chrono::system_clock::time_point timestamp;    // When snapshot was taken
        MemoryInfo memory;                                  // Current memory information
        Statistics stats;                                   // Summary statistics
    };

    /**
     * @brief Configuration for memory profiler
     */
    struct ProfilingConfig {
        uint32_t sample_interval_ms = 5000;     // Sampling interval in milliseconds
        bool enable_auto_logging = true;        // Automatically print logs on each snapshot
    };

    /**
     * @brief Threshold type for memory monitoring
     */
    enum class ThresholdType {
        TotalFree,             // Monitor total free memory (bytes)
        InternalFree,          // Monitor internal SRAM free memory (bytes)
        ExternalFree,          // Monitor external PSRAM free memory (bytes)
        TotalFreePercent,      // Monitor total free percentage
        InternalFreePercent,    // Monitor internal SRAM free percentage
        ExternalFreePercent,    // Monitor external PSRAM free percentage
        TotalLargestFreeBlock,    // Monitor total largest free block (bytes)
        InternalLargestFreeBlock, // Monitor internal SRAM largest free block (bytes)
        ExternalLargestFreeBlock, // Monitor external PSRAM largest free block (bytes)
    };

    using ProfilingSignal = boost::signals2::signal<void(const ProfileSnapshot &)>;
    using ProfilingSignalSlot = ProfilingSignal::slot_type;

    using ThresholdSignal = boost::signals2::signal<void(const ProfileSnapshot &)>;
    using ThresholdSignalSlot = ThresholdSignal::slot_type;

    /**
     * @brief Signal connection type
     *
     * @note This is an RAII smart handle for managing the lifetime of callbacks.
     *       When this object is destroyed, the corresponding callback is automatically disconnected.
     *       It is recommended to use `std::move()` to transfer ownership for manual management of the connection lifetime.
     */
    using SignalConnection = boost::signals2::scoped_connection;

    MemoryProfiler() = default;
    ~MemoryProfiler();

    // Disable copy and move
    MemoryProfiler(const MemoryProfiler &) = delete;
    MemoryProfiler &operator=(const MemoryProfiler &) = delete;
    MemoryProfiler(MemoryProfiler &&) = delete;
    MemoryProfiler &operator=(MemoryProfiler &&) = delete;

    /**
     * @brief Configure profiler settings
     *
     * @param config Configuration structure
     * @return true on success, false on failure
     */
    bool configure_profiling(const ProfilingConfig &config);

    /**
     * @brief Get current configuration
     *
     * @return Current configuration
     */
    ProfilingConfig get_profiling_config() const
    {
        boost::lock_guard lock(mutex_);
        return config_;
    }

    /**
     * @brief Start periodic profiling using TaskScheduler
     *
     * @param scheduler Shared pointer to TaskScheduler instance
     * @param period_ms Profiling period in milliseconds (0 = use config.sample_interval_ms)
     * @return true on success, false on failure
     */
    bool start_profiling(std::shared_ptr<TaskScheduler> scheduler, uint32_t period_ms = 0);

    /**
     * @brief Stop periodic profiling
     */
    void stop_profiling();

    /**
     * @brief Reset profiling data, does not affect configuration.
     */
    void reset_profiling();

    /**
     * @brief Check if profiling is active
     *
     * @return true if profiling, false otherwise
     */
    bool is_profiling() const
    {
        boost::lock_guard lock(mutex_);
        return (task_scheduler_ != nullptr);
    }

    /**
     * @brief Get the latest snapshot
     *
     * @return Shared pointer to the snapshot (nullptr if no snapshot available)
     */
    std::shared_ptr<ProfileSnapshot> get_profiling_latest_snapshot() const
    {
        boost::lock_guard lock(mutex_);
        return latest_snapshot_;
    }

    /**
     * @brief Connect a signal for profiling, called on each profiling task
     *
     * @param slot Signal slot function to be called on each profiling task, when a snapshot is taken
     * @return Signal connection object for managing the signal lifetime
     */
    SignalConnection connect_profiling_signal(ProfilingSignalSlot slot);

    /**
     * @brief Connect a signal for thresholds, called when thresholds are met
     *
     * @param type Type of threshold to monitor
     * @param threshold_value Threshold value (interpretation depends on type)
     * @param slot Signal slot function to be called when a threshold is met
     * @return Signal connection object for managing the signal lifetime
     */
    SignalConnection connect_threshold_signal(
        ThresholdType type, uint32_t threshold_value, ThresholdSignalSlot slot
    );

    /**
     * @brief Get singleton instance
     *
     * @return Reference to the singleton instance
     */
    static MemoryProfiler &get_instance()
    {
        static MemoryProfiler inst;
        return inst;
    }

    /**
     * @brief Take a snapshot of current memory state
     *
     * @param last_snapshot Last snapshot (set to nullptr for the first snapshot)
     * @return Shared pointer to the snapshot
     */
    static std::shared_ptr<ProfileSnapshot> take_snapshot(ProfileSnapshot *last_snapshot = nullptr);

    /**
     * @brief Print snapshot to log in formatted table
     *
     * @param snapshot Snapshot to print
     */
    static void print_snapshot(const ProfileSnapshot &snapshot);

private:
    /**
     * @brief Configuration for a threshold listener
     *
     */
    struct ThresholdListener {
        ThresholdType type;
        uint32_t threshold_value;
        ThresholdSignal signal;
    };

    /**
     * @brief Check thresholds and trigger callbacks
     *
     * @param snapshot Snapshot to check
     */
    void check_thresholds_and_trigger_callbacks(const ProfileSnapshot &snapshot) const;

    /**
     * @brief Sample current memory state
     *
     * @return Shared pointer to the memory information
     */
    static void sample_memory(MemoryInfo &mem_info);

    /**
     * @brief Check if a threshold is met
     *
     * @param snapshot Snapshot to check
     * @param type Threshold type
     * @param threshold_value Threshold value
     * @return true if threshold is met, false otherwise
     */
    static bool check_threshold(const ProfileSnapshot &snapshot, ThresholdType type, uint32_t threshold_value);

    mutable boost::mutex mutex_;
    ProfilingConfig config_{};
    std::shared_ptr<TaskScheduler> task_scheduler_;
    TaskScheduler::TaskId profiling_task_id_ = 0;

    // Profiling
    std::shared_ptr<ProfileSnapshot> latest_snapshot_;
    ProfilingSignal profiling_signal_;

    // Threshold
    std::vector<ThresholdListener> threshold_listeners_;
};

BROOKESIA_DESCRIBE_ENUM(
    MemoryProfiler::ThresholdType,
    TotalFree, InternalFree, ExternalFree,
    TotalFreePercent, InternalFreePercent, ExternalFreePercent,
    TotalLargestFreeBlock, InternalLargestFreeBlock, ExternalLargestFreeBlock
)
BROOKESIA_DESCRIBE_STRUCT(MemoryProfiler::ProfilingConfig, (), (sample_interval_ms, enable_auto_logging))
BROOKESIA_DESCRIBE_STRUCT(
    MemoryProfiler::HeapInfo, (), (total_size, free_size, largest_free_block, free_percent, used_percent)
)
BROOKESIA_DESCRIBE_STRUCT(
    MemoryProfiler::MemoryInfo, (), (
        internal, external, total_size, total_free, total_free_percent, total_largest_free_block
    )
)
BROOKESIA_DESCRIBE_STRUCT(
    MemoryProfiler::Statistics, (), (
        sample_count, min_total_free, min_internal_free, min_external_free, min_total_free_percent,
        min_internal_free_percent, min_external_free_percent, min_total_largest_free_block,
        min_internal_largest_free_block, min_external_largest_free_block
    )
)
BROOKESIA_DESCRIBE_STRUCT(MemoryProfiler::ProfileSnapshot, (), (timestamp, memory, stats))

} // namespace esp_brookesia::lib_utils
