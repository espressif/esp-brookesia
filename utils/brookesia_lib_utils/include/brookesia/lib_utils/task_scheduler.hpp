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
#include <optional>
#include <string>
#include <string_view>
#include <map>
#include <unordered_set>
#include <vector>
#include "boost/asio/io_context.hpp"
#include "boost/asio/executor_work_guard.hpp"
#include "boost/asio/steady_timer.hpp"
#include "boost/asio/strand.hpp"
#include "boost/thread/mutex.hpp"
#include "boost/thread/thread.hpp"
#include "boost/thread/future.hpp"
#include "brookesia/lib_utils/macro_configs.h"
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/lib_utils/thread_config.hpp"

namespace esp_brookesia::lib_utils {

/**
 * @brief Asynchronous task scheduler built on top of `boost::asio::io_context`.
 *
 * The scheduler supports immediate, delayed, and periodic tasks, optional task grouping,
 * serial execution within groups, suspension and resumption of timer-driven tasks,
 * and task lifecycle statistics.
 */
class TaskScheduler {
public:
    /**
     * @brief Identifier type assigned to each scheduled task.
     */
    using TaskId = uint64_t;
    /**
     * @brief Callable type for one-shot tasks.
     */
    using OnceTask = std::function<void()>;
    /**
     * @brief Callable type for periodic tasks.
     *
     * Returning `false` stops future executions.
     */
    using PeriodicTask = std::function<bool()>;
    /**
     * @brief Task group name type.
     */
    using Group = std::string;
    /**
     * @brief Executor type exposed by the underlying `boost::asio::io_context`.
     */
    using Executor = boost::asio::io_context::executor_type;

    /**
     * @brief Kind of scheduled task.
     */
    enum class TaskType {
        Immediate, ///< Task scheduled with `post()` or `dispatch()`.
        Delayed,   ///< One-shot timer task scheduled with `post_delayed()`.
        Periodic,  ///< Repeating timer task scheduled with `post_periodic()`.
    };

    /**
     * @brief Runtime state reported for a scheduled task.
     */
    enum class TaskState {
        Running,   ///< Task is active and eligible to execute.
        Suspended, ///< Task timer is suspended.
        Canceled,  ///< Task was canceled before completion.
        Finished   ///< Task finished execution or is no longer tracked.
    };

    /**
     * @brief Aggregate counters for task execution.
     */
    struct Statistics {
        size_t total_tasks{0};      ///< Total number of tasks ever scheduled.
        size_t completed_tasks{0};  ///< Number of tasks that completed successfully.
        size_t failed_tasks{0};     ///< Number of tasks that finished unsuccessfully.
        size_t canceled_tasks{0};   ///< Number of tasks canceled before completion.
        size_t suspended_tasks{0};  ///< Number of tasks currently suspended.
    };

    /**
     * @brief Callback invoked when a task is selected by io_context and about to execute
     *
     * @param group Group name of the task
     * @param task_id ID of the task about to execute
     * @param task_type Type of the task
     *
     * @note This callback is invoked after the task is selected by io_context and
     *       just before the actual task logic executes, guaranteeing that the task
     *       will execute (unless an exception occurs in the callback itself)
     */
    using PreExecuteCallback = std::function<void(const Group &, TaskId, TaskType)>;

    /**
     * @brief Callback invoked after a task completes execution
     *
     * @param group Group name of the completed task
     * @param task_id ID of the completed task
     * @param task_type Type of the task
     * @param success Whether the task executed successfully
     *
     * @note This callback is invoked after the task logic completes, regardless of
     *       success or failure, providing a hook for cleanup, logging, or statistics
     */
    using PostExecuteCallback = std::function<void(const Group &, TaskId, TaskType, bool success)>;

    /**
     * @brief Startup configuration for the scheduler worker threads.
     */
    struct StartConfig {
        /**
         * @brief Worker thread configurations used when the scheduler starts.
         *
         * The default configuration creates a single worker named `Worker`
         * with a 6 KiB stack.
         */
        std::vector<ThreadConfig> worker_configs{
            ThreadConfig{
                .name = "Worker",
                .stack_size = 6 * 1024,
            }
        };
        size_t worker_poll_interval_ms = 10;             ///< Worker polling interval in milliseconds.
        /**
         * @brief Optional global pre-execute callback applied to every task.
         *
         * The callback fires just before any task executes, regardless of group membership.
         */
        PreExecuteCallback pre_execute_callback = nullptr;
        /**
         * @brief Optional global post-execute callback applied to every task.
         *
         * The callback fires after any task completes, regardless of group membership.
         */
        PostExecuteCallback post_execute_callback = nullptr;
    };

