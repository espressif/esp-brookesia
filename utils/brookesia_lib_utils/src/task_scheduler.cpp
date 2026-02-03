/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/lib_utils/macro_configs.h"
#if !BROOKESIA_UTILS_TASK_SCHEDULER_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/lib_utils/check.hpp"
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/lib_utils/function_guard.hpp"
#include "brookesia/lib_utils/log.hpp"
#include "brookesia/lib_utils/task_scheduler.hpp"

namespace esp_brookesia::lib_utils {

TaskScheduler::~TaskScheduler()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_running()) {
        stop();
    }
}

bool TaskScheduler::start(const StartConfig &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: config(%1%)", BROOKESIA_DESCRIBE_TO_STR(config));

    boost::lock_guard<boost::mutex> lock(mutex_);

    if (is_running()) {
        BROOKESIA_LOGD("Already running");
        return true;
    }

    BROOKESIA_LOGI(
        "Starting with config:\n%1%", BROOKESIA_DESCRIBE_TO_STR_WITH_FMT(config, DESCRIBE_FORMAT_VERBOSE)
    );

    is_running_.store(true);

    lib_utils::FunctionGuard stop_guard([this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        stop();
    });

    BROOKESIA_CHECK_EXCEPTION_RETURN(
        io_context_ = std::make_unique<boost::asio::io_context>(), false, "Failed to create io_context"
    );
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        io_work_guard_ = std::make_unique<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>(
                             boost::asio::make_work_guard(*io_context_)
                         ), false, "Failed to create work guard"
    );

    reset_statistics();

    // Save callbacks from config
    pre_execute_callback_ = config.pre_execute_callback;
    post_execute_callback_ = config.post_execute_callback;

    for (const auto &thread_config : config.worker_configs) {
        auto thread_func =
        [this, name = thread_config.name, poll_interval_ms = config.worker_poll_interval_ms] {
            BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

            BROOKESIA_LOGI("Worker thread (%1%) started", name);

            while (!boost::this_thread::interruption_requested() && !io_context_->stopped())
            {
                try {
                    size_t executed = io_context_->poll();
                    if (executed == 0) {
                        boost::this_thread::sleep_for(boost::chrono::milliseconds(poll_interval_ms));
                    }
                } catch (const boost::thread_interrupted &) {
                    BROOKESIA_LOGI("Worker thread (%1%) interrupted", name);
                    break;
                } catch (const std::exception &e) {
                    BROOKESIA_LOGE("Worker thread (%1%) poll error: %2%", name, e.what());
                }
            }

            BROOKESIA_LOGI("Worker thread (%1%) stopped", name);
        };
        BROOKESIA_THREAD_CONFIG_GUARD(thread_config);
        BROOKESIA_CHECK_EXCEPTION_RETURN(
            threads_.create_thread(std::move(thread_func)), false, "Failed to create worker thread (%1%)",
            thread_config.name
        );
    }

    stop_guard.release();

    return true;
}

void TaskScheduler::stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_running()) {
        BROOKESIA_LOGD("Already stopped");
        return;
    }

    size_t task_count = 0;
    // Avoid compiler warning about unused variable
    (void)task_count;
    // Cancel all pending tasks
    {
        boost::lock_guard<boost::mutex> lock(mutex_);
        task_count = tasks_.size();
        for (auto& [id, handle] : tasks_) {
            handle->state = TaskState::Canceled;
            if (handle->timer) {
                handle->timer->cancel();
                handle->timer.reset(); // Immediately release timer
            }
            // Set promise value for canceled task
            bool expected = false;
            if (handle->promise && handle->promise_fulfilled.compare_exchange_strong(expected, true)) {
                BROOKESIA_CHECK_EXCEPTION_EXECUTE(handle->promise->set_value(false), {}, {
                    BROOKESIA_LOGW("Promise already set for task %1%", id);
                });
                handle->promise.reset();
            }
            canceled_tasks_++;
        }
    }

    // Stop io_context
    io_context_->stop();

    // Wait for all threads to finish
    BROOKESIA_LOGI("Interrupting %1% worker threads and waiting for them to finish", threads_.size());
    threads_.interrupt_all();
    threads_.join_all();

    // Clean up resources
    {
        boost::lock_guard<boost::mutex> lock(mutex_);
        tasks_.clear();
        groups_.clear();
        strands_.clear();
        group_configs_.clear();
        io_work_guard_.reset();
        io_context_.reset();
    }

    is_running_.store(false);

    BROOKESIA_LOGI(
        "Stopped, canceled %1% tasks, statistics: %2%", task_count, BROOKESIA_DESCRIBE_TO_STR(get_statistics())
    );
}

