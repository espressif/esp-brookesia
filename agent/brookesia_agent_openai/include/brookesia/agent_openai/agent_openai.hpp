/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "brookesia/service_helper/audio.hpp"
#include "brookesia/service_helper/agent/openai.hpp"
#include "brookesia/agent_manager/base.hpp"

namespace esp_brookesia::agent {

using OpenaiInfo = service::helper::AgentOpenai::Info;

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
        .name = service::helper::AgentOpenai::NAME.data(),
        .general_event_wait_timeout_ms = {10000, 100, 100, 100},
    };
    static constexpr AudioConfig DEFAULT_AUDIO_CONFIG{
        .encoder_feed_data_size = 2048,
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
            }
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
    bool reset_data() override;

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

    static void audio_data_callback(uint8_t *data, int len, void *ctx);
    static void audio_event_callback(int event, uint8_t *data, void *ctx);

    bool is_data_loaded_ = false;
    OpenaiInfo data_info_{};

    bool is_openai_initialized_ = false;
    bool is_openai_started_ = false;
};

BROOKESIA_DESCRIBE_ENUM(Openai::DataType, Info, Max);

} // namespace esp_brookesia::agent
