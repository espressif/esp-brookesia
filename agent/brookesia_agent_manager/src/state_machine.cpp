/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
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

using AgentManagerHelper = service::helper::AgentManager;
using SNTPHelper = service::helper::SNTP;

constexpr uint32_t TIME_SYNC_UPDATE_INTERVAL_MS = 1000;

constexpr uint32_t GENERAL_STATE_UPDATE_INTERVAL_MS = 10;

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
        return ((state_ == GeneralState::TimeSyncing) || (state_ == GeneralState::Starting) ||
                (state_ == GeneralState::Sleeping) || (state_ == GeneralState::WakingUp) ||
                (state_ == GeneralState::Stopping));
    }

    GeneralEvent get_transient_state_target_event();

private:
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

    // Only handle GeneralAction for transient states (Initing, Starting, Sleeping, WakingUp, Stopping)
    // ExtraAction (EventGot, Timeout) should not trigger do_general_action
    GeneralAction action_enum;
    if (!BROOKESIA_DESCRIBE_STR_TO_ENUM(action, action_enum)) {
        // Not a GeneralAction, skip (likely EventGot or Timeout)
        BROOKESIA_LOGD("Not a GeneralAction, skip");
        return true;
    }

    auto agent = context_.get_agent();
    BROOKESIA_CHECK_NULL_RETURN(agent, false, "Agent is not set");
    if (!agent->do_general_action(action_enum)) {
        BROOKESIA_LOGE("Do general action '%1%' in '%2%' state failed", action, from_state);
        return false;
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

    // Only handle ExtraAction (EventGot, Timeout) for transient states
    ExtraAction action_enum;
    if (!BROOKESIA_DESCRIBE_STR_TO_ENUM(action, action_enum)) {
        BROOKESIA_LOGD("Not a ExtraAction, skip");
        return true;
    }

    if (action_enum == ExtraAction::EventGot) {
        BROOKESIA_LOGD("State '%1%' exited with EventGot", BROOKESIA_DESCRIBE_TO_STR(state_));
    } else if (action_enum == ExtraAction::Timeout) {
        BROOKESIA_LOGE("State '%1%' exited with Timeout", BROOKESIA_DESCRIBE_TO_STR(state_));

        auto agent = context_.get_agent();
        BROOKESIA_CHECK_NULL_RETURN(agent, false, "Agent is not set");

        auto running_action = agent->get_running_general_action();
        BROOKESIA_LOGD("Get running action: %1%", BROOKESIA_DESCRIBE_TO_STR(running_action));

        auto failed_event = agent->get_general_action_failed_event(running_action);
        if (failed_event != GeneralEvent::Max) {
            BROOKESIA_LOGD("Trigger failed event: %1%", BROOKESIA_DESCRIBE_TO_STR(failed_event));
            agent->trigger_general_event(failed_event);
        } else {
            BROOKESIA_LOGD("No failed event found, skip");
        }
    } else if (action_enum == ExtraAction::Failed) {
        BROOKESIA_LOGE("State '%1%' exited with Failed", BROOKESIA_DESCRIBE_TO_STR(state_));
    }

    auto front_action = context_.pop_general_action_queue_front();
    if (front_action == GeneralAction::Max) {
        BROOKESIA_LOGD("No action in the queue, skip");
        return true;
    }

    // Since the current state of context_ is still a transient state,
    // it is necessary to directly trigger the action through context_'s state_machine_
    auto front_action_str = BROOKESIA_DESCRIBE_TO_STR(front_action);
    BROOKESIA_CHECK_FALSE_RETURN(
        context_.state_machine_->trigger_action(front_action_str, false), false, "Failed to trigger front action: %1%",
        front_action_str
    );
    BROOKESIA_LOGD("Triggered action '%1%' from the queue", front_action_str);

    return true;
}

void GeneralStateClass::on_update()
{
    auto agent = context_.get_agent();
    BROOKESIA_CHECK_NULL_EXIT(agent, "Agent is not set");

    // Check if the time is synced for TimeSyncing state
    if (state_ == GeneralState::TimeSyncing) {
        if (!context_.check_if_time_synced()) {
            BROOKESIA_LOGW("Time is not synced, check again after %1%ms...", TIME_SYNC_UPDATE_INTERVAL_MS);
        } else {
            BROOKESIA_LOGI("Time is synced");
            BROOKESIA_CHECK_FALSE_EXIT(
                context_.trigger_extra_action(ExtraAction::EventGot), "Failed to trigger TimeSynced event"
            );
        }
        return;
    }

    // Check if the target event is ready for other transient states (Starting, Sleeping, WakingUp, Stopping)
    auto target_event = get_transient_state_target_event();
    BROOKESIA_CHECK_FALSE_EXIT(target_event != GeneralEvent::Max, "Not a transient state");

    auto target_action = agent->get_general_action_from_target_event(target_event);
    BROOKESIA_CHECK_FALSE_EXIT(target_action != GeneralAction::Max, "Invalid target action");

    if (agent->is_general_event_ready(target_event)) {
        BROOKESIA_LOGD("Event %1% is ready, triggering EventGot", BROOKESIA_DESCRIBE_TO_STR(target_event));
        BROOKESIA_CHECK_FALSE_EXIT(
            context_.trigger_extra_action(ExtraAction::EventGot), "Failed to trigger extra action"
        );
    } else if (!agent->is_general_action_running(target_action)) {
        BROOKESIA_LOGD("Action %1% is not running, triggering Failed", BROOKESIA_DESCRIBE_TO_STR(target_action));
        BROOKESIA_CHECK_FALSE_EXIT(
            context_.trigger_extra_action(ExtraAction::Failed), "Failed to trigger extra action"
        );
    }
}

