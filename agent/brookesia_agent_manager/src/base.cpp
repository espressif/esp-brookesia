/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_gmf_afe.h"
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

// constexpr uint32_t AUDIO_CALL_START_ENCORDER_TIMEOUT_MS = 1000;
// constexpr uint32_t AUDIO_CALL_STOP_ENCORDER_TIMEOUT_MS = 100;
// constexpr uint32_t AUDIO_CALL_START_DECORDER_TIMEOUT_MS = 100;
// constexpr uint32_t AUDIO_CALL_STOP_DECORDER_TIMEOUT_MS = 100;
// constexpr uint32_t AUDIO_CALL_SET_ENCODER_READ_DATA_SIZE_TIMEOUT_MS = 100;
constexpr uint32_t AUDIO_CALL_START_ENCORDER_TIMEOUT_MS = 0;
constexpr uint32_t AUDIO_CALL_STOP_ENCORDER_TIMEOUT_MS = 0;
constexpr uint32_t AUDIO_CALL_START_DECORDER_TIMEOUT_MS = 0;
constexpr uint32_t AUDIO_CALL_STOP_DECORDER_TIMEOUT_MS = 0;
constexpr uint32_t AUDIO_CALL_SET_ENCODER_READ_DATA_SIZE_TIMEOUT_MS = 0;
constexpr size_t   AUDIO_ENCODER_FEED_DATA_SIZE_MORE = 100;

void Base::trigger_general_event(GeneralEvent event)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto [event_flag_bit, event_bit_value] = get_general_event_state_flag_bit(event);

#if BROOKESIA_UTILS_LOG_LEVEL <= BROOKESIA_UTILS_LOG_LEVEL_DEBUG
    BROOKESIA_LOGD(
        "running action(%1%)", BROOKESIA_DESCRIBE_TO_STR(get_running_general_action())
    );
    BROOKESIA_LOGD(
        "state_flags(%1%), flag bit(%2%), bit value(%3%)", BROOKESIA_DESCRIBE_TO_STR(state_flags_),
        BROOKESIA_DESCRIBE_TO_STR(event_flag_bit), BROOKESIA_DESCRIBE_TO_STR(event_bit_value)
    );
#endif
    if (is_general_event_ready(event)) {
        BROOKESIA_LOGD("Event(%1%) is already matched, skip", BROOKESIA_DESCRIBE_TO_STR(event));
        return;
    }

    auto event_action = get_general_action_from_target_event(event);
    BROOKESIA_CHECK_FALSE_EXIT(
        event_action != GeneralAction::Max, "No corresponding action for event: %1%", BROOKESIA_DESCRIBE_TO_STR(event)
    );
    auto action_flag_bit = get_general_action_state_flag_bit(event_action);
    BROOKESIA_CHECK_FALSE_EXIT(
        action_flag_bit != GeneralStateFlagBit::Max, "No corresponding flag bit for action: %1%",
        BROOKESIA_DESCRIBE_TO_STR(event_action)
    );

    bool is_event_action_running = is_general_action_running(event_action);
    bool is_unexpected_event = false;
    if (is_event_action_running) {
        BROOKESIA_LOGD("Event action(%1%) is already running, clear bit(%2%)", BROOKESIA_DESCRIBE_TO_STR(event_action), BROOKESIA_DESCRIBE_TO_STR(action_flag_bit));
        // Clear action running bit
        state_flags_.reset(BROOKESIA_DESCRIBE_ENUM_TO_NUM(action_flag_bit));
    } else if (is_general_event_unexpected(event)) {
        is_unexpected_event = true;
        BROOKESIA_LOGW(
            "Event action(%1%) is not running, unexpected event: %2%", BROOKESIA_DESCRIBE_TO_STR(event_action),
            BROOKESIA_DESCRIBE_TO_STR(event)
        );
        auto running_action = get_running_general_action();
        if (running_action != GeneralAction::Max) {
            auto running_action_flag_bit = get_general_action_state_flag_bit(running_action);
            BROOKESIA_LOGD(
                "Clear running action(%1%) bit(%2%)", BROOKESIA_DESCRIBE_TO_STR(running_action),
                BROOKESIA_DESCRIBE_TO_STR(running_action_flag_bit)
            );
            state_flags_.reset(BROOKESIA_DESCRIBE_ENUM_TO_NUM(running_action_flag_bit));
        } else {
            BROOKESIA_LOGD("No running action, skip");
        }

        // Force do the event action to clear the unexpected event
        BROOKESIA_LOGW("Force to do the event action: %1%", BROOKESIA_DESCRIBE_TO_STR(event_action));
        BROOKESIA_CHECK_FALSE_EXECUTE(do_general_action(event_action, true), {}, {
            BROOKESIA_LOGE("Failed to do general action: %1%", BROOKESIA_DESCRIBE_TO_STR(event_action));
        });
    } else {
        BROOKESIA_LOGW("Invalid event: %1%, skip", BROOKESIA_DESCRIBE_TO_STR(event));
        return;
    }

    // Set event matched bit
    BROOKESIA_LOGD("Set event bit(%1%), value(%2%)", BROOKESIA_DESCRIBE_TO_STR(event_flag_bit), event_bit_value);
    state_flags_.set(BROOKESIA_DESCRIBE_ENUM_TO_NUM(event_flag_bit), event_bit_value);

    if (callbacks_.general_event_happened_callback) {
        callbacks_.general_event_happened_callback(event, is_unexpected_event);
    }
}

