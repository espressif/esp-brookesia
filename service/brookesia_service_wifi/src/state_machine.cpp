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
#include "brookesia/service_wifi/service_wifi.hpp"
#include "brookesia/lib_utils/function_guard.hpp"
#include "brookesia/service_wifi/state_machine.hpp"

namespace esp_brookesia::service::wifi {

class GeneralStateClass : public lib_utils::StateBase {
public:
    using Helper = helper::Wifi;

    GeneralStateClass(StateMachine &context, GeneralState state)
        : lib_utils::StateBase(BROOKESIA_DESCRIBE_TO_STR(state))
        , context_(context)
        , state_(state)
    {}
    ~GeneralStateClass() = default;

    bool on_enter(const std::string &from_state, const std::string &action) override;

    bool is_transient() const
    {
        return ((state_ == GeneralState::Initing) || (state_ == GeneralState::Deiniting) ||
                (state_ == GeneralState::Starting) || (state_ == GeneralState::Stopping) ||
                (state_ == GeneralState::Connecting) || (state_ == GeneralState::Disconnecting));
    }

private:
    GeneralAction get_transient_state_target_action();
    GeneralEvent get_transient_state_target_event();

    StateMachine &context_;
    GeneralState state_ = GeneralState::Idle;
};

bool GeneralStateClass::on_enter(const std::string &from_state, const std::string &action)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: from_state(%1%), action(%2%)", from_state, action);

    if (from_state == BROOKESIA_DESCRIBE_TO_STR(state_) || from_state.empty() || action == "") {
        BROOKESIA_LOGD("Skip self state, empty from_state or empty action");
        return true;
    }

    if (!is_transient()) {
        BROOKESIA_LOGD("Current state '%1%' is not a transient state, skip", BROOKESIA_DESCRIBE_TO_STR(state_));
        return true;
    }

    auto target_action = get_transient_state_target_action();
    BROOKESIA_CHECK_FALSE_RETURN(target_action != GeneralAction::Max, false, "Invalid target action");

    auto hal = context_.get_hal();
    BROOKESIA_CHECK_NULL_RETURN(hal, false, "HAL is not set");
    if (!hal->do_general_action(target_action)) {
        BROOKESIA_LOGE(
            "Do general action '%1%' in '%2%' state failed", BROOKESIA_DESCRIBE_TO_STR(target_action),
            BROOKESIA_DESCRIBE_TO_STR(state_)
        );
        // Here we need to enter the transient state and wait for `Timeout` or `Failure` event to trigger
    }

    return true;
}

GeneralAction GeneralStateClass::get_transient_state_target_action()
{
    switch (state_) {
    case GeneralState::Initing:
        return GeneralAction::Init;
    case GeneralState::Deiniting:
        return GeneralAction::Deinit;
    case GeneralState::Starting:
        return GeneralAction::Start;
    case GeneralState::Stopping:
        return GeneralAction::Stop;
    case GeneralState::Connecting:
        return GeneralAction::Connect;
    case GeneralState::Disconnecting:
        return GeneralAction::Disconnect;
    default:
        return GeneralAction::Max;
    }
}

