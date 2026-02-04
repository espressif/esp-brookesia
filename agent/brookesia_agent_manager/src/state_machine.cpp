/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/agent_manager/macro_configs.h"
#if !BROOKESIA_AGENT_MANAGER_STATE_MACHINE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/lib_utils/function_guard.hpp"
#include "brookesia/service_helper/audio.hpp"
#include "brookesia/service_helper/sntp.hpp"
#include "brookesia/service_manager/service/manager.hpp"
#include "brookesia/agent_manager/state_machine.hpp"
#include "brookesia/agent_manager/manager.hpp"

namespace esp_brookesia::agent {

using ManagerHelper = helper::Manager;
using SNTPHelper = service::helper::SNTP;

constexpr uint32_t TIME_SYNC_UPDATE_INTERVAL_MS = 1000;

constexpr uint32_t GENERAL_ACTION_QUEUE_DISPATCH_INTERVAL_MS = 100;
constexpr uint32_t GENERAL_STATE_UPDATE_INTERVAL_MS = 100;

class GeneralStateClass : public lib_utils::StateBase {
public:
    GeneralStateClass(StateMachine &context, GeneralState state)
        : lib_utils::StateBase()
        , context_(context)
        , state_(state)
    {}
    ~GeneralStateClass() = default;

    bool on_enter(const std::string &from_state, const std::string &action) override;
    bool on_exit(const std::string &to_state, const std::string &action) override;
    void on_update() override;

    bool is_transient() const
    {
        return ((state_ == GeneralState::TimeSyncing) || (state_ == GeneralState::Activating) ||
                (state_ == GeneralState::Starting) || (state_ == GeneralState::Sleeping) ||
                (state_ == GeneralState::WakingUp) || (state_ == GeneralState::Stopping));
    }

    GeneralEvent get_transient_state_target_event();

private:
    void process_error_state();

    StateMachine &context_;
    GeneralState state_ = GeneralState::TimeSyncing;
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
        BROOKESIA_LOGD("Not a transient state, skip");
        return true;
    }

    // Only handle GeneralAction for transient states (Activating, Starting, Sleeping, WakingUp, Stopping)
    // ExtraAction (Success, Failed) should not trigger do_general_action
    GeneralAction action_enum;
    if (!BROOKESIA_DESCRIBE_STR_TO_ENUM(action, action_enum)) {
        // Not a GeneralAction, skip (likely Success or Failed)
        BROOKESIA_LOGD("Not a GeneralAction, skip");
        return true;
    }

    auto agent = context_.get_agent();
    BROOKESIA_CHECK_NULL_RETURN(agent, false, "Agent is not set");
    if (!agent->do_general_action(action_enum)) {
        BROOKESIA_LOGE("Do general action '%1%' in '%2%' state failed", action, from_state);
        // Here we need to enter the transient state and wait for `Failed` event to trigger the Stopped event
    }

    return true;
}

bool GeneralStateClass::on_exit(const std::string &to_state, const std::string &action)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: to_state(%1%), action(%2%)", to_state, action);

    if (!is_transient()) {
        BROOKESIA_LOGD("Not a transient state, skip");
        return true;
    }

    // Only handle ExtraAction (Success, Failed) for transient states
    ExtraAction action_enum;
    if (!BROOKESIA_DESCRIBE_STR_TO_ENUM(action, action_enum)) {
        BROOKESIA_LOGD("Not a ExtraAction, skip");
        return true;
    }

    BROOKESIA_LOGD(
        "State '%1%' exited with '%2%'", BROOKESIA_DESCRIBE_TO_STR(state_), BROOKESIA_DESCRIBE_TO_STR(action_enum)
    );

    if ((action_enum == ExtraAction::Timeout) || (action_enum == ExtraAction::Failed)) {
        BROOKESIA_LOGW("Timeout or Failed, trigger agent stopped event");
        process_error_state();
    }

    return true;
}

