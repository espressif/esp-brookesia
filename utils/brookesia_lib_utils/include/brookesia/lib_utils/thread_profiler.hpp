/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>
#include <boost/signals2.hpp>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "boost/thread/mutex.hpp"
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/lib_utils/task_scheduler.hpp"

namespace esp_brookesia::lib_utils {

/**
 * @brief Thread profiler for monitoring FreeRTOS task performance
 *
 * This class provides a C++ interface to monitor FreeRTOS task CPU usage,
 * stack usage, and other runtime statistics. It integrates naturally with
 * TaskScheduler for periodic profiling.
 */
class ThreadProfiler {
public:
    /**
     * @brief Task state enumeration (from FreeRTOS eTaskState)
     */
    enum class TaskState {
        Running = 0,    // eRunning
        Ready,          // eReady
        Blocked,        // eBlocked
        Suspended,      // eSuspended
        Deleted,        // eDeleted
        Invalid         // eInvalid
    };

    /**
     * @brief Task status for newly created or deleted tasks
     */
    enum class TaskStatus {
        Normal,         // Task exists in both snapshots
        Created,        // Task was created during sampling
        Deleted         // Task was deleted during sampling
    };

    /**
     * @brief Primary sort criteria (optional, can be disabled)
     */
    enum class PrimarySortBy {
        None,           // No primary sorting
        CoreId,         // Sort by CPU core ID first
    };

    /**
     * @brief Secondary sort criteria (always applied)
     */
    enum class SecondarySortBy {
        CpuPercent,     // Sort by CPU usage percentage (descending)
        Priority,       // Sort by task priority (descending)
        StackUsage,     // Sort by stack high water mark (ascending - lower means more used)
        Name            // Sort by task name (alphabetically)
    };

    /**
     * @brief Threshold type for filtering tasks
     */
    enum class ThresholdType {
        CpuPercent,     // Filter by CPU usage percentage
        Priority,       // Filter by task priority
        StackUsage      // Filter by stack high water mark (lower values = more critical)
    };

    /**
     * @brief Information about a single task
     */
    struct TaskInfo {
        std::string name;                       // Task name
        TaskHandle_t handle = nullptr;          // Task handle
        TaskState state = TaskState::Invalid;   // Current state
        uint32_t priority = 0;                  // Task priority
        BaseType_t core_id = -1;                // CPU core ID (-1 if not bound)
        uint32_t stack_high_water_mark = 0;     // Stack high water mark (bytes)
        bool is_stack_external = false;         // Stack allocated in external RAM
        uint32_t runtime_counter = 0;           // Runtime counter (raw)
        uint32_t elapsed_time = 0;              // Elapsed time during sample period
        uint32_t cpu_percent = 0;               // CPU usage percentage
        TaskStatus status = TaskStatus::Normal; // Task status (created/deleted/normal)
    };

    /**
     * @brief Statistics summary
     */
    struct Statistics {
        size_t total_tasks = 0;                 // Total number of tasks
        size_t running_tasks = 0;               // Number of running tasks
        size_t blocked_tasks = 0;               // Number of blocked tasks
        size_t suspended_tasks = 0;             // Number of suspended tasks
        uint32_t total_cpu_percent = 0;         // Total CPU usage (should be ~100% * num_cores)
        uint32_t sample_duration_ms = 0;        // Duration of sampling period
    };

    /**
     * @brief Snapshot of all task information at a point in time
     */
    struct ProfileSnapshot {
        std::chrono::system_clock::time_point timestamp{};    // When snapshot was taken
        std::vector<TaskInfo> tasks;                        // All task information
        Statistics stats{};                                    // Summary statistics
        uint32_t total_runtime = 0;                         // Total runtime counter
    };

    struct SampleResult {
        std::chrono::system_clock::time_point timestamp;
        std::vector<TaskStatus_t> task_status;
        uint32_t runtime;
    };

    /**
     * @brief Configuration for thread profiler
     */
    struct ProfilingConfig {
        uint32_t sampling_duration_ms = 1000;                   // Sampling duration between two snapshots
        uint32_t profiling_interval_ms = 5000;                  // Profiling interval for each sampling
        PrimarySortBy primary_sort = PrimarySortBy::CoreId;     // Primary sort (first level, can be None)
        SecondarySortBy secondary_sort = SecondarySortBy::CpuPercent;  // Secondary sort (second level, always applied)
        bool enable_auto_logging = true;                       // Automatically print logs on each snapshot
    };

