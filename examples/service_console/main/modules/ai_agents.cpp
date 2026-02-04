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

using ManagerHelper = esp_brookesia::agent::helper::Manager;
using CozeHelper = esp_brookesia::agent::helper::Coze;
using OpenaiHelper = esp_brookesia::agent::helper::Openai;
using XiaoZhiHelper = esp_brookesia::agent::helper::XiaoZhi;
using EmoteHelper = esp_brookesia::service::helper::ExpressionEmote;
using AudioHelper = esp_brookesia::service::helper::Audio;

#define XIAO_ZHI_AUDIO_URL_PREFIX "file://spiffs/xiaozhi/"

#if CONFIG_EXAMPLE_AGENTS_ENABLE_COZE
extern const char coze_private_key_pem_start[] asm("_binary_private_key_pem_start");
extern const char coze_private_key_pem_end[]   asm("_binary_private_key_pem_end");
#endif // CONFIG_EXAMPLE_AGENTS_ENABLE_COZE

bool AI_Agents::init()
{
    return init(Config{});
}

bool AI_Agents::init(const Config &config)
{
    if (is_initialized()) {
        BROOKESIA_LOGW("AI agents are already initialized");
        return true;
    }

    if (!ManagerHelper::is_available()) {
        BROOKESIA_LOGW("Agent manager is not available, skip initialization");
        return false;
    }

    BROOKESIA_LOGI("Initializing AI agents with configuration: %1%", BROOKESIA_DESCRIBE_TO_STR(config));

#if !CONFIG_EXAMPLE_ENABLE_BOARD_MANAGER
    BROOKESIA_LOGE("AI agents only supported when board manager is enabled, skip initialization");
    return false;
#else
    config_ = config;

    process_agent_general_events();
    process_emote();

    is_initialized_.store(true);

    BROOKESIA_LOGI("AI agents initialized successfully");

    return true;
#endif // CONFIG_EXAMPLE_ENABLE_BOARD_MANAGER
}

void AI_Agents::init_coze()
{
    if (!CozeHelper::is_available()) {
        BROOKESIA_LOGW("Coze agent is not available, skip initialization");
        return;
    }

    BROOKESIA_CHECK_FALSE_EXIT(is_initialized(), "AI agents are not initialized");

#if !CONFIG_EXAMPLE_AGENTS_ENABLE_COZE
    BROOKESIA_LOGW("Coze agent is not enabled, skip initialization");
#else
    BROOKESIA_LOGI("Setting Coze agent info...");
    auto result = ManagerHelper::call_function_sync(
                      ManagerHelper::FunctionId::SetAgentInfo, CozeHelper::get_name().data(), get_agent_coze_info()
                  );
    if (!result) {
        BROOKESIA_LOGE("Failed to set Coze agent info: %1%", result.error());
    } else {
        BROOKESIA_LOGI("Set Coze agent info successfully");
    }

    // Subscribe to coze event happened event
    auto coze_event_happened_slot = [this](const std::string & event_name, const std::string & coze_event) {
        BROOKESIA_LOG_TRACE_GUARD();

        BROOKESIA_LOGD("Params: event_name(%1%), coze_event(%2%)", event_name, coze_event);

        CozeHelper::CozeEvent coze_event_enum;
        auto convert_result = BROOKESIA_DESCRIBE_STR_TO_ENUM(coze_event, coze_event_enum);
        BROOKESIA_CHECK_FALSE_EXIT(convert_result, "Failed to convert coze event: %1%", coze_event);

        switch (coze_event_enum) {
        case CozeHelper::CozeEvent::InsufficientCreditsBalance: {
            BROOKESIA_LOGE("Insufficient credits balance");
            BROOKESIA_LOGW("Try to restart agent in %1% seconds...", config_.agent_restart_delay_s);

            auto task_scheduler = service::ServiceManager::get_instance().get_task_scheduler();
            BROOKESIA_CHECK_NULL_EXIT(task_scheduler, "Task scheduler is not available");

            auto task_func = []() {
                BROOKESIA_LOG_TRACE_GUARD();
                BROOKESIA_LOGW("Restarting agent...");
                ManagerHelper::call_function_async(
                    ManagerHelper::FunctionId::TriggerGeneralAction,
                    BROOKESIA_DESCRIBE_TO_STR(ManagerHelper::GeneralAction::Start)
                );
            };
            auto result = task_scheduler->post_delayed(std::move(task_func), config_.agent_restart_delay_s * 1000);
            BROOKESIA_CHECK_FALSE_EXIT(result, "Failed to post restart agent task");
            break;
        }
        default:
            break;
        }
    };
    auto coze_event_happened_connection = CozeHelper::subscribe_event(
            CozeHelper::EventId::CozeEventHappened, coze_event_happened_slot
                                          );
    if (coze_event_happened_connection.connected()) {
        service_connections_.push_back(std::move(coze_event_happened_connection));
    } else {
        BROOKESIA_LOGE("Failed to subscribe to Agent coze event happened event");
    }
#endif // CONFIG_EXAMPLE_AGENTS_ENABLE_COZE
}

