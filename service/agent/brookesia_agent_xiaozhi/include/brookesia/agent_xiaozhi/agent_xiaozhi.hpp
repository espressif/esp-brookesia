/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_event.h"
#include "brookesia/lib_utils/task_scheduler.hpp"
#include "brookesia/mcp_utils/mcp_utils.hpp"
#include "brookesia/service_helper/media/audio.hpp"
#include "brookesia/service_helper/agent/xiaozhi.hpp"
#include "brookesia/agent_manager/base.hpp"
#include "brookesia/agent_xiaozhi/macro_configs.h"

namespace esp_brookesia::agent {

/**
 * @brief Concrete agent implementation backed by the XiaoZhi service.
 */
class XiaoZhi: public Base {
public:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////// The following are the agent specific attributes /////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /**
     * @brief Default metadata advertised by the XiaoZhi agent.
     */
    inline static const AgentAttributes DEFAULT_AGENT_ATTRIBUTES{
        .name = service::helper::XiaoZhi::get_name().data(),
        .operation_timeout = {
            .activate = 60000,
            .start = 10000,
            .sleep = 5000,
            .wake_up = 5000,
        },
        .support_general_functions = {
            service::helper::AgentManager::AgentGeneralFunction::InterruptSpeaking,
        },
        .support_general_events = {
            service::helper::AgentManager::AgentGeneralEvent::SpeakingStatusChanged,
            service::helper::AgentManager::AgentGeneralEvent::ListeningStatusChanged,
            service::helper::AgentManager::AgentGeneralEvent::AgentSpeakingTextGot,
            service::helper::AgentManager::AgentGeneralEvent::UserSpeakingTextGot,
            service::helper::AgentManager::AgentGeneralEvent::EmoteGot,
        },
    };
    /**
     * @brief Default audio pipeline configuration used by the XiaoZhi agent.
     */
    static constexpr AudioConfig DEFAULT_AUDIO_CONFIG{
        .encoder = {
            .type = service::helper::Audio::CodecFormat::OPUS,
            .general = {
                .channels = 1,
                .sample_bits = 16,
                .sample_rate = 16000,
                .frame_duration = 60,
            },
            .extra = service::helper::Audio::EncoderExtraConfigOpus{
                .enable_vbr = false,
                .bitrate = 24000,
            },
            .fetch_data_size = 2048,
            .enable_afe = true,
            .afe_wake_start_timeout_ms = 30000,
            .afe_wake_end_timeout_ms = 10000,
        },
        .decoder = {
            .type = service::helper::Audio::CodecFormat::OPUS,
            .general = {
                .channels = 1,
                .sample_bits = 16,
                .sample_rate = 16000,
                .frame_duration = 60,
            },
        }
    };

    /**
     * @brief Get the singleton XiaoZhi agent instance.
     *
     * @return XiaoZhi& Singleton instance.
     */
    static XiaoZhi &get_instance()
    {
        static XiaoZhi instance;
        return instance;
    }
    /**
     * @brief Summary of available XiaoZhi chat transports.
     */
    struct ChatInfo {
        bool has_mqtt_config = false; ///< Whether MQTT transport information is available.
        bool has_websocket_config = false; ///< Whether WebSocket transport information is available.
    };

private:
    using Helper = service::helper::XiaoZhi;

    static std::string get_component_version();

    XiaoZhi()
        : Base(
              DEFAULT_AGENT_ATTRIBUTES,
              get_component_version(),
              DEFAULT_AUDIO_CONFIG
          )
    {
    }
    ~XiaoZhi() = default;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////// The following are the general interfaces which are inherited from the service::ServiceBase class /////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    bool on_init() override;
    void on_deinit() override;

    std::expected<boost::json::array, std::string> function_add_mcp_tools_with_service_function(
        const std::string &service_name, const boost::json::array &function_names
    );
    std::expected<boost::json::array, std::string> function_add_mcp_tools_with_custom_function(
        const boost::json::array &tools
    );
    std::expected<void, std::string> function_remove_mcp_tools(const boost::json::array &tools);
    std::expected<std::string, std::string> function_explain_image(
        const service::RawBuffer &image, const std::string &question
    );

