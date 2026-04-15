/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "sdkconfig.h"
#include "brookesia/service_manager.hpp"
#include "brookesia/service_helper.hpp"
#include "brookesia/agent_helper.hpp"
#include "private/utils.hpp"
#include "ai_agents.hpp"

using namespace esp_brookesia;

using AgentHelper = esp_brookesia::agent::helper::Manager;
using XiaoZhiHelper = esp_brookesia::agent::helper::XiaoZhi;
using EmoteHelper = esp_brookesia::service::helper::ExpressionEmote;
using AudioHelper = esp_brookesia::service::helper::Audio;

#define XIAO_ZHI_AUDIO_URL_PREFIX "file://spiffs/xiaozhi/"

constexpr const char *AUDIO_WAKEUP_WORD_MODEL_PARTITION_LABEL = "model";
constexpr const char *AUDIO_WAKEUP_WORD_MN_LANGUAGE = "cn";

bool AI_Agents::init(const Config &config)
{
    BROOKESIA_CHECK_NULL_RETURN(config.task_scheduler, false, "Task scheduler is not available");

    if (is_initialized()) {
        BROOKESIA_LOGW("AI agents are already initialized");
        return true;
    }

    if (!AgentHelper::is_available()) {
        BROOKESIA_LOGW("Agent manager is not available, skip initialization");
        return false;
    }

    // Configure the AFE
    AudioHelper::AFE_Config afe_config{
        .vad = AudioHelper::AFE_VAD_Config{},
        .wakenet = AudioHelper::AFE_WakeNetConfig{
            .model_partition_label = AUDIO_WAKEUP_WORD_MODEL_PARTITION_LABEL,
            .mn_language = AUDIO_WAKEUP_WORD_MN_LANGUAGE,
            .start_timeout_ms = config.afe_wakeup_start_timeout_ms,
            .end_timeout_ms = config.afe_wakeup_end_timeout_ms,
        },
    };
    auto set_afe_result = AudioHelper::call_function_sync(
                              AudioHelper::FunctionId::SetAFE_Config, BROOKESIA_DESCRIBE_TO_JSON(afe_config).as_object()
                          );
    BROOKESIA_CHECK_FALSE_RETURN(set_afe_result, false, "Failed to set audio AFE config: %1%", set_afe_result.error());

    task_scheduler_ = config.task_scheduler;
    emote_animation_duration_ms_ = config.emote_animation_duration_ms;

    process_agent_general_events();
    process_emote();

    return true;
}

void AI_Agents::process_agent_general_suspend_status_changed()
{
    auto slot = [this](const std::string & event_name, bool is_suspended) {
        BROOKESIA_LOG_TRACE_GUARD();

        BROOKESIA_LOGD("Params: event_name(%1%), is_suspended(%2%)", event_name, is_suspended);

        if (is_suspended) {
            // Trigger sleep action
            AgentHelper::call_function_async(
                AgentHelper::FunctionId::TriggerGeneralAction,
                BROOKESIA_DESCRIBE_TO_STR(AgentHelper::GeneralAction::Sleep)
            );
        }
        is_suspended_ = is_suspended;
    };
    auto connection = AgentHelper::subscribe_event(AgentHelper::EventId::SuspendStatusChanged, slot);
    if (connection.connected()) {
        service_connections_.push_back(std::move(connection));
    } else {
        BROOKESIA_LOGE("Failed to subscribe to Agent general suspend status changed event");
    }
}

void AI_Agents::process_agent_general_events()
{
    process_agent_general_suspend_status_changed();
}

