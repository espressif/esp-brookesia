/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <map>
#include <unordered_set>
#include <vector>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/lib_utils/thread_config.hpp"

namespace esp_brookesia::lib_utils {

class TaskScheduler {
public:
    using TaskId = uint64_t;
    using OnceTask = std::function<void()>;
    using PeriodicTask = std::function<bool()>; // Return false to stop periodic task early
    using Group = std::string; // Task group name
    using Executor = boost::asio::io_context::executor_type;

    enum class TaskType {
        Immediate,      // post() - immediate execution
        Delayed,        // post_delayed() - can be suspended/resumed
        Periodic,       // post_periodic() - can be suspended/resumed
    };

    enum class TaskState {
        Running,
        Suspended,
        Canceled,
        Finished
    };

    struct Statistics {
        size_t total_tasks{0};
        size_t completed_tasks{0};
        size_t failed_tasks{0};
        size_t canceled_tasks{0};
        size_t suspended_tasks{0};
    };

    struct GroupConfig {
        // If true, all tasks (Periodic, Delayed, and Once) in this group will not run in parallel.
        // Uses boost::asio::strand to ensure serialized execution of all tasks in the group.
        // When a task is executing, other tasks in the same group will be queued and executed sequentially.
        // For Periodic tasks: if the same task instance is already executing, skip this execution but continue scheduling next execution.
        // For Delayed and Once tasks: tasks are executed sequentially through the strand, no skipping.
        bool enable_serial_execution = false;
    };

    /**
     * @brief Callback invoked when a task is selected by io_context and about to execute
     *
     * @param task_id ID of the task about to execute
     * @param task_type Type of the task
     *
     * @note This callback is invoked after the task is selected by io_context and
     *       just before the actual task logic executes, guaranteeing that the task
     *       will execute (unless an exception occurs in the callback itself)
     */
    using PreExecuteCallback = std::function<void(TaskId, TaskType)>;
    /**
     * @brief Callback invoked after a task completes execution
     *
     * @param task_id ID of the completed task
     * @param task_type Type of the task
     * @param success Whether the task executed successfully
     *
     * @note This callback is invoked after the task logic completes, regardless of
     *       success or failure, providing a hook for cleanup, logging, or statistics
     */
    using PostExecuteCallback = std::function<void(TaskId, TaskType, bool success)>;

    struct StartConfig {
        std::vector<ThreadConfig> worker_configs = {
            {
                .name = "tsc_worker",
                .stack_size = 6 * 1024,
            }
        };
        size_t worker_poll_interval_ms = 10;
        PreExecuteCallback pre_execute_callback = nullptr;
        PostExecuteCallback post_execute_callback = nullptr;
    };

    TaskScheduler() = default;
    ~TaskScheduler();

    // Disable copy and move operations
    TaskScheduler(const TaskScheduler &) = delete;
    TaskScheduler &operator=(const TaskScheduler &) = delete;
    TaskScheduler(TaskScheduler &&) = delete;
    TaskScheduler &operator=(TaskScheduler &&) = delete;

    /**
     * @brief Start the task scheduler with custom configuration
     *
     * @param[in] config Configuration for the scheduler
     * @return true if started successfully, false otherwise
     */
    bool start(const StartConfig &config);

    /**
     * @brief Start the task scheduler with default configuration (single worker)
     *
     * @return true if started successfully, false otherwise
     */
    bool start()
    {
        return start(StartConfig{});
    }

    /**
     * @brief Stop the task scheduler
     *
     * Stops all workers and cancels all pending tasks
     */
    void stop();

    /**
     * @brief Check if the scheduler is running
     *
     * @return true if running, false otherwise
     */
    bool is_running() const
    {
        return is_running_.load();
    }

    /**
     * @brief Configure a task group
     *
     * @param[in] group Group name
     * @param[in] config Configuration for the group
     * @return true if configured successfully, false otherwise
     */
    bool configure_group(const Group &group, const GroupConfig &config);

    /**
     * @brief Dispatch a enqueued task for execution. When the task is posted within a task, the new task will be
     *        executed immediately instead of being enqueued.
     *
     * @param[in] task Task to execute
     * @param[out] id Optional pointer to receive the task ID
     * @param[in] group Optional group name for the task
     *
     * @return true if dispatched successfully, false otherwise
     */
    bool dispatch(OnceTask task, TaskId *id = nullptr, const Group &group = "");

