/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <functional>
#include <string>
#include <utility>
#include <vector>
#include <queue>
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/lib_utils/state_base.hpp"
#include "brookesia/lib_utils/state_machine.hpp"
#include "brookesia/service_helper/network/wifi.hpp"
#include "brookesia/service_wifi/hal/type.hpp"

namespace esp_brookesia::service::wifi {

using GeneralState = helper::Wifi::GeneralState;

/**
 * @brief Extra actions for state transitions in transient states.
 */
enum class ExtraAction {
    Success, ///< Action completed successfully.
    Failure, ///< Action failed.
    Timeout, ///< Action timed out.
    Max      ///< Sentinel value.
};

class GeneralStateClass;

/**
 * @brief Wi-Fi service general state machine.
 */
class StateMachine {
public:
    friend class GeneralStateClass;

    /**
     * @brief Handler invoked when a general action should be executed.
     */
    using GeneralActionHandler = std::function<bool(GeneralAction)>;

    /**
     * @brief Construct the Wi-Fi state machine.
     *
     * @param[in] action_handler Handler used to execute general actions.
     */
    StateMachine(GeneralActionHandler action_handler)
        : action_handler_(std::move(action_handler))
    {
    }

    /**
     * @brief Destroy the Wi-Fi state machine.
     */
    ~StateMachine();

    /**
     * @brief Initialize state-machine resources.
     *
     * @return `true` on success; otherwise `false`.
     */
    bool init();

    /**
     * @brief Deinitialize state-machine resources.
     */
    void deinit();

    /**
     * @brief Start state-machine execution.
     *
     * @return `true` on success; otherwise `false`.
     */
    bool start();

    /**
     * @brief Stop state-machine execution.
     */
    void stop();

    /**
     * @brief Check whether the state machine has been initialized.
     *
     * @return `true` if initialized; otherwise `false`.
     */
    bool is_inited()
    {
        return state_machine_ != nullptr;
    }

    /**
     * @brief Check whether the state machine is running.
     *
     * @return `true` if running; otherwise `false`.
     */
    bool is_running()
    {
        return (state_machine_ != nullptr) && state_machine_->is_running();
    }

    /**
     * @brief Trigger a general Wi-Fi action.
     *
     * @param[in] action General action to trigger.
     * @return `true` on success; otherwise `false`.
     */
    bool trigger_general_action(GeneralAction action);

    /**
     * @brief Trigger an extra transition action.
     *
     * @param[in] action Extra action to trigger.
     * @return `true` on success; otherwise `false`.
     */
    bool trigger_extra_action(ExtraAction action);

    /**
     * @brief Force the state machine to transition to a state.
     *
     * @param[in] state Target general state.
     * @return `true` on success; otherwise `false`.
     */
    bool force_transition_to(GeneralState state);

    /**
     * @brief Get the current general state.
     *
     * @return Current general state.
     */
    GeneralState get_current_state();

    /**
     * @brief Check whether the current state is transient.
     *
     * @return `true` if the current state is transient; otherwise `false`.
     */
    bool is_transient_state();

    /**
     * @brief Check whether any action transition is running.
     *
     * @return `true` if an action or transition is running; otherwise `false`.
     */
    bool is_action_running()
    {
        return is_transient_state() || state_machine_->has_transition_running();
    }

    /**
     * @brief Get the transient target state for a general action.
     *
     * @param[in] action General action.
     * @return Target transient state.
     */
    static GeneralState get_general_action_target_transient_state(GeneralAction action);

    /**
     * @brief Get the stable target state for a general action.
     *
     * @param[in] action General action.
     * @return Target stable state.
     */
    static GeneralState get_general_action_target_stable_state(GeneralAction action);

    /**
     * @brief Get the target state for a general event.
     *
     * @param[in] event General event.
     * @return Target state.
     */
    static GeneralState get_general_event_target_state(GeneralEvent event);

private:
    GeneralActionHandler action_handler_;
    std::unique_ptr<lib_utils::StateMachine> state_machine_;
    std::vector<std::shared_ptr<GeneralStateClass>> state_classes_;
};

BROOKESIA_DESCRIBE_ENUM(
    GeneralState, Idle, Initing, Inited, Deiniting, Starting, Started, Stopping,
    Connecting, Connected, Disconnecting, Max
);
BROOKESIA_DESCRIBE_ENUM(ExtraAction, Success, Failure, Timeout, Max);

} // namespace esp_brookesia::service::wifi
