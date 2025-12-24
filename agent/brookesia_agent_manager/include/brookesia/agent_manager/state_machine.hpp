/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <string>
#include <vector>
#include <queue>
#include "brookesia/lib_utils/state_machine.hpp"
#include "brookesia/agent_manager/base.hpp"

namespace esp_brookesia::agent {

using GeneralState = service::helper::AgentManager::GeneralState;

enum class ExtraAction {
    EventGot,
    Timeout,
    Failed,
    Max
};
BROOKESIA_DESCRIBE_ENUM(ExtraAction, EventGot, Timeout, Failed, Max);

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
    GeneralAction pop_general_action_queue_front();

    std::shared_ptr<Base> get_agent() const
    {
        return agent_;
    }

    lib_utils::TaskScheduler::Group get_group() const
    {
        return std::string(service::helper::AgentManager::get_name().data()) + "_state_machine";
    }

    bool is_transient_state();

private:
    bool is_initialized_ = false;
    bool is_running_ = false;

    std::unique_ptr<lib_utils::StateMachine> state_machine_;
    std::shared_ptr<Base> agent_;
    std::vector<std::shared_ptr<GeneralStateClass>> state_classes_;

    std::queue<GeneralAction> general_action_queue_;
};

} // namespace esp_brookesia::agent
