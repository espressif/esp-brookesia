/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "brookesia/service_helper/audio.hpp"
#include "brookesia/agent_helper/openai.hpp"
#include "brookesia/agent_manager/base.hpp"

namespace esp_brookesia::agent {

using OpenaiInfo = helper::Openai::Info;

class Openai: public Base {
public:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////// The following are the agent specific attributes /////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    enum class DataType {
        Info,
        Max,
    };

    inline static const AgentAttributes DEFAULT_AGENT_ATTRIBUTES{
        .name = helper::Openai::get_name().data(),
        .operation_timeout = {
            .start = 10000,
        },
    };
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

    static Openai &get_instance()
    {
        static Openai instance;
        return instance;
    }

private:
    Openai()
        : Base(DEFAULT_AGENT_ATTRIBUTES, DEFAULT_AUDIO_CONFIG)
    {
    }
    ~Openai() = default;

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

    bool on_encoder_data_ready(const uint8_t *data, size_t data_size) override;
    bool set_info(const boost::json::object &info) override;
    bool reset_data() override;

    std::vector<service::FunctionSchema> get_function_schemas() override
    {
        auto helper_schemas = helper::Openai::get_function_schemas();
        return std::vector<service::FunctionSchema>(helper_schemas.begin(), helper_schemas.end());
    }
    std::vector<service::EventSchema> get_event_schemas() override
    {
        auto helper_schemas = helper::Openai::get_event_schemas();
        return std::vector<service::EventSchema>(helper_schemas.begin(), helper_schemas.end());
    }
    service::ServiceBase::FunctionHandlerMap get_function_handlers() override
    {
        return {};
    }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////// The following are the agent specific interfaces ////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    template<DataType type>
    constexpr auto &get_data()
    {
        if constexpr (type == DataType::Info) {
            return data_info_;
        } else {
            static_assert(false, "Invalid data type");
        }
    }
    template<DataType type>
    void set_data(auto &&data)
    {
        if constexpr (type == DataType::Info) {
            data_info_ = std::forward<decltype(data)>(data);
        } else {
            static_assert(false, "Invalid data type");
        }
    }
    void try_load_data();
    void try_save_data(DataType type);
    void try_erase_data();

    bool validate_info(OpenaiInfo &info);

    bool is_openai_initialized() const
    {
        return is_openai_initialized_;
    }
    bool is_openai_started() const
    {
        return is_openai_started_;
    }

    bool on_audio_data(uint8_t *data, int len);
    bool on_audio_event(int event, uint8_t *data);

    bool is_data_loaded_ = false;
    OpenaiInfo data_info_{};

    bool is_openai_initialized_ = false;
    bool is_openai_started_ = false;
};

BROOKESIA_DESCRIBE_ENUM(Openai::DataType, Info, Max);

} // namespace esp_brookesia::agent
