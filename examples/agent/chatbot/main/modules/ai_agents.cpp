/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "sdkconfig.h"
#include "brookesia/lib_utils/function_guard.hpp"
#include "brookesia/service_manager.hpp"
#include "brookesia/service_helper.hpp"
#include "brookesia/mcp_utils/mcp_utils.hpp"
#include "brookesia/hal_interface.hpp"
#include "private/utils.hpp"
#include "modules/display/display.hpp"
#include "ai_agents.hpp"

using namespace esp_brookesia;

using AgentHelper = esp_brookesia::service::helper::AgentManager;
using CozeHelper = esp_brookesia::service::helper::Coze;
using OpenaiHelper = esp_brookesia::service::helper::Openai;
using XiaoZhiHelper = esp_brookesia::service::helper::XiaoZhi;
using EmoteHelper = esp_brookesia::service::helper::ExpressionEmote;
using AudioHelper = esp_brookesia::service::helper::Audio;
using AudioPlaybackHelper = esp_brookesia::service::helper::AudioPlayback;
using WifiHelper = esp_brookesia::service::helper::Wifi;
using DeviceHelper = esp_brookesia::service::helper::Device;
using VideoHelper = esp_brookesia::service::helper::Video;
using VideoEncoderHelper = esp_brookesia::service::helper::VideoEncoder<0>;
using VideoDecoderHelper = esp_brookesia::service::helper::VideoDecoder<0>;

#define XIAO_ZHI_AUDIO_URL_PREFIX "file://littlefs/xiaozhi/"

namespace {

constexpr const char *CAMERA_OPEN_TOOL_NAME = "Camera.Open";
constexpr const char *CAMERA_CLOSE_TOOL_NAME = "Camera.Close";
constexpr const char *CAMERA_TAKE_PHOTO_TOOL_NAME = "Camera.TakePhoto";
constexpr const char *CAMERA_TOOL_PARAM_QUESTION = "Question";
constexpr const char *CAMERA_DEFAULT_QUESTION = "What is in the image?";
constexpr uint16_t CAMERA_PREVIEW_WIDTH = 640;
constexpr uint16_t CAMERA_PREVIEW_HEIGHT = 480;
constexpr uint8_t CAMERA_PHOTO_FPS = 15;
constexpr uint8_t CAMERA_V4L2_BUFFER_COUNT = 2;
constexpr uint32_t CAMERA_FETCH_TIMEOUT_MS = 5000;
constexpr uint32_t CAMERA_EXPLAIN_TIMEOUT_MS = 15000;
constexpr int CAMERA_SINK_INDEX = 0;

using CameraDeviceInfo = DeviceHelper::CameraDeviceInfos::value_type;

std::mutex camera_mutex;
std::mutex camera_frame_mutex;
std::condition_variable camera_frame_cv;
std::vector<uint8_t> camera_frame_data;
bool camera_frame_ready = false;
bool camera_video_encoder_started = false;
bool camera_opened_by_mcp = false;
std::atomic_bool camera_video_decoder_started = false;
service::ServiceBinding camera_video_encoder_binding;
service::ServiceBinding camera_video_decoder_binding;
service::EventRegistry::SignalConnection camera_frame_ready_connection;

static bool add_camera_mcp_tool();
static service::FunctionResult camera_open_callback(service::FunctionParameterMap &&params);
static service::FunctionResult camera_close_callback(service::FunctionParameterMap &&params);
static service::FunctionResult camera_take_photo_callback(service::FunctionParameterMap &&params);
static bool ensure_camera_video_encoder_started(std::string &error_message);
static bool ensure_camera_video_decoder_started(std::string &error_message);
static void close_camera_video_pipeline();
static bool fetch_camera_frame(std::vector<uint8_t> &frame_data, std::string &error_message);
static bool get_camera_device_info(CameraDeviceInfo &device_info, std::string &error_message);
static bool subscribe_camera_frame_event(std::string &error_message);
static uint16_t get_camera_output_width();
static uint16_t get_camera_output_height();
static uint16_t get_camera_preview_width();
static uint16_t get_camera_preview_height();
static uint32_t get_camera_preview_x();
static uint32_t get_camera_preview_y();
static VideoHelper::EncoderConfig make_camera_encoder_config(const CameraDeviceInfo &device_info);
static std::optional<VideoHelper::EncoderSinkFormat> select_camera_source_format(
    const std::vector<VideoHelper::EncoderSinkFormat> &supported_formats
);
static VideoHelper::DecoderConfig make_camera_decoder_config();
static void reset_camera_frame_state();
static void on_camera_stream_frame_ready(
    const std::string &event_name, double sink_index, const boost::json::object &sink_info,
    const service::RawBuffer &frame
);
static service::FunctionResult make_camera_error_result(const std::string &error_message);
static std::string get_camera_question(const service::FunctionParameterMap &params);

} // namespace

