/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/agent_manager/macro_configs.h"
#if !BROOKESIA_AGENT_MANAGER_BASE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/service_helper/audio.hpp"
#include "brookesia/lib_utils/function_guard.hpp"
#include "brookesia/agent_manager/manager.hpp"
#include "brookesia/agent_manager/base.hpp"

namespace esp_brookesia::agent {

using AudioHelper = service::helper::Audio;
using ManagerHelper = helper::Manager;

std::string Base::get_call_task_group() const
{
    return Manager::get_instance().get_call_task_group();
}

std::string Base::get_event_task_group() const
{
    return Manager::get_instance().get_event_task_group();
}

std::string Base::get_request_task_group() const
{
    return Manager::get_instance().get_request_task_group();
}

std::string Base::get_state_task_group() const
{
    return Manager::get_instance().get_state_task_group();
}

ChatMode Base::get_chat_mode() const
{
    return Manager::get_instance().get_chat_mode();
}

void Base::reset_interrupted_speaking()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    is_interrupted_speaking_ = false;
}

bool Base::set_speaking(bool speaking)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: speaking(%1%)", BROOKESIA_DESCRIBE_TO_STR(speaking));

    if (is_speaking() == speaking) {
        BROOKESIA_LOGD("Speaking status is already %1%, skip", speaking ? "true" : "false");
        return true;
    }

    if (speaking && is_speaking_disabled()) {
        BROOKESIA_LOGD("Speaking is disabled, skip");
        return true;
    }

    is_speaking_ = speaking;

    if (!get_attributes().is_general_events_supported(ManagerHelper::AgentGeneralEvent::SpeakingStatusChanged)) {
        BROOKESIA_LOGD(
            "General event '%1%' is not supported, skip",
            BROOKESIA_DESCRIBE_TO_STR(ManagerHelper::AgentGeneralEvent::SpeakingStatusChanged)
        );
        return true;
    }

    auto result = Manager::get_instance().publish_event(
    BROOKESIA_DESCRIBE_ENUM_TO_STR(ManagerHelper::EventId::SpeakingStatusChanged), service::EventItemMap{
        {BROOKESIA_DESCRIBE_TO_STR(ManagerHelper::EventSpeakingStatusChangedParam::IsSpeaking), speaking}
    });
    BROOKESIA_CHECK_FALSE_RETURN(result, false, "Failed to publish speaking status changed event");

    return true;
}

bool Base::set_listening(bool listening)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: listening(%1%)", BROOKESIA_DESCRIBE_TO_STR(listening));

    if (is_listening() == listening) {
        BROOKESIA_LOGD("Listening status is already %1%, skip", listening ? "true" : "false");
        return true;
    }

    if (listening && is_listening_disabled()) {
        BROOKESIA_LOGD("Listening is disabled, skip");
        return true;
    }

    is_listening_ = listening;

    if (!get_attributes().is_general_events_supported(ManagerHelper::AgentGeneralEvent::ListeningStatusChanged)) {
        BROOKESIA_LOGD(
            "General event '%1%' is not supported, skip",
            BROOKESIA_DESCRIBE_TO_STR(ManagerHelper::AgentGeneralEvent::ListeningStatusChanged)
        );
        return true;
    }

    auto result = Manager::get_instance().publish_event(
    BROOKESIA_DESCRIBE_ENUM_TO_STR(ManagerHelper::EventId::ListeningStatusChanged), service::EventItemMap{
        {BROOKESIA_DESCRIBE_TO_STR(ManagerHelper::EventListeningStatusChangedParam::IsListening), listening}
    });
    BROOKESIA_CHECK_FALSE_RETURN(result, false, "Failed to publish listening status changed event");

    return true;
}

bool Base::set_emote(const std::string emote)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: emote(%1%)", emote);

    if (!get_attributes().is_general_events_supported(ManagerHelper::AgentGeneralEvent::EmoteGot)) {
        BROOKESIA_LOGD(
            "General event '%1%' is not supported, skip",
            BROOKESIA_DESCRIBE_TO_STR(ManagerHelper::AgentGeneralEvent::EmoteGot)
        );
        return true;
    }

    auto result = Manager::get_instance().publish_event(
    BROOKESIA_DESCRIBE_ENUM_TO_STR(ManagerHelper::EventId::EmoteGot), service::EventItemMap{
        {BROOKESIA_DESCRIBE_TO_STR(ManagerHelper::EventEmoteGotParam::Emote), emote}
    });
    BROOKESIA_CHECK_FALSE_RETURN(result, false, "Failed to publish emote got event");

    return true;
}