void AI_Agents::init_openai()
{
    if (!OpenaiHelper::is_available()) {
        BROOKESIA_LOGW("Openai agent is not available, skip initialization");
        return;
    }

    BROOKESIA_CHECK_FALSE_EXIT(is_initialized(), "AI agents are not initialized");

#if !CONFIG_EXAMPLE_AGENTS_ENABLE_OPENAI
    BROOKESIA_LOGW("Openai agent is not enabled, skip initialization");
#else
    BROOKESIA_LOGI("Setting Openai agent info...");
    auto result = ManagerHelper::call_function_sync(
                      ManagerHelper::FunctionId::SetAgentInfo, OpenaiHelper::get_name().data(), get_agent_openai_info()
                  );
    if (!result) {
        BROOKESIA_LOGE("Failed to set Openai agent info: %1%", result.error());
    } else {
        BROOKESIA_LOGI("Set Openai agent info successfully");
    }
#endif // CONFIG_EXAMPLE_AGENTS_ENABLE_OPENAI
}

void AI_Agents::init_xiaozhi()
{
    if (!XiaoZhiHelper::is_available()) {
        BROOKESIA_LOGW("XiaoZhi agent is not available, skip initialization");
        return;
    }

    BROOKESIA_CHECK_FALSE_EXIT(is_initialized(), "AI agents are not initialized");

#if !CONFIG_EXAMPLE_AGENTS_ENABLE_XIAOZHI
    BROOKESIA_LOGW("XiaoZhi agent is not enabled, skip initialization");
#else
    // Subscribe to activation code received event
    auto activation_code_received_slot = [this](const std::string & event_name, const std::string & code) {
        BROOKESIA_LOG_TRACE_GUARD();

        BROOKESIA_LOGD("Params: event_name(%1%), code(%2%)", event_name, code);

        BROOKESIA_LOGI("Got activation code: %1%", code);

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
#endif // CONFIG_EXAMPLE_AGENTS_ENABLE_XIAOZHI
}

void AI_Agents::process_agent_general_unexpected_events()
{
    auto general_event_happened_slot =
    [this](const std::string & event_name, const std::string & general_event, bool is_unexpected) {
        BROOKESIA_LOG_TRACE_GUARD();

        BROOKESIA_LOGD(
            "Params: event_name(%1%), general_event(%2%), is_unexpected(%3%)", event_name, general_event,
            BROOKESIA_DESCRIBE_TO_STR(is_unexpected)
        );

        if (!is_unexpected) {
            return;
        }

        BROOKESIA_LOGW("Detected unexpected general event: %1%", general_event);

        ManagerHelper::GeneralEvent general_event_enum;
        auto convert_result = BROOKESIA_DESCRIBE_STR_TO_ENUM(general_event, general_event_enum);
        BROOKESIA_CHECK_FALSE_EXIT(convert_result, "Failed to convert general event: %1%", general_event);

        switch (general_event_enum) {
        case ManagerHelper::GeneralEvent::Stopped: {
            BROOKESIA_LOGW("Try to restart agent in %1% seconds...", config_.agent_restart_delay_s);

            auto task_scheduler = service::ServiceManager::get_instance().get_task_scheduler();
            BROOKESIA_CHECK_NULL_EXIT(task_scheduler, "Task scheduler is not available");

            auto task_func = []() {
                BROOKESIA_LOG_TRACE_GUARD();
                BROOKESIA_LOGW("Restarting agent...");
                ManagerHelper::call_function_async(
                    ManagerHelper::FunctionId::TriggerGeneralAction,
                    BROOKESIA_DESCRIBE_TO_STR(ManagerHelper::GeneralAction::Start)
                );
            };
            auto result = task_scheduler->post_delayed(std::move(task_func), config_.agent_restart_delay_s * 1000);
            BROOKESIA_CHECK_FALSE_EXIT(result, "Failed to post restart agent task");
            break;
        }
        default:
            break;
        }
    };
    auto connection = ManagerHelper::subscribe_event(
                          ManagerHelper::EventId::GeneralEventHappened, general_event_happened_slot
                      );
    if (connection.connected()) {
        service_connections_.push_back(std::move(connection));
    } else {
        BROOKESIA_LOGE("Failed to subscribe to Agent general event happened event");
    }
}

void AI_Agents::process_agent_general_suspend_status_changed()
{
    auto slot = [this](const std::string & event_name, bool is_suspended) {
        BROOKESIA_LOG_TRACE_GUARD();

        BROOKESIA_LOGD("Params: event_name(%1%), is_suspended(%2%)", event_name, is_suspended);

        if (is_suspended) {
            // Trigger sleep action
            ManagerHelper::call_function_async(
                ManagerHelper::FunctionId::TriggerGeneralAction,
                BROOKESIA_DESCRIBE_TO_STR(ManagerHelper::GeneralAction::Sleep)
            );
        }
        is_suspended_ = is_suspended;
    };
    auto connection = ManagerHelper::subscribe_event(ManagerHelper::EventId::SuspendStatusChanged, slot);
    if (connection.connected()) {
        service_connections_.push_back(std::move(connection));
    } else {
        BROOKESIA_LOGE("Failed to subscribe to Agent general suspend status changed event");
    }
}

void AI_Agents::process_agent_general_events()
{
    // Process unexpected general events:
    //   1. Stopped: Restart the agent after a delay
    process_agent_general_unexpected_events();
    process_agent_general_suspend_status_changed();
}

void AI_Agents::process_emote_when_general_action_triggered()
{
    auto slot = [this](const std::string & event_name, const std::string & general_action) {
        BROOKESIA_LOG_TRACE_GUARD();

        BROOKESIA_LOGD("Params: event_name(%1%), general_action(%2%)", event_name, general_action);

        ManagerHelper::GeneralAction general_action_enum;
        auto convert_result = BROOKESIA_DESCRIBE_STR_TO_ENUM(general_action, general_action_enum);
        BROOKESIA_CHECK_FALSE_EXIT(convert_result, "Failed to convert general action: %1%", general_action);

        switch (general_action_enum) {
        case ManagerHelper::GeneralAction::TimeSync:
            EmoteHelper::call_function_async(EmoteHelper::FunctionId::SetEmoji, "winking");
            EmoteHelper::call_function_async(
                EmoteHelper::FunctionId::SetEventMessage,
                BROOKESIA_DESCRIBE_TO_STR(EmoteHelper::EventMessageType::System), "Time Syncing..."
            );
            break;
        case ManagerHelper::GeneralAction::Activate:
            EmoteHelper::call_function_async(EmoteHelper::FunctionId::SetEmoji, "winking");
            EmoteHelper::call_function_async(
                EmoteHelper::FunctionId::SetEventMessage,
                BROOKESIA_DESCRIBE_TO_STR(EmoteHelper::EventMessageType::System), "Activating..."
            );
            break;
        case ManagerHelper::GeneralAction::Start:
            is_sleeping_ = false;
            is_suspended_ = false;
            is_stopped_ = false;
            EmoteHelper::call_function_async(
                EmoteHelper::FunctionId::SetEventMessage,
                BROOKESIA_DESCRIBE_TO_STR(EmoteHelper::EventMessageType::System), "Starting..."
            );
            break;
        case ManagerHelper::GeneralAction::Sleep: {
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
        case ManagerHelper::GeneralAction::WakeUp:
            is_sleeping_ = false;
            EmoteHelper::call_function_async(
                EmoteHelper::FunctionId::SetEventMessage,
                BROOKESIA_DESCRIBE_TO_STR(EmoteHelper::EventMessageType::System), "Waking Up..."
            );
            break;
        case ManagerHelper::GeneralAction::Stop:
            is_stopped_ = true;
            EmoteHelper::call_function_async(EmoteHelper::FunctionId::HideEmoji);
            EmoteHelper::call_function_async(
                EmoteHelper::FunctionId::SetEventMessage,
                BROOKESIA_DESCRIBE_TO_STR(EmoteHelper::EventMessageType::System), "Stopping..."
            );
            break;
        default:
            break;
        }
    };
    auto connection = ManagerHelper::subscribe_event(ManagerHelper::EventId::GeneralActionTriggered, slot);
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

        ManagerHelper::GeneralEvent general_event_enum;
        auto convert_result = BROOKESIA_DESCRIBE_STR_TO_ENUM(general_event, general_event_enum);
        BROOKESIA_CHECK_FALSE_EXIT(convert_result, "Failed to convert general event: %1%", general_event);

        switch (general_event_enum) {
        case ManagerHelper::GeneralEvent::TimeSynced: {
            EmoteHelper::call_function_async(
                EmoteHelper::FunctionId::SetEventMessage,
                BROOKESIA_DESCRIBE_TO_STR(EmoteHelper::EventMessageType::System), "Time Synced"
            );
            break;
        }
        case ManagerHelper::GeneralEvent::Activated: {
            EmoteHelper::call_function_async(
                EmoteHelper::FunctionId::SetEventMessage,
                BROOKESIA_DESCRIBE_TO_STR(EmoteHelper::EventMessageType::System), "Activated"
            );
            break;
        }
        case ManagerHelper::GeneralEvent::Started: {
            EmoteHelper::call_function_async(EmoteHelper::FunctionId::SetEmoji, "neutral");
            EmoteHelper::call_function_async(
                EmoteHelper::FunctionId::SetEventMessage,
                BROOKESIA_DESCRIBE_TO_STR(EmoteHelper::EventMessageType::System), "Started"
            );
            break;
        }
        case ManagerHelper::GeneralEvent::Awake: {
            EmoteHelper::call_function_async(EmoteHelper::FunctionId::SetEmoji, "neutral");
            EmoteHelper::call_function_async(
                EmoteHelper::FunctionId::SetEventMessage,
                BROOKESIA_DESCRIBE_TO_STR(EmoteHelper::EventMessageType::System), "Awake"
            );
            break;
        }
        case ManagerHelper::GeneralEvent::Stopped: {
            EmoteHelper::call_function_async(
                EmoteHelper::FunctionId::SetEventMessage, BROOKESIA_DESCRIBE_TO_STR(EmoteHelper::EventMessageType::Idle)
            );
            break;
        }
        default:
            break;
        }
    };
    auto connection = ManagerHelper::subscribe_event(ManagerHelper::EventId::GeneralEventHappened, slot);
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
                BROOKESIA_DESCRIBE_TO_STR(EmoteHelper::EventMessageType::System), "Suspended"
            );
        } else {
            EmoteHelper::call_function_async(
                EmoteHelper::FunctionId::SetEventMessage,
                BROOKESIA_DESCRIBE_TO_STR(EmoteHelper::EventMessageType::Idle)
            );
        }
    };
    auto connection = ManagerHelper::subscribe_event(ManagerHelper::EventId::SuspendStatusChanged, slot);
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
    auto connection = ManagerHelper::subscribe_event(ManagerHelper::EventId::SpeakingStatusChanged, slot);
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
    auto connection = ManagerHelper::subscribe_event(ManagerHelper::EventId::ListeningStatusChanged, slot);
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
    auto connection = ManagerHelper::subscribe_event(ManagerHelper::EventId::AgentSpeakingTextGot, slot);
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
    auto connection = ManagerHelper::subscribe_event(ManagerHelper::EventId::UserSpeakingTextGot, slot);
    if (connection.connected()) {
        service_connections_.push_back(std::move(connection));
    } else {
        BROOKESIA_LOGE("Failed to subscribe to Agent user speaking text got event");
    }
}

