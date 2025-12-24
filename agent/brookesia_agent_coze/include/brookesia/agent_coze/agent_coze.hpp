/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_coze_chat.h"
#include "brookesia/agent_manager/base.hpp"
#include "brookesia/service_helper/agent/coze.hpp"
#include "brookesia/service_manager/macro_configs.h"

namespace esp_brookesia::agent {

using CozeAuthInfo = service::helper::AgentCoze::AuthInfo;
using CozeRobotInfo = service::helper::AgentCoze::RobotInfo;
using CozeInfo = service::helper::AgentCoze::Info;
using CozeErrorType = service::helper::AgentCoze::CozeEvent;

class Coze: public Base {
public:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////// The following are the agent specific attributes /////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    enum class DataType {
        Info,
        BotIndex,
        Max,
    };

    inline static const AgentAttributes DEFAULT_AGENT_ATTRIBUTES{
        .name = "Coze",
        .general_event_wait_timeout_ms = {10000, 100, 100, 100},
        .support_emote = true,
    };
    static constexpr AudioConfig DEFAULT_AUDIO_CONFIG{
        .encoder_feed_data_size = 1024,
        .encoder = {
            .type = service::helper::Audio::CodecFormat::G711A,
            .general = {
                .channels = 1,
                .sample_bits = 16,
                .sample_rate = 16000,
                .frame_duration = 60,
            }
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

    static Coze &get_instance()
    {
        static Coze instance;
        return instance;
    }

private:
    Coze()
        : Base(DEFAULT_AGENT_ATTRIBUTES, DEFAULT_AUDIO_CONFIG)
    {
    }
    ~Coze() = default;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////// The following are the general interfaces which are inherited from the Base class ////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    bool on_activate() override;

    bool on_init() override;
    bool on_start() override;
    void on_stop() override;
    bool on_sleep() override;
    void on_wakeup() override;

    bool on_encoder_data_ready(const uint8_t *data, size_t data_size) override;
    bool set_info(const boost::json::object &info) override;
    bool reset_data();

    std::expected<void, std::string> function_set_active_robot_index(double index);
    std::expected<double, std::string> function_get_active_robot_index();
    std::expected<boost::json::array, std::string> function_get_robot_infos();

    std::vector<service::FunctionSchema> get_function_schemas() override
    {
        auto helper_schemas = service::helper::AgentCoze::get_function_schemas();
        return std::vector<service::FunctionSchema>(helper_schemas.begin(), helper_schemas.end());
    }
    std::vector<service::EventSchema> get_event_schemas() override
    {
        auto helper_schemas = service::helper::AgentCoze::get_event_schemas();
        return std::vector<service::EventSchema>(helper_schemas.begin(), helper_schemas.end());
    }
    service::ServiceBase::FunctionHandlerMap get_function_handlers() override
    {
        return {
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                service::helper::AgentCoze, service::helper::AgentCoze::FunctionId::SetActiveRobotIndex, double,
                function_set_active_robot_index(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                service::helper::AgentCoze, service::helper::AgentCoze::FunctionId::GetActiveRobotIndex,
                function_get_active_robot_index()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                service::helper::AgentCoze, service::helper::AgentCoze::FunctionId::GetRobotInfos,
                function_get_robot_infos()
            )
        };
    }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////// The following are the agent specific interfaces ////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    bool is_chat_initialized()
    {
        return chat_handle_ != nullptr;
    }

    bool is_chat_started()
    {
        return is_chat_started_;
    }

    bool is_chat_connected()
    {
        return is_chat_connected_;
    }

    template<DataType type>
    constexpr auto &get_data()
    {
        if constexpr (type == DataType::Info) {
            return data_info_;
        } else if constexpr (type == DataType::BotIndex) {
            return active_robot_index_;
        } else {
            static_assert(false, "Invalid data type");
        }
    }
    template<DataType type>
    void set_data(const auto &data)
    {
        if constexpr (type == DataType::Info) {
            data_info_ = data;
        } else if constexpr (type == DataType::BotIndex) {
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

    bool on_audio_data(char *data, int len);
    bool on_audio_event(esp_coze_chat_event_t event, char *data);
    bool on_websocket_event(esp_coze_ws_event_t event);

    static esp_coze_chat_audio_type_t get_uplink_audio_type(service::helper::Audio::CodecFormat codec_format);
    static esp_coze_chat_audio_type_t get_downlink_audio_type(service::helper::Audio::CodecFormat codec_format);

    static void audio_data_callback(char *data, int len, void *ctx);
    static void audio_event_callback(esp_coze_chat_event_t event, char *data, void *ctx);
    static void websocket_event_callback(esp_coze_ws_event_t *event);

    static int parse_error_code(std::string_view data);
    static std::string generate_random_string(size_t length);
    static std::string get_access_token(const CozeAuthInfo &auth_info);
    static std::string get_emote(std::string_view data);

    bool is_data_loaded_ = false;
    uint8_t active_robot_index_ = 0;
    CozeInfo data_info_{};

    bool is_chat_started_ = false;
    bool is_chat_connected_ = false;
    esp_coze_chat_handle_t chat_handle_ = nullptr;
};

BROOKESIA_DESCRIBE_ENUM(Coze::DataType, Info, BotIndex, Max);

} // namespace esp_brookesia::agent