bool Base::set_agent_speaking_text(const std::string text)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: text(%1%)", text);

    if (!get_attributes().is_general_events_supported(ManagerHelper::AgentGeneralEvent::AgentSpeakingTextGot)) {
        BROOKESIA_LOGD(
            "General event '%1%' is not supported, skip",
            BROOKESIA_DESCRIBE_TO_STR(ManagerHelper::AgentGeneralEvent::AgentSpeakingTextGot)
        );
        return true;
    }

    auto result = Manager::get_instance().publish_event(
    BROOKESIA_DESCRIBE_ENUM_TO_STR(ManagerHelper::EventId::AgentSpeakingTextGot), service::EventItemMap{
        {BROOKESIA_DESCRIBE_TO_STR(ManagerHelper::EventAgentSpeakingTextGotParam::Text), text}
    });
    BROOKESIA_CHECK_FALSE_RETURN(result, false, "Failed to publish agent speaking text event");

    return true;
}

bool Base::set_user_speaking_text(const std::string text)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: text(%1%)", text);

    if (!get_attributes().is_general_events_supported(ManagerHelper::AgentGeneralEvent::UserSpeakingTextGot)) {
        BROOKESIA_LOGD(
            "General event '%1%' is not supported, skip",
            BROOKESIA_DESCRIBE_TO_STR(ManagerHelper::AgentGeneralEvent::UserSpeakingTextGot)
        );
        return true;
    }

    auto result = Manager::get_instance().publish_event(
    BROOKESIA_DESCRIBE_ENUM_TO_STR(ManagerHelper::EventId::UserSpeakingTextGot), service::EventItemMap{
        {BROOKESIA_DESCRIBE_TO_STR(ManagerHelper::EventUserSpeakingTextGotParam::Text), text}
    });
    BROOKESIA_CHECK_FALSE_RETURN(result, false, "Failed to publish user speaking text event");

    return true;
}

void Base::trigger_general_event(GeneralEvent event)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto event_str = BROOKESIA_DESCRIBE_TO_STR(event);
    BROOKESIA_LOGD("Params: event(%1%)", event_str);

    if (is_unexpected_event_processing_) {
        BROOKESIA_LOGD("Unexpected event processing, skip");
        return;
    }

    if (is_general_event_ready(event)) {
        BROOKESIA_LOGD("Event(%1%) is already matched, skip", event_str);
        return;
    }

    auto event_action = get_general_action_from_target_event(event);
    BROOKESIA_CHECK_FALSE_EXIT(event_action != GeneralAction::Max, "No corresponding action for event: %1%", event_str);

    auto is_unexpected_event = is_general_event_unexpected(event);
    if (is_unexpected_event) {
        BROOKESIA_LOGW("Detected unexpected event: %1%", event_str);

        // Clear the current action running bit
        auto running_action = get_running_general_action();
        update_general_action_state_bit(running_action, false);

        // Force do the event action to clear the unexpected event
        BROOKESIA_LOGW("Force to do the event action: %1%", BROOKESIA_DESCRIBE_TO_STR(event_action));
        is_unexpected_event_processing_ = true;
        BROOKESIA_CHECK_FALSE_EXECUTE(do_general_action(event_action, true), {}, {
            BROOKESIA_LOGE("Failed to do general action: %1%", BROOKESIA_DESCRIBE_TO_STR(event_action));
        });
        is_unexpected_event_processing_ = false;
    }

    // Since any state can trigger the Stopped event, all state flag bits need to be reset
    if (event == GeneralEvent::Stopped) {
        reset_general_state_flag_bits();
    } else {
        // Only clear target event action running bit
        update_general_action_state_bit(event_action, false);
    }
    // Set target event stable bit
    update_general_event_state_bit(event, true);

    if (is_unexpected_event && callbacks_.unexpected_general_event_happened) {
        callbacks_.unexpected_general_event_happened(event);
    }

    if (!Manager::get_instance().publish_event(
    BROOKESIA_DESCRIBE_ENUM_TO_STR(ManagerHelper::EventId::GeneralEventHappened), service::EventItemMap{
    {
        BROOKESIA_DESCRIBE_TO_STR(ManagerHelper::EventGeneralEventHappenedParam::Event),
            event_str
        },
        {
            BROOKESIA_DESCRIBE_TO_STR(ManagerHelper::EventGeneralEventHappenedParam::IsUnexpected),
            is_unexpected_event
        }
    }
            )) {
        BROOKESIA_LOGE("Failed to publish general event: %1%", event_str);
    }
}