void AI_Agents::process_emote_when_coze_event_happened()
{
    if (!CozeHelper::is_available()) {
        return;
    }

    auto slot = [this](const std::string & event_name, const std::string & coze_event) {
        BROOKESIA_LOG_TRACE_GUARD();

        BROOKESIA_LOGD("Params: event_name(%1%), coze_event(%2%)", event_name, coze_event);

        CozeHelper::CozeEvent coze_event_enum;
        auto convert_result = BROOKESIA_DESCRIBE_STR_TO_ENUM(coze_event, coze_event_enum);
        BROOKESIA_CHECK_FALSE_EXIT(convert_result, "Failed to convert coze event: %1%", coze_event);

        switch (coze_event_enum) {
        case CozeHelper::CozeEvent::InsufficientCreditsBalance: {
            auto task_scheduler = service::ServiceManager::get_instance().get_task_scheduler();
            BROOKESIA_CHECK_NULL_EXIT(task_scheduler, "Task scheduler is not available");

            auto task_func = [this]() {
                BROOKESIA_LOG_TRACE_GUARD();
                EmoteHelper::call_function_async(EmoteHelper::FunctionId::SetEmoji, "sad");
                EmoteHelper::call_function_async(
                    EmoteHelper::FunctionId::SetEventMessage,
                    BROOKESIA_DESCRIBE_TO_STR(EmoteHelper::EventMessageType::System),
                    (boost::format(
                         "Insufficient credits balance, please top up balance, will restart in %1% seconds..."
                     ) % (config_.agent_restart_delay_s - config_.coze_error_show_emote_delay_s)).str()
                );
            };
            auto result = task_scheduler->post_delayed(
                              std::move(task_func), config_.coze_error_show_emote_delay_s * 1000
                          );
            BROOKESIA_CHECK_FALSE_EXIT(result, "Failed to post restart agent task");
            break;
        }
        default:
            break;
        }
    };
    auto connection = CozeHelper::subscribe_event(CozeHelper::EventId::CozeEventHappened, slot);
    if (connection.connected()) {
        service_connections_.push_back(std::move(connection));
    } else {
        BROOKESIA_LOGE("Failed to subscribe to Agent coze event happened event");
    }
}

