/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/service_wifi/macro_configs.h"
#if !defined(BROOKESIA_SERVICE_WIFI_STATE_MACHINE_ENABLE_DEBUG_LOG)
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/lib_utils/function_guard.hpp"
#include "brookesia/service_wifi/state_machine.hpp"

namespace esp_brookesia::service::wifi {

constexpr uint32_t WAIT_STATE_MACHINE_FINISHED_TIMEOUT_MS =
    BROOKESIA_SERVICE_WIFI_STATE_MACHINE_WAIT_STATE_MACHINE_FINISHED_TIMEOUT_MS;

class GeneralStateClass : public lib_utils::StateBase {
public:
    using Helper = helper::Wifi;

    GeneralStateClass(std::shared_ptr<Hal> hal, GeneralState state)
        : lib_utils::StateBase()
        , hal_(hal)
        , state_(state)
    {}
    ~GeneralStateClass() = default;

    bool on_enter(const std::string &from_state, const std::string &action) override
    {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

        BROOKESIA_LOGD("Params: from_state(%1%), action(%2%)", from_state, action);

        if (from_state.empty() || (from_state == BROOKESIA_DESCRIBE_TO_STR(state_))) {
            BROOKESIA_LOGD("Skip operation");
            return true;
        }

        GeneralAction action_enum;
        BROOKESIA_CHECK_FALSE_RETURN(
            BROOKESIA_DESCRIBE_STR_TO_ENUM(action, action_enum), false, "Invalid action: %1%", action
        );
        if (!hal_->do_general_action(action_enum)) {
            BROOKESIA_LOGE("Do general action %1% in %2% state failed", action, from_state);
            return false;
        }

        return true;
    }

    bool on_exit(const std::string &to_state, const std::string &action) override
    {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

        BROOKESIA_LOGD("Params: to_state(%1%), action(%2%)", to_state, action);

        GeneralAction action_enum;
        BROOKESIA_CHECK_FALSE_RETURN(
            BROOKESIA_DESCRIBE_STR_TO_ENUM(action, action_enum), false, "Invalid action: %1%", action
        );
        if (!hal_->do_general_action(action_enum)) {
            BROOKESIA_LOGE("Do general action %1% to %2% state failed", action, to_state);
            return false;
        }

        return true;
    }

private:
    std::shared_ptr<Hal> hal_;
    GeneralState state_;
};

StateMachine::~StateMachine()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    deinit();
}

