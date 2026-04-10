/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <string>
#include <vector>
#include <queue>
#include "brookesia/lib_utils/state_machine.hpp"
#include "brookesia/lib_utils/task_scheduler.hpp"
#include "brookesia/agent_manager/macro_configs.h"
#include "brookesia/agent_manager/base.hpp"

namespace esp_brookesia::agent {

/**
 * @brief Re-exported stable and transient agent states.
 */
using GeneralState = helper::Manager::GeneralState;

/**
 * @brief Auxiliary actions used internally to finish state transitions.
 */
enum class ExtraAction {
    Success,
    Failed,
    Timeout,
    Max
};
BROOKESIA_DESCRIBE_ENUM(ExtraAction, Success, Failed, Timeout, Max);

class GeneralStateClass;

/**
 * @brief Coordinates high-level agent lifecycle transitions.
 */
class StateMachine  {
public:
    friend class GeneralStateClass;

    StateMachine() = default;
    ~StateMachine();

    /**
     * @brief Initialize the state machine for the current active agent.
     *
     * @return true if initialization succeeds.
     */
    bool init();
    /**
     * @brief Deinitialize the state machine.
     */
    void deinit();
    /**
     * @brief Start background processing for queued agent actions.
     *
     * @return true if the state machine starts successfully.
     */
    bool start();
    /**
     * @brief Stop background processing for queued agent actions.
     */
    void stop();

    bool is_initialized()
    {
        return is_initialized_;
    }
    bool is_running()
    {
        return is_running_;
    }

    /**
     * @brief Trigger time synchronization flow when required by the active agent.
     *
     * @return true if the transition request is accepted.
     */
    bool do_time_sync();
    /**
     * @brief Check whether time synchronization is already satisfied.
     *
     * @return true if time sync is not required or has already completed.
     */
    bool check_if_time_synced();

    /**
     * @brief Queue a high-level agent action.
     *
     * @param[in] action Action to perform.
     * @param[in] use_dispatch Whether to dispatch through the scheduler asynchronously.
     * @return true if the action request is accepted.
     */
    bool trigger_general_action(GeneralAction action, bool use_dispatch = false);
    /**
     * @brief Inject an auxiliary success/failure/timeout action.
     *
     * @param[in] action Auxiliary action to trigger.
     * @return true if the action request is accepted.
     */
    bool trigger_extra_action(ExtraAction action);

    /**
     * @brief Force the state machine into a specific state.
     *
     * @param[in] state Target state.
     * @return true if the forced transition succeeds.
     */
    bool force_transition_to(GeneralState state);

    /**
     * @brief Get the current state.
     *
     * @return GeneralState Current state-machine state.
     */
    GeneralState get_current_state();

    std::shared_ptr<Base> get_agent() const
    {
        return agent_;
    }

    /**
     * @brief Check whether the current state is transient.
     *
     * @return true if the current state represents an in-flight transition.
     */
    bool is_transient_state();

    /**
     * @brief Map an action to the transient state entered while it runs.
     *
     * @param[in] action General action to inspect.
     * @return GeneralState Corresponding transient state.
     */
    static GeneralState get_general_action_target_transient_state(GeneralAction action);
    /**
     * @brief Map an action to the stable state expected after success.
     *
     * @param[in] action General action to inspect.
     * @return GeneralState Target stable state.
     */
    static GeneralState get_general_action_target_stable_state(GeneralAction action);
    /**
     * @brief Map a completion event to its resulting state.
     *
     * @param[in] event General event to inspect.
     * @return GeneralState Resulting state.
     */
    static GeneralState get_general_event_target_state(GeneralEvent event);

private:
    bool is_initialized_ = false;
    bool is_running_ = false;

    std::shared_ptr<lib_utils::TaskScheduler> get_task_scheduler();

    bool is_action_running();
    bool is_general_action_queue_task_running();
    GeneralAction pop_general_action_queue_front();
    bool start_general_action_queue_task();
    void stop_general_action_queue_task();

    std::unique_ptr<lib_utils::StateMachine> state_machine_;
    std::shared_ptr<Base> agent_;
    std::vector<std::shared_ptr<GeneralStateClass>> state_classes_;

    std::queue<GeneralAction> general_action_queue_;
    lib_utils::TaskScheduler::TaskId  general_action_queue_task_ = 0;
};

} // namespace esp_brookesia::agent