    /**
     * @brief Post an enqueued task for execution
     *
     * @param[in] task Task to execute
     * @param[out] id Optional pointer to receive the task ID
     * @param[in] group Optional group name for the task
     *
     * @return true if posted successfully, false otherwise
     */
    bool post(OnceTask task, TaskId *id = nullptr, const Group &group = "");

    /**
     * @brief Post a delayed task for execution
     *
     * @param[in] task Task to execute
     * @param[in] delay_ms Delay in milliseconds before execution
     * @param[out] id Optional pointer to receive the task ID
     * @param[in] group Optional group name for the task
     * @return true if posted successfully, false otherwise
     */
    bool post_delayed(OnceTask task, int delay_ms, TaskId *id = nullptr, const Group &group = "");

    /**
     * @brief Post a periodic task for execution
     *
     * The task will be executed repeatedly at the specified interval.
     * Return false from the task to stop the periodic execution.
     *
     * @param[in] task Periodic task to execute (returns bool)
     * @param[in] interval_ms Interval in milliseconds between executions
     * @param[out] id Optional pointer to receive the task ID
     * @param[in] group Optional group name for the task
     * @return true if posted successfully, false otherwise
     */
    bool post_periodic(PeriodicTask task, int interval_ms, TaskId *id = nullptr, const Group &group = "");

    /**
     * @brief Post multiple tasks in batch
     *
     * @param[in] tasks Vector of tasks to execute
     * @param[out] ids Optional pointer to receive vector of task IDs
     * @param[in] group Optional group name for all tasks
     * @return true if all tasks posted successfully, false otherwise
     */
    bool post_batch(std::vector<OnceTask> tasks, std::vector<TaskId> *ids = nullptr, const Group &group = "");

    /**
     * @brief Cancel a task by ID
     *
     * @param[in] id Task ID to cancel
     */
    void cancel(TaskId id);

    /**
     * @brief Cancel all tasks in a group
     *
     * @param[in] group Group name
     */
    void cancel_group(const Group &group);

    /**
     * @brief Cancel all tasks
     */
    void cancel_all();

    /**
     * @brief Suspend a task by ID
     *
     * Only delayed and periodic tasks can be suspended.
     *
     * @param[in] id Task ID to suspend
     * @return true if suspended successfully, false otherwise
     */
    bool suspend(TaskId id);

    /**
     * @brief Suspend all tasks in a group
     *
     * @param[in] group Group name
     * @return Number of tasks suspended
     */
    size_t suspend_group(const Group &group);

    /**
     * @brief Suspend all tasks
     *
     * @return Number of tasks suspended
     */
    size_t suspend_all();

    /**
     * @brief Resume a suspended task by ID
     *
     * @param[in] id Task ID to resume
     * @return true if resumed successfully, false otherwise
     */
    bool resume(TaskId id);

    /**
     * @brief Resume all suspended tasks in a group
     *
     * @param[in] group Group name
     * @return Number of tasks resumed
     */
    size_t resume_group(const Group &group);

    /**
     * @brief Resume all suspended tasks
     *
     * @return Number of tasks resumed
     */
    size_t resume_all();

    /**
     * @brief Wait for a task to complete with optional timeout
     *
     * @param[in] id Task ID to wait for
     * @param[in] timeout_ms Timeout in milliseconds (-1 for infinite)
     * @return true if task completed within timeout, false otherwise
     */
    bool wait(TaskId id, int timeout_ms = -1);

    /**
     * @brief Wait for all tasks in a group to complete with optional timeout
     *
     * @param[in] group Group name
     * @param[in] timeout_ms Timeout in milliseconds (-1 for infinite)
     * @return true if all tasks completed within timeout, false otherwise
     */
    bool wait_group(const Group &group, int timeout_ms = -1);

    /**
     * @brief Wait for all tasks to complete with optional timeout
     *
     * @param[in] timeout_ms Timeout in milliseconds (-1 for infinite)
     * @return true if all tasks completed within timeout, false otherwise
     */
    bool wait_all(int timeout_ms = -1);

    /**
     * @brief Restart the timer for a delayed or periodic task
     *
     * This resets the timer countdown to its original interval. Useful for implementing
     * debounce or watchdog-like behavior where you want to postpone execution.
     *
     * @param[in] id Task ID to restart timer for
     * @return true if timer restarted successfully, false otherwise (task not found, wrong type, or not running)
     */
    bool restart_timer(TaskId id);