void GeneralStateClass::on_update()
{
    // BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto agent = context_.get_agent();
    BROOKESIA_CHECK_NULL_EXIT(agent, "Agent is not set");

    // Check if the time is synced for TimeSyncing state
    if (state_ == GeneralState::TimeSyncing) {
        if (!context_.check_if_time_synced()) {
            BROOKESIA_LOGW("Time is not synced, check again after %1%ms...", TIME_SYNC_UPDATE_INTERVAL_MS);
        } else {
            BROOKESIA_LOGI("Time is synced");
            BROOKESIA_CHECK_FALSE_EXIT(
                context_.trigger_extra_action(ExtraAction::Success), "Failed to trigger TimeSynced event"
            );
        }
        return;
    }

    // Check if the target event is ready for other transient states (Activating, Starting, Sleeping, WakingUp, Stopping)
    auto target_event = get_transient_state_target_event();
    BROOKESIA_CHECK_FALSE_EXIT(target_event != GeneralEvent::Max, "Not a transient state");

    auto target_action = agent->get_general_action_from_target_event(target_event);
    BROOKESIA_CHECK_FALSE_EXIT(target_action != GeneralAction::Max, "Invalid target action");

    if (agent->is_general_event_ready(target_event)) {
        BROOKESIA_LOGD("Event %1% is ready, triggering Success", BROOKESIA_DESCRIBE_TO_STR(target_event));
        BROOKESIA_CHECK_FALSE_EXIT(
            context_.trigger_extra_action(ExtraAction::Success), "Failed to trigger extra action"
        );
    } else if (agent->is_general_state_error()) {
        BROOKESIA_LOGD("Error state, triggering Failed");
        BROOKESIA_CHECK_FALSE_EXIT(
            context_.trigger_extra_action(ExtraAction::Failed), "Failed to trigger extra action"
        );
    }
}

GeneralEvent GeneralStateClass::get_transient_state_target_event()
{
    switch (state_) {
    case GeneralState::Activating:
        return GeneralEvent::Activated;
    case GeneralState::Starting:
        return GeneralEvent::Started;
    case GeneralState::Sleeping:
        return GeneralEvent::Slept;
    case GeneralState::WakingUp:
        return GeneralEvent::Awake;
    case GeneralState::Stopping:
        return GeneralEvent::Stopped;
    default:
        return GeneralEvent::Max;
    }
}

void GeneralStateClass::process_error_state()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    // Stop the general action queue task to avoid triggering other actions
    context_.stop_general_action_queue_task();

    // Trigger the Stopped event
    auto agent = context_.get_agent();
    BROOKESIA_CHECK_NULL_EXIT(agent, "Agent is not set");

    agent->trigger_general_event(GeneralEvent::Stopped);
}

StateMachine::~StateMachine()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    deinit();
}

