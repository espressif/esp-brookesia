/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/lib_utils/macro_configs.h"
#if !BROOKESIA_UTILS_STATIC_MACHINE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/lib_utils/check.hpp"
#include "brookesia/lib_utils/state_machine.hpp"
#include "brookesia/lib_utils/log.hpp"
#include "brookesia/lib_utils/function_guard.hpp"
#include <thread>
#include <chrono>

namespace esp_brookesia::lib_utils {

constexpr uint32_t STATE_MACHINE_STOP_TIMEOUT_MS = 100;

StateMachine::~StateMachine()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    // Ensure clean shutdown
    if (is_running()) {
        BROOKESIA_LOGD("State machine is still running, stopping...");
        stop();
    }
}

bool StateMachine::add_state(const std::string &name, StatePtr state)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: name(%1%), state(%2%)", name.c_str(), state);

    BROOKESIA_CHECK_NULL_RETURN(state, false, "Invalid argument");

    boost::lock_guard lock(mutex_);
    BROOKESIA_CHECK_FALSE_RETURN(states_.find(name) == states_.end(), false, "State '%1%' already exists", name);

    states_[name] = state;

    return true;
}

bool StateMachine::add_transition(const std::string &from, const std::string &action, const std::string &to)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: from(%1%), action(%2%), to(%3%)", from, action, to);

    boost::lock_guard lock(mutex_);
    BROOKESIA_CHECK_FALSE_RETURN(
        transitions_[from].find(action) == transitions_[from].end(), false,
        "Transition from '%1%' on action '%2%' already exists", from, action
    );

    transitions_[from][action] = to;

    return true;
}

bool StateMachine::start(std::shared_ptr<TaskScheduler> task_scheduler, const std::string &initial)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: initial(%1%)", initial);

    BROOKESIA_CHECK_NULL_RETURN(task_scheduler, false, "Task scheduler is null");

    // Check if already running
    if (is_running()) {
        BROOKESIA_LOGD("Already running");
        return true;
    }

    // Ensure scheduler is running
    if (!task_scheduler->is_running()) {
        BROOKESIA_LOGW("Scheduler is not running, starting it...");
        BROOKESIA_CHECK_FALSE_RETURN(task_scheduler->start(), false, "Failed to start scheduler");
    }

    // Setup guard for cleanup on failure
    boost::unique_lock lock(mutex_);
    lib_utils::FunctionGuard stop_guard([this, &lock]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        if (lock.owns_lock()) {
            lock.unlock();
        }
        stop();
    });

    // Verify initial state exists
    BROOKESIA_CHECK_FALSE_RETURN(
        states_.find(initial) != states_.end(), false, "Initial state '%1%' does not exist", initial
    );

    // Store scheduler and configure group
    task_scheduler_ = task_scheduler;

    // Configure group for serial execution (ensures thread-safe state transitions)
    TaskScheduler::GroupConfig config;
    config.enable_serial_execution = true;
    BROOKESIA_CHECK_FALSE_RETURN(
        task_scheduler->configure_group(task_group_name_, config), false,
        "Failed to configure group '%1%' for state machine", task_group_name_
    );
    BROOKESIA_LOGD("State machine configured to use serial execution for group '%1%'", task_group_name_);

    lock.unlock();

    // Enter initial state (without holding lock)
    BROOKESIA_CHECK_FALSE_RETURN(enter_initial_state(initial), false, "Failed to enter initial state '%1%'", initial);

    stop_guard.release();

    return true;
}

void StateMachine::stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_running()) {
        BROOKESIA_LOGD("Not running");
        return;
    }

    // Cancel tasks without holding lock (only cancel tasks created by this state machine)
    cancel_current_tasks();

    // Clear internal data under lock
    boost::lock_guard lock(mutex_);
    task_scheduler_.reset();      // Clear member to mark as stopped
    current_state_ = "";
    transition_count_ = 0;
}