GeneralEvent GeneralStateClass::get_transient_state_target_event()
{
    switch (state_) {
    case GeneralState::Initing:
        return GeneralEvent::Inited;
    case GeneralState::Deiniting:
        return GeneralEvent::Deinited;
    case GeneralState::Starting:
        return GeneralEvent::Started;
    case GeneralState::Stopping:
        return GeneralEvent::Stopped;
    case GeneralState::Connecting:
        return GeneralEvent::Connected;
    case GeneralState::Disconnecting:
        return GeneralEvent::Disconnected;
    default:
        return GeneralEvent::Max;
    }
}

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
        state_machine_ = std::make_unique<lib_utils::StateMachine>(),
        false, "Failed to create state machine"
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
            state_ptr = std::make_shared<GeneralStateClass>(*this, state_enum), false,
            "Failed to create state %1%", state_str
        );
        BROOKESIA_CHECK_FALSE_RETURN(
            state_machine_->add_state(state_ptr), false, "Failed to add state %1%", state_str
        );
        state_classes_.push_back(state_ptr);
    }

    /* Get action strings */
    auto action_init = BROOKESIA_DESCRIBE_TO_STR(GeneralAction::Init);
    auto action_deinit = BROOKESIA_DESCRIBE_TO_STR(GeneralAction::Deinit);
    auto action_start = BROOKESIA_DESCRIBE_TO_STR(GeneralAction::Start);
    auto action_stop = BROOKESIA_DESCRIBE_TO_STR(GeneralAction::Stop);
    auto action_connect = BROOKESIA_DESCRIBE_TO_STR(GeneralAction::Connect);
    auto action_disconnect = BROOKESIA_DESCRIBE_TO_STR(GeneralAction::Disconnect);
    auto action_success = BROOKESIA_DESCRIBE_TO_STR(ExtraAction::Success);
    auto action_timeout = BROOKESIA_DESCRIBE_TO_STR(ExtraAction::Timeout);
    auto action_failed = BROOKESIA_DESCRIBE_TO_STR(ExtraAction::Failure);

    /* Get state strings */
    auto state_idle = BROOKESIA_DESCRIBE_TO_STR(GeneralState::Idle);
    auto state_initing = BROOKESIA_DESCRIBE_TO_STR(GeneralState::Initing);
    auto state_inited = BROOKESIA_DESCRIBE_TO_STR(GeneralState::Inited);
    auto state_deiniting = BROOKESIA_DESCRIBE_TO_STR(GeneralState::Deiniting);
    auto state_starting = BROOKESIA_DESCRIBE_TO_STR(GeneralState::Starting);
    auto state_started = BROOKESIA_DESCRIBE_TO_STR(GeneralState::Started);
    auto state_stopping = BROOKESIA_DESCRIBE_TO_STR(GeneralState::Stopping);
    auto state_connecting = BROOKESIA_DESCRIBE_TO_STR(GeneralState::Connecting);
    auto state_connected = BROOKESIA_DESCRIBE_TO_STR(GeneralState::Connected);
    auto state_disconnecting = BROOKESIA_DESCRIBE_TO_STR(GeneralState::Disconnecting);

    /* Add transitions */
    /* ===================== Idle ===================== */
    // Idle --> Initing: Init
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_idle, action_init, state_initing), false,
        "Failed to add transition: Idle -> Init -> Initing"
    );

    /* ===================== Initing ===================== */
    // Initing --> Inited: Success
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_initing, action_success, state_inited), false,
        "Failed to add transition: Initing -> Success -> Inited"
    );
    // Initing --> Idle: Failure
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_initing, action_failed, state_idle), false,
        "Failed to add transition: Initing -> Failed -> Idle"
    );
    // Initing --> Idle: Timeout
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_initing, action_timeout, state_deiniting), false,
        "Failed to add transition: Initing -> Timeout -> Deiniting"
    );

    /* ===================== Inited ===================== */
    // Inited --> Starting: Start
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_inited, action_start, state_starting), false,
        "Failed to add transition: Inited -> Start -> Starting"
    );
    // Inited --> Deiniting: Deinit
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_inited, action_deinit, state_deiniting), false,
        "Failed to add transition: Inited -> Deinit -> Deiniting"
    );

    /* ===================== Deiniting ===================== */
    // Deiniting --> Idle: Success / Failure / Timeout
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_deiniting, action_success, state_idle), false,
        "Failed to add transition: Deiniting -> Success -> Idle"
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_deiniting, action_failed, state_idle), false,
        "Failed to add transition: Deiniting -> Failed -> Idle"
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_deiniting, action_timeout, state_idle), false,
        "Failed to add transition: Deiniting -> Timeout -> Idle"
    );

    /* ===================== Starting ===================== */
    // Starting --> Started: Success
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_starting, action_success, state_started), false,
        "Failed to add transition: Starting -> Success -> Started"
    );
    // Starting --> Inited: Failure
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_starting, action_failed, state_inited), false,
        "Failed to add transition: Starting -> Failed -> Inited"
    );
    // Starting --> Stopping: Timeout
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_starting, action_timeout, state_stopping), false,
        "Failed to add transition: Starting -> Timeout -> Stopping"
    );

    /* ===================== Started ===================== */
    // Started --> Connecting: Connect
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_started, action_connect, state_connecting), false,
        "Failed to add transition: Started -> Connect -> Connecting"
    );
    // Started --> Stopping: Stop
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_started, action_stop, state_stopping), false,
        "Failed to add transition: Started -> Stop -> Stopping"
    );

    /* ===================== Connecting ===================== */
    // Connecting --> Connected: Success
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_connecting, action_success, state_connected), false,
        "Failed to add transition: Connecting -> Success -> Connected"
    );
    // Connecting --> Started: Failure
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_connecting, action_failed, state_started), false,
        "Failed to add transition: Connecting -> Failed -> Started"
    );
    // Connecting --> Disconnecting: Timeout
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_connecting, action_timeout, state_disconnecting), false,
        "Failed to add transition: Connecting -> Timeout -> Disconnecting"
    );

    /* ===================== Connected ===================== */
    // Connected --> Disconnecting: Disconnect
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_connected, action_disconnect, state_disconnecting), false,
        "Failed to add transition: Connected -> Disconnect -> Disconnecting"
    );
    // Connected --> Stopping: Stop
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_connected, action_stop, state_stopping), false,
        "Failed to add transition: Connected -> Stop -> Stopping"
    );

    /* ===================== Disconnecting ===================== */
    // Disconnecting --> Started: Success / Failure / Timeout
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_disconnecting, action_success, state_started), false,
        "Failed to add transition: Disconnecting -> Success -> Started"
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_disconnecting, action_failed, state_started), false,
        "Failed to add transition: Disconnecting -> Failed -> Started"
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_disconnecting, action_timeout, state_started), false,
        "Failed to add transition: Disconnecting -> Timeout -> Started"
    );

    /* ===================== Stopping ===================== */
    // Stopping --> Inited: Success / Failure / Timeout
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_stopping, action_success, state_inited), false,
        "Failed to add transition: Stopping -> Success -> Inited"
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_stopping, action_failed, state_inited), false,
        "Failed to add transition: Stopping -> Failed -> Inited"
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_stopping, action_timeout, state_inited), false,
        "Failed to add transition: Stopping -> Timeout -> Inited"
    );

    /* Self transitions for stable states */
    // Idle --> Idle: Deinit
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_idle, action_deinit, state_idle), false,
        "Failed to add transition: Idle -> Deinit -> Idle"
    );
    // Inited --> Inited: Init
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_inited, action_init, state_inited), false,
        "Failed to add transition: Inited -> Init -> Inited"
    );
    // Started --> Started: Start / Disconnect
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_started, action_start, state_started), false,
        "Failed to add transition: Started -> Start -> Started"
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_started, action_disconnect, state_started), false,
        "Failed to add transition: Started -> Disconnect -> Started"
    );
    // Connected --> Connected: Start / Connect
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_connected, action_start, state_connected), false,
        "Failed to add transition: Connected -> Start -> Connected"
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_connected, action_connect, state_connected), false,
        "Failed to add transition: Connected -> Connect -> Connected"
    );

    /* Self transitions for transient states */
    // Initing --> Initing: Init
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_initing, action_init, state_initing), false,
        "Failed to add transition: Initing -> Init -> Initing"
    );
    // Deiniting --> Deiniting: Deinit
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_deiniting, action_deinit, state_deiniting), false,
        "Failed to add transition: Deiniting -> Deinit -> Deiniting"
    );
    // Starting --> Starting: Start
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_starting, action_start, state_starting), false,
        "Failed to add transition: Starting -> Start -> Starting"
    );
    // Stopping --> Stopping: Stop
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_stopping, action_stop, state_stopping), false,
        "Failed to add transition: Stopping -> Stop -> Stopping"
    );
    // Connecting --> Connecting: Start / Connect
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_connecting, action_start, state_connecting), false,
        "Failed to add transition: Connecting -> Start -> Connecting"
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_connecting, action_connect, state_connecting), false,
        "Failed to add transition: Connecting -> Connect -> Connecting"
    );
    // Disconnecting --> Disconnecting: Start / Disconnect
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_disconnecting, action_start, state_disconnecting), false,
        "Failed to add transition: Disconnecting -> Start -> Disconnecting"
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_disconnecting, action_disconnect, state_disconnecting), false,
        "Failed to add transition: Disconnecting -> Disconnect -> Disconnecting"
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

    if (is_running()) {
        BROOKESIA_LOGD("Running, stop it first");
        stop();
    }

    state_machine_.reset();
    state_classes_.clear();

    BROOKESIA_LOGI("State machine deinitialized");
}

