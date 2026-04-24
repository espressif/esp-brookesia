/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "sdkconfig.h"
#include "brookesia/service_manager.hpp"
#include "brookesia/service_helper.hpp"
#include "brookesia/agent_helper.hpp"
#include "brookesia/mcp_utils/mcp_utils.hpp"
#include "brookesia/hal_interface.hpp"
#include "private/utils.hpp"
#include "ai_agents.hpp"

using namespace esp_brookesia;

using AgentHelper = esp_brookesia::agent::helper::Manager;
using CozeHelper = esp_brookesia::agent::helper::Coze;
using OpenaiHelper = esp_brookesia::agent::helper::Openai;
using XiaoZhiHelper = esp_brookesia::agent::helper::XiaoZhi;
using EmoteHelper = esp_brookesia::service::helper::ExpressionEmote;
using AudioHelper = esp_brookesia::service::helper::Audio;
using BatteryHelper = esp_brookesia::service::helper::Battery;
using WifiHelper = esp_brookesia::service::helper::Wifi;

#define XIAO_ZHI_AUDIO_URL_PREFIX "file://spiffs/xiaozhi/"

constexpr const char *AUDIO_WAKEUP_WORD_MODEL_PARTITION_LABEL = "model";
constexpr const char *AUDIO_WAKEUP_WORD_MN_LANGUAGE = "cn";

#if CONFIG_EXAMPLE_AGENTS_ENABLE_COZE
extern const char coze_private_key_pem_start[] asm("_binary_private_key_pem_start");
extern const char coze_private_key_pem_end[]   asm("_binary_private_key_pem_end");
#endif // CONFIG_EXAMPLE_AGENTS_ENABLE_COZE