bool Base::feed_audio_decoder_data(const uint8_t *data, size_t data_size)
{
    // BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    // BROOKESIA_LOGD("Params: data(%1%), data_size(%1%)", data, data_size);

    // Skip if the agent is speaking disabled
    if (is_speaking_disabled()) {
        // BROOKESIA_LOGD("Speaking is disabled, skip");
        return true;
    }

    auto result = AudioHelper::call_function_sync(
                      AudioHelper::FunctionId::FeedDecoderData, service::RawBuffer(data, data_size)
                  );
    if (!result) {
        BROOKESIA_LOGE("Failed to feed audio data: %1%", result.error());
        return false;
    }

    return true;
}

void Base::on_stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!do_general_action(GeneralAction::Stop)) {
        BROOKESIA_LOGE("Failed to stop");
    }

    trigger_general_event(GeneralEvent::Stopped);

    clear_general_state_flag_bits();

    is_unexpected_event_processing_ = false;
}

bool Base::do_activate()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_FALSE_RETURN(on_activate(), false, "Failed to activate");

    return true;
}

bool Base::do_start()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    // Start the audio decoder
    BROOKESIA_CHECK_FALSE_RETURN(
        start_audio_decoder(), false, "Failed to start audio decoder"
    );
    lib_utils::FunctionGuard stop_decoder_guard([this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        stop_audio_decoder();
    });

    // Start the encoder
    BROOKESIA_CHECK_FALSE_RETURN(
        start_audio_encoder(), false, "Failed to start encoder"
    );
    lib_utils::FunctionGuard stop_encoder_guard([this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        stop_audio_encoder();
    });

    // Start the agent
    BROOKESIA_CHECK_FALSE_RETURN(on_startup(), false, "Failed to start");
    lib_utils::FunctionGuard stop_agent_guard([this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        stop();
    });

    stop_decoder_guard.release();
    stop_agent_guard.release();
    stop_encoder_guard.release();

    return true;
}

void Base::do_stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    on_shutdown();
    stop_audio_encoder();
    stop_audio_decoder();

    if (!set_speaking(false)) {
        BROOKESIA_LOGW("Failed to set speaking to false");
    }
    if (!set_listening(false)) {
        BROOKESIA_LOGW("Failed to set listening to false");
    }
    reset_interrupted_speaking();

    is_suspended_ = false;
    is_manual_listening_ = false;
}

bool Base::do_sleep()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    lib_utils::FunctionGuard wakeup_guard([this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        on_wakeup();
    });

    BROOKESIA_CHECK_FALSE_RETURN(on_sleep(), false, "Failed to sleep");

    wakeup_guard.release();

    is_manual_listening_ = false;

    if (!set_listening(false)) {
        BROOKESIA_LOGW("Failed to set listening to false");
    }
    if (!set_speaking(false)) {
        BROOKESIA_LOGW("Failed to set speaking to false");
    }

    return true;
}

bool Base::do_wakeup()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_FALSE_RETURN(!is_suspended(), false, "Suspended, should resume first");

    BROOKESIA_CHECK_FALSE_RETURN(on_wakeup(), false, "Failed to wakeup");

    return true;
}