bool TaskScheduler::post_internal(OnceTask task, TaskId *id, const Group &group, bool enable_immediate)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: group(%1%), id(%2%), enable_immediate(%3%)", group, id, enable_immediate);

    BROOKESIA_CHECK_FALSE_RETURN(is_running(), false, "Not running");

    auto handle = create_handle(TaskType::Immediate, false, 0, group);
    BROOKESIA_CHECK_NULL_RETURN(handle, false, "Failed to create task handle");

    auto task_wrapper = [this, handle, task = std::move(task)]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

        BROOKESIA_LOGD("Executing task %1%", handle->id);

        // Invoke pre-execute callback when task is about to execute
        invoke_pre_execute_callback(handle->id, handle->type);

        if (handle->state == TaskState::Canceled) {
            boost::lock_guard<boost::mutex> lock(mutex_);
            remove_task_internal(handle->id, handle->group);
            return;
        }

        bool success = false;
        // Will be executed when the function exits
        lib_utils::FunctionGuard exit_guard(
        [this, handle, &success]() {
            invoke_post_execute_callback(handle->id, handle->type, success);
            mark_finished(handle, success);
        }
        );

        BROOKESIA_CHECK_EXCEPTION_EXECUTE(task(), {
            success = false;
            return;
        }, {BROOKESIA_LOGE("Task %1% execution failed", handle->id);});

        success = true;
    };

    // Check if group has strand configured
    std::shared_ptr<boost::asio::strand<boost::asio::io_context::executor_type>> strand;
    {
        boost::lock_guard<boost::mutex> lock(mutex_);
        auto strand_it = strands_.find(group);
        if (strand_it != strands_.end()) {
            strand = strand_it->second;
        }
    }

    if (strand) {
        if (enable_immediate) {
            boost::asio::dispatch(*strand, std::move(task_wrapper));
        } else {
            boost::asio::post(*strand, std::move(task_wrapper));
        }
    } else {
        if (enable_immediate) {
            boost::asio::dispatch(*io_context_, std::move(task_wrapper));
        } else {
            boost::asio::post(*io_context_, std::move(task_wrapper));
        }
    }

    if (id) {
        *id = handle->id;
    }

    return true;
}

bool TaskScheduler::dispatch(OnceTask task, TaskId *id, const Group &group)
{
    return post_internal(std::move(task), id, group, true);
}

bool TaskScheduler::post(OnceTask task, TaskId *id, const Group &group)
{
    return post_internal(std::move(task), id, group, false);
}

bool TaskScheduler::post_delayed(OnceTask task, int delay_ms, TaskId *id, const Group &group)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: delay_ms(%1%), group(%2%), id(%3%)", delay_ms, group, id);

    BROOKESIA_CHECK_FALSE_RETURN(is_running(), false, "Not running");

    auto handle = create_handle(TaskType::Delayed, false, delay_ms, group);
    BROOKESIA_CHECK_NULL_RETURN(handle, false, "Failed to create task handle");

    // Save task for suspend/resume support (copy before move)
    handle->saved_task = task;

    schedule_once(handle, std::move(task));

    if (id) {
        *id = handle->id;
    }
    return true;
}

bool TaskScheduler::post_periodic(PeriodicTask task, int interval_ms, TaskId *id, const Group &group)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: interval_ms(%1%), group(%2%), id(%3%)", interval_ms, group, id);

    BROOKESIA_CHECK_FALSE_RETURN(is_running(), false, "Not running");

    auto handle = create_handle(TaskType::Periodic, true, interval_ms, group);
    BROOKESIA_CHECK_NULL_RETURN(handle, false, "Failed to create task handle");

    // Save task for suspend/resume support
    handle->saved_periodic_task = task;

    schedule_periodic(handle, std::move(task));

    if (id) {
        *id = handle->id;
    }
    return true;
}

bool TaskScheduler::post_batch(std::vector<OnceTask> tasks, std::vector<TaskId> *ids, const Group &group)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: group(%1%)", group);

    BROOKESIA_CHECK_FALSE_RETURN(is_running(), false, "Not running");

    if (ids) {
        ids->clear();
        ids->reserve(tasks.size());
    }

    for (auto &task : tasks) {
        TaskId task_id = 0;
        if (!post(OnceTask(std::move(task)), &task_id, group)) {
            BROOKESIA_LOGE("Failed to post task in batch");
            return false;
        }
        if (ids) {
            ids->push_back(task_id);
        }
    }

    BROOKESIA_LOGD("Posted batch of %zu tasks to group '%s'", tasks.size(), group.c_str());
    return true;
}

void TaskScheduler::cancel(TaskId id)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: id(%1%)", id);

    if (!is_running()) {
        BROOKESIA_LOGD("Not running, skip");
        return;
    }

    boost::lock_guard<boost::mutex> lock(mutex_);
    cancel_internal(id);
}