void AI_Agents::process_emote_when_general_action_triggered()
{
    auto slot = [this](const std::string & event_name, const std::string & general_action) {
        BROOKESIA_LOG_TRACE_GUARD();

        BROOKESIA_LOGD("Params: event_name(%1%), general_action(%2%)", event_name, general_action);

        AgentHelper::GeneralAction general_action_enum;
        auto convert_result = BROOKESIA_DESCRIBE_STR_TO_ENUM(general_action, general_action_enum);
        BROOKESIA_CHECK_FALSE_EXIT(convert_result, "Failed to convert general action: %1%", general_action);

        switch (general_action_enum) {
        case AgentHelper::GeneralAction::TimeSync:
            EmoteHelper::call_function_async(EmoteHelper::FunctionId::SetEmoji, "winking");
            EmoteHelper::call_function_async(
                EmoteHelper::FunctionId::SetEventMessage,
                BROOKESIA_DESCRIBE_TO_STR(EmoteHelper::EventMessageType::System), "[Agent] Time Syncing..."
            );
            break;
        case AgentHelper::GeneralAction::Activate:
            EmoteHelper::call_function_async(EmoteHelper::FunctionId::SetEmoji, "winking");
            EmoteHelper::call_function_async(
                EmoteHelper::FunctionId::SetEventMessage,
                BROOKESIA_DESCRIBE_TO_STR(EmoteHelper::EventMessageType::System), "[Agent] Activating..."
            );
            break;
        case AgentHelper::GeneralAction::Start:
            is_sleeping_ = false;
            is_suspended_ = false;
            is_stopped_ = false;
            EmoteHelper::call_function_async(
                EmoteHelper::FunctionId::SetEventMessage,
                BROOKESIA_DESCRIBE_TO_STR(EmoteHelper::EventMessageType::System), "[Agent] Starting..."
            );
            break;
        case AgentHelper::GeneralAction::Sleep: {
            is_sleeping_ = true;
            EmoteHelper::call_function_async(EmoteHelper::FunctionId::SetEmoji, "sleepy");
            if (!is_suspended()) {
                EmoteHelper::call_function_async(
                    EmoteHelper::FunctionId::SetEventMessage,
                    BROOKESIA_DESCRIBE_TO_STR(EmoteHelper::EventMessageType::Idle)
                );
            }
            break;
        }
        case AgentHelper::GeneralAction::WakeUp:
            is_sleeping_ = false;
            EmoteHelper::call_function_async(
                EmoteHelper::FunctionId::SetEventMessage,
                BROOKESIA_DESCRIBE_TO_STR(EmoteHelper::EventMessageType::System), "[Agent] Waking Up..."
            );
            break;
        case AgentHelper::GeneralAction::Stop:
            is_stopped_ = true;
            EmoteHelper::call_function_async(EmoteHelper::FunctionId::HideEmoji);
            EmoteHelper::call_function_async(
                EmoteHelper::FunctionId::SetEventMessage,
                BROOKESIA_DESCRIBE_TO_STR(EmoteHelper::EventMessageType::System), "[Agent] Stopping..."
            );
            break;
        default:
            break;
        }
    };
    auto connection = AgentHelper::subscribe_event(AgentHelper::EventId::GeneralActionTriggered, slot);
    if (connection.connected()) {
        service_connections_.push_back(std::move(connection));
    } else {
        BROOKESIA_LOGE("Failed to subscribe to Agent general action triggered event");
    }
}