bool StateMachine::init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_inited()) {
        BROOKESIA_LOGD("Already initialized");
        return true;
    }

    lib_utils::FunctionGuard deinit_guard([this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        deinit();
    });

    /* Create state machine */
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        state_machine_ = std::make_unique<lib_utils::StateMachine>(service::helper::Wifi::get_name().data()), false,
        "Failed to create state machine"
    );

    /* Create states and add to state machine */
    auto state_num_max = BROOKESIA_DESCRIBE_ENUM_TO_NUM(GeneralState::Max);
    for (size_t state_num = 0; state_num < state_num_max; state_num++) {
        GeneralState state_enum;
        BROOKESIA_CHECK_FALSE_RETURN(
            BROOKESIA_DESCRIBE_NUM_TO_ENUM(state_num, state_enum), false,
            "Failed to convert number %1% to enum", state_num
        );

        auto state_str = BROOKESIA_DESCRIBE_TO_STR(state_enum);
        std::shared_ptr<GeneralStateClass> state_ptr;
        BROOKESIA_CHECK_EXCEPTION_RETURN(
            state_ptr = std::make_shared<GeneralStateClass>(hal_, state_enum), false,
            "Failed to create state %1%", state_str
        );
        BROOKESIA_CHECK_FALSE_RETURN(
            state_machine_->add_state(state_str, state_ptr), false, "Failed to add state %1%", state_str
        );
    }

    /* Add transitions */
    /* Main flow (normal business forward path) */
    // Deinited --> Inited: Init
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(
            BROOKESIA_DESCRIBE_TO_STR(GeneralState::Deinited),
            BROOKESIA_DESCRIBE_TO_STR(GeneralAction::Init),
            BROOKESIA_DESCRIBE_TO_STR(GeneralState::Inited)
        ), false, "Failed to add transition: Deinited -> Init -> Inited"
    );
    // Inited --> Started: Start
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(
            BROOKESIA_DESCRIBE_TO_STR(GeneralState::Inited),
            BROOKESIA_DESCRIBE_TO_STR(GeneralAction::Start),
            BROOKESIA_DESCRIBE_TO_STR(GeneralState::Started)
        ), false, "Failed to add transition: Inited -> Start -> Started"
    );
    // Started --> Connected: Connect
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(
            BROOKESIA_DESCRIBE_TO_STR(GeneralState::Started),
            BROOKESIA_DESCRIBE_TO_STR(GeneralAction::Connect),
            BROOKESIA_DESCRIBE_TO_STR(GeneralState::Connected)
        ), false, "Failed to add transition: Started -> Connect -> Connected"
    );
    // Connected --> Started: Disconnect
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(
            BROOKESIA_DESCRIBE_TO_STR(GeneralState::Connected),
            BROOKESIA_DESCRIBE_TO_STR(GeneralAction::Disconnect),
            BROOKESIA_DESCRIBE_TO_STR(GeneralState::Started)
        ), false, "Failed to add transition: Connected -> Disconnect -> Started"
    );

    // Stop/rollback/terminate
    // Started --> Inited: Stop
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(
            BROOKESIA_DESCRIBE_TO_STR(GeneralState::Started),
            BROOKESIA_DESCRIBE_TO_STR(GeneralAction::Stop),
            BROOKESIA_DESCRIBE_TO_STR(GeneralState::Inited)
        ), false, "Failed to add transition: Started -> Stop -> Inited"
    );
    // Connected --> Inited: Stop
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(
            BROOKESIA_DESCRIBE_TO_STR(GeneralState::Connected),
            BROOKESIA_DESCRIBE_TO_STR(GeneralAction::Stop),
            BROOKESIA_DESCRIBE_TO_STR(GeneralState::Inited)
        ), false, "Failed to add transition: Connected -> Stop -> Inited"
    );
    // Inited --> Deinited: Deinit
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(
            BROOKESIA_DESCRIBE_TO_STR(GeneralState::Inited),
            BROOKESIA_DESCRIBE_TO_STR(GeneralAction::Deinit),
            BROOKESIA_DESCRIBE_TO_STR(GeneralState::Deinited)
        ), false, "Failed to add transition: Inited -> Deinit -> Deinited"
    );

    /* Enable self-transitions */
    // Deinited --> Deinited: Deinit
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(
            BROOKESIA_DESCRIBE_TO_STR(GeneralState::Deinited),
            BROOKESIA_DESCRIBE_TO_STR(GeneralAction::Deinit),
            BROOKESIA_DESCRIBE_TO_STR(GeneralState::Deinited)
        ), false, "Failed to add transition: Deinited -> Deinit -> Deinited"
    );
    // Inited --> Inited: Init
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(
            BROOKESIA_DESCRIBE_TO_STR(GeneralState::Inited),
            BROOKESIA_DESCRIBE_TO_STR(GeneralAction::Init),
            BROOKESIA_DESCRIBE_TO_STR(GeneralState::Inited)
        ), false, "Failed to add transition: Inited -> Init -> Inited"
    );
    // Started --> Started: Start
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(
            BROOKESIA_DESCRIBE_TO_STR(GeneralState::Started),
            BROOKESIA_DESCRIBE_TO_STR(GeneralAction::Start),
            BROOKESIA_DESCRIBE_TO_STR(GeneralState::Started)
        ), false, "Failed to add transition: Started -> Start -> Started"
    );
    // Connected --> Connected: Connect
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(
            BROOKESIA_DESCRIBE_TO_STR(GeneralState::Connected),
            BROOKESIA_DESCRIBE_TO_STR(GeneralAction::Connect),
            BROOKESIA_DESCRIBE_TO_STR(GeneralState::Connected)
        ), false, "Failed to add transition: Connected -> Connect -> Connected"
    );

    deinit_guard.release();

    BROOKESIA_LOGI("State machine initialized");

    return true;
}