void TaskScheduler::cancel_group(const Group &group)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: group(%1%)", group);

    if (!is_running()) {
        BROOKESIA_LOGD("Not running, skip");
        return;
    }

    boost::lock_guard<boost::mutex> lock(mutex_);
    auto group_it = groups_.find(group);
    if (group_it == groups_.end()) {
        BROOKESIA_LOGD("Group %1% not found", group);
        return;
    }

    // Copy task IDs to avoid iterator invalidation
    std::vector<TaskId> task_ids(group_it->second.begin(), group_it->second.end());

    // Cancel all tasks in the group
    size_t canceled_count = 0;
    for (auto id : task_ids) {
        if (tasks_.find(id) != tasks_.end()) {
            cancel_internal(id);
            canceled_count++;
        }
    }

    BROOKESIA_LOGD("Canceled group '%1%' with %2% tasks", group, canceled_count);
}

void TaskScheduler::cancel_all()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_running()) {
        BROOKESIA_LOGD("Not running, skip");
        return;
    }

    boost::lock_guard<boost::mutex> lock(mutex_);

    // Copy task IDs to avoid iterator invalidation
    std::vector<TaskId> task_ids;
    task_ids.reserve(tasks_.size());
    for (const auto& [id, handle] : tasks_) {
        task_ids.push_back(id);
    }

    // Cancel all tasks
    for (auto id : task_ids) {
        cancel_internal(id);
    }

    BROOKESIA_LOGD("Canceled all tasks, total: %1%", task_ids.size());
}

bool TaskScheduler::suspend(TaskId id)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: id(%1%)", id);

    BROOKESIA_CHECK_FALSE_RETURN(is_running(), false, "Not running");

    boost::lock_guard<boost::mutex> lock(mutex_);
    return suspend_internal(id);
}

size_t TaskScheduler::suspend_group(const Group &group)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: group(%1%)", group);

    BROOKESIA_CHECK_FALSE_RETURN(is_running(), 0, "Not running");

    boost::lock_guard<boost::mutex> lock(mutex_);
    auto group_it = groups_.find(group);
    if (group_it == groups_.end()) {
        BROOKESIA_LOGD("Group %1% not found", group);
        return 0;
    }

    // Copy task IDs to avoid iterator invalidation
    std::vector<TaskId> task_ids(group_it->second.begin(), group_it->second.end());

    // Suspend all tasks in the group
    size_t suspended_count = 0;
    for (auto id : task_ids) {
        if (suspend_internal(id)) {
            suspended_count++;
        }
    }

    BROOKESIA_LOGD("Suspended group '%1%' with %2% tasks", group, suspended_count);

    return suspended_count;
}

size_t TaskScheduler::suspend_all()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_FALSE_RETURN(is_running(), 0, "Not running");

    boost::lock_guard<boost::mutex> lock(mutex_);

    // Copy task IDs to avoid iterator invalidation
    std::vector<TaskId> task_ids;
    task_ids.reserve(tasks_.size());
    for (const auto& [id, handle] : tasks_) {
        task_ids.push_back(id);
    }

    // Suspend all tasks
    size_t suspended_count = 0;
    for (auto id : task_ids) {
        if (suspend_internal(id)) {
            suspended_count++;
        }
    }

    BROOKESIA_LOGD("Suspended all tasks, total: %1%", suspended_count);

    return suspended_count;
}

bool TaskScheduler::resume(TaskId id)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: id(%1%)", id);

    BROOKESIA_CHECK_FALSE_RETURN(is_running(), false, "Not running");

    boost::lock_guard<boost::mutex> lock(mutex_);
    return resume_internal(id);
}

size_t TaskScheduler::resume_group(const Group &group)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: group(%1%)", group);

    BROOKESIA_CHECK_FALSE_RETURN(is_running(), 0, "Not running");

    boost::lock_guard<boost::mutex> lock(mutex_);
    auto group_it = groups_.find(group);
    if (group_it == groups_.end()) {
        BROOKESIA_LOGD("Group %1% not found", group);
        return 0;
    }

    // Copy task IDs to avoid iterator invalidation
    std::vector<TaskId> task_ids(group_it->second.begin(), group_it->second.end());

    // Resume all tasks in the group
    size_t resumed_count = 0;
    for (auto id : task_ids) {
        if (resume_internal(id)) {
            resumed_count++;
        }
    }

    BROOKESIA_LOGD("Resumed group '%1%' with %2% tasks", group, resumed_count);

    return resumed_count;
}

size_t TaskScheduler::resume_all()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_FALSE_RETURN(is_running(), 0, "Not running");

    boost::lock_guard<boost::mutex> lock(mutex_);

    // Copy task IDs to avoid iterator invalidation
    std::vector<TaskId> task_ids;
    task_ids.reserve(tasks_.size());
    for (const auto& [id, handle] : tasks_) {
        task_ids.push_back(id);
    }

    // Resume all tasks
    size_t resumed_count = 0;
    for (auto id : task_ids) {
        if (resume_internal(id)) {
            resumed_count++;
        }
    }

    BROOKESIA_LOGD("Resumed all tasks, total: %1%", resumed_count);

    return resumed_count;
}

