/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "brookesia/lib_utils/state_base.hpp"
#include "brookesia/lib_utils/task_scheduler.hpp"
#include <map>
#include <memory>
#include <string>
#include <cstdint>
#include <boost/thread/shared_mutex.hpp>

namespace esp_brookesia::lib_utils {

/**
 * @brief Thread-safe Finite State Machine implementation
 *
 * Manages states, transitions, and state lifecycle with support for:
 * - State entry/exit guards (on_enter/on_exit)
 * - Periodic state updates (on_update)
 * - State timeouts with automatic action triggering
 * - Asynchronous state transitions via action queue
 * - Serial execution guarantee (no concurrent state transitions)
 * - Transition rollback on entry failure
 *
 * @note All state transitions are executed serially through the task scheduler's group mechanism,
 *       ensuring thread-safe operations even when actions are triggered from multiple threads.
 *       State callbacks (on_enter, on_exit, on_update) are executed without holding internal locks,
 *       allowing them to safely trigger new actions or perform blocking operations.
 */
class StateMachine {
public:
    using StatePtr = std::shared_ptr<StateBase>;
    using TransitionFinishCallback = std::function <
                                     void(const std::string &from, const std::string &action, const std::string &to)
                                     >;

    static constexpr const char *DEFAULT_TASK_GROUP_NAME = "state_machine";

    explicit StateMachine(const std::string &group_name = DEFAULT_TASK_GROUP_NAME)
        : task_group_name_(group_name)
    {
    }
    ~StateMachine();

    /**
     * @brief Add a state to the state machine
     *
     * @param name Unique name for the state
     * @param state Shared pointer to the state object
     * @return true if added successfully, false if state already exists
     */
    bool add_state(const std::string &name, StatePtr state);

    /**
     * @brief Add a transition between states
     *
     * @param from Source state name
     * @param action Action that triggers the transition
     * @param to Target state name
     * @return true if added successfully, false if transition already exists
     */
    bool add_transition(const std::string &from, const std::string &action, const std::string &to);

    /**
     * @brief Start the state machine with an initial state
     *
     * @param task_scheduler Shared pointer to the task scheduler
     * @param initial Name of the initial state
     * @return true if started successfully, false if state doesn't exist or on other errors
     *
     * @note If the state machine is already running, this returns true immediately.
     *       If the scheduler is not running, this will attempt to start it.
     *       The task group is automatically configured for serial execution to ensure thread-safe transitions.
     */
    bool start(std::shared_ptr<TaskScheduler> task_scheduler, const std::string &initial);

    /**
     * @brief Stop the state machine
     *
     * @note This method is thread-safe and can be called multiple times safely.
     *       It will wait for all pending state tasks to complete before returning.
     */
    void stop();

    /**
     * @brief Trigger an action to cause a state transition
     *
     * @param action Action name
     * @param use_dispatch Whether to use 'dispatch' to trigger the action, default is false
     *                     if true, use 'dispatch' to trigger the action, otherwise use 'post'
     * @return true if action was queued successfully, false otherwise
     *
     * @note The actual transition happens asynchronously in the task scheduler's serial queue.
     *       This ensures thread-safe state transitions even when called from multiple threads.
     */
    bool trigger_action(const std::string &action, bool use_dispatch = false);

    /**
     * @brief Wait for all transitions to finish within a timeout
     *
     * @param timeout_ms Timeout in milliseconds (-1 for infinite)
     * @return true if all transitions finished within timeout, false otherwise
     */
    bool wait_all_transitions(uint32_t timeout_ms);

    /**
     * @brief Force all transitions to be cancelled immediately and then transition to a specific state immediately
     * @note This does not trigger any callback or state update, it just immediately switches to the target state.
     *
     * @param target_state Target state name
     * @return true if transition to target state successfully, false otherwise
     */
    bool force_transition_to(const std::string &target_state);

    /**
     * @brief Set the callback function to be called when a transition finishes
     *
     * @param callback Callback function to be called when a transition finishes
     *
     * @note The callback is invoked with (from_state, action, to_state) parameters.
     *       The callback will be executed without holding internal locks, so it's safe
     *       to perform any operations including triggering new actions.
     */
    void register_transition_finish_callback(TransitionFinishCallback callback)
    {
        boost::lock_guard lock(mutex_);
        transition_finish_callback_ = std::move(callback);
    }

    /**
     * @brief Check if the state machine is running
     *
     * @return true if running, false otherwise
     *
     * @note This method is thread-safe.
     */
    bool is_running() const
    {
        boost::shared_lock lock(mutex_);
        return (task_scheduler_ != nullptr);
    }

    /**
     * @brief Check if any transition is currently pending or being processed
     *
     * @return true if one or more transitions are queued or in progress, false otherwise
     *
     * @note This method is thread-safe.
     */
    bool has_transition_running() const
    {
        boost::shared_lock lock(mutex_);
        return (transition_count_ > 0);
    }

    /**
     * @brief Check if any state is currently updating
     *
     * @return true if any state is updating, false otherwise
     *
     * @note This method is thread-safe.
     */
    bool has_state_updating() const
    {
        boost::shared_lock lock(mutex_);
        return (update_task_id_ != 0);
    }

    /**
     * @brief Get the current state name
     *
     * @return Current state name (empty string if not running or not started)
     *
     * @note This method is thread-safe and returns a copy of the state name.
     */
    std::string get_current_state() const
    {
        boost::shared_lock lock(mutex_);
        return current_state_;
    }

    /**
     * @brief Get the state pointer by name
     *
     * @param name State name
     * @return State pointer (nullptr if not found)
     *
     * @note This method is thread-safe.
     */
    StatePtr get_state_ptr(const std::string &name) const
    {
        boost::shared_lock lock(mutex_);
        auto it = states_.find(name);
        return it != states_.end() ? it->second : nullptr;
    }

private:
    /**
     * @brief Setup periodic and timeout tasks for a state
     *
     * @param name State name
     * @return true on success, false on failure
     */
    bool setup_state_tasks(const std::string &name);

    /**
     * @brief Enter the initial state (called during start)
     *
     * @param name Initial state name
     * @return true on success, false on failure
     */
    bool enter_initial_state(const std::string &name);

    /**
     * @brief Perform state transition (called from trigger_action)
     *
     * @param next Next state name
     * @param action Action name that triggered the transition
     * @return true on success, false on failure (with rollback)
     */
    bool transition_to(const std::string &next, const std::string &action = "");

    /**
     * @brief Cancel current state's periodic and timeout tasks
     *
     * @note This method is thread-safe and can be called without holding the main lock.
     */
    void cancel_current_tasks();

    mutable boost::shared_mutex mutex_;  // Protects all member variables below

    // State management
    std::map<std::string, StatePtr> states_;                                // name -> state object
    std::map<std::string, std::map<std::string, std::string>> transitions_; // from_state -> (action -> to_state)
    std::string current_state_;                                             // Current active state name

    // Callbacks
    TransitionFinishCallback transition_finish_callback_;  // Called after successful transition

    // Task scheduler integration
    std::shared_ptr<TaskScheduler> task_scheduler_;  // Scheduler for async operations (nullptr when stopped)
    TaskScheduler::TaskId timeout_task_id_ = 0;      // Current state's timeout task ID
    TaskScheduler::TaskId update_task_id_ = 0;       // Current state's periodic update task ID
    std::string task_group_name_;                    // Task group name for serial execution
    uint32_t transition_count_ = 0;                  // Counter for pending/running transitions
};

} // namespace esp_brookesia::lib_utils