void AI_Agents::process_emote_when_general_event_happened()
{
    auto slot = [this](const std::string & event_name, const std::string & general_event, bool is_unexpected) {
        BROOKESIA_LOG_TRACE_GUARD();

        BROOKESIA_LOGD(
            "Params: event_name(%1%), general_event(%2%), is_unexpected(%3%)", event_name, general_event,
            BROOKESIA_DESCRIBE_TO_STR(is_unexpected)
        );

        AgentHelper::GeneralEvent general_event_enum;
        auto convert_result = BROOKESIA_DESCRIBE_STR_TO_ENUM(general_event, general_event_enum);
        BROOKESIA_CHECK_FALSE_EXIT(convert_result, "Failed to convert general event: %1%", general_event);

        switch (general_event_enum) {
        case AgentHelper::GeneralEvent::TimeSynced: {
            EmoteHelper::call_function_async(
                EmoteHelper::FunctionId::SetEventMessage,
                BROOKESIA_DESCRIBE_TO_STR(EmoteHelper::EventMessageType::System), "[Agent] Time Synced"
            );
            break;
        }
        case AgentHelper::GeneralEvent::Activated: {
            EmoteHelper::call_function_async(
                EmoteHelper::FunctionId::SetEventMessage,
                BROOKESIA_DESCRIBE_TO_STR(EmoteHelper::EventMessageType::System), "[Agent] Activated"
            );
            break;
        }
        case AgentHelper::GeneralEvent::Started: {
            EmoteHelper::call_function_async(EmoteHelper::FunctionId::SetEmoji, "neutral");
            EmoteHelper::call_function_async(
                EmoteHelper::FunctionId::SetEventMessage,
                BROOKESIA_DESCRIBE_TO_STR(EmoteHelper::EventMessageType::System), "[Agent] Started"
            );
            break;
        }
        case AgentHelper::GeneralEvent::Awake: {
            EmoteHelper::call_function_async(EmoteHelper::FunctionId::SetEmoji, "neutral");
            EmoteHelper::call_function_async(
                EmoteHelper::FunctionId::SetEventMessage,
                BROOKESIA_DESCRIBE_TO_STR(EmoteHelper::EventMessageType::System), "[Agent] Awake"
            );
            break;
        }
        case AgentHelper::GeneralEvent::Stopped: {
            EmoteHelper::call_function_async(
                EmoteHelper::FunctionId::SetEventMessage, BROOKESIA_DESCRIBE_TO_STR(EmoteHelper::EventMessageType::Idle)
            );
            break;
        }
        default:
            break;
        }
    };
    auto connection = AgentHelper::subscribe_event(AgentHelper::EventId::GeneralEventHappened, slot);
    if (connection.connected()) {
        service_connections_.push_back(std::move(connection));
    } else {
        BROOKESIA_LOGE("Failed to subscribe to Agent general event happened event");
    }
}

void AI_Agents::process_emote_when_suspend_status_changed()
{
    auto slot = [this](const std::string & event_name, bool is_suspended) {
        BROOKESIA_LOG_TRACE_GUARD();

        BROOKESIA_LOGD("Params: event_name(%1%), is_suspended(%2%)", event_name, is_suspended);

        BROOKESIA_LOGI("Suspend status changed to %1%", BROOKESIA_DESCRIBE_TO_STR(is_suspended));

        if (is_suspended) {
            EmoteHelper::call_function_async(
                EmoteHelper::FunctionId::SetEventMessage,
                BROOKESIA_DESCRIBE_TO_STR(EmoteHelper::EventMessageType::System), "[Agent] Suspended"
            );
        } else {
            EmoteHelper::call_function_async(
                EmoteHelper::FunctionId::SetEventMessage,
                BROOKESIA_DESCRIBE_TO_STR(EmoteHelper::EventMessageType::Idle)
            );
        }
    };
    auto connection = AgentHelper::subscribe_event(AgentHelper::EventId::SuspendStatusChanged, slot);
    if (connection.connected()) {
        service_connections_.push_back(std::move(connection));
    } else {
        BROOKESIA_LOGE("Failed to subscribe to Agent suspend status changed event");
    }
}

void AI_Agents::process_emote_when_speaking_status_changed()
{
    auto slot = [this](const std::string & event_name, bool is_speaking) {
        BROOKESIA_LOG_TRACE_GUARD();

        auto speaking_status = BROOKESIA_DESCRIBE_TO_STR(is_speaking);
        BROOKESIA_LOGD(
            "Params: event_name(%1%), is_speaking(%2%)", event_name, speaking_status
        );

        BROOKESIA_LOGI("Speaking status changed to %1%", speaking_status);

        if (is_inactive()) {
            BROOKESIA_LOGD("Agent is inactive, skip");
            return;
        }

        if (is_speaking) {
            EmoteHelper::call_function_async(
                EmoteHelper::FunctionId::SetEventMessage,
                BROOKESIA_DESCRIBE_TO_STR(EmoteHelper::EventMessageType::Speak)
            );
        } else if (!is_listening()) {
            // Only hide event message when the agent is not listening
            EmoteHelper::call_function_async(EmoteHelper::FunctionId::HideEventMessage);
        }
        is_speaking_ = is_speaking;
    };
    auto connection = AgentHelper::subscribe_event(AgentHelper::EventId::SpeakingStatusChanged, slot);
    if (connection.connected()) {
        service_connections_.push_back(std::move(connection));
    } else {
        BROOKESIA_LOGE("Failed to subscribe to Agent speaking status changed event");
    }
}