namespace {
service::FunctionResult make_battery_status_result()
{
    auto status_result = BatteryHelper::call_function_sync<boost::json::object>(BatteryHelper::FunctionId::GetStatus);
    if (!status_result) {
        return service::FunctionResult{
            .success = false,
            .error_message = status_result.error(),
        };
    }

    return service::FunctionResult{
        .success = true,
        .data = std::move(status_result.value()),
    };
}

service::FunctionResult make_device_status_result()
{
    boost::json::object root;

    boost::json::object audio_speaker;
    auto volume_result = AudioHelper::call_function_sync<double>(AudioHelper::FunctionId::GetVolume);
    if (volume_result) {
        audio_speaker["volume"] = volume_result.value();
    }
    root["audio_speaker"] = std::move(audio_speaker);

    boost::json::object screen;
    auto [backlight_name, backlight_iface] = hal::get_first_interface<hal::DisplayBacklightIface>();
    if (backlight_iface) {
        uint8_t brightness = 0;
        if (backlight_iface->get_brightness(brightness)) {
            screen["brightness"] = brightness;
        }
    }
    root["screen"] = std::move(screen);

    if (BatteryHelper::is_running()) {
        auto battery_result = BatteryHelper::call_function_sync<boost::json::object>(BatteryHelper::FunctionId::GetStatus);
        if (battery_result) {
            root["battery"] = std::move(battery_result.value());
        }
    }

    return service::FunctionResult{
        .success = true,
        .data = std::move(root),
    };
}
} // namespace

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
    agent_restart_delay_s_ = config.agent_restart_delay_s;
    coze_error_show_emote_delay_s_ = config.coze_error_show_emote_delay_s;

    process_agent_general_events();
    process_emote();
    process_wifi_events();

    auto binding = service::ServiceManager::get_instance().bind(AgentHelper::get_name().data());
    BROOKESIA_CHECK_FALSE_RETURN(binding.is_valid(), false, "Failed to bind Agent manager service");

    service_bindings_.push_back(std::move(binding));

    BROOKESIA_LOGI("AI agents initialized successfully");

    return true;
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
    auto result = AgentHelper::call_function_sync(
                      AgentHelper::FunctionId::SetAgentInfo, CozeHelper::get_name().data(), get_agent_coze_info()
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
            BROOKESIA_LOGW("Try to restart agent in %1% seconds...", agent_restart_delay_s_);

            auto task_func = []() {
                BROOKESIA_LOG_TRACE_GUARD();
                BROOKESIA_LOGW("Restarting agent...");
                AgentHelper::call_function_async(
                    AgentHelper::FunctionId::TriggerGeneralAction,
                    BROOKESIA_DESCRIBE_TO_STR(AgentHelper::GeneralAction::Start)
                );
            };
            auto result = task_scheduler_->post_delayed(std::move(task_func), agent_restart_delay_s_ * 1000);
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
    auto result = AgentHelper::call_function_sync(
                      AgentHelper::FunctionId::SetAgentInfo, OpenaiHelper::get_name().data(), get_agent_openai_info()
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

    std::vector<AudioHelper::FunctionId> audio_functions = {
        AudioHelper::FunctionId::SetVolume,
        AudioHelper::FunctionId::GetVolume,
        AudioHelper::FunctionId::SetMute,
    };
    auto add_service_tools_result = XiaoZhiHelper::call_function_sync<boost::json::array>(
                                        XiaoZhiHelper::FunctionId::AddMCP_ToolsWithServiceFunction,
                                        std::string(AudioHelper::get_name()),
                                        BROOKESIA_DESCRIBE_TO_JSON(audio_functions).as_array()
                                    );
    BROOKESIA_CHECK_FALSE_EXIT(
        add_service_tools_result, "Failed to add service tools: %1%", add_service_tools_result.error()
    );
    BROOKESIA_LOGI("Added service tools: %1%", add_service_tools_result.value());

    std::vector<mcp_utils::CustomTool> custom_tools;

    custom_tools.push_back({
        .schema = {
            .name = "Device.GetStatus",
            .description =
            "Get the current device status (设备当前状态), including speaker volume, screen brightness, "
            "and battery status when available. Use this tool first for questions like 当前状态、音量多少、亮度多少、电量多少.",
        },
        .callback = +[](service::FunctionParameterMap &&)
        {
            return make_device_status_result();
        },
    });

    if (BatteryHelper::is_running()) {
        custom_tools.push_back({
            .schema = {
                .name = "Battery.GetStatus",
                .description =
                "Get current battery status (电量 / 电池百分比 / 剩余电量 / 是否充电). "
                "Use this tool whenever the user asks about battery level, remaining power, or charging state. "
                "Do not use volume or brightness tools for battery questions. "
                "Return object fields: level, charging, discharging, voltage_mv.",
            },
            .callback = +[](service::FunctionParameterMap &&)
            {
                return make_battery_status_result();
            },
        });
    }

    auto [backlight_name, backlight_iface] = hal::get_first_interface<hal::DisplayBacklightIface>();
    if (backlight_iface) {
        std::vector<mcp_utils::CustomTool> display_tools = {
            {
                .schema = {
                    .name = "Display.GetBrightness",
                    .description = "Get the brightness of the display",
                },
                .callback = +[](service::FunctionParameterMap && params)
                {
                    auto context = std::get_if<service::RawBuffer>(
                        &params[std::string(mcp_utils::CUSTOM_TOOL_PARAMETER_CONTEXT_NAME)]
                    );
                    if (!context) {
                        return service::FunctionResult{
                            .success = false,
                            .error_message = "Failed to get context",
                        };
                    }
                    auto backlight_iface = reinterpret_cast<hal::DisplayBacklightIface *>(context->to_ptr());
                    if (!backlight_iface) {
                        return service::FunctionResult{
                            .success = false,
                            .error_message = "Failed to convert backlight interface",
                        };
                    }

                    uint8_t brightness = 0;
                    if (!backlight_iface->get_brightness(brightness)) {
                        return service::FunctionResult{
                            .success = false,
                            .error_message = "Failed to get brightness",
                        };
                    }

                    return service::FunctionResult{
                        .success = true,
                        .data = brightness,
                    };
                },
                .context{backlight_iface.get()},
            },
            {
                .schema = {
                    .name = "Display.SetBrightness",
                    .description = "Set the brightness of the display",
                    .parameters = {
                        {
                            .name = "Brightness",
                            .description = "The brightness to set the display to, range from 0 to 100",
                            .type = service::FunctionValueType::Number
                        }
                    },
                },
                .callback = +[](service::FunctionParameterMap && params)
                {
                    auto context = std::get_if<service::RawBuffer>(
                        &params[std::string(mcp_utils::CUSTOM_TOOL_PARAMETER_CONTEXT_NAME)]
                    );
                    if (!context) {
                        return service::FunctionResult{
                            .success = false,
                            .error_message = "Failed to get context",
                        };
                    }
                    auto backlight_iface = reinterpret_cast<hal::DisplayBacklightIface *>(context->to_ptr());
                    if (!backlight_iface) {
                        return service::FunctionResult{
                            .success = false,
                            .error_message = "Failed to convert backlight interface",
                        };
                    }

                    auto brightness = std::get_if<double>(&params["Brightness"]);
                    if (!brightness) {
                        return service::FunctionResult{
                            .success = false,
                            .error_message = "Failed to get brightness",
                        };
                    }

                    if (!backlight_iface->set_brightness(static_cast<uint8_t>(*brightness))) {
                        return service::FunctionResult{
                            .success = false,
                            .error_message = "Failed to set brightness",
                        };
                    }

                    return service::FunctionResult{
                        .success = true,
                    };
                },
                .context{backlight_iface.get()},
            },
            {
                .schema = {
                    .name = "Display.TurnOn",
                    .description = "Turn on the display",
                },
                .callback = +[](service::FunctionParameterMap && params)
                {
                    auto context = std::get_if<service::RawBuffer>(
                        &params[std::string(mcp_utils::CUSTOM_TOOL_PARAMETER_CONTEXT_NAME)]
                    );
                    if (!context) {
                        return service::FunctionResult{
                            .success = false,
                            .error_message = "Failed to get context",
                        };
                    }
                    auto backlight_iface = reinterpret_cast<hal::DisplayBacklightIface *>(context->to_ptr());
                    if (!backlight_iface) {
                        return service::FunctionResult{
                            .success = false,
                            .error_message = "Failed to convert backlight interface",
                        };
                    }

                    if (!backlight_iface->turn_on()) {
                        return service::FunctionResult{
                            .success = false,
                            .error_message = "Failed to turn on display",
                        };
                    }

                    return service::FunctionResult{
                        .success = true,
                    };
                },
                .context{backlight_iface.get()},
            },
            {
                .schema = {
                    .name = "Display.TurnOff",
                    .description = "Turn off the display",
                },
                .callback = +[](service::FunctionParameterMap && params)
                {
                    auto context = std::get_if<service::RawBuffer>(
                        &params[std::string(mcp_utils::CUSTOM_TOOL_PARAMETER_CONTEXT_NAME)]
                    );
                    if (!context) {
                        return service::FunctionResult{
                            .success = false,
                            .error_message = "Failed to get context",
                        };
                    }
                    auto backlight_iface = reinterpret_cast<hal::DisplayBacklightIface *>(context->to_ptr());
                    if (!backlight_iface) {
                        return service::FunctionResult{
                            .success = false,
                            .error_message = "Failed to convert backlight interface",
                        };
                    }

                    if (!backlight_iface->turn_off()) {
                        return service::FunctionResult{
                            .success = false,
                            .error_message = "Failed to turn off display",
                        };
                    }

                    return service::FunctionResult{
                        .success = true,
                    };
                },
                .context{backlight_iface.get()},
            },
        };
        for (const auto &tool : display_tools) {
            custom_tools.push_back(std::move(tool));
        }
    }

    if (!custom_tools.empty()) {
        auto add_custom_tools_result = XiaoZhiHelper::call_function_sync<boost::json::array>(
                                           XiaoZhiHelper::FunctionId::AddMCP_ToolsWithCustomFunction,
                                           BROOKESIA_DESCRIBE_TO_JSON(custom_tools).as_array()
                                       );
        BROOKESIA_CHECK_FALSE_EXIT(
            add_custom_tools_result, "Failed to add custom tools: %1%", add_custom_tools_result.error()
        );
        BROOKESIA_LOGI("Added custom tools: %1%", add_custom_tools_result.value());
    }

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

        AgentHelper::GeneralEvent general_event_enum;
        auto convert_result = BROOKESIA_DESCRIBE_STR_TO_ENUM(general_event, general_event_enum);
        BROOKESIA_CHECK_FALSE_EXIT(convert_result, "Failed to convert general event: %1%", general_event);

        switch (general_event_enum) {
        case AgentHelper::GeneralEvent::Stopped: {
            if (!is_wifi_connected()) {
                BROOKESIA_LOGW("WiFi is not connected, skip restart agent");
                return;
            }

            BROOKESIA_LOGW("Try to restart agent in %1% seconds...", agent_restart_delay_s_);

            auto task_func = []() {
                BROOKESIA_LOG_TRACE_GUARD();
                BROOKESIA_LOGW("Restarting agent...");
                AgentHelper::call_function_sync(
                    AgentHelper::FunctionId::TriggerGeneralAction,
                    BROOKESIA_DESCRIBE_TO_STR(AgentHelper::GeneralAction::Start)
                );
            };
            auto result = task_scheduler_->post_delayed(std::move(task_func), agent_restart_delay_s_ * 1000);
            BROOKESIA_CHECK_FALSE_EXIT(result, "Failed to post restart agent task");
            break;
        }
        default:
            break;
        }
    };
    auto connection = AgentHelper::subscribe_event(
                          AgentHelper::EventId::GeneralEventHappened, general_event_happened_slot
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

bool AI_Agents::start_agent()
{
    // Ensure target agent is activated after switching
    auto activate_handler = [this](service::FunctionResult && result) {
        if (!result.success) {
            BROOKESIA_LOGE("Failed to activate agent: %1%", result.error_message);
        }
    };
    auto activate_result = AgentHelper::call_function_async(
                               AgentHelper::FunctionId::TriggerGeneralAction,
                               BROOKESIA_DESCRIBE_TO_STR(AgentHelper::GeneralAction::Activate), activate_handler
                           );
    BROOKESIA_CHECK_FALSE_RETURN(activate_result, false, "Failed to activate agent");

    auto start_handler = [this](service::FunctionResult && result) {
        if (!result.success) {
            BROOKESIA_LOGE("Failed to trigger general action: %1%", result.error_message);
        }
    };
    auto trigger_general_action_result = AgentHelper::call_function_async(
            AgentHelper::FunctionId::TriggerGeneralAction,
            BROOKESIA_DESCRIBE_TO_STR(AgentHelper::GeneralAction::Start), start_handler
                                         );
    BROOKESIA_CHECK_FALSE_RETURN(trigger_general_action_result, false, "Failed to trigger general action");

    return true;
}

void AI_Agents::stop_agent()
{
    AgentHelper::call_function_async(
        AgentHelper::FunctionId::TriggerGeneralAction, BROOKESIA_DESCRIBE_TO_STR(AgentHelper::GeneralAction::Stop)
    );
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
            auto task_func = [this]() {
                BROOKESIA_LOG_TRACE_GUARD();
                EmoteHelper::call_function_async(EmoteHelper::FunctionId::SetEmoji, "sad");
                EmoteHelper::call_function_async(
                    EmoteHelper::FunctionId::SetEventMessage,
                    BROOKESIA_DESCRIBE_TO_STR(EmoteHelper::EventMessageType::System),
                    (boost::format(
                         "[Agent] Insufficient credits balance, please top up balance, will restart in %1% seconds..."
                     ) % (agent_restart_delay_s_ - coze_error_show_emote_delay_s_)).str()
                );
            };
            auto result = task_scheduler_->post_delayed(
                              std::move(task_func), coze_error_show_emote_delay_s_ * 1000
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

    process_emote_when_coze_event_happened();
}

void AI_Agents::process_wifi_events()
{
    auto general_action_triggered_slot =
    [this](const std::string & event_name, const std::string & general_action) {
        BROOKESIA_LOG_TRACE_GUARD();

        BROOKESIA_LOGD("Params: event_name(%1%), general_action(%2%)", event_name, general_action);

        WifiHelper::GeneralAction general_action_enum;
        auto convert_result = BROOKESIA_DESCRIBE_STR_TO_ENUM(general_action, general_action_enum);
        BROOKESIA_CHECK_FALSE_EXIT(convert_result, "Failed to convert general action: %1%", general_action);

        std::string emote_message = "";
        lib_utils::TaskScheduler::OnceTask task_func;

        switch (general_action_enum) {
        case WifiHelper::GeneralAction::Start:
            emote_message = "[WiFi] Starting...";
            break;
        case WifiHelper::GeneralAction::Connect:
            task_func = [this]() {
                BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

                auto get_ap_result = WifiHelper::call_function_sync<boost::json::object>(
                                         WifiHelper::FunctionId::GetConnectAp
                                     );
                BROOKESIA_CHECK_FALSE_EXIT(get_ap_result, "Failed to get connect AP");

                WifiHelper::ConnectApInfo connect_ap_info;
                auto parse_result = BROOKESIA_DESCRIBE_FROM_JSON(get_ap_result.value(), connect_ap_info);
                BROOKESIA_CHECK_FALSE_EXIT(parse_result, "Failed to parse connect AP info");

                auto emote_message = (boost::format("[WiFi] Connecting to %1% ...") % connect_ap_info.ssid).str();
                auto set_emote_result = EmoteHelper::call_function_async(
                                            EmoteHelper::FunctionId::SetEventMessage,
                                            BROOKESIA_DESCRIBE_TO_STR(EmoteHelper::EventMessageType::System), emote_message
                                        );
                BROOKESIA_CHECK_FALSE_EXIT(set_emote_result, "Failed to set event message");
            };
            break;
        case WifiHelper::GeneralAction::Disconnect:
            emote_message = "[WiFi] Disconnecting...";
            break;
        case WifiHelper::GeneralAction::Stop:
            emote_message = "[WiFi] Stopping...";
            break;
        default:
            break;
        }

        if (!emote_message.empty()) {
            EmoteHelper::call_function_async(
                EmoteHelper::FunctionId::SetEventMessage,
                BROOKESIA_DESCRIBE_TO_STR(EmoteHelper::EventMessageType::System), emote_message
            );
        }
        if (task_func) {
            auto result = task_scheduler_->post(std::move(task_func));
            BROOKESIA_CHECK_FALSE_EXIT(result, "Failed to post task function");
        }
    };
    auto general_action_triggered_connection = WifiHelper::subscribe_event(
                WifiHelper::EventId::GeneralActionTriggered, general_action_triggered_slot
            );
    BROOKESIA_CHECK_FALSE_EXIT(
        general_action_triggered_connection.connected(), "Failed to subscribe to Agent WiFi general action triggered event"
    );
    service_connections_.push_back(std::move(general_action_triggered_connection));

    auto general_event_happened_slot =
    [this](const std::string & event_name, const std::string & wifi_event, bool is_unexpected) {
        BROOKESIA_LOG_TRACE_GUARD();

        BROOKESIA_LOGD(
            "Params: event_name(%1%), wifi_event(%2%), is_unexpected(%3%)", event_name, wifi_event, is_unexpected
        );

        WifiHelper::GeneralEvent wifi_event_enum;
        auto convert_result = BROOKESIA_DESCRIBE_STR_TO_ENUM(wifi_event, wifi_event_enum);
        BROOKESIA_CHECK_FALSE_EXIT(convert_result, "Failed to convert wifi event: %1%", wifi_event);

        std::string emote_message = "";
        lib_utils::TaskScheduler::OnceTask task_func;

        switch (wifi_event_enum) {
        case WifiHelper::GeneralEvent::Connected: {
            emote_message = "[WiFi] Connected";
            is_wifi_connected_ = true;
            task_func = [this]() {
                BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
                BROOKESIA_CHECK_FALSE_EXIT(start_agent(), "Failed to start agent");
            };
            break;
        }
        case WifiHelper::GeneralEvent::Disconnected: {
            emote_message = "[WiFi] Disconnected";
            is_wifi_connected_ = false;
            task_func = [this]() {
                BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
                stop_agent();
            };
            break;
        }
        default:
            break;
        }

        if (!emote_message.empty()) {
            EmoteHelper::call_function_async(
                EmoteHelper::FunctionId::SetEventMessage,
                BROOKESIA_DESCRIBE_TO_STR(EmoteHelper::EventMessageType::System), emote_message
            );
        }
        if (task_func) {
            auto result = task_scheduler_->post(std::move(task_func));
            BROOKESIA_CHECK_FALSE_EXIT(result, "Failed to post task function");
        }
    };
    auto general_event_happened_connection = WifiHelper::subscribe_event(
                WifiHelper::EventId::GeneralEventHappened, general_event_happened_slot
            );
    BROOKESIA_CHECK_FALSE_EXIT(
        general_event_happened_connection.connected(), "Failed to subscribe to Agent WiFi general event happened event"
    );
    service_connections_.push_back(std::move(general_event_happened_connection));

    auto soft_ap_event_happened_slot =
    [this](const std::string & event_name, const std::string & soft_ap_event) {
        BROOKESIA_LOG_TRACE_GUARD();

        BROOKESIA_LOGD("Params: event_name(%1%), soft_ap_event(%2%)", event_name, soft_ap_event);

        WifiHelper::SoftApEvent soft_ap_event_enum;
        auto convert_result = BROOKESIA_DESCRIBE_STR_TO_ENUM(soft_ap_event, soft_ap_event_enum);
        BROOKESIA_CHECK_FALSE_EXIT(convert_result, "Failed to convert soft AP event: %1%", soft_ap_event);

        switch (soft_ap_event_enum) {
        case WifiHelper::SoftApEvent::Started: {
            auto task_func = [this]() {
                BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
                auto get_soft_ap_params_result = WifiHelper::call_function_sync<boost::json::object>(
                                                     WifiHelper::FunctionId::GetSoftApParams
                                                 );
                BROOKESIA_CHECK_FALSE_EXIT(get_soft_ap_params_result, "Failed to get soft AP params");

                WifiHelper::SoftApParams soft_ap_params;
                auto parse_result = BROOKESIA_DESCRIBE_FROM_JSON(get_soft_ap_params_result.value(), soft_ap_params);
                BROOKESIA_CHECK_FALSE_EXIT(parse_result, "Failed to parse soft AP params");

                auto emote_message = (boost::format("[WiFi] SoftAP started. Please scan the QR code or connect to %1%(PWD:%2%)") %
                                      soft_ap_params.ssid % soft_ap_params.password).str();
                auto set_emote_result = EmoteHelper::call_function_async(
                                            EmoteHelper::FunctionId::SetEventMessage,
                                            BROOKESIA_DESCRIBE_TO_STR(EmoteHelper::EventMessageType::System), emote_message
                                        );
                BROOKESIA_CHECK_FALSE_EXIT(set_emote_result, "Failed to set event message");

                std::string qr_string = "WIFI:T:nopass;S:" + soft_ap_params.ssid + ";P:" + soft_ap_params.password + ";;";
                auto set_qr_result = EmoteHelper::call_function_async(
                                         EmoteHelper::FunctionId::SetQrcode, qr_string
                                     );
                BROOKESIA_CHECK_FALSE_EXIT(set_qr_result, "Failed to set QR code");
            };
            auto result = task_scheduler_->post(std::move(task_func));
            BROOKESIA_CHECK_FALSE_EXIT(result, "Failed to post task function");
            break;
        }
        default:
            break;
        }
    };
    auto soft_ap_event_happened_connection = WifiHelper::subscribe_event(
                WifiHelper::EventId::SoftApEventHappened, soft_ap_event_happened_slot
            );
    BROOKESIA_CHECK_FALSE_EXIT(
        soft_ap_event_happened_connection.connected(), "Failed to subscribe to Agent WiFi soft AP event happened event"
    );
    service_connections_.push_back(std::move(soft_ap_event_happened_connection));
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