bool Base::feed_audio_decoder_data(const uint8_t *data, size_t data_size)
{
    // BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    // BROOKESIA_LOGD("Params: data(%1%), data_size(%1%)", data, data_size);

    // Skip if the agent is speaking disabled
    if (is_speaking_disabled()) {
        return true;
    }

    auto result = AudioHelper::call_function_sync<void>(AudioHelper::FunctionId::FeedDecoderData,
    service::FunctionParameterMap{
        {
            BROOKESIA_DESCRIBE_TO_STR(AudioHelper::FunctionFeedDecoderDataParam::Data),
            service::RawBuffer(data, data_size)
        }
    }, 0);
    if (!result) {
        BROOKESIA_LOGE("Failed to feed audio data: %1%", result.error());
        return false;
    }

    return true;
}

bool Base::publish_service_event(const std::string &event, service::EventItemMap &&items, bool use_dispatch)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    return Manager::get_instance().publish_event(event, std::move(items), use_dispatch);
}

std::shared_ptr<lib_utils::TaskScheduler> Base::get_service_scheduler()
{
    return Manager::get_instance().get_task_scheduler();
}

bool Base::init()
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

    BROOKESIA_CHECK_FALSE_RETURN(on_init(), false, "Failed to initialize agent '%1%'", get_attributes().name);

    deinit_guard.release();

    BROOKESIA_LOGI("Initialized agent: %1%", get_attributes().name);

    return true;
}

void Base::deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_initialized()) {
        BROOKESIA_LOGD("Not initialized");
        return;
    }

    on_deinit();

    is_initialized_ = false;

    BROOKESIA_LOGI("Deinitialized agent: %1%", get_attributes().name);
}

bool Base::activate()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_active()) {
        BROOKESIA_LOGD("Already active, skip");
        return true;
    }

    is_active_ = true;

    lib_utils::FunctionGuard deactivate_guard([this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        deactivate();
    });

    BROOKESIA_CHECK_FALSE_RETURN(on_activate(), false, "Failed to activate agent");

    auto function_schemas = get_function_schemas();
    auto function_handlers = get_function_handlers();
    if (!function_schemas.empty()) {
        if (!Manager::get_instance().register_functions(std::move(function_schemas), std::move(function_handlers))) {
            BROOKESIA_LOGE("Failed to register functions");
        }
    }

    auto event_schemas = get_event_schemas();
    if (!event_schemas.empty()) {
        if (!Manager::get_instance().register_events(std::move(event_schemas))) {
            BROOKESIA_LOGE("Failed to register events");
        }
    }

    deactivate_guard.release();

    return true;
}

void Base::deactivate()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_active()) {
        BROOKESIA_LOGD("Not active, skip");
        return;
    }

    on_deactivate();

    // Stop the agent
    do_general_action(GeneralAction::Stop);

    is_active_ = false;
    state_flags_.reset();

    auto function_schemas = get_function_schemas();
    auto function_names = std::vector<std::string>();
    for (const auto &schema : function_schemas) {
        function_names.push_back(schema.name);
    }
    if (!function_names.empty()) {
        Manager::get_instance().unregister_functions(function_names);
    }

    auto event_schemas = get_event_schemas();
    auto event_names = std::vector<std::string>();
    for (const auto &schema : event_schemas) {
        event_names.push_back(schema.name);
    }
    if (!event_names.empty()) {
        Manager::get_instance().unregister_events(event_names);
    }
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
    BROOKESIA_CHECK_FALSE_RETURN(on_start(), false, "Failed to start");
    lib_utils::FunctionGuard stop_agent_guard([this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        on_stop();
    });

    stop_decoder_guard.release();
    stop_agent_guard.release();
    stop_encoder_guard.release();

    return true;
}