bool StateMachine::start()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_running()) {
        BROOKESIA_LOGD("Already running");
        return true;
    }

    lib_utils::FunctionGuard stop_guard([this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        stop();
    });

    // Set timeout for transient states
    auto action_timeout = BROOKESIA_DESCRIBE_TO_STR(ExtraAction::Timeout);
    state_classes_[BROOKESIA_DESCRIBE_ENUM_TO_NUM(GeneralState::Initing)]->set_timeout(
        BROOKESIA_SERVICE_WIFI_HAL_WAIT_EVENT_INITED_TIMEOUT_MS, action_timeout
    );
    state_classes_[BROOKESIA_DESCRIBE_ENUM_TO_NUM(GeneralState::Deiniting)]->set_timeout(
        BROOKESIA_SERVICE_WIFI_HAL_WAIT_EVENT_DEINITED_TIMEOUT_MS, action_timeout
    );
    state_classes_[BROOKESIA_DESCRIBE_ENUM_TO_NUM(GeneralState::Starting)]->set_timeout(
        BROOKESIA_SERVICE_WIFI_HAL_WAIT_EVENT_STARTED_TIMEOUT_MS, action_timeout
    );
    state_classes_[BROOKESIA_DESCRIBE_ENUM_TO_NUM(GeneralState::Stopping)]->set_timeout(
        BROOKESIA_SERVICE_WIFI_HAL_WAIT_EVENT_STOPPED_TIMEOUT_MS, action_timeout
    );
    state_classes_[BROOKESIA_DESCRIBE_ENUM_TO_NUM(GeneralState::Connecting)]->set_timeout(
        BROOKESIA_SERVICE_WIFI_HAL_WAIT_EVENT_CONNECTED_TIMEOUT_MS, action_timeout
    );
    state_classes_[BROOKESIA_DESCRIBE_ENUM_TO_NUM(GeneralState::Disconnecting)]->set_timeout(
        BROOKESIA_SERVICE_WIFI_HAL_WAIT_EVENT_DISCONNECTED_TIMEOUT_MS, action_timeout
    );

    BROOKESIA_CHECK_FALSE_RETURN(
    state_machine_->start({
        .task_scheduler  = Wifi::get_instance().get_task_scheduler(),
        .task_group_name = Wifi::get_instance().get_state_task_group(),
        .initial_state   = BROOKESIA_DESCRIBE_TO_STR(GeneralState::Idle),
    }), false, "Failed to start state machine");

    stop_guard.release();

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

    BROOKESIA_CHECK_FALSE_EXECUTE(force_transition_to(GeneralState::Idle), {}, {
        BROOKESIA_LOGE("Failed to force transition to idle state when stopping state machine");
    });
    state_machine_->stop();

    BROOKESIA_LOGI("State machine stopped");
}