bool StateMachine::init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_initialized()) {
        BROOKESIA_LOGD("Already initialized");
        return true;
    }

    lib_utils::FunctionGuard deinit_guard([this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        deinit();
    });

    is_initialized_ = true;

    /* Create state machine */
    auto group_name = Manager::get_instance().get_state_task_group();
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        state_machine_ = std::make_unique<lib_utils::StateMachine>(group_name), false,
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
            state_ptr = std::make_shared<GeneralStateClass>(*this, state_enum), false,
            "Failed to create state %1%", state_str
        );
        BROOKESIA_CHECK_FALSE_RETURN(
            state_machine_->add_state(state_str, state_ptr), false, "Failed to add state %1%", state_str
        );
        state_classes_.push_back(state_ptr);
    }

    /* Get action strings */
    auto action_activate = BROOKESIA_DESCRIBE_TO_STR(GeneralAction::Activate);
    auto action_start = BROOKESIA_DESCRIBE_TO_STR(GeneralAction::Start);
    auto action_stop = BROOKESIA_DESCRIBE_TO_STR(GeneralAction::Stop);
    auto action_sleep = BROOKESIA_DESCRIBE_TO_STR(GeneralAction::Sleep);
    auto action_wakeup = BROOKESIA_DESCRIBE_TO_STR(GeneralAction::WakeUp);
    auto action_event_got = BROOKESIA_DESCRIBE_TO_STR(ExtraAction::Success);
    auto action_timeout = BROOKESIA_DESCRIBE_TO_STR(ExtraAction::Timeout);
    auto action_failed = BROOKESIA_DESCRIBE_TO_STR(ExtraAction::Failed);
    /* Get state strings */
    auto state_time_syncing = BROOKESIA_DESCRIBE_TO_STR(GeneralState::TimeSyncing);
    auto state_idle = BROOKESIA_DESCRIBE_TO_STR(GeneralState::Ready);
    auto state_activating = BROOKESIA_DESCRIBE_TO_STR(GeneralState::Activating);
    auto state_activated = BROOKESIA_DESCRIBE_TO_STR(GeneralState::Activated);
    auto state_starting = BROOKESIA_DESCRIBE_TO_STR(GeneralState::Starting);
    auto state_started = BROOKESIA_DESCRIBE_TO_STR(GeneralState::Started);
    auto state_sleeping = BROOKESIA_DESCRIBE_TO_STR(GeneralState::Sleeping);
    auto state_slept = BROOKESIA_DESCRIBE_TO_STR(GeneralState::Slept);
    auto state_wakingup = BROOKESIA_DESCRIBE_TO_STR(GeneralState::WakingUp);
    auto state_stopping = BROOKESIA_DESCRIBE_TO_STR(GeneralState::Stopping);

    /* Set update interval for transient states */
    state_classes_[BROOKESIA_DESCRIBE_ENUM_TO_NUM(GeneralState::TimeSyncing)]->set_update_interval(
        TIME_SYNC_UPDATE_INTERVAL_MS
    );
    state_classes_[BROOKESIA_DESCRIBE_ENUM_TO_NUM(GeneralState::Activating)]->set_update_interval(
        GENERAL_STATE_UPDATE_INTERVAL_MS
    );
    state_classes_[BROOKESIA_DESCRIBE_ENUM_TO_NUM(GeneralState::Starting)]->set_update_interval(
        GENERAL_STATE_UPDATE_INTERVAL_MS
    );
    state_classes_[BROOKESIA_DESCRIBE_ENUM_TO_NUM(GeneralState::Sleeping)]->set_update_interval(
        GENERAL_STATE_UPDATE_INTERVAL_MS
    );
    state_classes_[BROOKESIA_DESCRIBE_ENUM_TO_NUM(GeneralState::WakingUp)]->set_update_interval(
        GENERAL_STATE_UPDATE_INTERVAL_MS
    );
    state_classes_[BROOKESIA_DESCRIBE_ENUM_TO_NUM(GeneralState::Stopping)]->set_update_interval(
        GENERAL_STATE_UPDATE_INTERVAL_MS
    );

    /* Add transitions */
    /* From stable states to transient states */
    // Ready --> Activating: Activate
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_idle, action_activate, state_activating), false,
        "Failed to add transition: Ready -> Activate -> Activating"
    );
    // Activated --> Starting: Start
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_activated, action_start, state_starting), false,
        "Failed to add transition: Activated -> Start -> Starting"
    );
    // Started --> Sleeping: Sleep
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_started, action_sleep, state_sleeping), false,
        "Failed to add transition: Started -> Sleep -> Sleeping"
    );
    // Slept --> WakingUp: WakeUp
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_slept, action_wakeup, state_wakingup), false,
        "Failed to add transition: Slept -> WakeUp -> WakingUp"
    );
    // Activated --> Stopping: Stop
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_activated, action_stop, state_stopping), false,
        "Failed to add transition: Activated -> Stop -> Stopping"
    );
    // Started --> Stopping: Stop
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_started, action_stop, state_stopping), false,
        "Failed to add transition: Started -> Stop -> Stopping"
    );
    // Slept --> Stopping: Stop
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_slept, action_stop, state_stopping), false,
        "Failed to add transition: Slept -> Stop -> Stopping"
    );

    /* From transient states to stable states (Success) */
    // TimeSyncing --> Ready: Success
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_time_syncing, action_event_got, state_idle), false,
        "Failed to add transition: TimeSyncing -> Success -> Ready"
    );
    // Activating --> Activated: Success
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_activating, action_event_got, state_activated), false,
        "Failed to add transition: Activating -> Success -> Activated"
    );
    // Starting --> Started: Success
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_starting, action_event_got, state_started), false,
        "Failed to add transition: Starting -> Success -> Started"
    );
    // Sleeping --> Slept: Success
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_sleeping, action_event_got, state_slept), false,
        "Failed to add transition: Sleeping -> Success -> Slept"
    );
    // WakingUp --> Started: Success
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_wakingup, action_event_got, state_started), false,
        "Failed to add transition: WakingUp -> Success -> Started"
    );
    // Stopping --> Ready: Success
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_stopping, action_event_got, state_idle), false,
        "Failed to add transition: Stopping -> Success -> Ready"
    );

    /* From transient states to Stopping (Stop) */
    // Activating --> Stopping: Stop
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_activating, action_stop, state_stopping), false,
        "Failed to add transition: Activating -> Stop -> Stopping"
    );
    // Starting --> Stopping: Stop
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_starting, action_stop, state_stopping), false,
        "Failed to add transition: Starting -> Stop -> Stopping"
    );
    // Sleeping --> Stopping: Stop
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_sleeping, action_stop, state_stopping), false,
        "Failed to add transition: Sleeping -> Stop -> Stopping"
    );
    // WakingUp --> Stopping: Stop
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_wakingup, action_stop, state_stopping), false,
        "Failed to add transition: WakingUp -> Stop -> Stopping"
    );

    /* From transient states to Ready (Timeout / Failed) */
    // Activating --> Ready: Timeout / Failed
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_activating, action_timeout, state_idle), false,
        "Failed to add transition: Activating -> Timeout -> Ready"
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_activating, action_failed, state_idle), false,
        "Failed to add transition: Activating -> Failed -> Ready"
    );
    // Starting --> Ready: Timeout / Failed
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_starting, action_timeout, state_idle), false,
        "Failed to add transition: Starting -> Timeout -> Ready"
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_starting, action_failed, state_idle), false,
        "Failed to add transition: Starting -> Failed -> Ready"
    );
    // Sleeping --> Ready: Timeout / Failed
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_sleeping, action_timeout, state_idle), false,
        "Failed to add transition: Sleeping -> Timeout -> Ready"
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_sleeping, action_failed, state_idle), false,
        "Failed to add transition: Sleeping -> Failed -> Ready"
    );
    // WakingUp --> Ready: Timeout / Failed
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_wakingup, action_timeout, state_idle), false,
        "Failed to add transition: WakingUp -> Timeout -> Ready"
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_wakingup, action_failed, state_idle), false,
        "Failed to add transition: WakingUp -> Failed -> Ready"
    );

    /* Self transitions */
    // Activating --> Activating: Activate
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_activating, action_activate, state_activating), false,
        "Failed to add transition: Activating -> Activate -> Activating"
    );
    // Activated --> Activated: Activate
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_activated, action_activate, state_activated), false,
        "Failed to add transition: Activated -> Activate -> Activated"
    );
    // Starting --> Starting: Start / WakeUp
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_starting, action_start, state_starting), false,
        "Failed to add transition: Starting -> Start -> Starting"
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_starting, action_wakeup, state_starting), false,
        "Failed to add transition: Starting -> WakeUp -> Starting"
    );
    // Started --> Started: Start / WakeUp
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_started, action_start, state_started), false,
        "Failed to add transition: Started -> Start -> Started"
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_started, action_wakeup, state_started), false,
        "Failed to add transition: Started -> WakeUp -> Started"
    );
    // Sleeping --> Sleeping: Sleep
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_sleeping, action_sleep, state_sleeping), false,
        "Failed to add transition: Sleeping -> Sleep -> Sleeping"
    );
    // Slept --> Slept: Sleep / Start
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_slept, action_sleep, state_slept), false,
        "Failed to add transition: Slept -> Sleep -> Slept"
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_slept, action_start, state_slept), false,
        "Failed to add transition: Slept -> Start -> Slept"
    );
    // WakingUp --> WakingUp: WakeUp / Start
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_wakingup, action_wakeup, state_wakingup), false,
        "Failed to add transition: WakingUp -> WakeUp -> WakingUp"
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_wakingup, action_start, state_wakingup), false,
        "Failed to add transition: WakingUp -> Start -> WakingUp"
    );
    // Stopping --> Stopping: Stop
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_stopping, action_stop, state_stopping), false,
        "Failed to add transition: Stopping -> Stop -> Stopping"
    );

    deinit_guard.release();

    BROOKESIA_LOGI("State machine initialized");

    return true;
}