#if CONFIG_EXAMPLE_AGENTS_ENABLE_COZE
extern const char coze_private_key_pem_start[] asm("_binary_private_key_pem_start");
extern const char coze_private_key_pem_end[]   asm("_binary_private_key_pem_end");
#endif // CONFIG_EXAMPLE_AGENTS_ENABLE_COZE

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

    task_scheduler_ = config.task_scheduler;
    emote_animation_duration_ms_ = config.emote_animation_duration_ms;
    agent_restart_delay_s_ = config.agent_restart_delay_s;
    coze_error_show_emote_delay_s_ = config.coze_error_show_emote_delay_s;

    auto binding = service::ServiceManager::get_instance().bind(AgentHelper::get_name().data());
    BROOKESIA_CHECK_FALSE_RETURN(binding.is_valid(), false, "Failed to bind Agent manager service");

    service_bindings_.push_back(std::move(binding));

#ifdef IDF_CI_BUILD
    BROOKESIA_LOGI("CI build detected, resetting Agent manager data");
    auto reset_data_result = AgentHelper::call_function_sync(AgentHelper::FunctionId::ResetData);
    BROOKESIA_CHECK_FALSE_RETURN(
        reset_data_result, false, "Failed to reset Agent manager data: %1%", reset_data_result.error()
    );