    /**
     * @brief Query the type of a task
     *
     * @param[in] id Task ID
     * @return Task type (Immediate, Delayed, or Periodic)
     */
    TaskType get_type(TaskId id) const;

    /**
     * @brief Query the state of a task
     *
     * @param[in] id Task ID
     * @return Task state (Running, Suspended, Canceled, or Finished)
     */
    TaskState get_state(TaskId id) const;

    /**
     * @brief Query the group of a task
     *
     * @param[in] id Task ID
     * @return Group name (empty string if not found)
     */
    Group get_group(TaskId id) const;

    /**
     * @brief Query the number of tasks in a group
     *
     * @param[in] group Group name
     * @return Number of tasks in the group
     */
    size_t get_group_task_count(const Group &group) const;

    /**
     * @brief Get all active groups
     *
     * @return Vector of active group names
     */
    std::vector<Group> get_active_groups() const;

    /**
     * @brief Get statistics about task execution
     *
     * @return Statistics structure with task counters
     */
    Statistics get_statistics() const;

    /**
     * @brief Get the executor
     *
     * @return Shared pointer to the executor (nullptr if not running)
     */
    std::shared_ptr<Executor> get_executor()
    {
        boost::lock_guard lock(mutex_);
        return io_context_ ? std::make_shared<Executor>(io_context_->get_executor()) : nullptr;
    }

    /**
     * @brief Get the number of worker threads
     *
     * @return Number of worker threads
     */
    size_t get_worker_count() const
    {
        boost::lock_guard lock(mutex_);
        return threads_.size();
    }

    /**
     * @brief Reset all statistics counters to zero
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
        std::shared_ptr<std::promise<bool>> promise; // Promise for task completion
        std::shared_future<bool> future; // Shared future for task completion
        std::atomic<bool> promise_fulfilled{false}; // Flag to prevent double-setting promise
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
    std::shared_future<bool> get_future(TaskId id);

    // Invoke pre-execute callback (thread-safe)
    void invoke_pre_execute_callback(TaskId task_id, TaskType task_type);

    // Invoke post-execute callback (thread-safe)
    void invoke_post_execute_callback(TaskId task_id, TaskType task_type, bool success);

private:
    std::atomic<bool> is_running_{false};
    std::unique_ptr<boost::asio::io_context> io_context_;
    std::unique_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> io_work_guard_;
    boost::thread_group threads_;
    std::map<TaskId, std::shared_ptr<TaskHandle>> tasks_;
    std::map<Group, std::unordered_set<TaskId>> groups_; // Mapping between groups and task IDs
    std::map<Group, std::shared_ptr<boost::asio::strand<boost::asio::io_context::executor_type>>> strands_; // Strand for each group
    std::map<Group, GroupConfig> group_configs_; // Group configurations
    mutable boost::mutex mutex_;
    std::atomic<TaskId> task_id_counter_{1};
    std::atomic<TaskId> total_tasks_{0};
    std::atomic<TaskId> completed_tasks_{0};
    std::atomic<TaskId> failed_tasks_{0};
    std::atomic<TaskId> canceled_tasks_{0};
    std::atomic<TaskId> suspended_tasks_{0};
    PreExecuteCallback pre_execute_callback_;
    PostExecuteCallback post_execute_callback_;
};

// Describe macros for TaskScheduler types
BROOKESIA_DESCRIBE_ENUM(TaskScheduler::TaskType, Immediate, Delayed, Periodic)
BROOKESIA_DESCRIBE_ENUM(TaskScheduler::TaskState, Running, Suspended, Canceled, Finished)
BROOKESIA_DESCRIBE_STRUCT(TaskScheduler::Statistics, (), (total_tasks, completed_tasks, failed_tasks, canceled_tasks, suspended_tasks))
BROOKESIA_DESCRIBE_STRUCT(TaskScheduler::GroupConfig, (), (enable_serial_execution))
BROOKESIA_DESCRIBE_STRUCT(TaskScheduler::StartConfig, (), (worker_configs, worker_poll_interval_ms, pre_execute_callback, post_execute_callback))

} // namespace esp_brookesia::lib_utils
