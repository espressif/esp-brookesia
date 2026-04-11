/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <string>
#include <vector>
#include <queue>
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/lib_utils/state_base.hpp"
#include "brookesia/lib_utils/state_machine.hpp"
#include "brookesia/service_helper/wifi.hpp"
#include "brookesia/service_wifi/hal/hal.hpp"

namespace esp_brookesia::service::wifi {

using GeneralState = helper::Wifi::GeneralState;

/**
 * @brief Extra actions for state transitions in transient states
 */
enum class ExtraAction {
    Success,    // Action completed successfully
    Failure,     // Action failed
    Timeout,    // Action timed out
    Max
};

class GeneralStateClass;

class StateMachine  {
public:
    friend class GeneralStateClass;

    StateMachine(std::shared_ptr<Hal> hal)
        : hal_(hal)
    {}
    ~StateMachine();

    bool init();
    void deinit();
    bool start();
    void stop();

    bool is_inited()
    {
        return state_machine_ != nullptr;
    }
    bool is_running()
    {
        return (state_machine_ != nullptr) && state_machine_->is_running();
    }

    bool trigger_general_action(GeneralAction action);
    bool trigger_extra_action(ExtraAction action);
    bool force_transition_to(GeneralState state);

    GeneralState get_current_state();

    std::shared_ptr<Hal> get_hal() const
    {
        return hal_;
    }

    bool is_transient_state();
    bool is_action_running()
    {
        return  is_transient_state() || state_machine_->has_transition_running();
    }

    static GeneralState get_general_action_target_transient_state(GeneralAction action);
    static GeneralState get_general_action_target_stable_state(GeneralAction action);
    static GeneralState get_general_event_target_state(GeneralEvent event);

private:
    std::shared_ptr<Hal> hal_;
    std::unique_ptr<lib_utils::StateMachine> state_machine_;
    std::vector<std::shared_ptr<GeneralStateClass>> state_classes_;
};

BROOKESIA_DESCRIBE_ENUM(
    GeneralState, Idle, Initing, Inited, Deiniting, Starting, Started, Stopping,
    Connecting, Connected, Disconnecting, Max
);
BROOKESIA_DESCRIBE_ENUM(ExtraAction, Success, Failure, Timeout, Max);

} // namespace esp_brookesia::service::wifi