bool TaskScheduler::wait(TaskId id, int timeout_ms)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: id(%1%), timeout_ms(%2%)", id, timeout_ms);

    BROOKESIA_CHECK_FALSE_RETURN(is_running(), false, "Not running");

    std::shared_future<bool> future;
    {
        boost::lock_guard<boost::mutex> lock(mutex_);
        auto it = tasks_.find(id);
        if (it == tasks_.end()) {
            BROOKESIA_LOGD("Task %1% not found (already finished)", id);
            return true; // Task already finished
        }
        future = it->second->future;
    }

    if (timeout_ms < 0) {
        // Wait indefinitely
        return future.get();
    } else {
        // Wait with timeout
        auto status = future.wait_for(std::chrono::milliseconds(timeout_ms));
        if (status == std::future_status::ready) {
            return future.get();
        }
        BROOKESIA_LOGW("Wait timeout for task %1% after %2% ms", id, timeout_ms);
        return false;
    }
}

bool TaskScheduler::wait_group(const Group &group, int timeout_ms)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: group(%1%), timeout_ms(%2%)", group, timeout_ms);

    BROOKESIA_CHECK_FALSE_RETURN(is_running(), false, "Not running");

    std::vector<TaskId> task_ids;

    // Get all task IDs in the group
    {
        boost::lock_guard<boost::mutex> lock(mutex_);
        auto group_it = groups_.find(group);
        if (group_it == groups_.end()) {
            BROOKESIA_LOGD("Group %1% not found or empty", group);
            return true;
        }
        task_ids.assign(group_it->second.begin(), group_it->second.end());
    }

    bool result = wait_tasks_internal(task_ids, timeout_ms);
    if (result) {
        BROOKESIA_LOGD("All tasks in group '%1%' completed", group);
    } else {
        BROOKESIA_LOGW("Wait timeout for group '%1%'", group);
    }
    return result;
}

bool TaskScheduler::wait_all(int timeout_ms)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: timeout_ms(%1%)", timeout_ms);

    BROOKESIA_CHECK_FALSE_RETURN(is_running(), false, "Not running");

    std::vector<TaskId> task_ids;

    // Get all current task IDs
    {
        boost::lock_guard<boost::mutex> lock(mutex_);
        if (tasks_.empty()) {
            BROOKESIA_LOGD("No tasks to wait for");
            return true;
        }
        task_ids.reserve(tasks_.size());
        for (const auto& [id, handle] : tasks_) {
            task_ids.push_back(id);
        }
    }

    bool result = wait_tasks_internal(task_ids, timeout_ms);
    if (result) {
        BROOKESIA_LOGD("All tasks completed");
    } else {
        BROOKESIA_LOGW("Wait timeout after %1% ms", timeout_ms);
    }
    return result;
}

bool TaskScheduler::restart_timer(TaskId id)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: id(%1%)", id);

    BROOKESIA_CHECK_FALSE_RETURN(is_running(), false, "Not running");

    boost::lock_guard<boost::mutex> lock(mutex_);

    auto it = tasks_.find(id);
    if (it == tasks_.end()) {
        BROOKESIA_LOGW("Task %1% not found", id);
        return false;
    }

    auto &handle = it->second;

    // Check if task type supports timer restart
    if (handle->type != TaskType::Delayed && handle->type != TaskType::Periodic) {
        BROOKESIA_LOGE(
            "Task %1% cannot restart timer: only Delayed and Periodic tasks support this (current type: %2%)",
            id, BROOKESIA_DESCRIBE_TO_STR(handle->type)
        );
        return false;
    }

    // Check if task is in running state
    if (handle->state != TaskState::Running) {
        BROOKESIA_LOGW(
            "Task %1% is not in running state (current: %2%)", id,
            BROOKESIA_DESCRIBE_TO_STR(handle->state.load())
        );
        return false;
    }

    // Cancel current timer and reschedule with original interval
    if (handle->timer) {
        handle->timer->cancel();
        // Note: schedule_once/schedule_periodic will call expires_after internally

        if (handle->type == TaskType::Delayed) {
            // For Delayed task, reschedule with saved task
            if (!handle->saved_task) {
                BROOKESIA_LOGE("Task %1% has no saved task closure", id);
                return false;
            }
            schedule_once(handle, handle->saved_task);
        } else {
            // For Periodic task, reschedule with saved periodic task
            if (!handle->saved_periodic_task) {
                BROOKESIA_LOGE("Task %1% has no saved periodic task closure", id);
                return false;
            }
            schedule_periodic(handle, handle->saved_periodic_task);
        }
    }

    BROOKESIA_LOGD("Task %1% timer restarted (interval: %2% ms)", id, handle->interval_ms);

    return true;
}

TaskScheduler::TaskType TaskScheduler::get_type(TaskId id) const
{
    boost::lock_guard<boost::mutex> lock(mutex_);
    auto it = tasks_.find(id);
    if (it == tasks_.end()) {
        return TaskType::Immediate;
    }
    return it->second->type;
}