bool Base::do_general_action(GeneralAction action, bool is_force)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto action_str = BROOKESIA_DESCRIBE_TO_STR(action);
    BROOKESIA_LOGD("Params: action(%1%), is_force(%2%)", action_str, is_force);

    if (is_general_action_running(action)) {
        BROOKESIA_LOGD("Action(%1%) is already running, skip", action_str);
        return true;
    }

    if (!is_force) {
        BROOKESIA_LOGD("Not force, check if the event is ready");
        GeneralEvent target_event = get_general_action_target_event(action);
#if (!BROOKESIA_LOG_DISABLE_DEBUG_TRACE) && (BROOKESIA_UTILS_LOG_LEVEL <= BROOKESIA_UTILS_LOG_LEVEL_DEBUG)
        BROOKESIA_LOGD(
            "running action(%1%)", BROOKESIA_DESCRIBE_TO_STR(get_running_general_action())
        );
        auto [event_flag_bit, event_bit_value] = get_general_event_state_flag_bit(target_event);
        BROOKESIA_LOGD(
            "state_flags(%1%), flag bit(%2%), bit value(%3%)", BROOKESIA_DESCRIBE_TO_STR(state_flags_),
            BROOKESIA_DESCRIBE_TO_STR(event_flag_bit), BROOKESIA_DESCRIBE_TO_STR(event_bit_value)
        );
#endif
        if (is_general_event_ready(target_event)) {
            BROOKESIA_LOGD("Event(%1%) is already matched, skip", BROOKESIA_DESCRIBE_TO_STR(target_event));
            return true;
        }
    } else {
        BROOKESIA_LOGD("Force, skip checking event ready");
    }

    // Publish "General Action Triggered" event before executing `do_xxxx()`, since `do_xxxx()` may call
    // trigger_general_event() directly, which publishes "General Event Happened" event.
    // This ensures correct event ordering: "General Action Triggered" -> "General Event Happened"
    if (!Manager::get_instance().publish_event(
    BROOKESIA_DESCRIBE_ENUM_TO_STR(ManagerHelper::EventId::GeneralActionTriggered), service::EventItemMap{
    {
        BROOKESIA_DESCRIBE_TO_STR(ManagerHelper::EventGeneralActionTriggeredParam::Action), action_str
        }
    }
            )) {
        BROOKESIA_LOGE("Failed to publish general action triggered event");
    }

    BROOKESIA_LOGI("Agent do '%1%' running", action_str);

    // Action running bit should be cleared by call `trigger_general_event()` when the action is done or failed
    update_general_action_state_bit(action, true);

    bool result = true;
    switch (action) {
    case GeneralAction::Activate: {
        result = do_activate();
        break;
    }
    case GeneralAction::Start: {
        result = do_start();
        break;
    }
    case GeneralAction::Sleep: {
        result = do_sleep();
        break;
    }
    case GeneralAction::WakeUp: {
        result = do_wakeup();
        break;
    }
    case GeneralAction::Stop: {
        do_stop();
        break;
    }
    default:
        BROOKESIA_LOGE("Invalid action: %1%", action_str);
        result = false;
        break;
    }

    if (!result) {
        update_general_state_error(true);
        BROOKESIA_CHECK_FALSE_RETURN(false, false, "Failed to do general action: %1%", action_str);
    }

    BROOKESIA_LOGI("Agent do '%1%' finished", action_str);

    return true;
}

bool Base::do_suspend()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_suspended()) {
        BROOKESIA_LOGD("Already suspended, skip");
        return true;
    }

    if (!is_general_event_ready(GeneralEvent::Started) || is_general_action_running(GeneralAction::Stop)) {
        BROOKESIA_LOGD("Started event is not ready or stop action is running, skip suspend");
        return true;
    }

    is_suspended_ = true;

    lib_utils::FunctionGuard resume_guard([this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        do_resume();
    });

    BROOKESIA_CHECK_FALSE_RETURN(on_suspend(), false, "Failed to suspend");
    stop_audio_encoder();
    stop_audio_decoder();

    resume_guard.release();

    if (!Manager::get_instance().publish_event(
    BROOKESIA_DESCRIBE_ENUM_TO_STR(ManagerHelper::EventId::SuspendStatusChanged), service::EventItemMap{
    {BROOKESIA_DESCRIBE_TO_STR(ManagerHelper::EventSuspendStatusChangedParam::IsSuspended), true}
    }
            )) {
        BROOKESIA_LOGE("Failed to publish suspend status changed event");
    }

    return true;
}

