/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <expected>

#include "brookesia/agent_manager/base.hpp"
#include "brookesia/service_helper/agent/coze.hpp"
#include "brookesia/lib_utils/task_scheduler.hpp"
#include "brookesia/service_manager/macro_configs.h"
#include "brookesia/agent_coze/macro_configs.h"

namespace esp_brookesia::agent {

/**
 * @brief Re-exported Coze authentication information type.
 */
using CozeAuthInfo = service::helper::Coze::AuthInfo;
/**
 * @brief Re-exported Coze robot metadata type.
 */
using CozeRobotInfo = service::helper::Coze::RobotInfo;
/**
 * @brief Re-exported Coze agent configuration type.
 */
using CozeInfo = service::helper::Coze::Info;
/**
 * @brief Re-exported Coze-specific event enumeration.
 */
using CozeErrorType = service::helper::Coze::CozeEvent;

/**
 * @brief Concrete agent implementation backed by the Coze service.
 */
class Coze: public Base {
public:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////// The following are the agent specific attributes /////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /**
     * @brief Persisted Coze runtime data items managed by the agent.
     */
    enum class DataType {
        BotIndex,
        Max,
    };

    /**
     * @brief Default metadata advertised by the Coze agent.
     */
    inline static const AgentAttributes DEFAULT_AGENT_ATTRIBUTES{
        .name = service::helper::Coze::get_name().data(),
        .operation_timeout = {
            .start = 5000,
        },
        .support_general_functions = {
            service::helper::AgentManager::AgentGeneralFunction::InterruptSpeaking,
        },
        .support_general_events = {
            service::helper::AgentManager::AgentGeneralEvent::SpeakingStatusChanged,
            service::helper::AgentManager::AgentGeneralEvent::ListeningStatusChanged,
            service::helper::AgentManager::AgentGeneralEvent::AgentSpeakingTextGot,
            service::helper::AgentManager::AgentGeneralEvent::EmoteGot,
        },
        .require_time_sync = true,
    };
    /**
     * @brief Default audio pipeline configuration used by the Coze agent.
     */
    static constexpr AudioConfig DEFAULT_AUDIO_CONFIG{
        .encoder = {
            .type = service::helper::Audio::CodecFormat::G711A,
            .general = {
                .channels = 1,
                .sample_bits = 16,
                .sample_rate = 16000,
                .frame_duration = 60,
            },
            .fetch_data_size = 1024,
            .enable_afe = true,
            .afe_wake_start_timeout_ms = 30000,
            .afe_wake_end_timeout_ms = 10000,
        },
        .decoder = {
            .type = service::helper::Audio::CodecFormat::G711A,
            .general = {
                .channels = 1,
                .sample_bits = 16,
                .sample_rate = 16000,
                .frame_duration = 60,
            }
        }
    };

    /**
     * @brief Get the singleton Coze agent instance.
     *
     * @return Coze& Singleton instance.
     */
    static Coze &get_instance()
    {
        static Coze instance;
        return instance;
    }

private:
    static std::string get_component_version();

    Coze()
        : Base(
              DEFAULT_AGENT_ATTRIBUTES,
              get_component_version(),
              DEFAULT_AUDIO_CONFIG
          )
    {
    }
    ~Coze() = default;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////// The following are the general interfaces which are inherited from the service::ServiceBase class /////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    bool on_init() override;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////// The following are the general interfaces which are inherited from the Base class ////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    bool on_activate() override;
    bool on_startup() override;
    bool on_sleep() override;
    bool on_wakeup() override;
    void on_shutdown() override;
    bool on_manual_stop_listening() override;
    bool on_interrupt_speaking() override;

    bool on_encoder_data_ready(const uint8_t *data, size_t data_size) override;
    bool set_info(const boost::json::object &info) override;
    bool reset_data() override;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////// The following are the agent specific interfaces ////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    std::expected<void, std::string> function_set_active_robot_index(double index);
    std::expected<double, std::string> function_get_active_robot_index();
    std::expected<boost::json::array, std::string> function_get_robot_infos();

    std::vector<service::FunctionSchema> get_function_schemas() override
    {
        auto helper_schemas = service::helper::Coze::get_function_schemas();
        return std::vector<service::FunctionSchema>(helper_schemas.begin(), helper_schemas.end());
    }
    std::vector<service::EventSchema> get_event_schemas() override
    {
        auto helper_schemas = service::helper::Coze::get_event_schemas();
        return std::vector<service::EventSchema>(helper_schemas.begin(), helper_schemas.end());
    }
    service::ServiceBase::FunctionHandlerMap get_function_handlers() override
    {
        return {
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                service::helper::Coze, service::helper::Coze::FunctionId::SetActiveRobotIndex, double,
                function_set_active_robot_index(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                service::helper::Coze, service::helper::Coze::FunctionId::GetActiveRobotIndex,
                function_get_active_robot_index()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                service::helper::Coze, service::helper::Coze::FunctionId::GetRobotInfos,
                function_get_robot_infos()
            )
        };
    }

    /**
     * @brief Whether the chat backend has been initialized.
     *
     * @return true if the backend handle exists.
     */
    bool is_chat_initialized()
    {
        return chat_handle_ != nullptr;
    }

    /**
     * @brief Whether the chat session has been started.
     *
     * @return true if startup completed successfully.
     */
    bool is_chat_started()
    {
        return is_chat_started_;
    }

    /**
     * @brief Whether the chat backend is connected to the remote service.
     *
     * @return true if connected.
     */
    bool is_chat_connected()
    {
        return is_chat_connected_;
    }

    /**
     * @brief Whether the chat backend is currently listening for audio.
     *
     * @return true if listening.
     */
    bool is_chat_listening()
    {
        return is_chat_listening_;
    }
    void set_chat_listening(bool listening);

    template<DataType type>
    constexpr auto &get_data()
    {
        if constexpr (type == DataType::BotIndex) {
            return active_robot_index_;
        } else {
            static_assert(false, "Invalid data type");
        }
    }
    template<DataType type>
    void set_data(const auto &data)
    {
        if constexpr (type == DataType::BotIndex) {
            active_robot_index_ = data;
        } else {
            static_assert(false, "Invalid data type");
        }
    }
    void try_load_data();
    void try_save_data(DataType type);
    void try_erase_data();

    bool validate_info(CozeInfo &info);
    bool get_mac_str(std::string &mac_str);

    bool on_audio_data(const uint8_t *data, int len);
    bool on_audio_event(uint8_t event_id, void *event_data);
    bool on_websocket_event(void *event_ptr);

    static int parse_error_code(std::string_view data);
    static std::string generate_random_string(size_t length);
    static std::string get_access_token(const CozeAuthInfo &auth_info);
    static std::string get_emote(std::string_view data);

    bool is_bot_index_loaded_ = false;
    uint8_t active_robot_index_ = 0;
    CozeInfo data_info_{};

    bool is_chat_started_ = false;
    bool is_chat_connected_ = false;
    bool is_chat_listening_ = false;
    void *chat_handle_ = nullptr;
    std::string speaking_text_{};
    lib_utils::TaskScheduler::TaskId speaking_text_task_id_ = 0;
};

BROOKESIA_DESCRIBE_ENUM(Coze::DataType, BotIndex, Max);

} // namespace esp_brookesia::agent
