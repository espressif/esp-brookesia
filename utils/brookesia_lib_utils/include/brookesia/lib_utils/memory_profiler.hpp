/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
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
#include "boost/signals2.hpp"
#include "boost/thread/mutex.hpp"
#include "brookesia/lib_utils/macro_configs.h"
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/lib_utils/task_scheduler.hpp"

namespace esp_brookesia::lib_utils {

/**
 * @brief Heap memory profiler for internal RAM and PSRAM usage.
 *
 * This class provides a C++ interface to monitor ESP32 heap memory usage,
 * including internal SRAM and external PSRAM. It integrates naturally with
 * TaskScheduler for periodic profiling.
 */
class MemoryProfiler {
public:
    /**
     * @brief Snapshot of a single heap region.
     */
    struct HeapInfo {
        size_t total_size = 0;          ///< Total heap size in bytes.
        size_t free_size = 0;           ///< Free heap size in bytes.
        size_t largest_free_block = 0;  ///< Size of the largest free block in bytes.
        size_t free_percent = 0;        ///< Free memory ratio expressed as a percentage.
        size_t used_percent = 0;        ///< Used memory ratio expressed as a percentage.

        HeapInfo() = default;
        HeapInfo(size_t total, size_t free, size_t largest)
            : total_size(total), free_size(free), largest_free_block(largest)
            , free_percent(total > 0 ? free * 100 / total : 0)
            , used_percent(total > 0 ? (100 - free_percent) : 0)
        {
        }
    };

    /**
     * @brief Aggregated memory information for all monitored heap regions.
     */
    struct MemoryInfo {
        HeapInfo internal;                ///< Internal SRAM heap information (`MALLOC_CAP_INTERNAL`).
        HeapInfo external;                ///< External PSRAM heap information (`MALLOC_CAP_SPIRAM`).
        size_t total_size = 0;            ///< Total heap size in bytes across all monitored heaps.
        size_t total_free = 0;            ///< Total free memory in bytes across all monitored heaps.
        size_t total_free_percent = 0;    ///< Total free memory ratio expressed as a percentage.
        size_t total_largest_free_block = 0; ///< Largest free block in bytes across all monitored heaps.
    };

    /**
     * @brief Running minimum statistics accumulated across snapshots.
     */
    struct Statistics {
        size_t sample_count = 0;                    ///< Number of snapshots processed.
        size_t min_total_free = 0;                  ///< Minimum total free memory observed, in bytes.
        size_t min_internal_free = 0;               ///< Minimum internal free memory observed, in bytes.
        size_t min_external_free = 0;               ///< Minimum external free memory observed, in bytes.
        size_t min_total_free_percent = 0;          ///< Minimum total free percentage observed.
        size_t min_internal_free_percent = 0;       ///< Minimum internal free percentage observed.
        size_t min_external_free_percent = 0;       ///< Minimum external free percentage observed.
        size_t min_total_largest_free_block = 0;    ///< Minimum total largest free block observed, in bytes.
        size_t min_internal_largest_free_block = 0; ///< Minimum internal largest free block observed, in bytes.
        size_t min_external_largest_free_block = 0; ///< Minimum external largest free block observed, in bytes.

        Statistics() = default;
        Statistics(const MemoryInfo &cur_memory, const Statistics &last_stats);
    };

    /**
     * @brief Memory profile snapshot captured at one point in time.
     */
    struct ProfileSnapshot {
        std::chrono::system_clock::time_point timestamp; ///< Snapshot capture time.
        MemoryInfo memory;                                ///< Instantaneous memory information.
        Statistics stats;                                 ///< Running statistics after this snapshot.
    };

    /**
     * @brief Configuration for periodic memory profiling.
     */
    struct ProfilingConfig {
        uint32_t sample_interval_ms = 5000; ///< Sampling interval in milliseconds.
        bool enable_auto_logging = true;    ///< Whether to log each generated snapshot automatically.
    };

    /**
     * @brief Metric used when registering threshold callbacks.
     */
    enum class ThresholdType {
        TotalFree,              ///< Total free memory in bytes.
        InternalFree,           ///< Internal SRAM free memory in bytes.
        ExternalFree,           ///< External PSRAM free memory in bytes.
        TotalFreePercent,       ///< Total free memory as a percentage.
        InternalFreePercent,    ///< Internal SRAM free memory as a percentage.
        ExternalFreePercent,    ///< External PSRAM free memory as a percentage.
        TotalLargestFreeBlock,  ///< Largest free block across all heaps, in bytes.
        InternalLargestFreeBlock, ///< Largest internal SRAM free block, in bytes.
        ExternalLargestFreeBlock, ///< Largest external PSRAM free block, in bytes.
        MinTotalFree,              ///< Minimum total free memory in bytes.
        MinInternalFree,           ///< Minimum internal SRAM free memory in bytes.
        MinExternalFree,           ///< Minimum external PSRAM free memory in bytes.
        MinTotalFreePercent,       ///< Minimum total free memory as a percentage.
        MinInternalFreePercent,    ///< Minimum internal SRAM free memory as a percentage.
        MinExternalFreePercent,    ///< Minimum external PSRAM free memory as a percentage.
        MinTotalLargestFreeBlock,  ///< Minimum largest free block across all heaps, in bytes.
        MinInternalLargestFreeBlock, ///< Minimum largest internal SRAM free block, in bytes.
        MinExternalLargestFreeBlock, ///< Minimum largest external PSRAM free block, in bytes.
    };

