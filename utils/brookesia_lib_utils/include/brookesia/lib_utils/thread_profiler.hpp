/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#if defined(ESP_PLATFORM)
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
#include "brookesia/lib_utils/macro_configs.h"
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/lib_utils/task_scheduler.hpp"

namespace esp_brookesia::lib_utils {

/**
 * @brief FreeRTOS task profiler for CPU and stack usage monitoring.
 *
 * This class provides a C++ interface to monitor FreeRTOS task CPU usage,
 * stack usage, and other runtime statistics. It integrates naturally with
 * TaskScheduler for periodic profiling.
 */
class ThreadProfiler {
public:
    /**
     * @brief Task state mirrored from `FreeRTOS` `eTaskState`.
     */
    enum class TaskState {
        Running = 0, ///< `eRunning`.
        Ready,       ///< `eReady`.
        Blocked,     ///< `eBlocked`.
        Suspended,   ///< `eSuspended`.
        Deleted,     ///< `eDeleted`.
        Invalid      ///< `eInvalid`.
    };

    /**
     * @brief Presence status of a task across two samples.
     */
    enum class TaskStatus {
        Normal,  ///< Task exists in both samples.
        Created, ///< Task appeared during the measurement window.
        Deleted  ///< Task disappeared during the measurement window.
    };

    /**
     * @brief Optional primary sort criterion.
     */
    enum class PrimarySortBy {
        None,   ///< Disable primary sorting.
        CoreId, ///< Sort by CPU core ID first.
    };

    /**
     * @brief Secondary sort criterion applied within the primary ordering.
     */
    enum class SecondarySortBy {
        CpuPercent, ///< Sort by CPU usage percentage in descending order.
        Priority,   ///< Sort by task priority in descending order.
        StackUsage, ///< Sort by stack high-water mark in ascending order.
        Name        ///< Sort by task name alphabetically.
    };

    /**
     * @brief Metric used for threshold filtering and callbacks.
     */
    enum class ThresholdType {
        CpuPercent, ///< Filter by CPU usage percentage.
        Priority,   ///< Filter by task priority.
        StackUsage  ///< Filter by stack high-water mark.
    };

    /**
     * @brief Profile data for a single FreeRTOS task.
     */
    struct TaskInfo {
        std::string name;                       ///< Task name.
        TaskHandle_t handle = nullptr;          ///< FreeRTOS task handle.
        TaskState state = TaskState::Invalid;   ///< Current task state.
        uint32_t priority = 0;                  ///< Current task priority.
        BaseType_t core_id = -1;                ///< Bound CPU core ID, or `-1` if unbound.
        uint32_t stack_high_water_mark = 0;     ///< Stack high-water mark in bytes.
        bool is_stack_external = false;         ///< Whether the stack is allocated in external RAM.
        uint32_t runtime_counter = 0;           ///< Raw runtime counter captured in the ending sample.
        uint32_t elapsed_time = 0;              ///< Runtime counter delta during the measurement window.
        uint32_t cpu_percent = 0;               ///< CPU usage percentage over the measurement window.
        TaskStatus status = TaskStatus::Normal; ///< Presence status relative to the two samples.
    };

    /**
     * @brief Aggregate statistics for a profiling snapshot.
     */
    struct Statistics {
        size_t total_tasks = 0;          ///< Total number of tasks in the snapshot.
        size_t running_tasks = 0;        ///< Number of running tasks.
        size_t blocked_tasks = 0;        ///< Number of blocked tasks.
        size_t suspended_tasks = 0;      ///< Number of suspended tasks.
        uint32_t total_cpu_percent = 0;  ///< Aggregate CPU usage percentage.
        uint32_t sample_duration_ms = 0; ///< Measurement window duration in milliseconds.
    };

    /**
     * @brief Snapshot of all sampled task information.
     */
    struct ProfileSnapshot {
        std::chrono::system_clock::time_point timestamp{}; ///< Snapshot capture time.
        std::vector<TaskInfo> tasks;                       ///< Task data for every sampled task.
        Statistics stats{};                               ///< Aggregate snapshot statistics.
        uint32_t total_runtime = 0;                       ///< Total runtime counter captured in the ending sample.
    };

    /**
     * @brief Raw sample captured at a single instant.
     */
    struct SampleResult {
        std::chrono::system_clock::time_point timestamp; ///< Sample capture time.
        std::vector<TaskStatus_t> task_status;           ///< Raw FreeRTOS task status array.
        uint32_t runtime;                                ///< Raw total runtime counter.
    };

    /**
     * @brief Configuration for periodic thread profiling.
     */
    struct ProfilingConfig {
        uint32_t sampling_duration_ms = 1000;                 ///< Delay between the start and end sample, in milliseconds.
        uint32_t profiling_interval_ms = 5000;                ///< Interval between profiling rounds, in milliseconds.
        PrimarySortBy primary_sort = PrimarySortBy::CoreId;   ///< Primary sort criterion.
        SecondarySortBy secondary_sort = SecondarySortBy::CpuPercent; ///< Secondary sort criterion.
        bool enable_auto_logging = true;                      ///< Whether to log each generated snapshot automatically.
    };

    /**
     * @brief Callback type accepted by `connect_profiling_signal()`.
     */
    using ProfilingSignalSlot = std::function<void(const ProfileSnapshot &)>;
    /**
     * @brief Callback type accepted by `connect_threshold_signal()`.
     */
    using ThresholdSignalSlot = std::function<void(const std::vector<TaskInfo>&)>;