bool StateMachine::trigger_action(const std::string &action, bool use_dispatch)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: action(%1%), use_dispatch(%2%)", action, use_dispatch);

    // Get scheduler reference and increment transition count under lock
    std::shared_ptr<TaskScheduler> scheduler;
    {
        boost::lock_guard lock(mutex_);
        BROOKESIA_CHECK_NULL_RETURN(task_scheduler_, false, "State machine is not running");
        scheduler = task_scheduler_;
        // Mark transition as pending immediately when trigger_action is called
        transition_count_++;
    }

    // Put action trigger logic into serial queue to avoid concurrency issues
    auto task = [this, action]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

        // Ensure transition_count_ is decremented when task completes
        lib_utils::FunctionGuard transition_guard([this]() {
            boost::lock_guard lock(mutex_);
            if (transition_count_ > 0) {
                transition_count_--;
            }
        });

        std::string last_state;
        std::string next_state;
        {
            boost::shared_lock lock(mutex_);

            BROOKESIA_CHECK_FALSE_EXIT(!current_state_.empty(), "State machine not started");

            auto it = transitions_.find(current_state_);
            BROOKESIA_CHECK_FALSE_EXIT(
                it != transitions_.end(), "No transitions defined for state '%1%'", current_state_
            );

            auto trans_it = it->second.find(action);
            BROOKESIA_CHECK_FALSE_EXIT(
                trans_it != it->second.end(), "No transition for action '%1%' in state '%2%'", action, current_state_
            );

            last_state = current_state_;
            next_state = trans_it->second;
        }

        // Call transition_to without holding the lock, passing action as action
        auto is_success = transition_to(next_state, action);
        if (!is_success) {
            BROOKESIA_LOGE("Failed to transition to state '%1%'", next_state);
            return;
        }

        // Get current state and callback without holding lock during callback execution
        std::string final_state;
        TransitionFinishCallback callback;
        {
            boost::shared_lock lock(mutex_);
            final_state = current_state_;
            callback = transition_finish_callback_;
        }

        // Call callback without holding the lock
        if (callback) {
            callback(last_state, action, final_state);
        }
    };
    if (use_dispatch) {
        BROOKESIA_CHECK_FALSE_RETURN(
            scheduler->dispatch(std::move(task), nullptr, task_group_name_), false,
            "Failed to dispatch trigger action task"
        );
    } else {
        BROOKESIA_CHECK_FALSE_RETURN(
            scheduler->post(std::move(task), nullptr, task_group_name_), false,
            "Failed to post trigger action task"
        );
    }

    return true;
}

bool StateMachine::wait_all_transitions(uint32_t timeout_ms)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: timeout_ms(%1%)", timeout_ms);

    BROOKESIA_CHECK_FALSE_RETURN(is_running(), false, "Not running");

    constexpr uint32_t POLL_INTERVAL_MS = 10;
    uint32_t elapsed_ms = 0;

    while (has_transition_running()) {
        if (elapsed_ms >= timeout_ms) {
            BROOKESIA_LOGE("Wait for all transitions finished within timeout %1% ms", timeout_ms);
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(POLL_INTERVAL_MS));
        elapsed_ms += POLL_INTERVAL_MS;
    }

    return true;
}

bool StateMachine::force_transition_to(const std::string &target_state)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: target_state(%1%)", target_state);

    BROOKESIA_CHECK_FALSE_RETURN(is_running(), false, "Not running");

    std::shared_ptr<TaskScheduler> scheduler;
    {
        boost::shared_lock lock(mutex_);
        scheduler = task_scheduler_;
    }

    // Cancel current tasks immediately
    cancel_current_tasks();

    // Transition to target state immediately
    {
        boost::lock_guard lock(mutex_);
        current_state_ = target_state;
    }

    return true;
}

bool StateMachine::setup_state_tasks(const std::string &name)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: name(%1%)", name);

    std::shared_ptr<TaskScheduler> scheduler;
    StatePtr state;
    {
        boost::shared_lock lock(mutex_);
        BROOKESIA_CHECK_NULL_RETURN(task_scheduler_, false, "Task scheduler is not available");
        scheduler = task_scheduler_;

        auto it = states_.find(name);
        BROOKESIA_CHECK_FALSE_RETURN(
            it != states_.end(), false, "Cannot setup tasks for non-existent state '%1%'", name
        );
        state = it->second;
    }

    // Periodic task (without holding lock, as scheduler operations may be slow)
    if (state->get_update_interval() > 0) {
        TaskScheduler::TaskId task_id = 0;
        scheduler->post_periodic([this, state]() -> bool {
            BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
            state->on_update();
            return true;
        }, state->get_update_interval(), &task_id, task_group_name_);

        // Update task_id requires lock protection
        {
            boost::lock_guard lock(mutex_);
            update_task_id_ = task_id;
        }
    }

    // Timeout task (without holding lock, as scheduler operations may be slow)
    if (state->get_timeout_ms() > 0 && !state->get_timeout_action().empty()) {
        TaskScheduler::TaskId task_id = 0;
        scheduler->post_delayed([this, ev = state->get_timeout_action()] {
            BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
            BROOKESIA_CHECK_FALSE_EXIT(this->trigger_action(ev, true), "Cannot trigger action '%1%'", ev);
        }, state->get_timeout_ms(), &task_id, task_group_name_);

        // Update task_id requires lock protection
        {
            boost::lock_guard lock(mutex_);
            timeout_task_id_ = task_id;
        }
    }

    return true;
}