    using ProfilingSignalSlot = std::function<void(const ProfileSnapshot &)>;
    using ThresholdSignalSlot = std::function<void(const std::vector<TaskInfo>&)>;

    using ProfilingSignal = boost::signals2::signal<void(const ProfileSnapshot &)>;
    using ThresholdSignal = boost::signals2::signal<void(const std::vector<TaskInfo>&)>;

    /**
     * @brief Signal connection type
     *
     * @note This is an RAII smart handle for managing the lifetime of callbacks.
     *       When this object is destroyed, the corresponding callback is automatically disconnected.
     *       It is recommended to use `std::move()` to transfer ownership for manual management of the connection lifetime.
     */
    using SignalConnection = boost::signals2::scoped_connection;

    // Disable copy and move
    ThreadProfiler(const ThreadProfiler &) = delete;
    ThreadProfiler &operator=(const ThreadProfiler &) = delete;
    ThreadProfiler(ThreadProfiler &&) = delete;
    ThreadProfiler &operator=(ThreadProfiler &&) = delete;

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
    ProfilingConfig get_profiling_config() const;

    /**
     * @brief Start periodic profiling using TaskScheduler
     *
     * @param scheduler Reference to TaskScheduler instance
     *                  NOTE: The caller must ensure the TaskScheduler remains valid
     *                  until stop_profiling() is called.
     * @param sampling_duration_ms Sampling duration in milliseconds. Should be less than profiling_interval_ms
     *      (0 = use config.sampling_duration_ms)
     * @param profiling_interval_ms Profiling interval in milliseconds. Should be greater than sampling_duration_ms
     *      (0 = use config.profiling_interval_ms)
     * @return true on success, false on failure
     */
    bool start_profiling(
        std::shared_ptr<TaskScheduler> scheduler, uint32_t sampling_duration_ms = 0, uint32_t profiling_interval_ms = 0
    );

    /**
     * @brief Stop periodic profiling
     *
     */
    void stop_profiling();

    /**
     * @brief Reset profiling data
     *
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
    std::shared_ptr<ProfileSnapshot> get_profiling_latest_snapshot()
    {
        boost::lock_guard lock(mutex_);
        return latest_snapshot_;
    }

    /**
     * @brief Connect a signal for profiling, called on each profiling task
     *
     * @param slot Signal slot function to be called on each profiling task, when a snapshot is taken
     * @return Signal connection object for managing the connection lifetime
     */
    SignalConnection connect_profiling_signal(ProfilingSignalSlot slot);

    /**
     * @brief Connect a signal for thresholds, called when thresholds are met
     *
     * @param type Type of threshold to monitor
     * @param threshold_value Threshold value (interpretation depends on type)
     * @param slot Signal slot function to be called when a threshold is met
     * @return Signal connection object for managing the connection lifetime
     */
    SignalConnection connect_threshold_signal(
        ThresholdType type, uint32_t threshold_value, ThresholdSignalSlot slot
    );

    static ThreadProfiler &get_instance()
    {
        static ThreadProfiler inst;
        return inst;
    }

    /**
     * @brief Sample task states at a single point in time
     *
     * @param out_array Output array for task status
     * @param out_runtime Output total runtime counter
     * @return true on success, false on failure
     */
    static std::shared_ptr<SampleResult> sample_tasks();

    /**
     * @brief Take a snapshot by performing one sample
     *
     * @param start_result Starting sample result
     * @param end_result Ending sample result
     * @return Shared pointer to the snapshot
     */
    static std::shared_ptr<ProfileSnapshot> take_snapshot(
        const SampleResult &start_result, const SampleResult &end_result
    );

    /**
     * @brief Sort tasks according to configuration
     *
     * @param tasks Task vector to sort (modified in place)
     * @param primary_sort Primary sort criteria
     * @param secondary_sort Secondary sort
     *
     * @note The tasks will be sorted by the primary sort criteria first, then by the secondary sort criteria.
     */
    static void sort_tasks(
        std::vector<TaskInfo> &tasks, PrimarySortBy primary_sort = PrimarySortBy::CoreId,
        SecondarySortBy secondary_sort = SecondarySortBy::CpuPercent
    );