#endif

    process_agent_general_events();
    process_emote();
    process_wifi_events();

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

    // Get device capabilities
    auto get_device_capabilities_result = DeviceHelper::call_function_sync<boost::json::array>(
            DeviceHelper::FunctionId::GetCapabilities
                                          );
    BROOKESIA_CHECK_FALSE_EXIT(
        get_device_capabilities_result, "Failed to get device capabilities: %1%", get_device_capabilities_result.error()
    );
    BROOKESIA_LOGI("Device capabilities: %1%", get_device_capabilities_result.value());
    DeviceHelper::Capabilities device_capabilities;
    auto convert_result = BROOKESIA_DESCRIBE_FROM_JSON(get_device_capabilities_result.value(), device_capabilities);
    BROOKESIA_CHECK_FALSE_EXIT(convert_result, "Failed to convert device capabilities");
    auto has_capability = [&device_capabilities](std::string_view interface_name) {
        for (const auto &device : device_capabilities) {
            for (const auto &interface : device.interfaces) {
                if (interface.type_name == interface_name) {
                    return true;
                }
            }
        }
        return false;
    };
    // Build device service tools
    std::vector<DeviceHelper::FunctionId> device_functions{
        DeviceHelper::FunctionId::GetCapabilities,
        DeviceHelper::FunctionId::GetBoardInfo
    };
    if (has_capability(hal::power::BatteryIface::NAME)) {
        device_functions.push_back(DeviceHelper::FunctionId::GetPowerBatteryInfo);
        device_functions.push_back(DeviceHelper::FunctionId::GetPowerBatteryState);
        device_functions.push_back(DeviceHelper::FunctionId::SetPowerBatteryChargingEnabled);
    }
    if (has_capability(hal::video::CameraIface::NAME)) {
        device_functions.push_back(DeviceHelper::FunctionId::GetCameraDeviceInfos);
    }
    auto add_device_service_tools_result = XiaoZhiHelper::call_function_sync<boost::json::array>(
            XiaoZhiHelper::FunctionId::AddMCP_ToolsWithServiceFunction,
            std::string(DeviceHelper::get_name()),
            BROOKESIA_DESCRIBE_TO_JSON(device_functions).as_array()
                                           );
    BROOKESIA_CHECK_FALSE_EXIT(
        add_device_service_tools_result, "Failed to add device service tools: %1%",
        add_device_service_tools_result.error()
    );
    BROOKESIA_LOGI("Added device service tools: %1%", add_device_service_tools_result.value());

    if (!add_camera_mcp_tool()) {
        BROOKESIA_LOGW("XiaoZhi camera MCP tool is unavailable, continue without camera");
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
        AudioPlaybackHelper::call_function_async(
            AudioPlaybackHelper::FunctionId::PlayUrls,
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
                auto result = AgentHelper::call_function_sync(
                                  AgentHelper::FunctionId::TriggerGeneralAction,
                                  BROOKESIA_DESCRIBE_TO_STR(AgentHelper::GeneralAction::Start)
                              );
                if (!result) {
                    BROOKESIA_LOGW("Failed to restart agent: %1%", result.error());
                }
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

void AI_Agents::process_emote_when_power_battery_state_changed()
{
    auto slot = [this](const std::string & event_name, const boost::json::object & state) {
        BROOKESIA_LOG_TRACE_GUARD();
        BROOKESIA_LOGD("Params: event_name(%1%), state(%2%)", event_name, state);

        if (!is_inactive()) {
            BROOKESIA_LOGD("Agent is inactive, skip");
        }

        hal::power::BatteryIface::State battery_state;
        auto parse_result = BROOKESIA_DESCRIBE_FROM_JSON(state, battery_state);
        BROOKESIA_CHECK_FALSE_EXIT(parse_result, "Failed to parse power battery state");

        std::string battery_message = "";
        if ((battery_state.charge_state == hal::power::BatteryIface::ChargeState::Unknown) ||
                (battery_state.charge_state == hal::power::BatteryIface::ChargeState::NotCharging)) {
            battery_message = "0,";
        } else {
            battery_message = "1,";
        }
        battery_message += std::to_string(battery_state.percentage.value_or(0));
        BROOKESIA_LOGD("Battery message: %1%", battery_message);

        EmoteHelper::call_function_async(
            EmoteHelper::FunctionId::SetEventMessage, BROOKESIA_DESCRIBE_TO_STR(EmoteHelper::EventMessageType::Battery),
            battery_message
        );
    };
    auto connection = DeviceHelper::subscribe_event(DeviceHelper::EventId::PowerBatteryStateChanged, slot);
    if (connection.connected()) {
        service_connections_.push_back(std::move(connection));
    } else {
        BROOKESIA_LOGE("Failed to subscribe to Agent power battery state changed event");
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
    process_emote_when_power_battery_state_changed();
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
        .voice = CONFIG_EXAMPLE_AGENTS_OPENAI_VOICE,
    };
    return BROOKESIA_DESCRIBE_TO_JSON(openai_info).as_object();
#else
    return boost::json::object {};
#endif // CONFIG_EXAMPLE_AGENTS_ENABLE_OPENAI
}

namespace {

static bool add_camera_mcp_tool()
{
    if (!VideoEncoderHelper::is_available()) {
        BROOKESIA_LOGW("Video encoder service is not available");
        return false;
    }

    std::string error_message;
    CameraDeviceInfo device_info;
    if (!get_camera_device_info(device_info, error_message)) {
        BROOKESIA_LOGW("Camera device is not available: %1%", error_message);
        return false;
    }
    if (!VideoDecoderHelper::is_available()) {
        BROOKESIA_LOGW("Video decoder service is not available, camera preview will be disabled");
    }

    mcp_utils::CustomTool camera_open_tool = {
        .schema = {
            .name = CAMERA_OPEN_TOOL_NAME,
            .description = "Open the board camera and show camera preview when display preview is available.",
            .require_scheduler = false,
        },
        .callback = camera_open_callback,
    };
    mcp_utils::CustomTool camera_close_tool = {
        .schema = {
            .name = CAMERA_CLOSE_TOOL_NAME,
            .description = "Close the board camera and return to the emote display.",
            .require_scheduler = false,
        },
        .callback = camera_close_callback,
    };
    mcp_utils::CustomTool camera_take_photo_tool = {
        .schema = {
            .name = CAMERA_TAKE_PHOTO_TOOL_NAME,
            .description = "Take a photo with the board camera and answer the question about it.",
            .parameters = {
                {
                    .name = CAMERA_TOOL_PARAM_QUESTION,
                    .description = "Question text.",
                    .type = service::FunctionValueType::String,
                    .default_value = service::FunctionValue(std::string(CAMERA_DEFAULT_QUESTION)),
                },
            },
            .require_scheduler = false,
        },
        .callback = camera_take_photo_callback,
    };
    std::vector<mcp_utils::CustomTool> camera_tools;
    camera_tools.push_back(std::move(camera_open_tool));
    camera_tools.push_back(std::move(camera_close_tool));
    camera_tools.push_back(std::move(camera_take_photo_tool));

    auto result = XiaoZhiHelper::call_function_sync<boost::json::array>(
                      XiaoZhiHelper::FunctionId::AddMCP_ToolsWithCustomFunction,
                      BROOKESIA_DESCRIBE_TO_JSON(camera_tools).as_array()
                  );
    if (!result) {
        BROOKESIA_LOGE("Failed to add camera MCP tool: %1%", result.error());
        return false;
    }

    BROOKESIA_LOGI("Added XiaoZhi camera MCP tools: %1%", result.value());
    BROOKESIA_LOGI(
        "Camera MCP uses device info: id(%1%), name(%2%), path(%3%)",
        device_info.id, device_info.name, device_info.device_path
    );
    return true;
}

static service::FunctionResult camera_open_callback(service::FunctionParameterMap &&params)
{
    BROOKESIA_LOG_TRACE_GUARD();

    (void)params;
    std::lock_guard<std::mutex> lock(camera_mutex);

    std::string error_message;
    if (!ensure_camera_video_encoder_started(error_message)) {
        BROOKESIA_LOGE("Failed to open camera video encoder: %1%", error_message);
        return make_camera_error_result(error_message);
    }


    bool preview_started = false;
    std::string preview_error_message;
    if (ensure_camera_video_decoder_started(preview_error_message)) {

        preview_started = true;
        if (!Display::get_instance().show_video()) {
            BROOKESIA_LOGW("Failed to switch Display to video source after opening camera");
        }
    } else {
        BROOKESIA_LOGW("Camera preview is disabled: %1%", preview_error_message);
    }


    camera_opened_by_mcp = true;

    return service::FunctionResult {
        .success = true,
        .data = service::FunctionValue(std::string(
                                           preview_started ? "Camera opened" : "Camera opened without preview"
                                       )),
    };
}

static service::FunctionResult camera_close_callback(service::FunctionParameterMap &&params)
{
    BROOKESIA_LOG_TRACE_GUARD();

    (void)params;
    std::lock_guard<std::mutex> lock(camera_mutex);

    camera_opened_by_mcp = false;
    close_camera_video_pipeline();
    if (!Display::get_instance().show_emote()) {
        BROOKESIA_LOGW("Failed to switch Display back to emote source");
    }

    return service::FunctionResult {
        .success = true,
        .data = service::FunctionValue(std::string("Camera closed")),
    };
}

static service::FunctionResult camera_take_photo_callback(service::FunctionParameterMap &&params)
{
    BROOKESIA_LOG_TRACE_GUARD();

    std::lock_guard<std::mutex> lock(camera_mutex);
    auto question = get_camera_question(params);
    const bool keep_camera_open = camera_opened_by_mcp && camera_video_encoder_started;

    std::string error_message;
    reset_camera_frame_state();
    if (!ensure_camera_video_encoder_started(error_message)) {
        BROOKESIA_LOGE("Failed to start camera video encoder: %1%", error_message);
        return make_camera_error_result(error_message);
    }

    auto close_camera_if_needed = [keep_camera_open]() {
        if (!keep_camera_open) {
            close_camera_video_pipeline();
            if (!Display::get_instance().show_emote()) {
                BROOKESIA_LOGW("Failed to switch Display back to emote source");
            }
        }
    };
    lib_utils::FunctionGuard close_camera_guard(close_camera_if_needed);

    std::string preview_error_message;
    const bool preview_started = ensure_camera_video_decoder_started(preview_error_message);
    if (!preview_started) {
        BROOKESIA_LOGW("Camera preview is disabled: %1%", preview_error_message);
    } else if (!Display::get_instance().show_video()) {
        BROOKESIA_LOGW("Failed to switch Display to video source, continue camera recognition");
    }

    std::vector<uint8_t> frame_data;
    if (!fetch_camera_frame(frame_data, error_message)) {
        BROOKESIA_LOGE("%1%", error_message);
        return make_camera_error_result(error_message);
    }

    auto explain_result = XiaoZhiHelper::call_function_sync<std::string>(
                              XiaoZhiHelper::FunctionId::ExplainImage,
                              service::RawBuffer(frame_data.data(), frame_data.size()),
                              question,
                              service::helper::Timeout(CAMERA_EXPLAIN_TIMEOUT_MS)
                          );
    if (!explain_result) {
        error_message = "Explain camera image failed: " + explain_result.error();
        BROOKESIA_LOGE("%1%", error_message);
        return make_camera_error_result(error_message);
    }
    close_camera_if_needed();
    close_camera_guard.release();

    return service::FunctionResult {
        .success = true,
        .data = service::FunctionValue(std::move(explain_result.value())),
    };
}

static bool ensure_camera_video_encoder_started(std::string &error_message)
{
    if (camera_video_encoder_started) {
        return true;
    }

    if (!VideoEncoderHelper::is_available()) {
        error_message = "Video encoder service is not available";
        return false;
    }


    CameraDeviceInfo device_info;
    if (!get_camera_device_info(device_info, error_message)) {
        return false;
    }
    const auto &device_path = device_info.device_path;


    if (!camera_video_encoder_binding.is_valid()) {
        auto binding = service::ServiceManager::get_instance().bind(VideoEncoderHelper::get_name().data());
        if (!binding.is_valid()) {
            error_message = "Bind video encoder service failed";
            return false;
        }
        camera_video_encoder_binding = std::move(binding);
    }


    if (!subscribe_camera_frame_event(error_message)) {
        return false;
    }


    auto encoder_config = make_camera_encoder_config(device_info);
    auto open_result = VideoEncoderHelper::call_function_sync(
                           VideoEncoderHelper::FunctionId::Open,
                           BROOKESIA_DESCRIBE_TO_JSON(encoder_config).as_object()
                       );
    if (!open_result) {
        error_message = "Open camera video encoder failed: " + open_result.error();
        return false;
    }


    auto start_result = VideoEncoderHelper::call_function_sync(VideoEncoderHelper::FunctionId::Start);
    if (!start_result) {
        auto close_result = VideoEncoderHelper::call_function_sync(VideoEncoderHelper::FunctionId::Close);
        if (!close_result) {
            BROOKESIA_LOGE("Failed to close camera video encoder after start failure: %1%", close_result.error());
        }
        error_message = "Start camera video encoder failed: " + start_result.error();
        return false;
    }


    camera_video_encoder_started = true;
    BROOKESIA_LOGI(
        "Camera video encoder started: device(%1%), preview(%2%x%3%)",
        device_path,
        get_camera_preview_width(),
        get_camera_preview_height()
    );

    return true;
}

static bool ensure_camera_video_decoder_started(std::string &error_message)
{
    if (camera_video_decoder_started.load()) {
        return true;
    }

    if (!VideoDecoderHelper::is_available()) {
        error_message = "Video decoder service is not available";
        return false;
    }
    if (Display::get_instance().output_name().empty()) {
        error_message = "Display output is not initialized";
        return false;
    }
    if ((Display::get_instance().width() == 0) || (Display::get_instance().height() == 0)) {
        error_message = "Display output size is invalid";
        return false;
    }

    if (!camera_video_decoder_binding.is_valid()) {
        auto binding = service::ServiceManager::get_instance().bind(VideoDecoderHelper::get_name().data());
        if (!binding.is_valid()) {
            error_message = "Bind video decoder service failed";
            return false;
        }
        camera_video_decoder_binding = std::move(binding);
    }

    auto decoder_config = make_camera_decoder_config();
    auto open_result = VideoDecoderHelper::call_function_sync(
                           VideoDecoderHelper::FunctionId::Open,
                           BROOKESIA_DESCRIBE_TO_JSON(decoder_config).as_object()
                       );
    if (!open_result) {
        error_message = "Open camera video decoder failed: " + open_result.error();
        return false;
    }

    auto start_result = VideoDecoderHelper::call_function_sync(VideoDecoderHelper::FunctionId::Start);
    if (!start_result) {
        auto close_result = VideoDecoderHelper::call_function_sync(VideoDecoderHelper::FunctionId::Close);
        if (!close_result) {
            BROOKESIA_LOGE("Failed to close camera video decoder after start failure: %1%", close_result.error());
        }
        error_message = "Start camera video decoder failed: " + start_result.error();
        return false;
    }

    camera_video_decoder_started.store(true);
    BROOKESIA_LOGI(
        "Camera video decoder preview started: output(%1%, %2%x%3%), preview(%4%x%5% at %6%,%7%)",
        Display::get_instance().output_name(),
        Display::get_instance().width(),
        Display::get_instance().height(),
        get_camera_preview_width(),
        get_camera_preview_height(),
        get_camera_preview_x(),
        get_camera_preview_y()
    );

    return true;
}

static void close_camera_video_pipeline()
{
    if (camera_video_decoder_started.exchange(false)) {
        auto close_result = VideoDecoderHelper::call_function_sync(VideoDecoderHelper::FunctionId::Close);
        if (!close_result) {
            BROOKESIA_LOGW("Failed to close camera video decoder: %1%", close_result.error());
        }
    }

    if (camera_video_encoder_started) {
        auto close_result = VideoEncoderHelper::call_function_sync(VideoEncoderHelper::FunctionId::Close);
        if (!close_result) {
            BROOKESIA_LOGW("Failed to close camera video encoder: %1%", close_result.error());
        }
        camera_video_encoder_started = false;
    }
}

static bool fetch_camera_frame(std::vector<uint8_t> &frame_data, std::string &error_message)
{
    std::unique_lock<std::mutex> lock(camera_frame_mutex);
    auto got_frame = camera_frame_cv.wait_for(lock, std::chrono::milliseconds(CAMERA_FETCH_TIMEOUT_MS), []() {
        return camera_frame_ready;
    });
    if (!got_frame) {
        error_message = "Wait camera stream frame timed out";
        return false;
    }
    if (camera_frame_data.empty()) {
        error_message = "Camera frame is empty";
        return false;
    }

    frame_data = camera_frame_data;
    camera_frame_ready = false;

    return true;
}

static bool get_camera_device_info(CameraDeviceInfo &device_info, std::string &error_message)
{
    auto result = DeviceHelper::call_function_sync<boost::json::array>(DeviceHelper::FunctionId::GetCameraDeviceInfos);
    if (!result) {
        error_message = "Get camera device infos failed: " + result.error();
        return false;
    }

    DeviceHelper::CameraDeviceInfos device_infos;
    if (!BROOKESIA_DESCRIBE_FROM_JSON(result.value(), device_infos)) {
        error_message = "Failed to parse camera device infos";
        return false;
    }

    for (const auto &info : device_infos) {
        if (!info.device_path.empty()) {
            device_info = info;
            return true;
        }
    }

    if (device_infos.empty()) {
        error_message = "Camera device info is not available";
    } else {
        error_message = "Camera device info list does not contain a valid path";
    }
    return false;
}

static bool subscribe_camera_frame_event(std::string &error_message)
{
    if (camera_frame_ready_connection.connected()) {
        return true;
    }

    auto connection = VideoEncoderHelper::subscribe_event(
                          VideoEncoderHelper::EventId::StreamSinkFrameReady, on_camera_stream_frame_ready
                      );
    if (!connection.connected()) {
        error_message = "Subscribe camera frame event failed";
        return false;
    }

    camera_frame_ready_connection = std::move(connection);

    return true;
}

static uint16_t get_camera_output_width()
{
    return static_cast<uint16_t>(Display::get_instance().width());
}

static uint16_t get_camera_output_height()
{
    return static_cast<uint16_t>(Display::get_instance().height());
}

static uint16_t get_camera_preview_width()
{
    return CAMERA_PREVIEW_WIDTH;
}

static uint16_t get_camera_preview_height()
{
    return CAMERA_PREVIEW_HEIGHT;
}

static uint32_t get_camera_preview_x()
{
    const auto output_width = get_camera_output_width();
    const auto preview_width = get_camera_preview_width();
    if (output_width < preview_width) {
        return 0;
    }

    return (output_width - preview_width) / 2;
}

static uint32_t get_camera_preview_y()
{
    const auto output_height = get_camera_output_height();
    const auto preview_height = get_camera_preview_height();
    if (output_height < preview_height) {
        return 0;
    }

    return (output_height - preview_height) / 2;
}

static std::optional<VideoHelper::EncoderSinkFormat> select_camera_source_format(
    const std::vector<VideoHelper::EncoderSinkFormat> &supported_formats
)
{
    // Prefer raw source formats the capture pipeline can convert/encode efficiently.
    // The order reflects common DVP/MIPI sensor outputs (e.g. OV3660 emits YUYV).
    static constexpr VideoHelper::EncoderSinkFormat preferred[] = {
        VideoHelper::EncoderSinkFormat::RGB565,
        VideoHelper::EncoderSinkFormat::YUV422,
        VideoHelper::EncoderSinkFormat::O_UYY_E_VYY,
        VideoHelper::EncoderSinkFormat::YUV420,
    };

    for (auto format : preferred) {
        if (std::find(supported_formats.begin(), supported_formats.end(), format) != supported_formats.end()) {
            return format;
        }
    }

    return std::nullopt;
}

static VideoHelper::EncoderConfig make_camera_encoder_config(const CameraDeviceInfo &device_info)
{
    const auto preview_width = get_camera_preview_width();
    const auto preview_height = get_camera_preview_height();

    auto source = VideoHelper::EncoderSourceConfig{
        .device_path = device_info.device_path,
        .fixed_width = preview_width,
        .fixed_height = preview_height,
        .v4l2_buffer_count = CAMERA_V4L2_BUFFER_COUNT,
    };

    // Pick a fixed source format from the formats the camera actually reports. When the
    // list is empty (older HAL or enumeration failed), leave it unset and let the capture
    // pipeline auto-negotiate.
    auto selected_format = select_camera_source_format(device_info.supported_formats);
    if (selected_format.has_value()) {
        source.fixed_format = selected_format.value();
    }

    return VideoHelper::EncoderConfig{
        .sinks = std::vector<VideoHelper::EncoderSinkInfo>({
            {
                .format = VideoHelper::EncoderSinkFormat::MJPEG,
                .width = preview_width,
                .height = preview_height,
                .fps = CAMERA_PHOTO_FPS,
            },
        }),
        .enable_stream_mode = true,
        .source = source,
    };
}

static VideoHelper::DecoderConfig make_camera_decoder_config()
{
    return VideoHelper::DecoderConfig{
        .width = get_camera_preview_width(),
        .height = get_camera_preview_height(),
        .source_format = VideoHelper::DecoderSourceFormat::MJPEG,
        .enable_stream_mode = true,
        .enable_hw_acceleration = true,
        .display = VideoHelper::DecoderDisplayConfig{
            .output_name = "",
            .source_name = std::string(VideoHelper::DISPLAY_SOURCE_NAME),
            .source_role = std::string(VideoHelper::DISPLAY_SOURCE_ROLE),
            .x = get_camera_preview_x(),
            .y = get_camera_preview_y(),
            .draw_timeout_ms = 1000,
            .publish_sink_event = false,
        },
    };
}

static void reset_camera_frame_state()
{
    std::lock_guard<std::mutex> lock(camera_frame_mutex);
    camera_frame_data.clear();
    camera_frame_ready = false;
}

static void on_camera_stream_frame_ready(
    const std::string &event_name, double sink_index, const boost::json::object &sink_info,
    const service::RawBuffer &frame
)
{
    BROOKESIA_LOG_TRACE_GUARD();

    (void)event_name;
    (void)sink_info;
    if (static_cast<int>(sink_index) != CAMERA_SINK_INDEX) {
        return;
    }

    bool notify_frame = false;
    if ((frame.data_ptr != nullptr) && (frame.data_size > 0)) {
        std::lock_guard<std::mutex> lock(camera_frame_mutex);
        if (camera_frame_data.empty()) {
            camera_frame_data.assign(frame.data_ptr, frame.data_ptr + frame.data_size);
            camera_frame_ready = true;
            notify_frame = true;
        }
    }
    if (notify_frame) {
        camera_frame_cv.notify_all();
    }

    if (camera_video_decoder_started.load() && (frame.data_ptr != nullptr) && (frame.data_size > 0)) {
        auto feed_result = VideoDecoderHelper::call_function_sync(VideoDecoderHelper::FunctionId::FeedFrame, frame);
        if (!feed_result) {
            BROOKESIA_LOGW("Failed to feed camera frame to video decoder: %1%", feed_result.error());
        }
    }
}

static service::FunctionResult make_camera_error_result(const std::string &error_message)
{
    return service::FunctionResult {
        .success = false,
        .error_message = error_message,
    };
}

static std::string get_camera_question(const service::FunctionParameterMap &params)
{
    auto question_it = params.find(CAMERA_TOOL_PARAM_QUESTION);
    if (question_it == params.end()) {
        return CAMERA_DEFAULT_QUESTION;
    }

    auto question = std::get_if<std::string>(&question_it->second);
    if ((question == nullptr) || question->empty()) {
        return CAMERA_DEFAULT_QUESTION;
    }

    return *question;
}
} // namespace