TaskScheduler::TaskState TaskScheduler::get_state(TaskId id) const
{
    boost::lock_guard<boost::mutex> lock(mutex_);
    auto it = tasks_.find(id);
    if (it == tasks_.end()) {
        return TaskState::Finished;
    }

    return it->second->state.load();
}

TaskScheduler::Group TaskScheduler::get_group(TaskId id) const
{
    boost::lock_guard<boost::mutex> lock(mutex_);
    auto it = tasks_.find(id);
    if (it == tasks_.end()) {
        return "";
    }
    return it->second->group;
}

size_t TaskScheduler::get_group_task_count(const Group &group) const
{
    boost::lock_guard<boost::mutex> lock(mutex_);
    auto it = groups_.find(group);

    return (it != groups_.end()) ? it->second.size() : 0;
}

std::vector<TaskScheduler::Group> TaskScheduler::get_active_groups() const
{
    boost::lock_guard<boost::mutex> lock(mutex_);
    std::vector<Group> result;
    result.reserve(groups_.size());
    for (const auto& [group, ids] : groups_) {
        result.push_back(group);
    }

    return result;
}

TaskScheduler::Statistics TaskScheduler::get_statistics() const
{
    Statistics stats;
    stats.total_tasks = total_tasks_.load();
    stats.completed_tasks = completed_tasks_.load();
    stats.failed_tasks = failed_tasks_.load();
    stats.canceled_tasks = canceled_tasks_.load();
    stats.suspended_tasks = suspended_tasks_.load();

    return stats;
}

void TaskScheduler::reset_statistics()
{
    total_tasks_ = 0;
    completed_tasks_ = 0;
    failed_tasks_ = 0;
    canceled_tasks_ = 0;
    suspended_tasks_ = 0;
}

bool TaskScheduler::configure_group(const Group &group, const GroupConfig &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: group(%1%), enable_serial_execution(%2%)", group, config.enable_serial_execution);

    BROOKESIA_CHECK_FALSE_RETURN(is_running(), false, "Not running");
    BROOKESIA_CHECK_FALSE_RETURN(!group.empty(), false, "Group name cannot be empty");

    boost::lock_guard<boost::mutex> lock(mutex_);

    group_configs_[group] = config;

    // Create strand for group if configured
    if (config.enable_serial_execution && strands_.find(group) == strands_.end()) {
        strands_[group] = std::make_shared<boost::asio::strand<boost::asio::io_context::executor_type>>(
                              io_context_->get_executor()
                          );
        BROOKESIA_LOGD("Created strand for group '%1%'", group);
    }

    return true;
}

bool TaskScheduler::wait_tasks_internal(const std::vector<TaskId> &task_ids, int timeout_ms)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: task_count(%1%), timeout_ms(%2%)", task_ids.size(), timeout_ms);

    if (task_ids.empty()) {
        return true;
    }

    auto start = std::chrono::steady_clock::now();

    // Wait for each task
    for (auto task_id : task_ids) {
        int remaining_timeout = timeout_ms;
        if (timeout_ms >= 0) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::steady_clock::now() - start
                           ).count();
            remaining_timeout = timeout_ms - static_cast<int>(elapsed);
            if (remaining_timeout <= 0) {
                BROOKESIA_LOGW("Wait timeout after %1% ms", timeout_ms);
                return false;
            }
        }

        if (!wait(task_id, remaining_timeout)) {
            BROOKESIA_LOGW("Wait timeout for task %1%", task_id);
            return false;
        }
    }

    return true;
}

std::shared_ptr<TaskScheduler::TaskHandle> TaskScheduler::create_handle(
    TaskType type, bool repeat, int interval_ms, const Group &group)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD(
        "Params: type(%1%), repeat(%2%), interval_ms(%3%), group(%4%)",
        BROOKESIA_DESCRIBE_TO_STR(type), repeat, interval_ms, group
    );

    std::shared_ptr<TaskHandle> handle;
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        handle = std::make_shared<TaskHandle>(), nullptr, "Failed to create task handle"
    );

    handle->id = next_id();
    handle->type = type;
    handle->repeat = repeat;
    handle->interval_ms = interval_ms;
    handle->group = group;

    // Get strand for group if configured, and create timer with strand executor
    std::shared_ptr<boost::asio::strand<boost::asio::io_context::executor_type>> strand;
    {
        boost::lock_guard lock(mutex_);
        tasks_[handle->id] = handle;
        // If group is specified, add to group mapping
        if (!group.empty()) {
            groups_[group].insert(handle->id);
            // Check if group has strand configured
            auto strand_it = strands_.find(group);
            if (strand_it != strands_.end()) {
                strand = strand_it->second;
            }
        }
        total_tasks_++;
    }

    // Create timer with strand executor if available, otherwise use io_context executor
    if (strand) {
        handle->timer = std::make_shared<boost::asio::steady_timer>(*strand);
    } else {
        handle->timer = std::make_shared<boost::asio::steady_timer>(*io_context_);
    }
    handle->promise = std::make_shared<std::promise<bool>>();
    handle->future = handle->promise->get_future().share();

    BROOKESIA_LOGD("Created task %1% (group: %2%)", handle->id, group);

    return handle;
}