void AI_Agents::process_emote_when_listening_status_changed()
{
    auto slot = [this](const std::string & event_name, bool is_listening) {
        BROOKESIA_LOG_TRACE_GUARD();

        auto listening_status = BROOKESIA_DESCRIBE_TO_STR(is_listening);
        BROOKESIA_LOGD(
            "Params: event_name(%1%), is_listening(%2%)", event_name, listening_status
        );

        BROOKESIA_LOGI("Listening status changed to %1%", listening_status);

        if (is_inactive()) {
            BROOKESIA_LOGD("Agent is inactive, skip");
            return;
        }

        if (is_listening) {
            EmoteHelper::call_function_async(
                EmoteHelper::FunctionId::SetEventMessage,
                BROOKESIA_DESCRIBE_TO_STR(EmoteHelper::EventMessageType::Listen)
            );
        } else if (!is_speaking()) {
            // Only hide event message when the agent is not speaking
            EmoteHelper::call_function_async(EmoteHelper::FunctionId::HideEventMessage);
        }
        is_listening_ = is_listening;
    };
    auto connection = AgentHelper::subscribe_event(AgentHelper::EventId::ListeningStatusChanged, slot);
    if (connection.connected()) {
        service_connections_.push_back(std::move(connection));
    } else {
        BROOKESIA_LOGE("Failed to subscribe to Agent listening status changed event");
    }
}

void AI_Agents::process_emote_when_agent_speaking_text_got()
{
    auto slot = [this](const std::string & event_name, const std::string & text) {
        BROOKESIA_LOG_TRACE_GUARD();

        BROOKESIA_LOGD("Params: event_name(%1%), text(%2%)", event_name, text);

        BROOKESIA_LOGI("Got agent speaking text: %1%", text);

        if (is_inactive()) {
            BROOKESIA_LOGD("Agent is inactive, skip");
            return;
        }

        EmoteHelper::call_function_async(
            EmoteHelper::FunctionId::SetEventMessage,
            BROOKESIA_DESCRIBE_TO_STR(EmoteHelper::EventMessageType::Speak), text
        );
    };
    auto connection = AgentHelper::subscribe_event(AgentHelper::EventId::AgentSpeakingTextGot, slot);
    if (connection.connected()) {
        service_connections_.push_back(std::move(connection));
    } else {
        BROOKESIA_LOGE("Failed to subscribe to Agent agent speaking text got event");
    }
}

void AI_Agents::process_emote_when_user_speaking_text_got()
{
    auto slot = [this](const std::string & event_name, const std::string & text) {
        BROOKESIA_LOG_TRACE_GUARD();

        BROOKESIA_LOGD("Params: event_name(%1%), text(%2%)", event_name, text);

        BROOKESIA_LOGI("Got user speaking text: %1%", text);

        if (is_inactive()) {
            BROOKESIA_LOGD("Agent is inactive, skip");
            return;
        }

        EmoteHelper::call_function_async(
            EmoteHelper::FunctionId::SetEventMessage,
            BROOKESIA_DESCRIBE_TO_STR(EmoteHelper::EventMessageType::User), text
        );
    };
    auto connection = AgentHelper::subscribe_event(AgentHelper::EventId::UserSpeakingTextGot, slot);
    if (connection.connected()) {
        service_connections_.push_back(std::move(connection));
    } else {
        BROOKESIA_LOGE("Failed to subscribe to Agent user speaking text got event");
    }
}

