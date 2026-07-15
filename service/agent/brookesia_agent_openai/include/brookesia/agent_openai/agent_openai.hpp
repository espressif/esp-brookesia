/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <expected>

#include "brookesia/service_helper/media/audio.hpp"
#include "brookesia/service_helper/agent/openai.hpp"
#include "brookesia/agent_manager/base.hpp"
#include "brookesia/agent_openai/macro_configs.h"

namespace esp_brookesia::agent {

/**
 * @brief Re-exported runtime configuration type for the OpenAI agent.
 */
using OpenaiInfo = service::helper::Openai::Info;

/**
 * @brief Concrete agent implementation backed by the OpenAI service.
 */
class Openai: public Base {
public:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////// The following are the agent specific attributes /////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /**
     * @brief Default metadata advertised by the OpenAI agent.
     */
    inline static const AgentAttributes DEFAULT_AGENT_ATTRIBUTES{
        .name = service::helper::Openai::get_name().data(),
        .operation_timeout = {
            .start = 10000,
        },
    };
    /**
     * @brief Default audio pipeline configuration used by the OpenAI agent.
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
     * @brief Get the singleton OpenAI agent instance.
     *
     * @return Openai& Singleton instance.
     */
    static Openai &get_instance()
    {
        static Openai instance;
        return instance;
    }

private:
    static std::string get_component_version();

    Openai()
        : Base(
              DEFAULT_AGENT_ATTRIBUTES,
              get_component_version(),
              DEFAULT_AUDIO_CONFIG
          )
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
        auto helper_schemas = service::helper::Openai::get_function_schemas();
        return std::vector<service::FunctionSchema>(helper_schemas.begin(), helper_schemas.end());
    }
    std::vector<service::EventSchema> get_event_schemas() override
    {
        auto helper_schemas = service::helper::Openai::get_event_schemas();
        return std::vector<service::EventSchema>(helper_schemas.begin(), helper_schemas.end());
    }
    service::ServiceBase::FunctionHandlerMap get_function_handlers() override
    {
        return {};
    }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////// The following are the agent specific interfaces ////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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

    OpenaiInfo data_info_{};

    bool is_openai_initialized_ = false;
    bool is_openai_started_ = false;
};

} // namespace esp_brookesia::agent