void TaskScheduler::schedule_once(std::shared_ptr<TaskHandle> handle, OnceTask task)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: handle(%1%)", handle->id);

    handle->timer->expires_after(std::chrono::milliseconds(handle->interval_ms));
    handle->timer->async_wait([this, handle, task = std::move(task)](const boost::system::error_code & ec) {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

        BROOKESIA_LOGD("Params: ec(%1%)", ec);

        // If suspended, don't remove the task - it will be resumed later
        if (handle->state == TaskState::Suspended) {
            BROOKESIA_LOGD(
                "Task %1% is %2%, keeping it alive", handle->id, BROOKESIA_DESCRIBE_TO_STR(handle->state.load())
            );
            return;
        }

        if (ec || handle->state == TaskState::Canceled) {
            // If operation_aborted but task is still Running, it means restart_timer was called
            // Don't remove the task in this case - a new timer has been scheduled
            if (ec == boost::asio::error::operation_aborted && handle->state == TaskState::Running) {
                BROOKESIA_LOGD("Task %1% timer was restarted, not removing", handle->id);
                return;
            }
            boost::lock_guard<boost::mutex> lock(mutex_);
            remove_task_internal(handle->id, handle->group);
            return;
        }

        // Invoke pre-execute callback when task is about to execute
        invoke_pre_execute_callback(handle->id, handle->type);

        bool success = false;
        // Will be executed when the function exits
        lib_utils::FunctionGuard exit_guard(
        [this, handle, &success]() {
            invoke_post_execute_callback(handle->id, handle->type, success);
            mark_finished(handle, success);
        }
        );

        BROOKESIA_CHECK_EXCEPTION_EXECUTE(task(), {
            success = false;
            return;
        }, {BROOKESIA_LOGE("Delayed task %1% execution failed", handle->id);});

        success = true;
    });
}

void TaskScheduler::schedule_periodic(std::shared_ptr<TaskHandle> handle, PeriodicTask task)
{
    // BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    handle->timer->expires_after(std::chrono::milliseconds(handle->interval_ms));
    handle->timer->async_wait([this, handle, task](const boost::system::error_code & ec) {
        // BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

        // BROOKESIA_LOGD("Params: ec(%1%)", ec);

        // If suspended, don't remove the task - it will be resumed later
        if (handle->state == TaskState::Suspended) {
            BROOKESIA_LOGD(
                "Periodic task %1% is %2%, keeping it alive", handle->id,
                BROOKESIA_DESCRIBE_TO_STR(handle->state.load())
            );
            return;
        }

        if (ec || handle->state == TaskState::Canceled) {
            // If operation_aborted but task is still Running, it means restart_timer was called
            // Don't remove the task in this case - a new timer has been scheduled
            if (ec == boost::asio::error::operation_aborted && handle->state == TaskState::Running) {
                BROOKESIA_LOGD("Periodic task %1% timer was restarted, not removing", handle->id);
                return;
            }
            boost::lock_guard<boost::mutex> lock(mutex_);
            remove_task_internal(handle->id, handle->group);
            return;
        }

        // Check if task is already executing to prevent parallel execution
        bool expected = false;
        if (!handle->is_executing.compare_exchange_strong(expected, true)) {
            BROOKESIA_LOGD(
                "Periodic task %1% is already executing, skipping this execution", handle->id
            );
            // Schedule next execution even if we skip this one
            if (handle->repeat && handle->state == TaskState::Running) {
                schedule_periodic(handle, task);
            }
            return;
        }

        // Invoke pre-execute callback when task is about to execute
        invoke_pre_execute_callback(handle->id, handle->type);

        bool success = false;
        auto do_task = [this, handle, task, &success]() {
            bool should_continue = task();
            success = true;

            if (should_continue && handle->repeat && handle->state == TaskState::Running) {
                // Task will continue, not finished yet
                schedule_periodic(handle, task);
            } else {
                // Task is finished
                mark_finished(handle, true);
            }
        };

        // Will be executed when the function exits
        lib_utils::FunctionGuard exit_guard(
        [this, handle, &success]() {
            // Clear execution flag when task completes
            handle->is_executing.store(false);
            invoke_post_execute_callback(handle->id, handle->type, success);
        }
        );

        BROOKESIA_CHECK_EXCEPTION_EXECUTE(do_task(), {
            success = false;
            handle->is_executing.store(false);
            mark_finished(handle, false);
            return;
        }, {BROOKESIA_LOGE("Periodic task(%1%) execution failed", handle->id);});
    });
}