void AI_Agents::process_emote_when_emote_got()
{
    auto slot = [this](const std::string & event_name, const std::string & emote) {
        BROOKESIA_LOG_TRACE_GUARD();

        BROOKESIA_LOGD("Params: event_name(%1%), emote(%2%)", event_name, emote);

        BROOKESIA_LOGI("Got emote: %1%", emote);

        if (is_inactive()) {
            BROOKESIA_LOGD("Agent is sleeping or suspended or stopped, skip");
            return;
        }

        EmoteHelper::call_function_async(
            EmoteHelper::FunctionId::InsertAnimation, emote, config_.emote_animation_duration_ms
        );
    };
    auto connection = ManagerHelper::subscribe_event(ManagerHelper::EventId::EmoteGot, slot);
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

    auto binding = service::ServiceManager::get_instance().bind(EmoteHelper::get_name().data());
    if (!binding.is_valid()) {
        BROOKESIA_LOGE("Failed to bind Emote service");
    }
    service_bindings_.push_back(std::move(binding));

    process_emote_when_general_action_triggered();
    process_emote_when_general_event_happened();
    process_emote_when_suspend_status_changed();
    process_emote_when_speaking_status_changed();
    process_emote_when_listening_status_changed();
    process_emote_when_agent_speaking_text_got();
    process_emote_when_user_speaking_text_got();
    process_emote_when_emote_got();

    process_emote_when_coze_event_happened();
}