bool StateMachine::trigger_general_action(GeneralAction action)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto action_str = BROOKESIA_DESCRIBE_TO_STR(action);
    BROOKESIA_LOGD("Params: action(%1%)", action_str);

    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->trigger_action(action_str), false, "Failed to trigger action: '%1%'", action_str
    );

    return true;
}

bool StateMachine::trigger_extra_action(ExtraAction action)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto action_str = BROOKESIA_DESCRIBE_TO_STR(action);
    BROOKESIA_LOGD("Params: action(%1%)", action_str);

    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->trigger_action(action_str), false, "Failed to trigger action: %1%", action_str
    );

    return true;
}

bool StateMachine::force_transition_to(GeneralState state)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: state(%1%)", BROOKESIA_DESCRIBE_TO_STR(state));

    std::string state_str;
    if (state == GeneralState::Max) {
        state_str = "";
    } else {
        state_str = BROOKESIA_DESCRIBE_TO_STR(state);
    }
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->force_transition_to(state_str), false, "Failed to force transition to %1% state", state_str
    );

    return true;
}

GeneralState StateMachine::get_current_state()
{
    GeneralState state = GeneralState::Max;
    BROOKESIA_DESCRIBE_STR_TO_ENUM(state_machine_->get_current_state(), state);

    return state;
}

bool StateMachine::is_transient_state()
{
    auto current_state = get_current_state();
    return (((current_state == GeneralState::Initing) || (current_state == GeneralState::Deiniting) ||
             (current_state == GeneralState::Starting) || (current_state == GeneralState::Stopping) ||
             (current_state == GeneralState::Connecting) || (current_state == GeneralState::Disconnecting)));
}

GeneralState StateMachine::get_general_action_target_transient_state(GeneralAction action)
{
    switch (action) {
    case GeneralAction::Init:
        return GeneralState::Initing;
    case GeneralAction::Deinit:
        return GeneralState::Deiniting;
    case GeneralAction::Start:
        return GeneralState::Starting;
    case GeneralAction::Stop:
        return GeneralState::Stopping;
    case GeneralAction::Connect:
        return GeneralState::Connecting;
    case GeneralAction::Disconnect:
        return GeneralState::Disconnecting;
    default:
        return GeneralState::Max;
    }
}

GeneralState StateMachine::get_general_action_target_stable_state(GeneralAction action)
{
    switch (action) {
    case GeneralAction::Init:
        return GeneralState::Inited;
    case GeneralAction::Deinit:
        return GeneralState::Idle;
    case GeneralAction::Start:
        return GeneralState::Started;
    case GeneralAction::Stop:
        return GeneralState::Inited;
    case GeneralAction::Connect:
        return GeneralState::Connected;
    case GeneralAction::Disconnect:
        return GeneralState::Started;
    default:
        return GeneralState::Max;
    }
}

GeneralState StateMachine::get_general_event_target_state(GeneralEvent event)
{
    switch (event) {
    case GeneralEvent::Inited:
        return GeneralState::Inited;
    case GeneralEvent::Deinited:
        return GeneralState::Idle;
    case GeneralEvent::Started:
        return GeneralState::Started;
    case GeneralEvent::Stopped:
        return GeneralState::Inited;
    case GeneralEvent::Connected:
        return GeneralState::Connected;
    case GeneralEvent::Disconnected:
        return GeneralState::Started;
    default:
        return GeneralState::Max;
    }
}

} // namespace esp_brookesia::service::wifi
