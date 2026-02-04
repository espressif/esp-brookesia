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
#include "brookesia/agent_manager/base.hpp"

namespace esp_brookesia::agent {

using GeneralState = helper::Manager::GeneralState;

enum class ExtraAction {
    Success,
    Failed,
    Timeout,
    Max
};
BROOKESIA_DESCRIBE_ENUM(ExtraAction, Success, Failed, Timeout, Max);

class GeneralStateClass;

class StateMachine  {
public:
    friend class GeneralStateClass;

    StateMachine() = default;
    ~StateMachine();

    bool init();
    void deinit();
    bool start();
    void stop();

    bool is_initialized()
    {
        return is_initialized_;
    }
    bool is_running()
    {
        return is_running_;
    }

    bool do_time_sync();
    bool check_if_time_synced();

    bool trigger_general_action(GeneralAction action, bool use_dispatch = false);
    bool trigger_extra_action(ExtraAction action);

    bool force_transition_to(GeneralState state);

    GeneralState get_current_state();

    std::shared_ptr<Base> get_agent() const
    {
        return agent_;
    }

    bool is_transient_state();

    static GeneralState get_general_action_target_transient_state(GeneralAction action);
    static GeneralState get_general_action_target_stable_state(GeneralAction action);
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