GeneralEvent GeneralStateClass::get_transient_state_target_event()
{
    switch (state_) {
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
    auto action_start = BROOKESIA_DESCRIBE_TO_STR(GeneralAction::Start);
    auto action_stop = BROOKESIA_DESCRIBE_TO_STR(GeneralAction::Stop);
    auto action_sleep = BROOKESIA_DESCRIBE_TO_STR(GeneralAction::Sleep);
    auto action_wakeup = BROOKESIA_DESCRIBE_TO_STR(GeneralAction::WakeUp);
    auto action_event_got = BROOKESIA_DESCRIBE_TO_STR(ExtraAction::EventGot);
    auto action_timeout = BROOKESIA_DESCRIBE_TO_STR(ExtraAction::Timeout);
    auto action_failed = BROOKESIA_DESCRIBE_TO_STR(ExtraAction::Failed);
    /* Get state strings */
    auto state_time_syncing = BROOKESIA_DESCRIBE_TO_STR(GeneralState::TimeSyncing);
    auto state_time_synced = BROOKESIA_DESCRIBE_TO_STR(GeneralState::TimeSynced);
    auto state_starting = BROOKESIA_DESCRIBE_TO_STR(GeneralState::Starting);
    auto state_stopping = BROOKESIA_DESCRIBE_TO_STR(GeneralState::Stopping);
    auto state_started = BROOKESIA_DESCRIBE_TO_STR(GeneralState::Started);
    auto state_pausing = BROOKESIA_DESCRIBE_TO_STR(GeneralState::Sleeping);
    auto state_resuming = BROOKESIA_DESCRIBE_TO_STR(GeneralState::WakingUp);
    auto state_sleepd = BROOKESIA_DESCRIBE_TO_STR(GeneralState::Slept);

    /* Set update interval for transient states */
    state_classes_[BROOKESIA_DESCRIBE_ENUM_TO_NUM(GeneralState::TimeSyncing)]->set_update_interval(
        TIME_SYNC_UPDATE_INTERVAL_MS
    );
    state_classes_[BROOKESIA_DESCRIBE_ENUM_TO_NUM(GeneralState::Starting)]->set_update_interval(
        GENERAL_STATE_UPDATE_INTERVAL_MS
    );
    state_classes_[BROOKESIA_DESCRIBE_ENUM_TO_NUM(GeneralState::Stopping)]->set_update_interval(
        GENERAL_STATE_UPDATE_INTERVAL_MS
    );
    state_classes_[BROOKESIA_DESCRIBE_ENUM_TO_NUM(GeneralState::Sleeping)]->set_update_interval(
        GENERAL_STATE_UPDATE_INTERVAL_MS
    );
    state_classes_[BROOKESIA_DESCRIBE_ENUM_TO_NUM(GeneralState::WakingUp)]->set_update_interval(
        GENERAL_STATE_UPDATE_INTERVAL_MS
    );

    /* Add transitions */
    /* From stable states to transient states */
    // TimeSynced --> Starting: Start
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_time_synced, action_start, state_starting), false,
        "Failed to add transition: TimeSynced -> Start -> Starting"
    );
    // Started --> Sleeping: Sleep
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_started, action_sleep, state_pausing), false,
        "Failed to add transition: Started -> Sleep -> Sleeping"
    );
    // Slept --> WakingUp: WakeUp
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_sleepd, action_wakeup, state_resuming), false,
        "Failed to add transition: Slept -> WakeUp -> WakingUp"
    );
    // Started --> Stopping: Stop
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_started, action_stop, state_stopping), false,
        "Failed to add transition: Started -> Stop -> Stopping"
    );
    // Slept --> Stopping: Stop
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_sleepd, action_stop, state_stopping), false,
        "Failed to add transition: Slept -> Stop -> Stopping"
    );

    /* From transient states to stable states (EventGot) */
    // TimeSyncing --> TimeSynced: EventGot
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_time_syncing, action_event_got, state_time_synced), false,
        "Failed to add transition: TimeSyncing -> EventGot -> TimeSynced"
    );
    // Starting --> Started: EventGot
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_starting, action_event_got, state_started), false,
        "Failed to add transition: Starting -> EventGot -> Started"
    );
    // Sleeping --> Slept: EventGot
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_pausing, action_event_got, state_sleepd), false,
        "Failed to add transition: Sleeping -> EventGot -> Slept"
    );
    // WakingUp --> Started: EventGot
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_resuming, action_event_got, state_started), false,
        "Failed to add transition: WakingUp -> EventGot -> Started"
    );
    // Stopping --> TimeSynced: EventGot
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_stopping, action_event_got, state_time_synced), false,
        "Failed to add transition: Stopping -> EventGot -> TimeSynced"
    );

    /* From transient states to stable states (Timeout/Failed) */
    // Starting --> TimeSynced: Timeout/Failed
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_starting, action_timeout, state_time_synced), false,
        "Failed to add transition: Starting -> Timeout -> TimeSynced"
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_starting, action_failed, state_time_synced), false,
        "Failed to add transition: Starting -> Failed -> TimeSynced"
    );
    // Sleeping --> Started: Timeout/Failed
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_pausing, action_timeout, state_started), false,
        "Failed to add transition: Sleeping -> Timeout -> Started"
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_pausing, action_failed, state_started), false,
        "Failed to add transition: Sleeping -> Failed -> Started"
    );
    // WakingUp --> Slept: Timeout/Failed
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_resuming, action_timeout, state_sleepd), false,
        "Failed to add transition: WakingUp -> Timeout -> Slept"
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_resuming, action_failed, state_sleepd), false,
        "Failed to add transition: WakingUp -> Failed -> Slept"
    );
    // Stopping --> TimeSynced: Timeout/Failed
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_stopping, action_timeout, state_time_synced), false,
        "Failed to add transition: Stopping -> Timeout -> TimeSynced"
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_stopping, action_failed, state_time_synced), false,
        "Failed to add transition: Stopping -> Failed -> TimeSynced"
    );

    /* Self transitions */
    // TimeSyncing --> TimeSyncing: Stop
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_time_syncing, action_stop, state_time_syncing), false,
        "Failed to add transition: TimeSyncing -> Stop -> TimeSyncing"
    );
    // TimeSynced --> TimeSynced: Stop
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_time_synced, action_stop, state_time_synced), false,
        "Failed to add transition: TimeSynced -> Stop -> TimeSynced"
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
    // Sleeping --> Sleeping: Sleep / Start
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_pausing, action_sleep, state_pausing), false,
        "Failed to add transition: Sleeping -> Sleep -> Sleeping"
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_pausing, action_start, state_pausing), false,
        "Failed to add transition: Sleeping -> Start -> Sleeping"
    );
    // Slept --> Slept: Sleep / Start
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_sleepd, action_sleep, state_sleepd), false,
        "Failed to add transition: Slept -> Sleep -> Slept"
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_sleepd, action_start, state_sleepd), false,
        "Failed to add transition: Slept -> Start -> Slept"
    );
    // WakingUp --> WakingUp: WakeUp / Start
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_resuming, action_wakeup, state_resuming), false,
        "Failed to add transition: WakingUp -> WakeUp -> WakingUp"
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_resuming, action_start, state_resuming), false,
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

    /* Set timeout for transient states */
    auto &timeout_array = agent_->get_attributes().general_event_wait_timeout_ms;
    auto action_timeout = BROOKESIA_DESCRIBE_TO_STR(ExtraAction::Timeout);
    state_classes_[BROOKESIA_DESCRIBE_ENUM_TO_NUM(GeneralState::Starting)]->set_timeout(
        timeout_array[BROOKESIA_DESCRIBE_ENUM_TO_NUM(GeneralEvent::Started)], action_timeout
    );
    state_classes_[BROOKESIA_DESCRIBE_ENUM_TO_NUM(GeneralState::Stopping)]->set_timeout(
        timeout_array[BROOKESIA_DESCRIBE_ENUM_TO_NUM(GeneralEvent::Stopped)], action_timeout
    );
    state_classes_[BROOKESIA_DESCRIBE_ENUM_TO_NUM(GeneralState::Sleeping)]->set_timeout(
        timeout_array[BROOKESIA_DESCRIBE_ENUM_TO_NUM(GeneralEvent::Slept)], action_timeout
    );
    state_classes_[BROOKESIA_DESCRIBE_ENUM_TO_NUM(GeneralState::WakingUp)]->set_timeout(
        timeout_array[BROOKESIA_DESCRIBE_ENUM_TO_NUM(GeneralEvent::Awake)], action_timeout
    );

    auto scheduler = Manager::get_instance().get_task_scheduler();
    BROOKESIA_CHECK_NULL_RETURN(scheduler, false, "Scheduler is not set");
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->start(scheduler, BROOKESIA_DESCRIBE_TO_STR(GeneralState::TimeSyncing)), false,
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

    while (!general_action_queue_.empty()) {
        general_action_queue_.pop();
    }
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

    // If the general action queue is not empty or any action is running, add the action to the queue
    if (!general_action_queue_.empty() || state_machine_->is_updating()) {
        auto front_action = general_action_queue_.front();
        if (front_action == action) {
            BROOKESIA_LOGD("Action %1% is already in the queue front, skip", action_str);
            return true;
        }
        BROOKESIA_LOGD("Added action %1% to the queue", action_str);
        general_action_queue_.push(action);
    } else {
        BROOKESIA_LOGD("No action in the queue or is running, trigger the action directly");
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
        state_machine_->trigger_action(action_str), false, "Failed to trigger extra action: %1%", action_str
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

} // namespace esp_brookesia::agent