bool Base::do_resume()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_suspended()) {
        BROOKESIA_LOGD("Not suspended, skip");
        return true;
    }

    // Start the audio decoder
    BROOKESIA_CHECK_FALSE_RETURN(
        start_audio_decoder(), false, "Failed to start audio decoder"
    );
    lib_utils::FunctionGuard stop_decoder_guard([this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        stop_audio_decoder();
    });

    // Start the encoder
    BROOKESIA_CHECK_FALSE_RETURN(
        start_audio_encoder(), false, "Failed to start encoder"
    );
    lib_utils::FunctionGuard stop_encoder_guard([this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        stop_audio_encoder();
    });

    BROOKESIA_CHECK_FALSE_RETURN(on_resume(), false, "Failed to resume");

    is_suspended_ = false;

    stop_decoder_guard.release();
    stop_encoder_guard.release();

    if (!Manager::get_instance().publish_event(
    BROOKESIA_DESCRIBE_ENUM_TO_STR(ManagerHelper::EventId::SuspendStatusChanged), service::EventItemMap{
    {BROOKESIA_DESCRIBE_TO_STR(ManagerHelper::EventSuspendStatusChangedParam::IsSuspended), false}
    }
            )) {
        BROOKESIA_LOGE("Failed to publish suspend status changed event");
    }

    return true;
}

bool Base::do_interrupt_speaking()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_FALSE_RETURN(
        get_attributes().is_general_functions_supported(ManagerHelper::AgentGeneralFunction::InterruptSpeaking), false,
        "Agent does not support general function '%1%'",
        BROOKESIA_DESCRIBE_TO_STR(ManagerHelper::AgentGeneralFunction::InterruptSpeaking)
    );

    if (!is_speaking()) {
        BROOKESIA_LOGD("Not speaking, skip");
        return true;
    }

    is_interrupted_speaking_ = true;

    lib_utils::FunctionGuard reset_guard([this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        reset_interrupted_speaking();
    });

    BROOKESIA_CHECK_FALSE_RETURN(on_interrupt_speaking(), false, "Failed to interrupt speaking");

    reset_guard.release();

    return true;
}

bool Base::do_manual_start_listening()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_FALSE_RETURN(!is_suspended(), false, "Suspended, should resume first");

    if (is_manual_listening()) {
        BROOKESIA_LOGD("Already listening, skip");
        return true;
    }

    if (!is_general_event_ready(GeneralEvent::Started) || is_general_action_running(GeneralAction::Stop)) {
        BROOKESIA_LOGE("Not started or stop action is running, failed");
        return false;
    }

    if (get_attributes().is_general_functions_supported(ManagerHelper::AgentGeneralFunction::InterruptSpeaking)) {
        if (!do_interrupt_speaking()) {
            BROOKESIA_LOGW("Failed to interrupt speaking");
        }
    }

    BROOKESIA_CHECK_FALSE_RETURN(on_manual_start_listening(), false, "Failed to manually start listening");

    is_manual_listening_ = true;

    // Must be called after `is_manual_listening_` is changed
    if (!set_listening(true)) {
        BROOKESIA_LOGW("Failed to set listening to true");
    }

    return true;
}

bool Base::do_manual_stop_listening()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_manual_listening()) {
        BROOKESIA_LOGD("Not listening, skip");
        return true;
    }

    BROOKESIA_CHECK_FALSE_RETURN(on_manual_stop_listening(), false, "Failed to manually stop listening");

    is_manual_listening_ = false;

    // Must be called after `is_manual_listening_` is changed
    if (!set_listening(false)) {
        BROOKESIA_LOGW("Failed to set listening to false");
    }

    return true;
}

bool Base::start_audio_decoder()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_decoder_started()) {
        BROOKESIA_LOGD("Already started, skip");
        return true;
    }

    auto &decoder_config = get_audio_config().decoder;
    AudioHelper::call_function_async(
        AudioHelper::FunctionId::StartDecoder, BROOKESIA_DESCRIBE_TO_JSON(decoder_config).as_object()
    );

    is_decoder_started_ = true;

    return true;
}

void Base::stop_audio_decoder()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_decoder_started()) {
        BROOKESIA_LOGD("Not started, skip");
        return;
    }

    AudioHelper::call_function_async(AudioHelper::FunctionId::StopDecoder);
    is_decoder_started_ = false;
}