void StateMachine::deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_initialized()) {
        BROOKESIA_LOGD("Not initialized");
        return;
    }

    if (is_running()) {
        BROOKESIA_LOGD("Running, stop it first");
        stop();
    }

    state_machine_.reset();

    is_initialized_ = false;

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

    is_running_ = true;

    agent_ = Manager::get_instance().get_active_agent();
    BROOKESIA_CHECK_NULL_RETURN(agent_, false, "Agent is null");
    auto &agent_attributes = agent_->get_attributes();
    GeneralState agent_init_state = agent_attributes.require_time_sync ?
                                    GeneralState::TimeSyncing : GeneralState::Ready;

    // Set timeout for transient states, use action 'Stop' to handle timeout
    auto &operation_timeout = agent_attributes.operation_timeout;
    auto action_timeout = BROOKESIA_DESCRIBE_TO_STR(ExtraAction::Timeout);
    state_classes_[BROOKESIA_DESCRIBE_ENUM_TO_NUM(GeneralState::Activating)]->set_timeout(
        operation_timeout.activate, action_timeout
    );
    state_classes_[BROOKESIA_DESCRIBE_ENUM_TO_NUM(GeneralState::Starting)]->set_timeout(
        operation_timeout.start, action_timeout
    );
    state_classes_[BROOKESIA_DESCRIBE_ENUM_TO_NUM(GeneralState::Stopping)]->set_timeout(
        operation_timeout.stop, action_timeout
    );
    state_classes_[BROOKESIA_DESCRIBE_ENUM_TO_NUM(GeneralState::Sleeping)]->set_timeout(
        operation_timeout.sleep, action_timeout
    );
    state_classes_[BROOKESIA_DESCRIBE_ENUM_TO_NUM(GeneralState::WakingUp)]->set_timeout(
        operation_timeout.wake_up, action_timeout
    );

    auto scheduler = Manager::get_instance().get_task_scheduler();
    BROOKESIA_CHECK_NULL_RETURN(scheduler, false, "Scheduler is not set");
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->start(scheduler, BROOKESIA_DESCRIBE_TO_STR(agent_init_state)), false,
        "Failed to start state machine"
    );

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

    stop_general_action_queue_task();
    if (!state_machine_->force_transition_to(BROOKESIA_DESCRIBE_TO_STR(GeneralState::Max))) {
        BROOKESIA_LOGE("Failed to force transition to invalid state");
    }
    state_machine_->stop();

    is_running_ = false;

    BROOKESIA_LOGI("State machine stopped");
}