void AI_Agents::process_emote_when_xiaozhi_events()
{
    if (!XiaoZhiHelper::is_available()) {
        BROOKESIA_LOGW("XiaoZhi agent is not available, skip initialization");
        return;
    }

    BROOKESIA_CHECK_FALSE_EXIT(is_initialized(), "AI agents are not initialized");

    // Subscribe to activation code received event
    auto activation_code_received_slot = [this](const std::string & event_name, const std::string & code) {
        BROOKESIA_LOG_TRACE_GUARD();

        BROOKESIA_LOGD("Params: event_name(%1%), code(%2%)", event_name, code);

        BROOKESIA_LOGI("[Agent] Got activation code: %1%", code);

        if (EmoteHelper::is_available()) {
            EmoteHelper::call_function_async(
                EmoteHelper::FunctionId::SetEventMessage,
                BROOKESIA_DESCRIBE_TO_STR(EmoteHelper::EventMessageType::System), code
            );
        }

        // Build URL list: activation.mp3 followed by digit files (0.mp3 - 9.mp3)
        std::vector<std::string> urls;
        urls.reserve(code.size() + 1);
        urls.push_back(XIAO_ZHI_AUDIO_URL_PREFIX "activation.mp3");
        for (char c : code) {
            if (c >= '0' && c <= '9') {
                urls.push_back(XIAO_ZHI_AUDIO_URL_PREFIX + std::string(1, c) + ".mp3");
            }
        }
        AudioHelper::call_function_async(
            AudioHelper::FunctionId::PlayUrls,
            BROOKESIA_DESCRIBE_TO_JSON(urls).as_array(),
            BROOKESIA_DESCRIBE_TO_JSON(AudioHelper::PlayUrlConfig{}).as_object()
        );
    };
    auto activation_code_received_connection = XiaoZhiHelper::subscribe_event(
                XiaoZhiHelper::EventId::ActivationCodeReceived, activation_code_received_slot
            );
    if (activation_code_received_connection.connected()) {
        service_connections_.push_back(std::move(activation_code_received_connection));
    } else {
        BROOKESIA_LOGE("Failed to subscribe to XiaoZhi activation code received event");
    }
}

void AI_Agents::process_emote_when_emote_got()
{
    auto slot = [this](const std::string & event_name, std::string emote) {
        BROOKESIA_LOG_TRACE_GUARD();

        BROOKESIA_LOGD("Params: event_name(%1%), emote(%2%)", event_name, emote);

        BROOKESIA_LOGI("Got emote: %1%", emote);

        if (is_inactive()) {
            BROOKESIA_LOGD("Agent is sleeping or suspended or stopped, skip");
            return;
        }

        // There is no "cool" emote in the emote.json file, so we need to replace it with "winking"
        if (emote == "cool") {
            emote = "winking";
        }

        EmoteHelper::call_function_async(
            EmoteHelper::FunctionId::InsertAnimation, emote, emote_animation_duration_ms_
        );
    };
    auto connection = AgentHelper::subscribe_event(AgentHelper::EventId::EmoteGot, slot);
    if (connection.connected()) {
        service_connections_.push_back(std::move(connection));
    } else {
        BROOKESIA_LOGE("Failed to subscribe to Agent emote got event");
    }
}

void AI_Agents::process_emote()
{
    if (!EmoteHelper::is_available()) {
        BROOKESIA_LOGW("Emote service is not available, skip");
        return;
    }

    process_emote_when_general_action_triggered();
    process_emote_when_general_event_happened();
    process_emote_when_suspend_status_changed();
    process_emote_when_speaking_status_changed();
    process_emote_when_listening_status_changed();
    process_emote_when_agent_speaking_text_got();
    process_emote_when_user_speaking_text_got();
    process_emote_when_emote_got();
    process_emote_when_xiaozhi_events();
}