bool Base::start_audio_encoder()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_encoder_started()) {
        BROOKESIA_LOGD("Already started, skip");
        return true;
    }

    // Subscribe to the recorder data ready event
    auto encoder_data_ready_slot = [this](const std::string & event_name, const service::RawBuffer & item) {
        // BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

        // BROOKESIA_LOGD("Params: event_name(%1%), item(%2%)", event_name, BROOKESIA_DESCRIBE_TO_STR(item));

        // Skip if the agent is speaking disabled
        if (is_listening_disabled()) {
            return;
        }

        BROOKESIA_CHECK_FALSE_EXIT(
            on_encoder_data_ready(item.to_const_ptr<uint8_t>(), item.data_size), "Failed to handle encoder data ready"
        );
    };
    encoder_data_ready_connection_ = AudioHelper::subscribe_event(
                                         AudioHelper::EventId::EncoderDataReady, encoder_data_ready_slot
                                     );
    BROOKESIA_CHECK_FALSE_RETURN(
        encoder_data_ready_connection_.connected(), false, "Failed to subscribe to encoder data ready event"
    );

#if BROOKESIA_AGENT_MANAGER_ENABLE_AFE_EVENT_PROCESSING
    // Subscribe to the afe event
    auto afe_event_happened_slot = [this](const std::string & event_name, const std::string & event) {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

        BROOKESIA_LOGD("Params: event_name(%1%), event(%2%)", event_name, event);

        AudioHelper::AFE_Event afe_event;
        BROOKESIA_CHECK_FALSE_EXIT(
            BROOKESIA_DESCRIBE_STR_TO_ENUM(event, afe_event), "Failed to convert afe event: %1%", event
        );

        switch (afe_event) {
        case AudioHelper::AFE_Event::WakeStart:
            if (is_suspended()) {
                BROOKESIA_LOGD("Suspended, skip");
                return;
            }
            if (is_listening()) {
                BROOKESIA_LOGD("Already listening, skip");
                return;
            }
            if (is_speaking() &&
                    get_attributes().is_general_functions_supported(ManagerHelper::AgentGeneralFunction::InterruptSpeaking)) {
                do_interrupt_speaking();
            }
            if (is_general_event_ready(GeneralEvent::Slept) || is_general_action_running(GeneralAction::Sleep)) {
                if (callbacks_.afe_event_happened) {
                    callbacks_.afe_event_happened(AudioHelper::AFE_Event::WakeStart);
                }
            }
            break;
        case AudioHelper::AFE_Event::WakeEnd:
            if (is_suspended()) {
                BROOKESIA_LOGD("Suspended, skip");
                return;
            }
            if (!is_general_event_ready(GeneralEvent::Awake) && !is_general_action_running(GeneralAction::WakeUp)) {
                BROOKESIA_LOGD("Not awake or wake up action is running, skip");
                return;
            }
            if (callbacks_.afe_event_happened) {
                callbacks_.afe_event_happened(AudioHelper::AFE_Event::WakeEnd);
            }
            break;
        case AudioHelper::AFE_Event::VAD_Start:
            break;
        case AudioHelper::AFE_Event::VAD_End:
            break;
        default:
            break;
        }
    };
    afe_event_happened_connection_ = AudioHelper::subscribe_event(
                                         AudioHelper::EventId::AFE_EventHappened, afe_event_happened_slot
                                     );
    BROOKESIA_CHECK_FALSE_RETURN(
        afe_event_happened_connection_.connected(), false, "Failed to subscribe to afe event"
    );
#endif

    // Start the encoder
    auto &encoder_config = get_audio_config().encoder;
    AudioHelper::call_function_async(
        AudioHelper::FunctionId::StartEncoder, BROOKESIA_DESCRIBE_TO_JSON(encoder_config).as_object()
    );

    is_encoder_started_ = true;

    return true;
}

void Base::stop_audio_encoder()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_encoder_started()) {
        BROOKESIA_LOGD("Not started, skip");
        return;
    }

    afe_event_happened_connection_.disconnect();
    encoder_data_ready_connection_.disconnect();
    AudioHelper::call_function_async(AudioHelper::FunctionId::StopEncoder);

    is_encoder_started_ = false;
}