    /**
     * @brief Signal type emitted for each profiling snapshot.
     */
    using ProfilingSignal = boost::signals2::signal<void(const ProfileSnapshot &)>;
    /**
     * @brief Slot type accepted by `connect_profiling_signal()`.
     */
    using ProfilingSignalSlot = ProfilingSignal::slot_type;

    /**
     * @brief Signal type emitted when a threshold condition is met.
     */
    using ThresholdSignal = boost::signals2::signal<void(const ProfileSnapshot &)>;
    /**
     * @brief Slot type accepted by `connect_threshold_signal()`.
     */
    using ThresholdSignalSlot = ThresholdSignal::slot_type;

    /**
     * @brief Signal connection type
     *
     * @note This is an RAII smart handle for managing the lifetime of callbacks.
     *       When this object is destroyed, the corresponding callback is automatically disconnected.
     *       It is recommended to use `std::move()` to transfer ownership for manual management of the connection lifetime.
     */
    using SignalConnection = boost::signals2::scoped_connection;

    /**
     * @brief Construct an idle memory profiler.
     */
    MemoryProfiler() = default;
    ~MemoryProfiler();

    /// Copy construction is not supported.
    MemoryProfiler(const MemoryProfiler &) = delete;
    /// Copy assignment is not supported.
    MemoryProfiler &operator=(const MemoryProfiler &) = delete;
    /// Move construction is not supported.
    MemoryProfiler(MemoryProfiler &&) = delete;
    /// Move assignment is not supported.
    MemoryProfiler &operator=(MemoryProfiler &&) = delete;

    /**
     * @brief Update the profiling configuration.
     *
     * @param config New profiler configuration.
     * @return `true` after the configuration is stored.
     */
    bool configure_profiling(const ProfilingConfig &config);

    /**
     * @brief Get the current profiling configuration.
     *
     * @return Active `ProfilingConfig`.
     */
    ProfilingConfig get_profiling_config() const
    {
        boost::lock_guard lock(mutex_);
        return config_;
    }

    /**
     * @brief Start periodic profiling with a task scheduler.
     *
     * @param scheduler Scheduler used to execute periodic sampling.
     * @param period_ms Sampling period in milliseconds. Pass `0` to use `config.sample_interval_ms`.
     * @return `true` on success, or `false` if startup fails.
     */
    bool start_profiling(std::shared_ptr<TaskScheduler> scheduler, uint32_t period_ms = 0);

    /**
     * @brief Stop periodic profiling.
     */
    void stop_profiling();

    /**
     * @brief Reset captured snapshots and registered callbacks without changing configuration.
     */
    void reset_profiling();

    /**
     * @brief Check whether periodic profiling is active.
     *
     * @return `true` when a profiling task is active, or `false` otherwise.
     */
    bool is_profiling() const
    {
        boost::lock_guard lock(mutex_);
        return (task_scheduler_ != nullptr);
    }

    /**
     * @brief Get the latest captured snapshot.
     *
     * @return Shared pointer to the latest snapshot, or `nullptr` if no snapshot is available.
     */
    std::shared_ptr<ProfileSnapshot> get_profiling_latest_snapshot() const
    {
        boost::lock_guard lock(mutex_);
        return latest_snapshot_;
    }

    /**
     * @brief Subscribe to every profiling snapshot.
     *
     * @param slot Callback invoked whenever a new snapshot is produced.
     * @return RAII connection handle for the registered callback.
     */
    SignalConnection connect_profiling_signal(ProfilingSignalSlot slot);

    /**
     * @brief Subscribe to threshold events for a specific metric.
     *
     * @param type Metric to monitor.
     * @param threshold_value Threshold value interpreted according to @p type.
     * @param slot Callback invoked when the threshold is met by a snapshot.
     * @return RAII connection handle for the registered callback.
     */
    SignalConnection connect_threshold_signal(
        ThresholdType type, uint32_t threshold_value, ThresholdSignalSlot slot
    );

    /**
     * @brief Get the process-wide singleton instance.
     *
     * @return Reference to the singleton profiler.
     */
    static MemoryProfiler &get_instance()
    {
        static MemoryProfiler inst;
        return inst;
    }

    /**
     * @brief Capture the current memory state.
     *
     * @param last_snapshot Previous snapshot used to update running statistics, or `nullptr` for the first sample.
     * @return Shared pointer to the new snapshot, or `nullptr` if allocation fails.
     */
    static std::shared_ptr<ProfileSnapshot> take_snapshot(ProfileSnapshot *last_snapshot = nullptr);

    /**
     * @brief Print a formatted snapshot summary to the log.
     *
     * @param snapshot Snapshot to print.
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
     * @param mem_info Output structure populated with the latest memory information.
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
