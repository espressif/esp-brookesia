/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
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
#include <boost/thread/mutex.hpp>

namespace esp_brookesia::lib_utils {

/**
 * @brief Thread-safe Finite State Machine implementation
 *
 * Manages states, transitions, and state lifecycle with support for:
 * - State entry/exit guards (on_enter/on_exit)
 * - Periodic state updates (on_update)
 * - State timeouts with automatic event triggering
 * - Asynchronous state transitions via event queue
 * - Serial execution guarantee (no concurrent state transitions)
 * - Transition rollback on entry failure
 *
 * @note All state transitions are executed serially through the task scheduler's group mechanism,
 *       ensuring thread-safe operations even when events are triggered from multiple threads.
 *       State callbacks (on_enter, on_exit, on_update) are executed without holding internal locks,
 *       allowing them to safely trigger new events or perform blocking operations.
 */
class StateMachine {
public:
    using StatePtr = std::shared_ptr<StateBase>;
    using TransitionFinishCallback = std::function <
                                     void(const std::string &from, const std::string &event, const std::string &to)
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
     * @param event Event that triggers the transition
     * @param to Target state name
     * @return true if added successfully, false if transition already exists
     */
    bool add_transition(const std::string &from, const std::string &event, const std::string &to);

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
     * @brief Trigger an event to cause a state transition
     *
     * @param event Event name
     * @return true if event was queued successfully, false otherwise
     *
     * @note The actual transition happens asynchronously in the task scheduler's serial queue.
     *       This ensures thread-safe state transitions even when called from multiple threads.
     */
    bool trigger_event(const std::string &event);

    /**
     * @brief Set the callback function to be called when a transition finishes
     *
     * @param callback Callback function to be called when a transition finishes
     *
     * @note The callback is invoked with (from_state, event, to_state) parameters.
     *       The callback will be executed without holding internal locks, so it's safe
     *       to perform any operations including triggering new events.
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
        boost::lock_guard lock(mutex_);
        return (task_scheduler_ != nullptr);
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
        boost::lock_guard lock(mutex_);
        return current_state_;
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
     * @brief Perform state transition (called from trigger_event)
     *
     * @param next Next state name
     * @return true on success, false on failure (with rollback)
     */
    bool transition_to(const std::string &next);

    /**
     * @brief Cancel current state's periodic and timeout tasks
     *
     * @note This method is thread-safe and can be called without holding the main lock.
     */
    void cancel_current_tasks();

private:
    mutable boost::mutex mutex_;  // Protects all member variables below

    // State management
    std::map<std::string, StatePtr> states_;                                // name -> state object
    std::map<std::string, std::map<std::string, std::string>> transitions_; // from_state -> (event -> to_state)
    std::string current_state_;                                             // Current active state name

    // Callbacks
    TransitionFinishCallback transition_finish_callback_;  // Called after successful transition

    // Task scheduler integration
    std::shared_ptr<TaskScheduler> task_scheduler_;  // Scheduler for async operations (nullptr when stopped)
    TaskScheduler::TaskId timeout_task_id_ = 0;      // Current state's timeout task ID
    TaskScheduler::TaskId update_task_id_ = 0;       // Current state's periodic update task ID
    std::string task_group_name_;                    // Task group name for serial execution
};

} // namespace esp_brookesia::lib_utils