void Base::do_stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    on_stop();
    stop_audio_encoder();
    stop_audio_decoder();

    is_suspended_ = false;
    is_interrupted_speaking_ = false;
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

    return true;
}

void Base::do_wakeup()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    on_wakeup();
}

bool Base::do_general_action(GeneralAction action, bool is_force)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: action(%1%), is_force(%2%)", BROOKESIA_DESCRIBE_TO_STR(action), is_force);

    if (is_general_action_running(action)) {
        BROOKESIA_LOGD("Action(%1%) is already running, skip", BROOKESIA_DESCRIBE_TO_STR(action));
        return true;
    }

    if (!is_force) {
        BROOKESIA_LOGD("Not force, check if the event is ready");
        GeneralEvent target_event = get_general_action_target_event(action);
#if BROOKESIA_UTILS_LOG_LEVEL <= BROOKESIA_UTILS_LOG_LEVEL_DEBUG
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

    BROOKESIA_LOGI("Agent '%1%' running", BROOKESIA_DESCRIBE_TO_STR(action));

    if (callbacks_.general_action_triggered_callback) {
        callbacks_.general_action_triggered_callback(action);
    }

    auto state_flag_bit = get_general_action_state_flag_bit(action);
    if (state_flag_bit != GeneralStateFlagBit::Max) {
        BROOKESIA_LOGD("Set action bit(%1%)", BROOKESIA_DESCRIBE_TO_STR(state_flag_bit));
        state_flags_.set(BROOKESIA_DESCRIBE_ENUM_TO_NUM(state_flag_bit));
    }

    lib_utils::FunctionGuard restore_state_flag_guard([this, state_flag_bit]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        if (state_flag_bit != GeneralStateFlagBit::Max) {
            BROOKESIA_LOGD("Reset action bit(%1%)", BROOKESIA_DESCRIBE_TO_STR(state_flag_bit));
            state_flags_.reset(BROOKESIA_DESCRIBE_ENUM_TO_NUM(state_flag_bit));
        }
    });

    bool result = true;
    switch (action) {
    case GeneralAction::Start: {
        result = do_start();
        break;
    }
    case GeneralAction::Stop: {
        do_stop();
        break;
    }
    case GeneralAction::Sleep: {
        result = do_sleep();
        break;
    }
    case GeneralAction::WakeUp: {
        do_wakeup();
        break;
    }
    default:
        BROOKESIA_LOGE("Invalid action: %1%", BROOKESIA_DESCRIBE_TO_STR(action));
        result = false;
        break;
    }
    BROOKESIA_CHECK_FALSE_RETURN(result, false, "Failed to do general action: %1%", BROOKESIA_DESCRIBE_TO_STR(action));

    restore_state_flag_guard.release();

    BROOKESIA_LOGI("Agent '%1%' finished", BROOKESIA_DESCRIBE_TO_STR(action));

    return true;
}

bool Base::do_suspend()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_suspended()) {
        BROOKESIA_LOGD("Already suspended, skip");
        return true;
    }

    is_suspended_ = true;

    lib_utils::FunctionGuard resume_guard([this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        do_resume();
    });

    BROOKESIA_CHECK_FALSE_RETURN(on_suspend(), false, "Failed to suspend");

    if (callbacks_.suspend_status_changed_callback) {
        callbacks_.suspend_status_changed_callback(true);
    }

    resume_guard.release();

    return true;
}

void Base::do_resume()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_suspended()) {
        BROOKESIA_LOGD("Not suspended, skip");
        return;
    }

    on_resume();

    if (callbacks_.suspend_status_changed_callback) {
        callbacks_.suspend_status_changed_callback(false);
    }

    is_suspended_ = false;
}

bool Base::do_interrupt_speaking()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_FALSE_RETURN(
        get_attributes().support_interrupt_speaking, false, "Agent does not support interrupt speaking"
    );

    is_interrupted_speaking_ = true;

    lib_utils::FunctionGuard reset_guard([this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        reset_interrupted_speaking();
    });

    BROOKESIA_CHECK_FALSE_RETURN(on_interrupt_speaking(), false, "Failed to interrupt speaking");

    reset_guard.release();

    return true;
}

