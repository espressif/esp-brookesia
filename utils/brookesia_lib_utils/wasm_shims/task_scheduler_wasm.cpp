/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/lib_utils/macro_configs.h"
#if !BROOKESIA_UTILS_TASK_SCHEDULER_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif

#include <algorithm>
#include <memory>
#include <vector>

#if defined(__EMSCRIPTEN__)
#include "emscripten/emscripten.h"
#endif

#include "brookesia/lib_utils/check.hpp"
#include "brookesia/lib_utils/log.hpp"
#include "brookesia/lib_utils/task_scheduler.hpp"

namespace esp_brookesia::lib_utils {

namespace {

void schedule_async_call(void (*callback)(void *), void *arg, int delay_ms)
{
#if defined(__EMSCRIPTEN__)
    emscripten_async_call(callback, arg, std::max(delay_ms, 0));
#else
    (void)delay_ms;
    callback(arg);
#endif
}

void set_promise_value(const std::shared_ptr<boost::promise<bool>> &promise, bool value)
{
    if (promise == nullptr) {
        return;
    }
    try {
        promise->set_value(value);
    } catch (const boost::future_error &) {
    }
}

} // namespace

struct WasmTimerContext {
    TaskScheduler *scheduler = nullptr;
    std::shared_ptr<TaskScheduler::TaskHandle> handle;
    TaskScheduler::OnceTask once_task;
    TaskScheduler::PeriodicTask periodic_task;
    uint64_t generation = 0;
};

TaskScheduler::~TaskScheduler()
{
    stop();
}

bool TaskScheduler::start(const StartConfig &config)
{
    boost::lock_guard lock(mutex_);
    if (is_running_) {
        return true;
    }
    pre_execute_callbacks_[""] = config.pre_execute_callback;
    post_execute_callbacks_[""] = config.post_execute_callback;
    worker_wait_slot_count_.store(0);
    is_running_ = true;
    return true;
}

bool TaskScheduler::is_current_thread_worker() const
{
    return false;
}

bool TaskScheduler::is_current_thread_in_group(const Group &group) const
{
    (void)group;
    return false;
}

bool TaskScheduler::try_acquire_worker_wait_slot()
{
    return true;
}

void TaskScheduler::release_worker_wait_slot()
{
}

void TaskScheduler::stop()
{
    cancel_all();
    boost::lock_guard lock(mutex_);
    is_running_ = false;
    worker_wait_slot_count_.store(0);
    pre_execute_callbacks_.clear();
    post_execute_callbacks_.clear();
}

bool TaskScheduler::configure_group(const Group &group, const GroupConfig &config)
{
    boost::lock_guard lock(mutex_);
    pre_execute_callbacks_[group] = config.pre_execute_callback;
    post_execute_callbacks_[group] = config.post_execute_callback;
    return true;
}

bool TaskScheduler::post_internal(OnceTask task, TaskId *id, const Group &group, bool enable_immediate)
{
    (void)enable_immediate;
    BROOKESIA_CHECK_FALSE_RETURN(task != nullptr, false, "Invalid task");
    if (!is_running_) {
        BROOKESIA_LOGW("TaskScheduler is not running, executing task inline");
    }
    auto handle = create_handle(TaskType::Immediate, false, 0, group);
    if (id != nullptr) {
        *id = handle->id;
    }

    invoke_pre_execute_callback(handle->id, handle->type, handle->group);
    bool success = true;
    try {
        task();
    } catch (const std::exception &e) {
        success = false;
        BROOKESIA_LOGE("Immediate task[%1%] failed: %2%", handle->id, e.what());
    } catch (...) {
        success = false;
        BROOKESIA_LOGE("Immediate task[%1%] failed with unknown exception", handle->id);
    }
    invoke_post_execute_callback(handle->id, handle->type, success, handle->group);
    mark_finished(handle, success);
    return success;
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
    BROOKESIA_CHECK_FALSE_RETURN(task != nullptr, false, "Invalid delayed task");
    if (!is_running_) {
        BROOKESIA_LOGW("TaskScheduler is not running, delayed task will still use browser timer");
    }
    auto handle = create_handle(TaskType::Delayed, false, std::max(delay_ms, 0), group);
    handle->saved_task = task;
    if (id != nullptr) {
        *id = handle->id;
    }
    schedule_once(handle, std::move(task));
    return true;
}

bool TaskScheduler::post_periodic(PeriodicTask task, int interval_ms, TaskId *id, const Group &group)
{
    BROOKESIA_CHECK_FALSE_RETURN(task != nullptr, false, "Invalid periodic task");
    auto handle = create_handle(TaskType::Periodic, true, std::max(interval_ms, 0), group);
    handle->saved_periodic_task = task;
    if (id != nullptr) {
        *id = handle->id;
    }
    schedule_periodic(handle, std::move(task));
    return true;
}

bool TaskScheduler::post_batch(std::vector<OnceTask> tasks, std::vector<TaskId> *ids, const Group &group)
{
    if (ids != nullptr) {
        ids->clear();
        ids->reserve(tasks.size());
    }
    for (auto &task : tasks) {
        TaskId task_id = 0;
        BROOKESIA_CHECK_FALSE_RETURN(post(std::move(task), &task_id, group), false, "Failed to post batch task");
        if (ids != nullptr) {
            ids->push_back(task_id);
        }
    }
    return true;
}

void TaskScheduler::cancel(TaskId id)
{
    boost::lock_guard lock(mutex_);
    cancel_internal(id);
}

void TaskScheduler::cancel_group(const Group &group)
{
    std::vector<TaskId> task_ids;
    {
        boost::lock_guard lock(mutex_);
        auto group_it = groups_.find(group);
        if (group_it == groups_.end()) {
            return;
        }
        task_ids.assign(group_it->second.begin(), group_it->second.end());
    }
    for (const auto task_id : task_ids) {
        cancel(task_id);
    }
}

void TaskScheduler::cancel_all()
{
    std::vector<TaskId> task_ids;
    {
        boost::lock_guard lock(mutex_);
        task_ids.reserve(tasks_.size());
        for (const auto &[task_id, _] : tasks_) {
            task_ids.push_back(task_id);
        }
    }
    for (const auto task_id : task_ids) {
        cancel(task_id);
    }
}

bool TaskScheduler::suspend(TaskId id)
{
    boost::lock_guard lock(mutex_);
    return suspend_internal(id);
}

size_t TaskScheduler::suspend_group(const Group &group)
{
    std::vector<TaskId> task_ids;
    {
        boost::lock_guard lock(mutex_);
        auto group_it = groups_.find(group);
        if (group_it == groups_.end()) {
            return 0;
        }
        task_ids.assign(group_it->second.begin(), group_it->second.end());
    }
    size_t count = 0;
    for (const auto task_id : task_ids) {
        if (suspend(task_id)) {
            count++;
        }
    }
    return count;
}

size_t TaskScheduler::suspend_all()
{
    std::vector<TaskId> task_ids;
    {
        boost::lock_guard lock(mutex_);
        for (const auto &[task_id, _] : tasks_) {
            task_ids.push_back(task_id);
        }
    }
    size_t count = 0;
    for (const auto task_id : task_ids) {
        if (suspend(task_id)) {
            count++;
        }
    }
    return count;
}

bool TaskScheduler::resume(TaskId id)
{
    boost::lock_guard lock(mutex_);
    return resume_internal(id);
}

size_t TaskScheduler::resume_group(const Group &group)
{
    std::vector<TaskId> task_ids;
    {
        boost::lock_guard lock(mutex_);
        auto group_it = groups_.find(group);
        if (group_it == groups_.end()) {
            return 0;
        }
        task_ids.assign(group_it->second.begin(), group_it->second.end());
    }
    size_t count = 0;
    for (const auto task_id : task_ids) {
        if (resume(task_id)) {
            count++;
        }
    }
    return count;
}

size_t TaskScheduler::resume_all()
{
    std::vector<TaskId> task_ids;
    {
        boost::lock_guard lock(mutex_);
        for (const auto &[task_id, _] : tasks_) {
            task_ids.push_back(task_id);
        }
    }
    size_t count = 0;
    for (const auto task_id : task_ids) {
        if (resume(task_id)) {
            count++;
        }
    }
    return count;
}

bool TaskScheduler::wait(TaskId id, int timeout_ms)
{
    (void)timeout_ms;
    boost::lock_guard lock(mutex_);
    auto task_it = tasks_.find(id);
    return (task_it == tasks_.end()) || (task_it->second->state == TaskState::Finished);
}

bool TaskScheduler::wait_group(const Group &group, int timeout_ms)
{
    (void)timeout_ms;
    boost::lock_guard lock(mutex_);
    auto group_it = groups_.find(group);
    return (group_it == groups_.end()) || group_it->second.empty();
}

bool TaskScheduler::wait_all(int timeout_ms)
{
    (void)timeout_ms;
    boost::lock_guard lock(mutex_);
    return tasks_.empty();
}

bool TaskScheduler::restart_timer(TaskId id)
{
    boost::lock_guard lock(mutex_);
    auto task_it = tasks_.find(id);
    if ((task_it == tasks_.end()) || (task_it->second->state != TaskState::Running)) {
        return false;
    }
    auto handle = task_it->second;
    handle->generation.fetch_add(1, std::memory_order_relaxed);
    if (handle->type == TaskType::Delayed) {
        schedule_once(handle, handle->saved_task);
        return true;
    }
    if (handle->type == TaskType::Periodic) {
        schedule_periodic(handle, handle->saved_periodic_task);
        return true;
    }
    return false;
}

TaskScheduler::TaskType TaskScheduler::get_type(TaskId id) const
{
    boost::lock_guard lock(mutex_);
    auto task_it = tasks_.find(id);
    return (task_it != tasks_.end()) ? task_it->second->type : TaskType::Immediate;
}

TaskScheduler::TaskState TaskScheduler::get_state(TaskId id) const
{
    boost::lock_guard lock(mutex_);
    auto task_it = tasks_.find(id);
    return (task_it != tasks_.end()) ? task_it->second->state.load() : TaskState::Finished;
}

TaskScheduler::Group TaskScheduler::get_group(TaskId id) const
{
    boost::lock_guard lock(mutex_);
    auto task_it = tasks_.find(id);
    return (task_it != tasks_.end()) ? task_it->second->group : Group{};
}

size_t TaskScheduler::get_group_task_count(const Group &group) const
{
    boost::lock_guard lock(mutex_);
    auto group_it = groups_.find(group);
    return (group_it != groups_.end()) ? group_it->second.size() : 0;
}

std::vector<TaskScheduler::Group> TaskScheduler::get_active_groups() const
{
    boost::lock_guard lock(mutex_);
    std::vector<Group> groups;
    groups.reserve(groups_.size());
    for (const auto &[group, task_ids] : groups_) {
        if (!task_ids.empty()) {
            groups.push_back(group);
        }
    }
    return groups;
}

TaskScheduler::Statistics TaskScheduler::get_statistics() const
{
    return {
        .total_tasks = static_cast<size_t>(total_tasks_.load()),
        .completed_tasks = static_cast<size_t>(completed_tasks_.load()),
        .failed_tasks = static_cast<size_t>(failed_tasks_.load()),
        .canceled_tasks = static_cast<size_t>(canceled_tasks_.load()),
        .suspended_tasks = static_cast<size_t>(suspended_tasks_.load()),
    };
}

void TaskScheduler::reset_statistics()
{
    total_tasks_ = 0;
    completed_tasks_ = 0;
    failed_tasks_ = 0;
    canceled_tasks_ = 0;
    suspended_tasks_ = 0;
}

std::shared_ptr<TaskScheduler::TaskHandle> TaskScheduler::create_handle(
    TaskType type, bool repeat, int interval_ms, const Group &group)
{
    auto handle = std::make_shared<TaskHandle>();
    handle->id = next_id();
    handle->type = type;
    handle->repeat = repeat;
    handle->interval_ms = interval_ms;
    handle->group = group;
    handle->promise = std::make_shared<boost::promise<bool>>();
    handle->future = handle->promise->get_future().share();
    {
        boost::lock_guard lock(mutex_);
        tasks_[handle->id] = handle;
        if (!group.empty()) {
            groups_[group].insert(handle->id);
        }
        total_tasks_++;
    }
    return handle;
}

void TaskScheduler::schedule_once(std::shared_ptr<TaskHandle> handle, OnceTask task)
{
    auto context = std::make_unique<WasmTimerContext>();
    context->scheduler = this;
    context->handle = std::move(handle);
    context->once_task = std::move(task);
    context->generation = context->handle->generation.fetch_add(1, std::memory_order_relaxed) + 1;
    const int delay_ms = context->handle->interval_ms;
    schedule_async_call([](void *arg) {
        std::unique_ptr<WasmTimerContext> context(static_cast<WasmTimerContext *>(arg));
        auto *scheduler = context->scheduler;
        auto handle = context->handle;
        if ((scheduler == nullptr) || (handle == nullptr) ||
                (handle->generation.load(std::memory_order_relaxed) != context->generation) ||
                (handle->state != TaskState::Running)) {
            return;
        }
        scheduler->invoke_pre_execute_callback(handle->id, handle->type, handle->group);
        bool success = true;
        try {
            context->once_task();
        } catch (const std::exception &e) {
            success = false;
            BROOKESIA_LOGE("Delayed task[%1%] failed: %2%", handle->id, e.what());
        } catch (...) {
            success = false;
            BROOKESIA_LOGE("Delayed task[%1%] failed with unknown exception", handle->id);
        }
        scheduler->invoke_post_execute_callback(handle->id, handle->type, success, handle->group);
        scheduler->mark_finished(handle, success);
    }, context.release(), delay_ms);
}

void TaskScheduler::schedule_periodic(std::shared_ptr<TaskHandle> handle, PeriodicTask task)
{
    auto context = std::make_unique<WasmTimerContext>();
    context->scheduler = this;
    context->handle = std::move(handle);
    context->periodic_task = std::move(task);
    context->generation = context->handle->generation.fetch_add(1, std::memory_order_relaxed) + 1;
    const int delay_ms = context->handle->interval_ms;
    schedule_async_call([](void *arg) {
        std::unique_ptr<WasmTimerContext> context(static_cast<WasmTimerContext *>(arg));
        auto *scheduler = context->scheduler;
        auto handle = context->handle;
        if ((scheduler == nullptr) || (handle == nullptr) ||
                (handle->generation.load(std::memory_order_relaxed) != context->generation) ||
                (handle->state != TaskState::Running)) {
            return;
        }
        if (handle->is_executing.exchange(true)) {
            scheduler->schedule_periodic(handle, context->periodic_task);
            return;
        }
        scheduler->invoke_pre_execute_callback(handle->id, handle->type, handle->group);
        bool keep_running = false;
        bool success = true;
        try {
            keep_running = context->periodic_task();
        } catch (const std::exception &e) {
            success = false;
            BROOKESIA_LOGE("Periodic task[%1%] failed: %2%", handle->id, e.what());
        } catch (...) {
            success = false;
            BROOKESIA_LOGE("Periodic task[%1%] failed with unknown exception", handle->id);
        }
        handle->is_executing = false;
        scheduler->invoke_post_execute_callback(handle->id, handle->type, success, handle->group);
        if (success && keep_running && (handle->state == TaskState::Running)) {
            scheduler->schedule_periodic(handle, context->periodic_task);
        } else {
            scheduler->mark_finished(handle, success);
        }
    }, context.release(), delay_ms);
}

void TaskScheduler::cancel_internal(TaskId task_id)
{
    auto task_it = tasks_.find(task_id);
    if (task_it == tasks_.end()) {
        return;
    }
    auto handle = task_it->second;
    if (handle->state == TaskState::Suspended) {
        suspended_tasks_--;
    }
    handle->generation.fetch_add(1, std::memory_order_relaxed);
    handle->state = TaskState::Canceled;
    canceled_tasks_++;
    set_promise_value(handle->promise, false);
    remove_task_internal(task_id, handle->group);
}

bool TaskScheduler::suspend_internal(TaskId task_id)
{
    auto task_it = tasks_.find(task_id);
    if (task_it == tasks_.end()) {
        return false;
    }
    auto handle = task_it->second;
    if ((handle->type == TaskType::Immediate) || (handle->state != TaskState::Running)) {
        return false;
    }
    handle->generation.fetch_add(1, std::memory_order_relaxed);
    handle->state = TaskState::Suspended;
    suspended_tasks_++;
    return true;
}

bool TaskScheduler::resume_internal(TaskId task_id)
{
    auto task_it = tasks_.find(task_id);
    if (task_it == tasks_.end()) {
        return false;
    }
    auto handle = task_it->second;
    if (handle->state != TaskState::Suspended) {
        return false;
    }
    handle->state = TaskState::Running;
    if (suspended_tasks_ > 0) {
        suspended_tasks_--;
    }
    if (handle->type == TaskType::Delayed) {
        schedule_once(handle, handle->saved_task);
        return true;
    }
    if (handle->type == TaskType::Periodic) {
        schedule_periodic(handle, handle->saved_periodic_task);
        return true;
    }
    return false;
}

bool TaskScheduler::wait_tasks_internal(const std::vector<TaskId> &task_ids, int timeout_ms)
{
    (void)timeout_ms;
    boost::lock_guard lock(mutex_);
    for (const auto task_id : task_ids) {
        if (tasks_.find(task_id) != tasks_.end()) {
            return false;
        }
    }
    return true;
}

void TaskScheduler::remove_task_internal(TaskId task_id, const Group &group)
{
    tasks_.erase(task_id);
    if (!group.empty()) {
        auto group_it = groups_.find(group);
        if (group_it != groups_.end()) {
            group_it->second.erase(task_id);
            if (group_it->second.empty()) {
                groups_.erase(group_it);
            }
        }
    }
}

void TaskScheduler::mark_finished(std::shared_ptr<TaskHandle> handle, bool success)
{
    if (handle == nullptr) {
        return;
    }
    boost::lock_guard lock(mutex_);
    if (handle->state == TaskState::Canceled) {
        return;
    }
    if (handle->state == TaskState::Suspended && suspended_tasks_ > 0) {
        suspended_tasks_--;
    }
    handle->state = TaskState::Finished;
    if (success) {
        completed_tasks_++;
    } else {
        failed_tasks_++;
    }
    set_promise_value(handle->promise, success);
    remove_task_internal(handle->id, handle->group);
}

boost::shared_future<bool> TaskScheduler::get_future(TaskId id)
{
    boost::lock_guard lock(mutex_);
    auto task_it = tasks_.find(id);
    return (task_it != tasks_.end()) ? task_it->second->future : boost::shared_future<bool>();
}

void TaskScheduler::invoke_pre_execute_callback(TaskId task_id, TaskType task_type, const Group &group)
{
    PreExecuteCallback global_callback;
    PreExecuteCallback group_callback;
    {
        boost::lock_guard lock(mutex_);
        auto global_it = pre_execute_callbacks_.find("");
        if (global_it != pre_execute_callbacks_.end()) {
            global_callback = global_it->second;
        }
        auto group_it = pre_execute_callbacks_.find(group);
        if (group_it != pre_execute_callbacks_.end()) {
            group_callback = group_it->second;
        }
    }
    if (global_callback) {
        global_callback(group, task_id, task_type);
    }
    if (group_callback) {
        group_callback(group, task_id, task_type);
    }
}

void TaskScheduler::invoke_post_execute_callback(TaskId task_id, TaskType task_type, bool success, const Group &group)
{
    PostExecuteCallback global_callback;
    PostExecuteCallback group_callback;
    {
        boost::lock_guard lock(mutex_);
        auto global_it = post_execute_callbacks_.find("");
        if (global_it != post_execute_callbacks_.end()) {
            global_callback = global_it->second;
        }
        auto group_it = post_execute_callbacks_.find(group);
        if (group_it != post_execute_callbacks_.end()) {
            group_callback = group_it->second;
        }
    }
    if (global_callback) {
        global_callback(group, task_id, task_type, success);
    }
    if (group_callback) {
        group_callback(group, task_id, task_type, success);
    }
}

} // namespace esp_brookesia::lib_utils