    /**
     * @brief Configuration for a task group.
     */
    struct GroupConfig {
        /**
         * @brief Serialize all tasks in this group through a strand when set to `true`.
         *
         * Periodic tasks skip overlapping executions for the same task instance,
         * while one-shot tasks are queued and executed sequentially.
         */
        bool enable_serial_execution = false;
        /**
         * @brief Optional parent group name whose execution context should be reused.
         *
         * When the parent group is serial, this group also runs serially through the parent strand.
         */
        Group parent_group = "";
        /**
         * @brief Optional group pre-execute callback applied to all tasks in the group.
         */
        PreExecuteCallback pre_execute_callback = nullptr;
        /**
         * @brief Optional group post-execute callback applied to all tasks in the group.
         */
        PostExecuteCallback post_execute_callback = nullptr;
    };

    /**
     * @brief Construct an idle task scheduler.
     */
    TaskScheduler() = default;
    /**
     * @brief Stop the scheduler and release its resources.
     */
    ~TaskScheduler();

    /// Copy construction is not supported.
    TaskScheduler(const TaskScheduler &) = delete;
    /// Copy assignment is not supported.
    TaskScheduler &operator=(const TaskScheduler &) = delete;
    /// Move construction is not supported.
    TaskScheduler(TaskScheduler &&) = delete;
    /// Move assignment is not supported.
    TaskScheduler &operator=(TaskScheduler &&) = delete;

    /**
     * @brief Start the scheduler with a custom worker configuration.
     *
     * @param[in] config Worker-thread and callback configuration.
     * @return `true` on success, or `false` when startup fails.
     */
    bool start(const StartConfig &config);

    /**
     * @brief Start the scheduler with the default configuration.
     *
     * @return `true` on success, or `false` when startup fails.
     */
    bool start()
    {
        return start(StartConfig{});
    }

    /**
     * @brief Stop the scheduler and cancel all pending tasks.
     *
     * Running tasks are allowed to finish, worker threads are joined, and internal state is reset.
     */
    void stop();

    /**
     * @brief Check whether the scheduler is running.
     *
     * @return `true` when worker threads and the `io_context` are active, or `false` otherwise.
     */
    bool is_running() const
    {
        return is_running_.load();
    }

    /**
     * @brief Configure execution behavior for a task group.
     *
     * @param[in] group Group name.
     * @param[in] config Group behavior configuration.
     * @return `true` if the group was configured successfully, or `false` otherwise.
     */
    bool configure_group(const Group &group, const GroupConfig &config);

    /**
     * @brief Dispatch a task for immediate execution when possible.
     *
     * When called from a scheduler worker thread, the task may run inline instead of being enqueued.
     *
     * @param[in] task Task to execute.
     * @param[out] id Optional pointer that receives the assigned task ID.
     * @param[in] group Optional task group name.
     *
     * @return `true` if the task was scheduled successfully, or `false` otherwise.
     */
    bool dispatch(OnceTask task, TaskId *id = nullptr, const Group &group = "");

    /**
     * @brief Post a task to the scheduler queue.
     *
     * @param[in] task Task to execute.
     * @param[out] id Optional pointer that receives the assigned task ID.
     * @param[in] group Optional task group name.
     *
     * @return `true` if the task was scheduled successfully, or `false` otherwise.
     */
    bool post(OnceTask task, TaskId *id = nullptr, const Group &group = "");

    /**
     * @brief Schedule a one-shot task to run after a delay.
     *
     * @param[in] task Task to execute.
     * @param[in] delay_ms Delay before execution, in milliseconds.
     * @param[out] id Optional pointer that receives the assigned task ID.
     * @param[in] group Optional task group name.
     * @return `true` if the task was scheduled successfully, or `false` otherwise.
     */
    bool post_delayed(OnceTask task, int delay_ms, TaskId *id = nullptr, const Group &group = "");

    /**
     * @brief Schedule a periodic task.
     *
     * The task will be executed repeatedly at the specified interval.
     * Return false from the task to stop the periodic execution.
     *
     * @param[in] task Periodic task to execute. Returning `false` stops future runs.
     * @param[in] interval_ms Interval between executions, in milliseconds.
     * @param[out] id Optional pointer that receives the assigned task ID.
     * @param[in] group Optional task group name.
     * @return `true` if the task was scheduled successfully, or `false` otherwise.
     */
    bool post_periodic(PeriodicTask task, int interval_ms, TaskId *id = nullptr, const Group &group = "");

    /**
     * @brief Post multiple one-shot tasks as a batch.
     *
     * @param[in] tasks Vector of tasks to execute.
     * @param[out] ids Optional pointer that receives the assigned task IDs.
     * @param[in] group Optional task group name applied to every task.
     * @return `true` if all tasks were scheduled successfully, or `false` otherwise.
     */
    bool post_batch(std::vector<OnceTask> tasks, std::vector<TaskId> *ids = nullptr, const Group &group = "");