bool Base::is_general_action_running(GeneralAction action)
{
    if (action == GeneralAction::Max) {
        auto running_action = get_running_general_action();
        if (running_action != GeneralAction::Max) {
            return true;
        }
        return false;
    }

    return action == get_running_general_action();
}

bool Base::is_general_event_ready(GeneralEvent event)
{
    auto [flag_bit, bit_value] = get_general_event_state_flag_bit(event);
    BROOKESIA_CHECK_FALSE_RETURN(
        flag_bit != GeneralStateFlagBit::Max, false, "Invalid event: %1%", BROOKESIA_DESCRIBE_TO_STR(event)
    );

    if ((event == GeneralEvent::Stopped) && is_general_action_running(GeneralAction::Activate)) {
        // BROOKESIA_LOGD("Allow for failed action");
        return false;
    }

    return state_flags_.test(BROOKESIA_DESCRIBE_ENUM_TO_NUM(flag_bit)) == bit_value;
}

bool Base::is_general_event_unexpected(GeneralEvent event)
{
    if (get_running_general_action() == get_general_action_from_target_event(event)) {
        return false;
    }

    switch (event) {
    case GeneralEvent::Stopped:
        return is_general_action_running(GeneralAction::Activate) || is_general_event_ready(GeneralEvent::Activated);
    case GeneralEvent::Slept:
        return is_general_action_running(GeneralAction::WakeUp) || is_general_event_ready(GeneralEvent::Awake);
    default:
        return false;
    }
}

GeneralAction Base::get_running_general_action()
{
    for (auto action = static_cast<GeneralAction>(0); action < GeneralAction::Max;
            action = static_cast<GeneralAction>(static_cast<int>(action) + 1)) {
        auto flag_bit = get_general_action_state_flag_bit(action);
        // Only handle transient state flag bits
        BROOKESIA_CHECK_FALSE_RETURN(
            flag_bit != GeneralStateFlagBit::Max, GeneralAction::Max, "Invalid action: %1%",
            BROOKESIA_DESCRIBE_TO_STR(action)
        );
        if (state_flags_.test(BROOKESIA_DESCRIBE_ENUM_TO_NUM(flag_bit))) {
            return action;
        }
    }
    return GeneralAction::Max;
}

void Base::update_general_action_state_bit(GeneralAction action, bool enable)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD(
        "Params: action(%1%), enable(%2%)", BROOKESIA_DESCRIBE_TO_STR(action), BROOKESIA_DESCRIBE_TO_STR(enable)
    );

    if (action == GeneralAction::Max) {
        BROOKESIA_LOGD("Invalid action, skip");
        return;
    }

    auto flag_bit = get_general_action_state_flag_bit(action);
    if (flag_bit == GeneralStateFlagBit::Max) {
        BROOKESIA_LOGD("Invalid action, skip");
        return;
    }

    BROOKESIA_LOGD(
        "Target bit(%1%), original state flags(%2%)", BROOKESIA_DESCRIBE_TO_STR(flag_bit),
        BROOKESIA_DESCRIBE_TO_STR(state_flags_)
    );

    state_flags_.set(BROOKESIA_DESCRIBE_ENUM_TO_NUM(flag_bit), enable);

    BROOKESIA_LOGD(
        "New state flags(%1%)", BROOKESIA_DESCRIBE_TO_STR(state_flags_)
    );
}

void Base::update_general_event_state_bit(GeneralEvent event, bool enable)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD(
        "Params: event(%1%), enable(%2%)", BROOKESIA_DESCRIBE_TO_STR(event), BROOKESIA_DESCRIBE_TO_STR(enable)
    );

    if (event == GeneralEvent::Max) {
        BROOKESIA_LOGD("Invalid event, skip");
        return;
    }

    auto [flag_bit, bit_value] = get_general_event_state_flag_bit(event);
    if (flag_bit == GeneralStateFlagBit::Max) {
        BROOKESIA_LOGD("Invalid event, skip");
        return;
    }

    BROOKESIA_LOGD(
        "Target bit(%1%), original state flags(%2%)", BROOKESIA_DESCRIBE_TO_STR(flag_bit),
        BROOKESIA_DESCRIBE_TO_STR(state_flags_)
    );

    state_flags_.set(BROOKESIA_DESCRIBE_ENUM_TO_NUM(flag_bit), enable ? bit_value : !bit_value);

    BROOKESIA_LOGD(
        "New state flags(%1%)", BROOKESIA_DESCRIBE_TO_STR(state_flags_)
    );
}