void StateMachine::deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_inited()) {
        BROOKESIA_LOGD("Not initialized");
        return;
    }

    state_machine_.reset();
}

bool StateMachine::start()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_running()) {
        BROOKESIA_LOGD("Already running");
        return true;
    }

    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->start(
            task_scheduler_, BROOKESIA_DESCRIBE_TO_STR(GeneralState::Deinited)), false,
        "Failed to start state machine"
    );

    BROOKESIA_LOGI("State machine started");

    return true;
}

void StateMachine::stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_running()) {
        BROOKESIA_LOGD("Not running");
        return;
    }

    BROOKESIA_CHECK_FALSE_EXECUTE(trigger_general_action(GeneralAction::Deinit), {}, {
        BROOKESIA_LOGE("Failed to trigger deinit action when stopping state machine");
    });
    if (!state_machine_->wait_all_transitions(WAIT_STATE_MACHINE_FINISHED_TIMEOUT_MS)) {
        BROOKESIA_LOGW(
            "Wait for all transitions to be cancelled within timeout %1% ms, force transition to deinited state",
            WAIT_STATE_MACHINE_FINISHED_TIMEOUT_MS
        );
        BROOKESIA_CHECK_FALSE_EXECUTE(force_transition_to(BROOKESIA_DESCRIBE_TO_STR(GeneralState::Deinited)), {}, {
            BROOKESIA_LOGE("Failed to force transition to deinited state when stopping state machine");
        });
    }
    state_machine_->stop();
}

bool StateMachine::trigger_general_action(GeneralAction action, bool use_dispatch)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: action(%1%)", BROOKESIA_DESCRIBE_TO_STR(action));

    BROOKESIA_CHECK_FALSE_RETURN(is_running(), false, "Not running");

    GeneralAction pre_action;
    bool need_pre_action = false;

    /* Handle special cases */
    switch (action) {
    case GeneralAction::Deinit:
        if (hal_->is_general_event_ready(GeneralEvent::Started)) {
            BROOKESIA_LOGD("WiFi is started, trigger stop action first");
            pre_action = GeneralAction::Stop;
            need_pre_action = true;
        }
        break;
    case GeneralAction::Start:
        if (hal_->is_general_event_ready(GeneralEvent::Deinited)) {
            BROOKESIA_LOGD("WiFi is deinited, trigger init action first");
            pre_action = GeneralAction::Init;
            need_pre_action = true;
        }
        break;
    case GeneralAction::Connect:
        if (hal_->is_general_event_ready(GeneralEvent::Stopped)) {
            BROOKESIA_LOGD("WiFi is stopped, trigger start action first");
            pre_action = GeneralAction::Start;
            need_pre_action = true;
        } else if (hal_->is_general_event_ready(GeneralEvent::Connected)) {
            BROOKESIA_LOGD("WiFi is connected, trigger disconnect action first");
            pre_action = GeneralAction::Disconnect;
            need_pre_action = true;
        }
        break;
    default:
        break;
    }

    if (need_pre_action) {
        BROOKESIA_LOGD("Recursive trigger pre action: %1%", BROOKESIA_DESCRIBE_TO_STR(pre_action));
        BROOKESIA_CHECK_FALSE_RETURN(
            trigger_general_action(pre_action, use_dispatch), false, "Failed to trigger pre action: %1%",
            BROOKESIA_DESCRIBE_TO_STR(pre_action)
        );
    }

    // Trigger target action
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->trigger_action(BROOKESIA_DESCRIBE_TO_STR(action), use_dispatch), false,
        "Failed to trigger target action: %1%", BROOKESIA_DESCRIBE_TO_STR(action)
    );

    return true;
}

bool StateMachine::force_transition_to(const std::string &state)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: state(%1%)", state);

    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->force_transition_to(state), false, "Failed to force transition to %1% state", state
    );

    return true;
}

} // namespace esp_brookesia::service::wifi