    /**
     * @brief Cancel a task by ID.
     *
     * @param[in] id Task ID to cancel.
     */
    void cancel(TaskId id);

    /**
     * @brief Cancel every task in a group.
     *
     * @param[in] group Group name.
     */
    void cancel_group(const Group &group);

    /**
     * @brief Cancel all tracked tasks.
     */
    void cancel_all();

    /**
     * @brief Suspend a delayed or periodic task.
     *
     * Only delayed and periodic tasks can be suspended.
     *
     * @param[in] id Task ID to suspend.
     * @return `true` if the task was suspended successfully, or `false` otherwise.
     */
    bool suspend(TaskId id);

    /**
     * @brief Suspend all suspendable tasks in a group.
     *
     * @param[in] group Group name.
     * @return Number of tasks suspended.
     */
    size_t suspend_group(const Group &group);

    /**
     * @brief Suspend all suspendable tasks.
     *
     * @return Number of tasks suspended.
     */
    size_t suspend_all();

    /**
     * @brief Resume a suspended delayed or periodic task.
     *
     * @param[in] id Task ID to resume.
     * @return `true` if the task was resumed successfully, or `false` otherwise.
     */
    bool resume(TaskId id);

    /**
     * @brief Resume all suspended tasks in a group.
     *
     * @param[in] group Group name.
     * @return Number of tasks resumed.
     */
    size_t resume_group(const Group &group);

    /**
     * @brief Resume all suspended tasks.
     *
     * @return Number of tasks resumed.
     */
    size_t resume_all();

    /**
     * @brief Wait for a task to complete.
     *
     * @param[in] id Task ID to wait for.
     * @param[in] timeout_ms Timeout in milliseconds. `-1` waits indefinitely.
     * @return `true` if the task completed within the timeout, or `false` otherwise.
     */
    bool wait(TaskId id, int timeout_ms = -1);

    /**
     * @brief Wait for all tasks in a group to complete.
     *
     * @param[in] group Group name.
     * @param[in] timeout_ms Timeout in milliseconds. `-1` waits indefinitely.
     * @return `true` if all tasks in the group completed within the timeout, or `false` otherwise.
     */
    bool wait_group(const Group &group, int timeout_ms = -1);

    /**
     * @brief Wait for all tasks to complete.
     *
     * @param[in] timeout_ms Timeout in milliseconds. `-1` waits indefinitely.
     * @return `true` if all tasks completed within the timeout, or `false` otherwise.
     */
    bool wait_all(int timeout_ms = -1);

    /**
     * @brief Restart the timer for a delayed or periodic task.
     *
     * This resets the timer countdown to its original interval. Useful for implementing
     * debounce or watchdog-like behavior where you want to postpone execution.
     *
     * @param[in] id Task ID whose timer should restart.
     * @return `true` if the timer restarted successfully, or `false` when the task is missing,
     *         has the wrong type, or is not running.
     */
    bool restart_timer(TaskId id);

    /**
     * @brief Query the type of a task.
     *
     * @param[in] id Task ID.
     * @return Task type for the ID, or `TaskType::Immediate` when the task is unknown.
     */
    TaskType get_type(TaskId id) const;

    /**
     * @brief Query the state of a task.
     *
     * @param[in] id Task ID.
     * @return Task state for the ID, or `TaskState::Finished` when the task is unknown.
     */
    TaskState get_state(TaskId id) const;

    /**
     * @brief Query the group of a task.
     *
     * @param[in] id Task ID.
     * @return Group name, or an empty string when the task is unknown or ungrouped.
     */
    Group get_group(TaskId id) const;

    /**
     * @brief Query the number of tracked tasks in a group.
     *
     * @param[in] group Group name.
     * @return Number of tasks currently associated with the group.
     */
    size_t get_group_task_count(const Group &group) const;

    /**
     * @brief Get all groups that currently contain tasks.
     *
     * @return Vector of active group names.
     */
    std::vector<Group> get_active_groups() const;

    /**
     * @brief Get execution statistics accumulated by the scheduler.
     *
     * @return `Statistics` structure with task counters.
     */
    Statistics get_statistics() const;

    /**
     * @brief Get a shared executor handle for the underlying `io_context`.
     *
     * @return Shared pointer to the executor, or `nullptr` when the scheduler is not running.
     */
    std::shared_ptr<Executor> get_executor()
    {
        boost::lock_guard lock(mutex_);
        return io_context_ ? std::make_shared<Executor>(io_context_->get_executor()) : nullptr;
    }

    /**
     * @brief Get the number of worker threads.
     *
     * @return Number of worker threads currently owned by the scheduler.
     */
    size_t get_worker_count() const
    {
        boost::lock_guard lock(mutex_);
        return threads_.size();
    }

