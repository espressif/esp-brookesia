/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_event.h"
#include "brookesia/lib_utils/task_scheduler.hpp"
#include "brookesia/service_helper/audio.hpp"
#include "brookesia/agent_helper/xiaozhi.hpp"
#include "brookesia/agent_manager/base.hpp"

namespace esp_brookesia::agent {

class XiaoZhi: public Base {
public:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////// The following are the agent specific attributes /////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    inline static const AgentAttributes DEFAULT_AGENT_ATTRIBUTES{
        .name = helper::XiaoZhi::get_name().data(),
        .operation_timeout = {
            .activate = 60000,
            .start = 5000,
            .sleep = 2000,
            .wake_up = 2000,
        },
        .support_general_functions = {
            helper::Manager::AgentGeneralFunction::InterruptSpeaking,
        },
        .support_general_events = {
            helper::Manager::AgentGeneralEvent::SpeakingStatusChanged,
            helper::Manager::AgentGeneralEvent::ListeningStatusChanged,
            helper::Manager::AgentGeneralEvent::AgentSpeakingTextGot,
            helper::Manager::AgentGeneralEvent::UserSpeakingTextGot,
            helper::Manager::AgentGeneralEvent::EmoteGot,
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

    static XiaoZhi &get_instance()
    {
        static XiaoZhi instance;
        return instance;
    }

private:
    XiaoZhi()
        : Base(DEFAULT_AGENT_ATTRIBUTES, DEFAULT_AUDIO_CONFIG)
    {
    }
    ~XiaoZhi() = default;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////// The following are the general interfaces which are inherited from the service::ServiceBase class /////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    bool on_init() override;

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

    std::vector<service::EventSchema> get_event_schemas() override
    {
        auto helper_schemas = helper::XiaoZhi::get_event_schemas();
        return std::vector<service::EventSchema>(helper_schemas.begin(), helper_schemas.end());
    }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////// The following are the agent specific interfaces ////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    bool is_xiaozhi_initialized() const
    {
        return is_xiaozhi_initialized_;
    }
    bool is_xiaozhi_started() const
    {
        return is_xiaozhi_started_;
    }
    bool is_xiaozhi_audio_channel_opened() const
    {
        return is_xiaozhi_audio_channel_opened_.load();
    }

    bool on_agent_event(int32_t event_id);
    bool on_chat_data(uint8_t *data, size_t len);
    bool on_chat_event(uint8_t event, void *event_data);

    bool is_xiaozhi_initialized_ = false;
    bool is_xiaozhi_started_ = false;
    std::atomic<bool> is_xiaozhi_audio_channel_opened_ = false;

    uint32_t chat_handle_ = 0;
    esp_event_handler_instance_t agent_event_instance_ = nullptr;
    lib_utils::TaskScheduler::TaskId http_info_task_id_ = 0;
};

} // namespace esp_brookesia::agent
