/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string>
#include <vector>
#include "sdkconfig.h"
#include "brookesia/lib_utils.hpp"
#include "brookesia/service_helper.hpp"
#include "agents_auth.hpp"

using namespace esp_brookesia;

using AgentHelper = service::helper::AgentManager;
using CozeHelper = service::helper::AgentCoze;
using OpenaiHelper = service::helper::AgentOpenai;

#if CONFIG_EXAMPLE_AGENTS_ENABLE_COZE
extern const char private_key_txt_start[] asm("_binary_private_key_txt_start");
extern const char private_key_txt_end[]   asm("_binary_private_key_txt_end");
#endif // CONFIG_EXAMPLE_AGENTS_ENABLE_COZE

void agents_auth_init()
{
#if CONFIG_EXAMPLE_AGENTS_ENABLE_COZE
    BROOKESIA_LOGI("Setting coze agent authentication...");
    CozeHelper::Info coze_info{
        .authorization = {
            .app_id = CONFIG_EXAMPLE_AGENTS_COZE_APP_ID,
            .public_key = CONFIG_EXAMPLE_AGENTS_COZE_PUBLIC_KEY,
            .private_key = std::string(private_key_txt_start, private_key_txt_end - private_key_txt_start) + "\0",
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
    {
        auto result = AgentHelper::call_function_sync<void>(AgentHelper::FunctionId::SetAgentInfo,
        service::FunctionParameterMap({
            {
                BROOKESIA_DESCRIBE_TO_STR(AgentHelper::FunctionSetAgentInfoParam::Name),
                "Coze"
            },
            {
                BROOKESIA_DESCRIBE_TO_STR(AgentHelper::FunctionSetAgentInfoParam::Info),
                BROOKESIA_DESCRIBE_TO_JSON(coze_info).as_object()
            }
        }), 0);
        if (!result) {
            BROOKESIA_LOGE("Failed to set coze agent info: %1%", result.error());
        } else {
            BROOKESIA_LOGI("Set coze agent info successfully");
        }
    }
#endif // CONFIG_EXAMPLE_AGENTS_ENABLE_COZE

#if CONFIG_EXAMPLE_AGENTS_ENABLE_OPENAI
    BROOKESIA_LOGI("Setting openai agent authentication...");
    OpenaiHelper::Info openai_info{
        .model = CONFIG_EXAMPLE_AGENTS_OPENAI_MODEL,
        .api_key = CONFIG_EXAMPLE_AGENTS_OPENAI_API_KEY,
    };
    {
        auto result = AgentHelper::call_function_sync<void>(AgentHelper::FunctionId::SetAgentInfo,
        service::FunctionParameterMap({
            {
                BROOKESIA_DESCRIBE_TO_STR(AgentHelper::FunctionSetAgentInfoParam::Name),
                "Openai"
            },
            {
                BROOKESIA_DESCRIBE_TO_STR(AgentHelper::FunctionSetAgentInfoParam::Info),
                BROOKESIA_DESCRIBE_TO_JSON(openai_info).as_object()
            }
        }), 0);
        if (!result) {
            BROOKESIA_LOGE("Failed to set openai agent info: %1%", result.error());
        } else {
            BROOKESIA_LOGI("Set openai agent info successfully");
        }
    }
#endif // CONFIG_EXAMPLE_AGENTS_ENABLE_OPENAI
}