    /**
     * @brief Reset all accumulated statistics counters to zero.
     */
    void reset_statistics();

private:
    struct TaskHandle {
        TaskId id;
        std::shared_ptr<boost::asio::steady_timer> timer;
        std::atomic<TaskState> state{TaskState::Running};
        TaskType type{TaskType::Immediate};
        bool repeat{false};
        int interval_ms{0};
        Group group; // Group that this task belongs to
        std::shared_ptr<boost::promise<bool>> promise; // Promise for task completion
        boost::shared_future<bool> future; // Shared future for task completion
        std::atomic<bool> is_executing{false}; // Flag to prevent parallel execution of periodic tasks

        // For suspend/resume support
        std::chrono::steady_clock::time_point suspend_time;
        std::chrono::milliseconds remaining_time{0};
        OnceTask saved_task;              // Saved task closure for Delayed tasks
        PeriodicTask saved_periodic_task;  // Saved task closure for Periodic tasks
    };

    // Generate next task ID
    TaskId next_id()
    {
        return task_id_counter_.fetch_add(1, std::memory_order_relaxed);
    }

    // Create task handle
    std::shared_ptr<TaskHandle> create_handle(TaskType type, bool repeat, int interval_ms, const Group &group);

    // Schedule a one-time task
    void schedule_once(std::shared_ptr<TaskHandle> handle, OnceTask task);

    // Schedule a periodic task (supports return value control)
    void schedule_periodic(std::shared_ptr<TaskHandle> handle, PeriodicTask task);

    // Internal method: cancel task (caller must have already acquired lock)
    void cancel_internal(TaskId task_id);

    // Internal method: suspend task (caller must have already acquired lock)
    bool suspend_internal(TaskId task_id);

    // Internal method: resume task (caller must have already acquired lock)
    bool resume_internal(TaskId task_id);

    // Internal method: wait for a set of tasks to complete
    bool wait_tasks_internal(const std::vector<TaskId> &task_ids, int timeout_ms);

    // Internal method: remove task and maintain group relationships (caller must have already acquired lock)
    void remove_task_internal(TaskId task_id, const Group &group);

    // Internal method: post or dispatch task based on enable_immediate flag
    bool post_internal(OnceTask task, TaskId *id, const Group &group, bool enable_immediate);

    // Mark task as finished
    void mark_finished(std::shared_ptr<TaskHandle> handle, bool success);

    // Get future for task completion
    boost::shared_future<bool> get_future(TaskId id);

    // Invoke pre-execute callbacks: global ("") first, then group-specific (thread-safe)
    void invoke_pre_execute_callback(TaskId task_id, TaskType task_type, const Group &group);

    // Invoke post-execute callbacks: global ("") first, then group-specific (thread-safe)
    void invoke_post_execute_callback(TaskId task_id, TaskType task_type, bool success, const Group &group);

private:
    std::atomic<bool> is_running_{false};
    std::unique_ptr<boost::asio::io_context> io_context_;
    std::unique_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> io_work_guard_;
    boost::thread_group threads_;
    std::map<TaskId, std::shared_ptr<TaskHandle>> tasks_;
    std::map<Group, std::unordered_set<TaskId>> groups_; // Mapping between groups and task IDs
    std::map<Group, std::shared_ptr<boost::asio::strand<boost::asio::io_context::executor_type>>> strands_; // Strand for each group
    mutable boost::mutex mutex_;
    std::atomic<TaskId> task_id_counter_{1};
    std::atomic<TaskId> total_tasks_{0};
    std::atomic<TaskId> completed_tasks_{0};
    std::atomic<TaskId> failed_tasks_{0};
    std::atomic<TaskId> canceled_tasks_{0};
    std::atomic<TaskId> suspended_tasks_{0};
    std::map<Group, PreExecuteCallback> pre_execute_callbacks_;   ///< Per-group pre-execute callbacks; key "" = global.
    std::map<Group, PostExecuteCallback> post_execute_callbacks_; ///< Per-group post-execute callbacks; key "" = global.
};

// Describe macros for TaskScheduler types
BROOKESIA_DESCRIBE_ENUM(TaskScheduler::TaskType, Immediate, Delayed, Periodic)
BROOKESIA_DESCRIBE_ENUM(TaskScheduler::TaskState, Running, Suspended, Canceled, Finished)
BROOKESIA_DESCRIBE_STRUCT(TaskScheduler::Statistics, (), (total_tasks, completed_tasks, failed_tasks, canceled_tasks, suspended_tasks))
BROOKESIA_DESCRIBE_STRUCT(TaskScheduler::GroupConfig, (), (enable_serial_execution, parent_group))
BROOKESIA_DESCRIBE_STRUCT(TaskScheduler::StartConfig, (), (worker_configs, worker_poll_interval_ms, pre_execute_callback, post_execute_callback))

} // namespace esp_brookesia::lib_utils