    /**
     * @brief Signal type emitted for each profiling snapshot.
     */
    using ProfilingSignal = boost::signals2::signal<void(const ProfileSnapshot &)>;
    /**
     * @brief Signal type emitted when threshold matches are found.
     */
    using ThresholdSignal = boost::signals2::signal<void(const std::vector<TaskInfo>&)>;

    /**
     * @brief Signal connection type
     *
     * @note This is an RAII smart handle for managing the lifetime of callbacks.
     *       When this object is destroyed, the corresponding callback is automatically disconnected.
     *       It is recommended to use `std::move()` to transfer ownership for manual management of the connection lifetime.
     */
    using SignalConnection = boost::signals2::scoped_connection;

    /// Copy construction is not supported.
    ThreadProfiler(const ThreadProfiler &) = delete;
    /// Copy assignment is not supported.
    ThreadProfiler &operator=(const ThreadProfiler &) = delete;
    /// Move construction is not supported.
    ThreadProfiler(ThreadProfiler &&) = delete;
    /// Move assignment is not supported.
    ThreadProfiler &operator=(ThreadProfiler &&) = delete;

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
    ProfilingConfig get_profiling_config() const;

    /**
     * @brief Start periodic thread profiling using a task scheduler.
     *
     * @param scheduler Scheduler used to run the two-stage sampling jobs.
     *                  The caller must keep it alive until `stop_profiling()` returns.
     * @param sampling_duration_ms Time between the first and second sample in milliseconds.
     *                             Pass `0` to use `config.sampling_duration_ms`.
     * @param profiling_interval_ms Period between profiling rounds in milliseconds.
     *                              Pass `0` to use `config.profiling_interval_ms`.
     * @return `true` on success, or `false` if profiling cannot be started.
     */
    bool start_profiling(
        std::shared_ptr<TaskScheduler> scheduler, uint32_t sampling_duration_ms = 0, uint32_t profiling_interval_ms = 0
    );

    /**
     * @brief Stop periodic profiling.
     */
    void stop_profiling();

    /**
     * @brief Reset captured data and registered callbacks without changing configuration.
     */
    void reset_profiling();

    /**
     * @brief Check whether periodic profiling is active.
     *
     * @return `true` when profiling tasks are active, or `false` otherwise.
     */
    bool is_profiling() const
    {
        boost::lock_guard lock(mutex_);
        return (task_scheduler_ != nullptr);
    }

    /**
     * @brief Get the latest captured profiling snapshot.
     *
     * @return Shared pointer to the latest snapshot, or `nullptr` if none is available.
     */
    std::shared_ptr<ProfileSnapshot> get_profiling_latest_snapshot()
    {
        boost::lock_guard lock(mutex_);
        return latest_snapshot_;
    }

    /**
     * @brief Subscribe to every generated profiling snapshot.
     *
     * @param slot Callback invoked whenever a new snapshot is produced.
     * @return RAII connection handle for the registered callback.
     */
    SignalConnection connect_profiling_signal(ProfilingSignalSlot slot);

    /**
     * @brief Subscribe to threshold matches for a specific metric.
     *
     * @param type Metric to monitor.
     * @param threshold_value Threshold value interpreted according to @p type.
     * @param slot Callback invoked with the tasks that matched the threshold.
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
    static ThreadProfiler &get_instance()
    {
        static ThreadProfiler inst;
        return inst;
    }

    /**
     * @brief Capture a single raw scheduler sample.
     *
     * @return Shared pointer to the captured sample, or `nullptr` on failure.
     */
    static std::shared_ptr<SampleResult> sample_tasks();

    /**
     * @brief Build a profiling snapshot from two raw samples.
     *
     * @param start_result Sample taken at the beginning of the measurement window.
     * @param end_result Sample taken at the end of the measurement window.
     * @return Shared pointer to the computed snapshot, or `nullptr` on failure.
     */
    static std::shared_ptr<ProfileSnapshot> take_snapshot(
        const SampleResult &start_result, const SampleResult &end_result
    );

    /**
     * @brief Sort task entries in place.
     *
     * @param tasks Task vector to sort in place.
     * @param primary_sort Optional primary sort criterion.
     * @param secondary_sort Secondary sort criterion.
     *
     * @note The tasks will be sorted by the primary sort criteria first, then by the secondary sort criteria.
     */
    static void sort_tasks(
        std::vector<TaskInfo> &tasks, PrimarySortBy primary_sort = PrimarySortBy::CoreId,
        SecondarySortBy secondary_sort = SecondarySortBy::CpuPercent
    );

    /**
     * @brief Print a formatted snapshot table to the log.
     *
     * @param snapshot Snapshot to print.
     * @param primary_sort Primary sort criterion shown in the first sort column.
     * @param secondary_sort Secondary sort criterion shown in the second sort column.
     */
    static void print_snapshot(
        const ProfileSnapshot &snapshot, PrimarySortBy primary_sort = PrimarySortBy::CoreId,
        SecondarySortBy secondary_sort = SecondarySortBy::CpuPercent
    );

    /**
     * @brief Find a task by name inside a snapshot.
     *
     * @param snapshot Snapshot to search.
     * @param name Task name to search for.
     * @param task Output object that receives the matching task info.
     * @return `true` if a matching task is found, or `false` otherwise.
     */
    static bool get_task_by_name(const ProfileSnapshot &snapshot, const std::string &name, TaskInfo &task);

    /**
     * @brief Collect tasks whose metric meets a threshold.
     *
     * @param snapshot Snapshot to inspect.
     * @param type Threshold metric to apply.
     * @param threshold_value Threshold value interpreted according to @p type.
     * @return Vector containing every task that satisfies the threshold.
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
#endif // defined(ESP_PLATFORM)