void TaskScheduler::cancel_internal(TaskId task_id)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: task_id(%1%)", task_id);

    auto it = tasks_.find(task_id);
    if (it == tasks_.end()) {
        BROOKESIA_LOGD("Task %1% not found", task_id);
        return;
    }

    auto &handle = it->second;
    handle->state = TaskState::Canceled;
    if (handle->timer) {
        handle->timer->cancel();
    }
    canceled_tasks_++;

    // Set promise value for canceled task (atomically check and set flag to prevent race condition)
    bool expected = false;
    if (handle->promise && handle->promise_fulfilled.compare_exchange_strong(expected, true)) {
        handle->promise->set_value(false);
        handle->promise.reset();
    }

    remove_task_internal(task_id, handle->group);

    BROOKESIA_LOGD("Task %1% canceled", task_id);
}

bool TaskScheduler::suspend_internal(TaskId task_id)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: task_id(%1%)", task_id);

    auto it = tasks_.find(task_id);
    if (it == tasks_.end()) {
        BROOKESIA_LOGW("Task %1% not found", task_id);
        return false;
    }

    auto &handle = it->second;

    // Check if task type supports suspend
    if (handle->type != TaskType::Delayed && handle->type != TaskType::Periodic) {
        BROOKESIA_LOGE(
            "Task %1% cannot be suspended: only Delayed and Periodic tasks support suspend (current type: %2%)",
            task_id, BROOKESIA_DESCRIBE_TO_STR(handle->type)
        );
        return false;
    }

    if (handle->state != TaskState::Running) {
        BROOKESIA_LOGW("Task %1% is not in running state (current: %2%)", task_id, static_cast<int>(handle->state.load()));
        return false;
    }

    // Cancel current timer and record remaining time
    if (handle->timer) {
        auto expiry = handle->timer->expiry();
        auto now = std::chrono::steady_clock::now();
        handle->remaining_time = std::chrono::duration_cast<std::chrono::milliseconds>(expiry - now);
        handle->suspend_time = now;
        handle->timer->cancel();
    }

    handle->state = TaskState::Suspended;
    suspended_tasks_++;

    BROOKESIA_LOGD("Task %1% suspended (remaining: %2% ms)", task_id, handle->remaining_time.count());

    return true;
}

bool TaskScheduler::resume_internal(TaskId task_id)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: task_id(%1%)", task_id);

    auto it = tasks_.find(task_id);
    if (it == tasks_.end()) {
        BROOKESIA_LOGW("Task %1% not found", task_id);
        return false;
    }

    auto &handle = it->second;

    // Check if task type supports resume
    if (handle->type != TaskType::Delayed && handle->type != TaskType::Periodic) {
        BROOKESIA_LOGE(
            "Task %1% cannot be resumed: only Delayed and Periodic tasks support resume (current type: %2%)",
            task_id, BROOKESIA_DESCRIBE_TO_STR(handle->type)
        );
        return false;
    }

    if (handle->state != TaskState::Suspended) {
        BROOKESIA_LOGW(
            "Task %1% is not in suspended state (current: %2%)", task_id,
            BROOKESIA_DESCRIBE_TO_STR(handle->state.load())
        );
        return false;
    }

    handle->state = TaskState::Running;
    suspended_tasks_--;

    // Reschedule the task based on its type
    if (handle->type == TaskType::Delayed) {
        // For Delayed task, reschedule with remaining time
        if (!handle->saved_task) {
            BROOKESIA_LOGE("Task %1% has no saved task closure", task_id);
            return false;
        }

        // Use remaining time if positive, otherwise execute immediately
        int delay_ms = std::max(0LL, handle->remaining_time.count());

        BROOKESIA_LOGD("Rescheduling delayed task %1% with remaining time: %2% ms", task_id, delay_ms);

        // Update interval_ms to remaining time for proper rescheduling
        handle->interval_ms = delay_ms;
        schedule_once(handle, handle->saved_task);

    } else if (handle->type == TaskType::Periodic) {
        // For Periodic task, reschedule with remaining time for first execution, then use original interval
        if (!handle->saved_periodic_task) {
            BROOKESIA_LOGE("Task %1% has no saved periodic task closure", task_id);
            return false;
        }

        // Use remaining time if positive, otherwise execute immediately
        int delay_ms = std::max(0LL, handle->remaining_time.count());

        BROOKESIA_LOGD("Rescheduling periodic task %1% with remaining time: %2% ms, then interval: %3% ms",
                       task_id, delay_ms, handle->interval_ms);

        // First execution with remaining time
        handle->timer->expires_after(std::chrono::milliseconds(delay_ms));
        handle->timer->async_wait([this, handle, original_interval = handle->interval_ms](const boost::system::error_code & ec) {
            BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

            BROOKESIA_LOGD("Params: ec(%1%)", ec);

            // If suspended again, don't remove the task
            if (handle->state == TaskState::Suspended) {
                BROOKESIA_LOGD(
                    "Resumed periodic task %1% is %2%, keeping it alive", handle->id,
                    BROOKESIA_DESCRIBE_TO_STR(handle->state.load())
                );
                return;
            }

            if (ec || handle->state == TaskState::Canceled) {
                boost::lock_guard<boost::mutex> lock(mutex_);
                remove_task_internal(handle->id, handle->group);
                return;
            }

            // Check if task is already executing to prevent parallel execution
            bool expected = false;
            if (!handle->is_executing.compare_exchange_strong(expected, true)) {
                BROOKESIA_LOGD(
                    "Resumed periodic task %1% is already executing, skipping this execution", handle->id
                );
                // Schedule next execution even if we skip this one
                if (handle->repeat && handle->state == TaskState::Running) {
                    handle->interval_ms = original_interval;
                    schedule_periodic(handle, handle->saved_periodic_task);
                }
                return;
            }

            // Invoke pre-execute callback when task is about to execute
            invoke_pre_execute_callback(handle->id, handle->type);

            bool success = false;
            auto do_task = [this, handle, original_interval, &success]() {
                bool should_continue = handle->saved_periodic_task();
                success = true;

                if (should_continue && handle->repeat && handle->state == TaskState::Running) {
                    // Task will continue, not finished yet
                    // Restore original interval for subsequent executions
                    handle->interval_ms = original_interval;
                    schedule_periodic(handle, handle->saved_periodic_task);
                } else {
                    // Task is finished
                    mark_finished(handle, true);
                }
            };

            // Will be executed when the function exits
            lib_utils::FunctionGuard exit_guard(
            [this, handle, &success]() {
                // Clear execution flag when task completes
                handle->is_executing.store(false);
                invoke_post_execute_callback(handle->id, handle->type, success);
            }
            );

            BROOKESIA_CHECK_EXCEPTION_EXECUTE(do_task(), {
                success = false;
                handle->is_executing.store(false);
                mark_finished(handle, false);
                return;
            }, {BROOKESIA_LOGE("Resumed periodic task(%1%) execution failed", handle->id);});
        });
    }

    BROOKESIA_LOGD("Task %1% resumed and rescheduled", task_id);

    return true;
}

