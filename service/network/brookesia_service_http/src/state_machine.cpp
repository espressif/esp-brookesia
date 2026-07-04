/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/service_http/macro_configs.h"
#if !BROOKESIA_SERVICE_HTTP_STATE_MACHINE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include <exception>
#include "private/utils.hpp"
#include "brookesia/lib_utils/function_guard.hpp"
#include "brookesia/lib_utils/state_base.hpp"
#include "brookesia/service_http/state_machine.hpp"

namespace esp_brookesia::service::http {

class GeneralStateClass : public lib_utils::StateBase {
public:
    GeneralStateClass(StateMachine &context, GeneralState state)
        : lib_utils::StateBase(BROOKESIA_DESCRIBE_TO_STR(state))
        , context_(context)
        , state_(state)
    {}
    ~GeneralStateClass() = default;

    void on_update() override;

private:
    StateMachine &context_;
    GeneralState state_ = GeneralState::Idle;
};

void GeneralStateClass::on_update()
{
    if (state_ != GeneralState::TimeSyncing) {
        return;
    }

    if (!context_.check_if_time_synced()) {
        BROOKESIA_LOGD("Time is not synced, check again later");
        return;
    }

    BROOKESIA_LOGI("Time is synced");
    BROOKESIA_CHECK_FALSE_EXIT(context_.complete_time_sync(), "Failed to complete time sync");
}

StateMachine::~StateMachine() noexcept
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    try {
        deinit();
    } catch (const std::exception &e) {
        BROOKESIA_LOGE("Detected exception while destroying HTTP state machine: %1%", e.what());
    } catch (...) {
        BROOKESIA_LOGE("Detected unknown exception while destroying HTTP state machine");
    }
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

    BROOKESIA_CHECK_EXCEPTION_RETURN(
        state_machine_ = std::make_unique<lib_utils::StateMachine>(),
        false, "Failed to create state machine"
    );

    auto state_num_max = static_cast<size_t>(BROOKESIA_DESCRIBE_ENUM_TO_NUM(GeneralState::Max));
    for (size_t state_num = 0; state_num < state_num_max; state_num++) {
        GeneralState state_enum;
        BROOKESIA_CHECK_FALSE_RETURN(
            BROOKESIA_DESCRIBE_NUM_TO_ENUM(state_num, state_enum), false,
            "Failed to convert number %1% to state enum", state_num
        );
        auto state_ptr = std::make_shared<GeneralStateClass>(*this, state_enum);
        if (state_enum == GeneralState::TimeSyncing) {
            state_ptr->set_update_interval(BROOKESIA_SERVICE_HTTP_TIME_SYNC_POLL_MS);
        }
        BROOKESIA_CHECK_FALSE_RETURN(
            state_machine_->add_state(state_ptr), false,
            "Failed to add state %1%", BROOKESIA_DESCRIBE_TO_STR(state_enum)
        );
    }

    auto state_idle = BROOKESIA_DESCRIBE_TO_STR(GeneralState::Idle);
    auto state_inited = BROOKESIA_DESCRIBE_TO_STR(GeneralState::Inited);
    auto state_time_syncing = BROOKESIA_DESCRIBE_TO_STR(GeneralState::TimeSyncing);
    auto state_started = BROOKESIA_DESCRIBE_TO_STR(GeneralState::Started);
    auto state_stopping = BROOKESIA_DESCRIBE_TO_STR(GeneralState::Stopping);
    auto state_stopped = BROOKESIA_DESCRIBE_TO_STR(GeneralState::Stopped);

    // Direct lifecycle transitions matching how Http::set_general_state actually drives
    // the state machine: Idle -> Inited (on_init) -> Started (on_start) -> Stopping ->
    // Stopped (on_stop) -> Idle (on_deinit), with Stopped -> Started supporting restart.
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_idle, state_inited, state_inited), false,
        "Failed to add Idle -> Inited transition"
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_inited, state_started, state_started), false,
        "Failed to add Inited -> Started transition"
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_inited, state_time_syncing, state_time_syncing), false,
        "Failed to add Inited -> TimeSyncing transition"
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_time_syncing, state_started, state_started), false,
        "Failed to add TimeSyncing -> Started transition"
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_started, state_stopping, state_stopping), false,
        "Failed to add Started -> Stopping transition"
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_stopping, state_stopped, state_stopped), false,
        "Failed to add Stopping -> Stopped transition"
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_stopped, state_started, state_started), false,
        "Failed to add Stopped -> Started transition"
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_stopped, state_idle, state_idle), false,
        "Failed to add Stopped -> Idle transition"
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->add_transition(state_inited, state_idle, state_idle), false,
        "Failed to add Inited -> Idle transition"
    );

    deinit_guard.release();

    return true;
}

void StateMachine::deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_running()) {
        stop();
    }
    state_machine_.reset();
}

bool StateMachine::start(std::shared_ptr<lib_utils::TaskScheduler> scheduler, const lib_utils::TaskScheduler::Group &group)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_running()) {
        BROOKESIA_LOGD("Already running");
        return true;
    }

    // The state machine is only started by Http::on_start, by which point the
    // service is already in the Inited general state. Aligning the initial state
    // here lets the Inited -> Started action fire successfully.
    BROOKESIA_CHECK_FALSE_RETURN(
    state_machine_->start({
        .task_scheduler = scheduler,
        .task_group_name = group,
        .initial_state = BROOKESIA_DESCRIBE_TO_STR(GeneralState::Inited),
    }), false, "Failed to start state machine"
    );

    return true;
}

void StateMachine::stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_running()) {
        BROOKESIA_LOGD("Not running");
        return;
    }
    state_machine_->stop();
}

void StateMachine::set_time_sync_checker(TimeSyncChecker checker)
{
    time_sync_checker_ = std::move(checker);
}

void StateMachine::set_state_changed_callback(StateChangedCallback callback)
{
    state_changed_callback_ = std::move(callback);
}

bool StateMachine::trigger_state(GeneralState state)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (state == GeneralState::Max) {
        return false;
    }
    return state_machine_->trigger_action(BROOKESIA_DESCRIBE_TO_STR(state));
}

GeneralState StateMachine::get_current_state() const
{
    if (!is_running()) {
        return GeneralState::Max;
    }

    GeneralState state = GeneralState::Max;
    BROOKESIA_DESCRIBE_STR_TO_ENUM(state_machine_->get_current_state(), state);
    return state;
}

bool StateMachine::check_if_time_synced() const
{
    return time_sync_checker_ && time_sync_checker_();
}

bool StateMachine::complete_time_sync()
{
    BROOKESIA_CHECK_FALSE_RETURN(
        state_machine_->trigger_action(BROOKESIA_DESCRIBE_TO_STR(GeneralState::Started), true), false,
        "Failed to trigger Started state"
    );
    if (state_changed_callback_) {
        state_changed_callback_(GeneralState::Started);
    }
    return true;
}

} // namespace esp_brookesia::service::http