bool Base::start_audio_decoder()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto &decoder_config = get_audio_config().decoder;
    if constexpr (AUDIO_CALL_START_DECORDER_TIMEOUT_MS > 0) {
        auto result = AudioHelper::call_function_sync<void>(AudioHelper::FunctionId::StartDecoder,
        service::FunctionParameterMap{
            {
                BROOKESIA_DESCRIBE_TO_STR(AudioHelper::FunctionStartDecoderParam::Config),
                BROOKESIA_DESCRIBE_TO_JSON(decoder_config).as_object()
            }
        }, AUDIO_CALL_START_DECORDER_TIMEOUT_MS);
        if (!result) {
            BROOKESIA_LOGE("Failed to start audio decoder: %1%", result.error());
            return false;
        }
    } else {
        AudioHelper::call_function_async(AudioHelper::FunctionId::StartDecoder,
        service::FunctionParameterMap{
            {
                BROOKESIA_DESCRIBE_TO_STR(AudioHelper::FunctionStartDecoderParam::Config),
                BROOKESIA_DESCRIBE_TO_JSON(decoder_config).as_object()
            }
        });
    }

    return true;
}

void Base::stop_audio_decoder()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if constexpr (AUDIO_CALL_STOP_DECORDER_TIMEOUT_MS > 0) {
        auto result = AudioHelper::call_function_sync<void>(
                          AudioHelper::FunctionId::StopDecoder, {}, AUDIO_CALL_STOP_DECORDER_TIMEOUT_MS
                      );
        if (!result) {
            BROOKESIA_LOGE("Failed to stop audio decoder: %1%", result.error());
        }
    } else {
        AudioHelper::call_function_async(AudioHelper::FunctionId::StopDecoder, {});
    }
}

bool Base::start_audio_encoder()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto &audio_config = get_audio_config();

    // Get the agent audio data size
    size_t audio_data_size = audio_config.encoder_feed_data_size;
    if (audio_config.encoder.type == AudioHelper::CodecFormat::PCM) {
        auto &general = audio_config.encoder.general;
        audio_data_size = general.frame_duration * general.sample_rate * general.channels *
                          general.sample_bits / 8 / 1000 + AUDIO_ENCODER_FEED_DATA_SIZE_MORE;
        audio_data_size = audio_data_size > 0 ? audio_data_size : audio_config.encoder_feed_data_size;
    }
    if constexpr (AUDIO_CALL_SET_ENCODER_READ_DATA_SIZE_TIMEOUT_MS > 0) {
        auto result = AudioHelper::call_function_sync<void>(AudioHelper::FunctionId::SetEncoderReadDataSize,
        service::FunctionParameterMap{
            {
                BROOKESIA_DESCRIBE_TO_STR(AudioHelper::FunctionSetEncoderReadDataSizeParam::Size),
                static_cast<double>(audio_data_size)
            }
        }, AUDIO_CALL_SET_ENCODER_READ_DATA_SIZE_TIMEOUT_MS);
        if (!result) {
            BROOKESIA_LOGE("Failed to set encoder read data size: %1%", result.error());
            return false;
        }
    } else {
        AudioHelper::call_function_async(AudioHelper::FunctionId::SetEncoderReadDataSize,
        service::FunctionParameterMap{
            {
                BROOKESIA_DESCRIBE_TO_STR(AudioHelper::FunctionSetEncoderReadDataSizeParam::Size),
                static_cast<double>(audio_data_size)
            }
        });
    }

    // Subscribe to the recorder data ready event
    auto encoder_data_ready_slot = [this](const std::string & event_name, const service::EventItemMap & event_items) {
        // BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

        // BROOKESIA_LOGD("Params: event_name(%1%), event_items(%2%)", event_name, BROOKESIA_DESCRIBE_TO_STR(event_items));

        // Skip if the agent is speaking disabled
        if (is_listening_disabled()) {
            return;
        }

        auto item = std::get<service::RawBuffer>(
                        event_items.at(BROOKESIA_DESCRIBE_TO_STR(AudioHelper::EventEncoderDataReadyParam::Data))
                    );
        BROOKESIA_CHECK_FALSE_EXIT(
            on_encoder_data_ready(item.data_ptr, item.data_size), "Failed to handle recorder data ready"
        );
    };
    encoder_data_ready_connection_ = AudioHelper::subscribe_event(
                                         AudioHelper::EventId::EncoderDataReady, encoder_data_ready_slot
                                     );
    BROOKESIA_CHECK_FALSE_RETURN(
        encoder_data_ready_connection_.connected(), false, "Failed to subscribe to recorder data ready event"
    );

    // Subscribe to the encoder event
    auto encoder_event_happened_slot = [this](const std::string & event_name, const service::EventItemMap & event_items) {
        // BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

        // BROOKESIA_LOGD("Params: event_name(%1%), event_items(%2%)", event_name, BROOKESIA_DESCRIBE_TO_STR(event_items));

        auto item = std::get<service::RawBuffer>(
                        event_items.at(BROOKESIA_DESCRIBE_TO_STR(AudioHelper::EventEncoderEventHappenedParam::Event))
                    );
        auto afe_evt = item.to_const_ptr<esp_gmf_afe_evt_t>();
        BROOKESIA_CHECK_NULL_EXIT(afe_evt, "AFE event is null");

        switch (afe_evt->type) {
        case ESP_GMF_AFE_EVT_WAKEUP_START: {
            BROOKESIA_LOGI("wakeup start");
            break;
        }
        case ESP_GMF_AFE_EVT_WAKEUP_END:
            BROOKESIA_LOGI("wakeup end");
            break;
        case ESP_GMF_AFE_EVT_VAD_START:
            BROOKESIA_LOGI("vad start");
            break;
        case ESP_GMF_AFE_EVT_VAD_END:
            BROOKESIA_LOGI("vad end");
            break;
        case ESP_GMF_AFE_EVT_VCMD_DECT_TIMEOUT:
            BROOKESIA_LOGI("vcmd detect timeout");
            break;
        default:
            break;
        }
    };
    encoder_event_happened_connection_ = AudioHelper::subscribe_event(
            AudioHelper::EventId::EncoderEventHappened, encoder_event_happened_slot
                                         );
    BROOKESIA_CHECK_FALSE_RETURN(
        encoder_event_happened_connection_.connected(), false, "Failed to subscribe to encoder event"
    );

    // Start the encoder
    if constexpr (AUDIO_CALL_START_ENCORDER_TIMEOUT_MS > 0) {
        auto result = AudioHelper::call_function_sync<void>(AudioHelper::FunctionId::StartEncoder,
        service::FunctionParameterMap{
            {
                BROOKESIA_DESCRIBE_TO_STR(AudioHelper::FunctionStartEncoderParam::Config),
                BROOKESIA_DESCRIBE_TO_JSON(audio_config.encoder).as_object()
            }
        }, AUDIO_CALL_START_ENCORDER_TIMEOUT_MS);
        if (!result) {
            BROOKESIA_LOGE("Failed to start audio encoder: %1%", result.error());
            return false;
        }
    } else {
        AudioHelper::call_function_async(AudioHelper::FunctionId::StartEncoder,
        service::FunctionParameterMap{
            {
                BROOKESIA_DESCRIBE_TO_STR(AudioHelper::FunctionStartEncoderParam::Config),
                BROOKESIA_DESCRIBE_TO_JSON(audio_config.encoder).as_object()
            }
        });
    }

    return true;
}