boost::json::object AI_Agents::get_agent_coze_info()
{
#if CONFIG_EXAMPLE_AGENTS_ENABLE_COZE
    CozeHelper::Info coze_info {
        .authorization = {
            .session_name = "",
            .device_id = "",
            .custom_consumer = "",
            .app_id = CONFIG_EXAMPLE_AGENTS_COZE_APP_ID,
            .user_id = "",
            .public_key = CONFIG_EXAMPLE_AGENTS_COZE_PUBLIC_KEY,
            .private_key = std::string(
                coze_private_key_pem_start, coze_private_key_pem_end - coze_private_key_pem_start
            ) + "\0",
        },
        .robots = {
#if CONFIG_EXAMPLE_AGENTS_COZE_BOT1_ENABLE
            {
                .name = CONFIG_EXAMPLE_AGENTS_COZE_BOT1_NAME,
                .bot_id = CONFIG_EXAMPLE_AGENTS_COZE_BOT1_ID,
                .voice_id = CONFIG_EXAMPLE_AGENTS_COZE_BOT1_VOICE_ID,
                .description = CONFIG_EXAMPLE_AGENTS_COZE_BOT1_DESCRIPTION,
            },
#endif // CONFIG_EXAMPLE_AGENTS_COZE_BOT1_ENABLE
#if CONFIG_EXAMPLE_AGENTS_COZE_BOT2_ENABLE
            {
                .name = CONFIG_EXAMPLE_AGENTS_COZE_BOT2_NAME,
                .bot_id = CONFIG_EXAMPLE_AGENTS_COZE_BOT2_ID,
                .voice_id = CONFIG_EXAMPLE_AGENTS_COZE_BOT2_VOICE_ID,
                .description = CONFIG_EXAMPLE_AGENTS_COZE_BOT2_DESCRIPTION,
            },
#endif // CONFIG_EXAMPLE_AGENTS_COZE_BOT2_ENABLE
        },
    };
    return BROOKESIA_DESCRIBE_TO_JSON(coze_info).as_object();
#else
    return boost::json::object {};
#endif // CONFIG_EXAMPLE_AGENTS_ENABLE_COZE
}

boost::json::object AI_Agents::get_agent_openai_info()
{
#if CONFIG_EXAMPLE_AGENTS_ENABLE_OPENAI
    OpenaiHelper::Info openai_info {
        .model = CONFIG_EXAMPLE_AGENTS_OPENAI_MODEL,
        .api_key = CONFIG_EXAMPLE_AGENTS_OPENAI_API_KEY,
    };
    return BROOKESIA_DESCRIBE_TO_JSON(openai_info).as_object();
#else
    return boost::json::object {};
#endif // CONFIG_EXAMPLE_AGENTS_ENABLE_OPENAI
}