bool StateMachine::do_time_sync()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto result = SNTPHelper::call_function_sync<void>(SNTPHelper::FunctionId::Start);
    if (!result) {
        BROOKESIA_LOGE("Failed to start SNTP: %1%", result.error());
        return false;
    }

    return true;
}

bool StateMachine::check_if_time_synced()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto result = SNTPHelper::call_function_sync<bool>(SNTPHelper::FunctionId::IsTimeSynced);
    if (!result) {
        BROOKESIA_LOGE("Failed to check if time is synced: %1%", result.error());
        return false;
    }

    if (!result.value()) {
        BROOKESIA_LOGW("Time is not synced, check again after %1%ms...", TIME_SYNC_UPDATE_INTERVAL_MS);
        return false;
    }

    BROOKESIA_LOGI("Time is synced");

    return true;
}

bool StateMachine::trigger_general_action(GeneralAction action, bool use_dispatch)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto action_str = BROOKESIA_DESCRIBE_TO_STR(action);
    BROOKESIA_LOGD("Params: action(%1%)", action_str);

    BROOKESIA_CHECK_FALSE_RETURN(is_running(), false, "Not running");

    /* Handle special cases */
    GeneralAction pre_action;
    bool need_pre_action = false;
    switch (action) {
    case GeneralAction::Start:
        if (!agent_->is_general_event_ready(GeneralEvent::Activated)) {
            pre_action = GeneralAction::Activate;
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

    // If any action is running or the general action queue task is running, add the action to the queue
    if (is_action_running() || is_general_action_queue_task_running() || !general_action_queue_.empty()) {
        auto front_action = general_action_queue_.front();
        if ((action == front_action) || (action == agent_->get_running_general_action())) {
            BROOKESIA_LOGD("Action %1% is already in the queue front or running, skip", action_str);
            return true;
        }

        BROOKESIA_LOGD("Added action %1% to the queue", action_str);
        general_action_queue_.push(action);

        // If the general action queue task is not running, post it
        BROOKESIA_CHECK_FALSE_RETURN(
            start_general_action_queue_task(), false, "Failed to start general action queue task"
        );
    } else {
        BROOKESIA_LOGD("Trigger action directly");
        // Otherwise, trigger the action directly
        BROOKESIA_CHECK_FALSE_RETURN(
            state_machine_->trigger_action(action_str, use_dispatch), false, "Failed to trigger general action: %1%",
            action_str
        );
    }

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

GeneralAction StateMachine::pop_general_action_queue_front()
{
    auto action = GeneralAction::Max;
    if (general_action_queue_.empty()) {
        return action;
    }

    action = general_action_queue_.front();
    general_action_queue_.pop();

    return action;
}

bool StateMachine::is_transient_state()
{
    auto current_state = get_current_state();
    return (((current_state == GeneralState::TimeSyncing) || (current_state == GeneralState::Starting) ||
             (current_state == GeneralState::Sleeping) || (current_state == GeneralState::WakingUp) ||
             (current_state == GeneralState::Stopping)));
}

bool StateMachine::is_general_action_queue_task_running()
{
    if (general_action_queue_task_ == 0) {
        return false;
    }

    auto scheduler = Manager::get_instance().get_task_scheduler();
    if (!scheduler ||
            (scheduler->get_state(general_action_queue_task_) != lib_utils::TaskScheduler::TaskState::Running)) {
        return false;
    }

    return true;
}

GeneralState StateMachine::get_general_action_target_transient_state(GeneralAction action)
{
    switch (action) {
    case GeneralAction::Activate:
        return GeneralState::Activating;
    case GeneralAction::Start:
        return GeneralState::Starting;
    case GeneralAction::Stop:
        return GeneralState::Stopping;
    case GeneralAction::Sleep:
        return GeneralState::Sleeping;
    case GeneralAction::WakeUp:
        return GeneralState::WakingUp;
    default:
        return GeneralState::Max;
    }
}

GeneralState StateMachine::get_general_action_target_stable_state(GeneralAction action)
{
    switch (action) {
    case GeneralAction::Activate:
        return GeneralState::Activated;
    case GeneralAction::Start:
        return GeneralState::Started;
    case GeneralAction::Stop:
        return GeneralState::Ready;
    case GeneralAction::Sleep:
        return GeneralState::Slept;
    case GeneralAction::WakeUp:
        return GeneralState::Started;
    default:
        return GeneralState::Max;
    }
}

GeneralState StateMachine::get_general_event_target_state(GeneralEvent event)
{
    switch (event) {
    case GeneralEvent::Activated:
        return GeneralState::Activated;
    case GeneralEvent::Started:
        return GeneralState::Started;
    case GeneralEvent::Stopped:
        return GeneralState::Ready;
    case GeneralEvent::Slept:
        return GeneralState::Slept;
    case GeneralEvent::Awake:
        return GeneralState::Started;
    default:
        return GeneralState::Max;
    }
}

std::shared_ptr<lib_utils::TaskScheduler> StateMachine::get_task_scheduler()
{
    return Manager::get_instance().get_task_scheduler();
}

bool StateMachine::is_action_running()
{
    auto running_action = agent_->get_running_general_action();
    // BROOKESIA_LOGD("Running action: %1%", BROOKESIA_DESCRIBE_TO_STR(running_action));

    auto has_transition_running = state_machine_->has_transition_running();
    // BROOKESIA_LOGD("State machine has transition running: %1%", has_transition_running);

    auto has_state_updating = state_machine_->has_state_updating();
    // BROOKESIA_LOGD("State machine has state updating: %1%", has_state_updating);

    return (running_action != GeneralAction::Max) || has_transition_running || has_state_updating;
}

bool StateMachine::start_general_action_queue_task()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_general_action_queue_task_running()) {
        BROOKESIA_LOGD("Already running, skip");
        return true;
    }

    auto scheduler = Manager::get_instance().get_task_scheduler();
    BROOKESIA_CHECK_NULL_RETURN(scheduler, false, "Scheduler is not set");
    auto task = [this]() {
        // BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

        if (is_action_running()) {
            // BROOKESIA_LOGD("Action is running, skip");
            return true;
        }

        auto front_action = pop_general_action_queue_front();
        if (front_action == GeneralAction::Max) {
            BROOKESIA_LOGD("No action in the queue, stop task");
            return false;
        }

        auto front_action_str = BROOKESIA_DESCRIBE_TO_STR(front_action);
        BROOKESIA_CHECK_FALSE_RETURN(
            state_machine_->trigger_action(front_action_str, false), true,
            "Failed to trigger front action: %1%", front_action_str
        );
        BROOKESIA_LOGD(
            "Triggered action '%1%' from the queue, remaining actions: %2%", front_action_str,
            general_action_queue_.size()
        );

        if (general_action_queue_.empty()) {
            BROOKESIA_LOGD("No action in the queue, stop task");
            general_action_queue_task_ = 0;
            return false;
        }

        return true;
    };
    auto group = Manager::get_instance().get_state_task_group();
    auto result = scheduler->post_periodic(
                      task, GENERAL_ACTION_QUEUE_DISPATCH_INTERVAL_MS, &general_action_queue_task_, group
                  );
    BROOKESIA_CHECK_FALSE_RETURN(result, false, "Failed to post general action queue task to group");

    return true;
}

void StateMachine::stop_general_action_queue_task()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (general_action_queue_task_ != 0) {
        auto scheduler = Manager::get_instance().get_task_scheduler();
        if (scheduler) {
            scheduler->cancel(general_action_queue_task_);
        }
        general_action_queue_task_ = 0;
    }
    general_action_queue_ = std::queue<GeneralAction>();
}

} // namespace esp_brookesia::agent