    std::vector<service::FunctionSchema> get_function_schemas() override
    {
        auto function_schemas = Helper::get_function_schemas();
        return std::vector<service::FunctionSchema>(function_schemas.begin(), function_schemas.end());
    }
    std::vector<service::EventSchema> get_event_schemas() override
    {
        auto event_schemas = Helper::get_event_schemas();
        return std::vector<service::EventSchema>(event_schemas.begin(), event_schemas.end());
    }
    service::ServiceBase::FunctionHandlerMap get_function_handlers() override
    {
        return {
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_2(
                Helper, Helper::FunctionId::AddMCP_ToolsWithServiceFunction, std::string, boost::json::array,
                function_add_mcp_tools_with_service_function(PARAM1, PARAM2)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::AddMCP_ToolsWithCustomFunction, boost::json::array,
                function_add_mcp_tools_with_custom_function(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::RemoveMCP_Tools, boost::json::array,
                function_remove_mcp_tools(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_2(
                Helper, Helper::FunctionId::ExplainImage, service::RawBuffer, std::string,
                function_explain_image(PARAM1, PARAM2)
            ),
        };
    }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////// The following are the general interfaces which are inherited from the Base class /////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    bool on_activate() override;
    bool on_startup() override;
    bool on_sleep() override;
    bool on_wakeup() override;
    void on_shutdown() override;
    bool on_interrupt_speaking() override;
    bool on_manual_start_listening() override;
    bool on_manual_stop_listening() override;

    bool on_encoder_data_ready(const uint8_t *data, size_t data_size) override;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////// The following are the agent specific interfaces ////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /**
     * @brief Whether the XiaoZhi backend has been initialized.
     *
     * @return true if initialization completed successfully.
     */
    bool is_xiaozhi_initialized() const
    {
        return is_xiaozhi_initialized_;
    }
    /**
     * @brief Whether the XiaoZhi backend has been started.
     *
     * @return true if startup completed successfully.
     */
    bool is_xiaozhi_started() const
    {
        return is_xiaozhi_started_;
    }
    /**
     * @brief Whether the XiaoZhi audio channel is currently open.
     *
     * @return true if the uplink/downlink audio channel is available.
     */
    bool is_xiaozhi_audio_channel_opened() const
    {
        return is_xiaozhi_audio_channel_opened_.load();
    }

    bool on_agent_event(int32_t event_id);
    bool on_chat_data(const uint8_t *data, size_t len);
    bool on_chat_event(uint8_t event, void *event_data);

    /**
     * @brief Replace the cached chat transport capabilities.
     *
     * @param[in] chat_info New transport capability snapshot.
     */
    void set_chat_info(const ChatInfo &chat_info);
    /**
     * @brief Get the cached chat transport capabilities.
     *
     * @return const ChatInfo& Current chat transport snapshot.
     */
    const ChatInfo &get_chat_info() const
    {
        return chat_info_;
    }

    bool is_xiaozhi_initialized_ = false;
    bool is_xiaozhi_started_ = false;
    std::atomic<bool> is_xiaozhi_audio_channel_opened_ = false;

    uint32_t chat_handle_ = 0;
    esp_event_handler_instance_t agent_event_instance_ = nullptr;
    lib_utils::TaskScheduler::TaskId get_chat_info_task_ = 0;
    lib_utils::TaskScheduler::TaskId open_audio_channel_task_ = 0;
    ChatInfo chat_info_{};

    mcp_utils::ToolRegistry mcp_tool_registry_;

    void *image_explain_handle_ = nullptr;
};

BROOKESIA_DESCRIBE_STRUCT(XiaoZhi::ChatInfo, (), (has_mqtt_config, has_websocket_config));

} // namespace esp_brookesia::agent