    /**
     * @brief Print snapshot to log in formatted table
     *
     * @param snapshot Snapshot to print
     * @param primary_sort Primary sort criteria. The first column will be the primary sort column.
     * @param secondary_sort Secondary sort. The second column will be the secondary sort column.
     */
    static void print_snapshot(
        const ProfileSnapshot &snapshot, PrimarySortBy primary_sort = PrimarySortBy::CoreId,
        SecondarySortBy secondary_sort = SecondarySortBy::CpuPercent
    );

    /**
     * @brief Find task by name in latest snapshot
     *
     * @param snapshot Snapshot to search in
     * @param name Task name to search for
     * @param task Output task info
     * @return true if task found, false otherwise
     */
    static bool get_task_by_name(const ProfileSnapshot &snapshot, const std::string &name, TaskInfo &task);

    /**
     * @brief Get tasks filtered by threshold
     *
     * @param snapshot Snapshot to search in
     * @param threshold_value Threshold value (interpretation depends on type)
     * @param type Type of threshold (CpuPercent, Priority, or StackUsage)
     * @return Vector of tasks meeting the threshold
     *
     * @note For CpuPercent: returns tasks with CPU >= threshold_value
     *       For Priority: returns tasks with priority >= threshold_value
     *       For StackUsage: returns tasks with stack HWM <= threshold_value (lower = more used)
     */
    static std::vector<TaskInfo> get_tasks_above_threshold(
        const ProfileSnapshot &snapshot, ThresholdType type, uint32_t threshold_value
    );

private:
    ThreadProfiler() = default;
    ~ThreadProfiler();

    /**
     * @brief Check thresholds and trigger signals if conditions are met
     *
     * @param snapshot Current profile snapshot
     */
    void check_thresholds_and_trigger_callbacks(const ProfileSnapshot &snapshot);

    /**
     * @brief Convert FreeRTOS eTaskState to TaskState enum
     *
     * @param state FreeRTOS task state
     * @return ThreadProfiler::TaskState
     */
    static TaskState convert_task_state(eTaskState state);

    /**
     * @brief Get string representation of task state
     *
     * @param state Task state
     * @return String representation
     */
    static const char *get_state_string(TaskState state);

    /**
     * @brief Configuration for a threshold listener
     */
    struct ThresholdListener {
        ThresholdType type;
        uint32_t threshold_value;
    };

    ProfilingConfig config_{};
    mutable boost::mutex mutex_;

    // Profiling
    std::shared_ptr<ProfileSnapshot> latest_snapshot_;
    std::shared_ptr<SampleResult> prev_result_;
    ProfilingSignal profiling_signal_;
    // Threshold
    std::map<ThresholdType, ThresholdSignal> threshold_signals_;
    std::vector<ThresholdListener> threshold_listeners_;

    // TaskScheduler integration
    std::shared_ptr<TaskScheduler> task_scheduler_;
    TaskScheduler::TaskId profiling_task_id_ = 0;
    TaskScheduler::TaskId sampling_task_id_ = 0;
};

BROOKESIA_DESCRIBE_ENUM(ThreadProfiler::TaskState, Running, Ready, Blocked, Suspended, Deleted, Invalid)
BROOKESIA_DESCRIBE_ENUM(ThreadProfiler::TaskStatus, Normal, Created, Deleted)
BROOKESIA_DESCRIBE_ENUM(ThreadProfiler::PrimarySortBy, None, CoreId)
BROOKESIA_DESCRIBE_ENUM(ThreadProfiler::SecondarySortBy, CpuPercent, Priority, StackUsage, Name)
BROOKESIA_DESCRIBE_ENUM(ThreadProfiler::ThresholdType, CpuPercent, Priority, StackUsage)
BROOKESIA_DESCRIBE_STRUCT(
    ThreadProfiler::TaskInfo, (),
    (name, priority, core_id, stack_high_water_mark, is_stack_external, runtime_counter, elapsed_time, cpu_percent)
)
BROOKESIA_DESCRIBE_STRUCT(
    ThreadProfiler::Statistics, (),
    (total_tasks, running_tasks, blocked_tasks, suspended_tasks, total_cpu_percent, sample_duration_ms)
)
BROOKESIA_DESCRIBE_STRUCT(ThreadProfiler::ProfilingConfig, (), (sampling_duration_ms, profiling_interval_ms, enable_auto_logging))

} // namespace esp_brookesia::lib_utils