void Base::reset_general_state_flag_bits()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Original state flags(%1%)", BROOKESIA_DESCRIBE_TO_STR(state_flags_));

    state_flags_.reset();
    state_flags_.set(BROOKESIA_DESCRIBE_ENUM_TO_NUM(GeneralStateFlagBit::Ready), true);

    BROOKESIA_LOGD("New state flags(%1%)", BROOKESIA_DESCRIBE_TO_STR(state_flags_));
}

void Base::clear_general_state_flag_bits()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Original state flags(%1%)", BROOKESIA_DESCRIBE_TO_STR(state_flags_));

    state_flags_.reset();

    BROOKESIA_LOGD("New state flags(%1%)", BROOKESIA_DESCRIBE_TO_STR(state_flags_));
}

void Base::update_general_state_error(bool enable)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: enable(%1%)", BROOKESIA_DESCRIBE_TO_STR(enable));

    state_flags_.set(BROOKESIA_DESCRIBE_ENUM_TO_NUM(GeneralStateFlagBit::Error), enable);
}

GeneralEvent Base::get_general_action_target_event(GeneralAction action)
{
    switch (action) {
    case GeneralAction::TimeSync:
        return GeneralEvent::TimeSynced;
    case GeneralAction::Activate:
        return GeneralEvent::Activated;
    case GeneralAction::Start:
        return GeneralEvent::Started;
    case GeneralAction::Stop:
        return GeneralEvent::Stopped;
    case GeneralAction::Sleep:
        return GeneralEvent::Slept;
    case GeneralAction::WakeUp:
        return GeneralEvent::Awake;
    default:
        return GeneralEvent::Max;
    }
}

GeneralAction Base::get_general_action_from_target_event(GeneralEvent event)
{
    switch (event) {
    case GeneralEvent::TimeSynced:
        return GeneralAction::TimeSync;
    case GeneralEvent::Activated:
        return GeneralAction::Activate;
    case GeneralEvent::Started:
        return GeneralAction::Start;
    case GeneralEvent::Stopped:
        return GeneralAction::Stop;
    case GeneralEvent::Slept:
        return GeneralAction::Sleep;
    case GeneralEvent::Awake:
        return GeneralAction::WakeUp;
    default:
        return GeneralAction::Max;
    }
}

GeneralStateFlagBit Base::get_general_action_state_flag_bit(GeneralAction action)
{
    switch (action) {
    case GeneralAction::TimeSync:
        return GeneralStateFlagBit::TimeSyncing;
    case GeneralAction::Activate:
        return GeneralStateFlagBit::Activating;
    case GeneralAction::Start:
        return GeneralStateFlagBit::Starting;
    case GeneralAction::Stop:
        return GeneralStateFlagBit::Stopping;
    case GeneralAction::Sleep:
        return GeneralStateFlagBit::Sleeping;
    case GeneralAction::WakeUp:
        return GeneralStateFlagBit::WakingUp;
    default:
        return GeneralStateFlagBit::Max;
    }
}

std::pair<GeneralStateFlagBit, bool> Base::get_general_event_state_flag_bit(GeneralEvent event)
{
    bool bit_value = true;
    GeneralStateFlagBit flag_bit = GeneralStateFlagBit::Max;
    switch (event) {
    case GeneralEvent::TimeSynced:
        flag_bit = GeneralStateFlagBit::TimeSyncing;
        break;
    case GeneralEvent::Activated:
        flag_bit = GeneralStateFlagBit::Activated;
        break;
    case GeneralEvent::Started:
        flag_bit = GeneralStateFlagBit::Started;
        break;
    case GeneralEvent::Stopped:
        flag_bit = GeneralStateFlagBit::Activated;
        bit_value = false;
        break;
    case GeneralEvent::Slept:
        flag_bit = GeneralStateFlagBit::Slept;
        break;
    case GeneralEvent::Awake:
        flag_bit = GeneralStateFlagBit::Slept;
        bit_value = false;
        break;
    default:
        break;
    }

    return {flag_bit, bit_value};
}

} // namespace esp_brookesia::agent