void TaskScheduler::remove_task_internal(TaskId task_id, const Group &group)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: task_id(%1%), group(%2%)", task_id, group);

    // Remove from group
    if (!group.empty()) {
        auto group_it = groups_.find(group);
        if (group_it != groups_.end()) {
            group_it->second.erase(task_id);
            if (group_it->second.empty()) {
                groups_.erase(group_it);
            }
        }
    }

    // Clean up task handle
    auto it = tasks_.find(task_id);
    if (it != tasks_.end()) {
        // Ensure timer is canceled and cleaned up
        if (it->second && it->second->timer) {
            it->second->timer->cancel();
            it->second->timer.reset();
        }
        tasks_.erase(it);
    }
}

void TaskScheduler::mark_finished(std::shared_ptr<TaskHandle> handle, bool success)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: success(%1%)", success);

    handle->state = TaskState::Finished;

    if (success) {
        completed_tasks_++;
    } else {
        failed_tasks_++;
    }

    // Set promise value (atomically check and set flag to prevent race condition with cancel)
    bool expected = false;
    if (handle->promise && handle->promise_fulfilled.compare_exchange_strong(expected, true)) {
        handle->promise->set_value(success);
        handle->promise.reset();
    }

    boost::lock_guard<boost::mutex> lock(mutex_);
    remove_task_internal(handle->id, handle->group);

    BROOKESIA_LOGD("Task %1% finished (success: %2%)", handle->id, success);
}

std::shared_future<bool> TaskScheduler::get_future(TaskId id)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: id(%1%)", id);

    boost::lock_guard<boost::mutex> lock(mutex_);
    auto it = tasks_.find(id);
    BROOKESIA_CHECK_FALSE_RETURN(it != tasks_.end(), std::shared_future<bool>(), "Task %1% not found", id);

    return it->second->future;
}

void TaskScheduler::invoke_pre_execute_callback(TaskId task_id, TaskType task_type)
{
    std::function<void(TaskId, TaskType)> callback;
    {
        boost::lock_guard<boost::mutex> lock(mutex_);
        callback = pre_execute_callback_;
    }

    if (callback) {
        BROOKESIA_CHECK_EXCEPTION_EXECUTE(
        callback(task_id, task_type), {},
        {BROOKESIA_LOGE("Pre-execute callback error for task %1%", task_id);}
        );
    }
}

void TaskScheduler::invoke_post_execute_callback(TaskId task_id, TaskType task_type, bool success)
{
    std::function<void(TaskId, TaskType, bool)> callback;
    {
        boost::lock_guard<boost::mutex> lock(mutex_);
        callback = post_execute_callback_;
    }

    if (callback) {
        BROOKESIA_CHECK_EXCEPTION_EXECUTE(
        callback(task_id, task_type, success), {},
        {BROOKESIA_LOGE("Post-execute callback error for task %1%", task_id);}
        );
    }
}

} // namespace esp_brookesia::lib_utils
