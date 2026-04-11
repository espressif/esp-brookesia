/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <cstdint>
#include "boost/thread/shared_mutex.hpp"
#include "brookesia/lib_utils/macro_configs.h"
#include "brookesia/lib_utils/state_base.hpp"
#include "brookesia/lib_utils/task_scheduler.hpp"

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
    /**
     * @brief Shared pointer type used for registered states.
     */
    using StatePtr = std::shared_ptr<StateBase>;
    /**
     * @brief Callback type invoked after a transition completes successfully.
     */
    using TransitionFinishCallback = std::function <
                                     void(const std::string &from, const std::string &action, const std::string &to)
                                     >;

    /**
     * @brief Default scheduler group name used by the state machine.
     */
    static constexpr const char *DEFAULT_TASK_GROUP_NAME = "state_machine";

    /**
     * @brief Configuration for the state machine.
     */
    struct Config {
        /**
         * @brief Task scheduler used to serialize transitions, timeouts, and periodic updates.
         */
        std::shared_ptr<TaskScheduler> task_scheduler;
        /**
         * @brief Task group name used for serialized state-machine tasks.
         */
        std::string task_group_name = DEFAULT_TASK_GROUP_NAME;
        /**
         * @brief Task group configuration.
         */
        TaskScheduler::GroupConfig task_group_config = {
            .enable_serial_execution = true,
        };
        /**
         * @brief Initial state name.
         */
        std::string initial_state;
    };

    /**
     * @brief Constructor
     */
    StateMachine() = default;

    /**
     * @brief Stop the state machine and release owned resources.
     */
    ~StateMachine();

    /**
     * @brief Register a named state object.
     *
     * @param state Shared pointer to the state implementation.
     * @return `true` if the state was added, or `false` when a state with the same name already exists.
     */
    bool add_state(StatePtr state);

    /**
     * @brief Register a transition between two states.
     *
     * @param from Source state name.
     * @param action Action name that triggers the transition.
     * @param to Target state name.
     * @return `true` if the transition was added, or `false` when the same `(from, action)` mapping already exists.
     */
    bool add_transition(const std::string &from, const std::string &action, const std::string &to);

    /**
     * @brief Start the state machine and enter the initial state.
     *
     * @param config Configuration for the state machine.
     * @return `true` on success, or `false` if the initial state cannot be entered or startup fails.
     *
     * @note If the state machine is already running, this returns true immediately.
     *       If the scheduler is not running, this will attempt to start it.
     *       The task group is automatically configured for serial execution to ensure thread-safe transitions.
     */
    bool start(Config config);

    /**
     * @brief Stop the state machine and wait for pending scheduled work to finish.
     *
     * @note This method is thread-safe and can be called multiple times safely.
     *       It will wait for all pending state tasks to complete before returning.
     */
    void stop();

    /**
     * @brief Queue or dispatch a transition action.
     *
     * @param action Transition action name.
     * @param use_dispatch Set to `true` to call `TaskScheduler::dispatch()` instead of `post()`.
     * @return `true` if the action was scheduled successfully, or `false` otherwise.
     *
     * @note The actual transition happens asynchronously in the task scheduler's serial queue.
     *       This ensures thread-safe state transitions even when called from multiple threads.
     */
    bool trigger_action(const std::string &action, bool use_dispatch = false);

    /**
     * @brief Wait until no transitions are queued or running.
     *
     * @param timeout_ms Timeout in milliseconds. The special value `static_cast<uint32_t>(-1)` waits indefinitely.
     * @return `true` if all transitions completed before the timeout, or `false` otherwise.
     */
    bool wait_all_transitions(uint32_t timeout_ms);

    /**
     * @brief Cancel scheduled state tasks and overwrite the current state name immediately.
     *
     * @note This bypasses `on_exit()`, `on_enter()`, timeout scheduling, periodic updates, and transition callbacks.
     *
     * @param target_state State name assigned as the new current state.
     * @return `true` once the internal state name has been updated.
     */
    bool force_transition_to(const std::string &target_state);

    /**
     * @brief Register a callback invoked after a successful transition.
     *
     * @param callback Callback invoked with `(from_state, action, to_state)`.
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
     * @brief Check whether the state machine has been started.
     *
     * @return `true` when the machine owns a scheduler and is running, or `false` otherwise.
     *
     * @note This method is thread-safe.
     */
    bool is_running() const
    {
        boost::shared_lock lock(mutex_);
        return (task_scheduler_ != nullptr);
    }

    /**
     * @brief Check whether any transitions are queued or currently executing.
     *
     * @return `true` when one or more transitions are pending or in progress, or `false` otherwise.
     *
     * @note This method is thread-safe.
     */
    bool has_transition_running() const
    {
        boost::shared_lock lock(mutex_);
        return (transition_count_ > 0);
    }

    /**
     * @brief Check whether the current state has an active periodic update task.
     *
     * @return `true` when a periodic update task is installed, or `false` otherwise.
     *
     * @note This method is thread-safe.
     */
    bool has_state_updating() const
    {
        boost::shared_lock lock(mutex_);
        return (update_task_id_ != 0);
    }

    /**
     * @brief Get the name of the current state.
     *
     * @return Current state name, or an empty string when the machine has not started.
     *
     * @note This method is thread-safe and returns a copy of the state name.
     */
    std::string get_current_state() const
    {
        boost::shared_lock lock(mutex_);
        return current_state_;
    }

    /**
     * @brief Look up a registered state by name.
     *
     * @param name State name to look up.
     * @return Shared pointer to the registered state, or `nullptr` if not found.
     *
     * @note This method is thread-safe.
     */
    StatePtr get_state_ptr(const std::string &name) const
    {
        boost::shared_lock lock(mutex_);
        return find_state_unlocked(name);
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

    /**
     * @brief Look up a state by name without acquiring the mutex.
     *
     * @param name State name to search for.
     * @return Shared pointer to the matching state, or `nullptr` if not found.
     *
     * @note The caller must hold `mutex_` (shared or exclusive) before calling this method.
     */
    StatePtr find_state_unlocked(const std::string &name) const
    {
        auto it = std::find_if(states_.begin(), states_.end(), [&name](const StatePtr & s) {
            return s->get_name() == name;
        });
        return it != states_.end() ? *it : nullptr;
    }

    mutable boost::shared_mutex mutex_;  // Protects all member variables below

    // State management
    std::vector<StatePtr> states_;                                          // ordered list of registered states
    std::map<std::string, std::map<std::string, std::string>> transitions_; // from_state -> (action -> to_state)
    std::string current_state_;                                             // Current active state name

    // Callbacks
    TransitionFinishCallback transition_finish_callback_;  // Called after successful transition

    // Task scheduler integration
    std::shared_ptr<TaskScheduler> task_scheduler_;  // Scheduler for async operations (nullptr when stopped)
    std::string task_group_name_;                    // Task group name for serial execution
    TaskScheduler::TaskId timeout_task_id_ = 0;      // Current state's timeout task ID
    TaskScheduler::TaskId update_task_id_ = 0;       // Current state's periodic update task ID
    uint32_t transition_count_ = 0;                  // Counter for pending/running transitions
};

} // namespace esp_brookesia::lib_utils
