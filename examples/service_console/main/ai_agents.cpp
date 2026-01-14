/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "sdkconfig.h"
#include "brookesia/service_manager.hpp"
#include "brookesia/service_helper.hpp"
#include "private/utils.hpp"
#include "ai_agents.hpp"

using namespace esp_brookesia;

using AgentHelper = service::helper::AgentManager;
using CozeHelper = service::helper::AgentCoze;
using OpenaiHelper = service::helper::AgentOpenai;
using EmoteHelper = service::helper::ExpressionEmote;

constexpr uint32_t EMOTE_ANIMATION_DURATION_MS = 5000;

#if CONFIG_EXAMPLE_AGENTS_ENABLE_COZE
extern const char coze_private_key_pem_start[] asm("_binary_private_key_pem_start");
extern const char coze_private_key_pem_end[]   asm("_binary_private_key_pem_end");
#endif // CONFIG_EXAMPLE_AGENTS_ENABLE_COZE

/* Service manager instance */
static service::ServiceManager &service_manager = service::ServiceManager::get_instance();
/* Keep service bindings to avoid frequent start and stop of services */
static std::vector<service::ServiceBinding> service_bindings;
/* Keep event connections */
static std::vector<service::EventRegistry::SignalConnection> service_connections;

void ai_agents_init()
{
    if (!AgentHelper::is_available()) {
        BROOKESIA_LOGW("Agent service is not enabled");
        return;
    }

    BROOKESIA_LOGI("Initializing agents...");

#if !CONFIG_EXAMPLE_ENABLE_BOARD_MANAGER
    BROOKESIA_LOGE("Only supported when board manager is enabled, skip");
#else
#if CONFIG_EXAMPLE_AGENTS_ENABLE_COZE
    {
        BROOKESIA_LOGI("Setting coze agent authentication...");
        CozeHelper::Info coze_info{
            .authorization = {
                .app_id = CONFIG_EXAMPLE_AGENTS_COZE_APP_ID,
                .public_key = CONFIG_EXAMPLE_AGENTS_COZE_PUBLIC_KEY,
                .private_key = std::string(coze_private_key_pem_start, coze_private_key_pem_end - coze_private_key_pem_start) + "\0",
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
        auto result = AgentHelper::call_function_sync<void>(AgentHelper::FunctionId::SetAgentInfo,
        service::FunctionParameterMap({
            {
                BROOKESIA_DESCRIBE_TO_STR(AgentHelper::FunctionSetAgentInfoParam::Name),
                CozeHelper::NAME.data()
            },
            {
                BROOKESIA_DESCRIBE_TO_STR(AgentHelper::FunctionSetAgentInfoParam::Info),
                BROOKESIA_DESCRIBE_TO_JSON(coze_info).as_object()
            }
        }));
        if (!result) {
            BROOKESIA_LOGE("Failed to set coze agent info: %1%", result.error());
        } else {
            BROOKESIA_LOGI("Set coze agent info successfully");
        }
    }
#endif // CONFIG_EXAMPLE_AGENTS_ENABLE_COZE

#if CONFIG_EXAMPLE_AGENTS_ENABLE_OPENAI
    {
        BROOKESIA_LOGI("Setting openai agent authentication...");
        OpenaiHelper::Info openai_info{
            .model = CONFIG_EXAMPLE_AGENTS_OPENAI_MODEL,
            .api_key = CONFIG_EXAMPLE_AGENTS_OPENAI_API_KEY,
        };
        auto result = AgentHelper::call_function_sync(AgentHelper::FunctionId::SetAgentInfo,
        service::FunctionParameterMap({
            {
                BROOKESIA_DESCRIBE_TO_STR(AgentHelper::FunctionSetAgentInfoParam::Name),
                OpenaiHelper::NAME.data()
            },
            {
                BROOKESIA_DESCRIBE_TO_STR(AgentHelper::FunctionSetAgentInfoParam::Info),
                BROOKESIA_DESCRIBE_TO_JSON(openai_info).as_object()
            }
        }));
        if (!result) {
            BROOKESIA_LOGE("Failed to set openai agent info: %1%", result.error());
        } else {
            BROOKESIA_LOGI("Set openai agent info successfully");
        }
    }
#endif // CONFIG_EXAMPLE_AGENTS_ENABLE_OPENAI

    /* If emote service is available, subscribe to emote got event and set emote */
    if (EmoteHelper::is_available()) {
        auto binding = service_manager.bind(EmoteHelper::get_name().data());
        if (!binding.is_valid()) {
            BROOKESIA_LOGE("Failed to bind Emote service");
        } else {
            service_bindings.push_back(std::move(binding));

            auto emote_got_slot = [](const std::string & event, const service::EventItemMap & items) {
                BROOKESIA_LOG_TRACE_GUARD();

                BROOKESIA_LOGD("Params: event(%1%), items(%2%)", event, BROOKESIA_DESCRIBE_TO_STR(items));

                auto &emote = std::get<std::string>(
                                  items.at(BROOKESIA_DESCRIBE_TO_STR(AgentHelper::EventEmoteGotParam::Emote))
                              );

                BROOKESIA_LOGI("Got emote: %1%", emote);

                EmoteHelper::call_function_async(EmoteHelper::FunctionId::InsertAnimation,
                service::FunctionParameterMap{
                    {
                        BROOKESIA_DESCRIBE_TO_STR(EmoteHelper::FunctionInsertAnimationParam::Animation),
                        emote,
                    },
                    {
                        BROOKESIA_DESCRIBE_TO_STR(EmoteHelper::FunctionInsertAnimationParam::DurationMs),
                        static_cast<double>(EMOTE_ANIMATION_DURATION_MS),
                    }
                });
            };
            auto connection = AgentHelper::subscribe_event(AgentHelper::EventId::EmoteGot, emote_got_slot);
            if (connection.connected()) {
                service_connections.push_back(std::move(connection));
            } else {
                BROOKESIA_LOGE("Failed to subscribe to Agent emote got event");
            }
        }
    }
#endif // CONFIG_EXAMPLE_ENABLE_BOARD_MANAGER
}