bool StateMachine::enter_initial_state(const std::string &name)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: name(%1%)", name);

    // Step 1: Cancel any existing tasks (without holding lock)
    cancel_current_tasks();

    // Step 2: Get state object under lock
    StatePtr state;
    {
        boost::shared_lock lock(mutex_);
        auto it = states_.find(name);
        BROOKESIA_CHECK_FALSE_RETURN(
            it != states_.end(), false, "Initial state '%1%' does not exist", name
        );
        state = it->second;
    }

    // Step 3: Call on_enter guard (without holding lock to allow state logic to run freely)
    BROOKESIA_CHECK_FALSE_RETURN(
        state->on_enter(), false, "Entry denied: cannot enter initial state '%1%'", name
    );

    // Step 4: Update current state under lock
    {
        boost::lock_guard lock(mutex_);
        current_state_ = name;
    }

    // Step 5: Setup state tasks (without holding lock, setup_state_tasks manages its own locks)
    BROOKESIA_CHECK_FALSE_RETURN(setup_state_tasks(name), false, "Cannot setup tasks for initial state '%1%'", name);

    return true;
}

bool StateMachine::transition_to(const std::string &next, const std::string &action)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: next(%1%), action(%2%)", next, action);

    std::string previous_state;
    StatePtr current_state_obj;
    StatePtr next_state_obj;

    // Step 1: Validate transition and get state objects under lock
    {
        boost::shared_lock lock(mutex_);

        // Ignore self-transition
        if (current_state_ == next) {
            BROOKESIA_LOGD("Ignoring self-transition to '%1%'", next);
            return true;
        }

        // Verify current state exists
        auto current_it = states_.find(current_state_);
        BROOKESIA_CHECK_FALSE_RETURN(
            current_it != states_.end(), false, "Current state '%1%' does not exist", current_state_
        );

        // Verify next state exists
        auto next_it = states_.find(next);
        BROOKESIA_CHECK_FALSE_RETURN(
            next_it != states_.end(), false, "Next state '%1%' does not exist", next
        );

        previous_state = current_state_;
        current_state_obj = current_it->second;
        next_state_obj = next_it->second;
    }

    // Step 2: Exit current state (without holding lock to allow state logic to run freely)
    BROOKESIA_LOGD("Exiting state '%1%' to '%2%' by action '%3%'", previous_state, next, action);
    auto is_success = current_state_obj->on_exit(next, action);
    if (!is_success) {
        BROOKESIA_LOGE("Exit denied: cannot exit '%1%' to '%2%'", previous_state, next);
        return false;
    }

    // Step 3: Cancel current state's tasks (without holding lock to avoid deadlock)
    cancel_current_tasks();

    // Step 4: Enter new state (without holding lock to allow state logic to run freely)
    BROOKESIA_LOGD("Entering state '%1%' from '%2%' by action '%3%'", next, previous_state, action);
    if (!next_state_obj->on_enter(previous_state, action)) {
        // Entry failed, rollback to original state
        BROOKESIA_LOGE("Entry denied: cannot enter '%1%' from '%2%'", next, previous_state);
        BROOKESIA_LOGW("Rolling back to state '%1%'", previous_state);

        // Re-enter previous state (best effort rollback)
        current_state_obj->on_enter();

        // Restore previous state under lock
        {
            boost::lock_guard lock(mutex_);
            current_state_ = previous_state;
        }

        // Re-setup tasks for previous state (setup_state_tasks manages its own locks)
        BROOKESIA_CHECK_FALSE_RETURN(
            setup_state_tasks(previous_state), false, "Cannot setup tasks for state '%1%' during rollback", previous_state
        );
        return false;
    }

    // Step 5: Successful transition - update state and setup new tasks
    {
        boost::lock_guard lock(mutex_);
        current_state_ = next;
    }

    // Setup tasks for new state (setup_state_tasks manages its own locks)
    BROOKESIA_CHECK_FALSE_RETURN(setup_state_tasks(next), false, "Cannot setup tasks for state '%1%'", next);

    BROOKESIA_LOGD("Successfully transitioned from '%1%' to '%2%'", previous_state, next);
    return true;
}

void StateMachine::cancel_current_tasks()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    // Acquire lock to read and clear task IDs and get scheduler reference
    std::shared_ptr<TaskScheduler> scheduler;
    TaskScheduler::TaskId timeout_id = 0;
    TaskScheduler::TaskId update_id = 0;

    {
        boost::lock_guard lock(mutex_);
        scheduler = task_scheduler_;
        timeout_id = timeout_task_id_;
        update_id = update_task_id_;
        timeout_task_id_ = 0;
        update_task_id_ = 0;
    }

    // Cancel tasks without holding lock (scheduler operations may be slow)
    if (scheduler) {
        if (timeout_id) {
            scheduler->cancel(timeout_id);
        }
        if (update_id) {
            scheduler->cancel(update_id);
        }
    }
}

} // namespace esp_brookesia::lib_utils