void Base::stop_audio_encoder()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    encoder_event_happened_connection_.disconnect();
    encoder_data_ready_connection_.disconnect();

    if constexpr (AUDIO_CALL_STOP_ENCORDER_TIMEOUT_MS > 0) {
        auto result = AudioHelper::call_function_sync<void>(
                          AudioHelper::FunctionId::StopEncoder, {}, AUDIO_CALL_STOP_ENCORDER_TIMEOUT_MS
                      );
        if (!result) {
            BROOKESIA_LOGE("Failed to stop audio encoder: %1%", result.error());
        }
    } else {
        AudioHelper::call_function_async(AudioHelper::FunctionId::StopEncoder, {});
    }
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

    if ((event == GeneralEvent::Stopped) && is_general_action_running(GeneralAction::Start)) {
        // BROOKESIA_LOGD("Allow for failed action");
        return false;
    }

    return state_flags_.test(BROOKESIA_DESCRIBE_ENUM_TO_NUM(flag_bit)) == bit_value;
}

bool Base::is_general_event_unexpected(GeneralEvent event)
{
    switch (event) {
    case GeneralEvent::Stopped:
        return is_general_action_running(GeneralAction::Start) || is_general_event_ready(GeneralEvent::Started);
    case GeneralEvent::Slept:
        return is_general_action_running(GeneralAction::WakeUp) || is_general_event_ready(GeneralEvent::Awake);
    default:
        return false;
    }
}

GeneralAction Base::get_running_general_action()
{
    for (auto action = GeneralAction::Start; action < GeneralAction::Max;
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

GeneralEvent Base::get_general_action_failed_event(GeneralAction action)
{
    switch (action) {
    case GeneralAction::Start:
        return GeneralEvent::Stopped;
    case GeneralAction::Sleep:
        return GeneralEvent::Awake;
    default:
        return GeneralEvent::Max;
    }
}

GeneralEvent Base::get_general_action_target_event(GeneralAction action)
{
    switch (action) {
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
    case GeneralEvent::Started:
        flag_bit = GeneralStateFlagBit::Started;
        break;
    case GeneralEvent::Stopped:
        flag_bit = GeneralStateFlagBit::Started;
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
