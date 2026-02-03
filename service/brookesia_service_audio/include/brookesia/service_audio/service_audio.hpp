/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <string>
#include <map>
#include <queue>
#include <mutex>
#include "boost/format.hpp"
#include "boost/thread/thread.hpp"
#include "brookesia/service_helper/audio.hpp"
#include "brookesia/service_manager/service/base.hpp"
#include "brookesia/service_manager/macro_configs.h"
#include "brookesia/lib_utils/describe_helpers.hpp"

namespace esp_brookesia::service {

using AudioPeripheralConfig = helper::Audio::PeripheralConfig;
using AudioPlaybackConfig = helper::Audio::PlaybackConfig;
using AudioMixerGainConfig = helper::Audio::MixerGainConfig;
using AudioCodecFormat = helper::Audio::CodecFormat;
using AudioCodecGeneralConfig = helper::Audio::CodecGeneralConfig;
using AudioEncoderStaticConfig = helper::Audio::EncoderStaticConfig;
using AudioEncoderExtraConfigOpus = helper::Audio::EncoderExtraConfigOpus;
using AudioEncoderDynamicConfig = helper::Audio::EncoderDynamicConfig;
using AudioDecoderStaticConfig = helper::Audio::DecoderStaticConfig;
using AudioDecoderDynamicConfig = helper::Audio::DecoderDynamicConfig;
using AudioAFE_VAD_Config = helper::Audio::AFE_VAD_Config;
using AudioAFE_WakeNetConfig = helper::Audio::AFE_WakeNetConfig;
using AudioAFE_Config = helper::Audio::AFE_Config;
using AudioPlayControlAction = helper::Audio::PlayControlAction;
using AudioPlayState = helper::Audio::PlayState;
using AudioPlayUrlConfig = helper::Audio::PlayUrlConfig;

class Audio : public ServiceBase {
public:
    enum class DataType {
        PlayerVolume,
        Max,
    };

    static Audio &get_instance()
    {
        static Audio instance;
        return instance;
    }

private:
    using Helper = helper::Audio;

    Audio()
        : ServiceBase({
        .name = Helper::get_name().data(),
#if BROOKESIA_SERVICE_AUDIO_ENABLE_WORKER
        .task_scheduler_config = lib_utils::TaskScheduler::StartConfig{
            .worker_configs = {
                lib_utils::ThreadConfig{
                    .name = BROOKESIA_SERVICE_AUDIO_WORKER_NAME,
                    .core_id = BROOKESIA_SERVICE_AUDIO_WORKER_CORE_ID,
                    .priority = BROOKESIA_SERVICE_AUDIO_WORKER_PRIORITY,
                    .stack_size = BROOKESIA_SERVICE_AUDIO_WORKER_STACK_SIZE,
                    .stack_in_ext = BROOKESIA_SERVICE_AUDIO_WORKER_STACK_IN_EXT,
                }
            },
            .worker_poll_interval_ms = BROOKESIA_SERVICE_AUDIO_WORKER_POLL_INTERVAL_MS,
        }
#endif // BROOKESIA_SERVICE_AUDIO_ENABLE_WORKER
    })
    {}
    ~Audio() = default;

    bool on_init() override;
    void on_deinit() override;
    bool on_start() override;
    void on_stop() override;

    std::expected<void, std::string> function_set_peripheral(const boost::json::object &config);
    std::expected<void, std::string> function_set_playback_config(const boost::json::object &config);
    std::expected<void, std::string> function_set_encoder_static_config(const boost::json::object &config);
    std::expected<void, std::string> function_set_decoder_static_config(const boost::json::object &config);
    std::expected<void, std::string> function_set_afe_config(const boost::json::object &config);
    std::expected<void, std::string> function_play_url(const std::string &url, const boost::json::object &config);
    std::expected<void, std::string> function_play_urls(
        const boost::json::array &urls, const boost::json::object &config
    );
    std::expected<void, std::string> function_play_control(const std::string &action);
    std::expected<void, std::string> function_set_volume(double volume);
    std::expected<double, std::string> function_get_volume();
    std::expected<void, std::string> function_start_encoder(const boost::json::object &config);
    std::expected<void, std::string> function_stop_encoder();
    std::expected<void, std::string> function_start_decoder(const boost::json::object &config);
    std::expected<void, std::string> function_stop_decoder();
    std::expected<void, std::string> function_feed_decoder_data(const RawBuffer &data);

    std::vector<FunctionSchema> get_function_schemas() override
    {
        auto function_schemas = Helper::get_function_schemas();
        return std::vector<FunctionSchema>(function_schemas.begin(), function_schemas.end());
    }

    std::vector<EventSchema> get_event_schemas() override
    {
        auto event_schemas = Helper::get_event_schemas();
        return std::vector<EventSchema>(event_schemas.begin(), event_schemas.end());
    }
    ServiceBase::FunctionHandlerMap get_function_handlers() override
    {
        return {
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::SetPeripheralConfig, boost::json::object,
                function_set_peripheral(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::SetPlaybackConfig, boost::json::object,
                function_set_playback_config(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::SetEncoderStaticConfig, boost::json::object,
                function_set_encoder_static_config(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::SetDecoderStaticConfig, boost::json::object,
                function_set_decoder_static_config(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::SetAFE_Config, boost::json::object,
                function_set_afe_config(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_2(
                Helper, Helper::FunctionId::PlayUrl, std::string, boost::json::object,
                function_play_url(PARAM1, PARAM2)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_2(
                Helper, Helper::FunctionId::PlayUrls, boost::json::array, boost::json::object,
                function_play_urls(PARAM1, PARAM2)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::PlayControl, std::string,
                function_play_control(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::SetVolume, double,
                function_set_volume(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::GetVolume,
                function_get_volume()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::StartEncoder, boost::json::object,
                function_start_encoder(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::StopEncoder,
                function_stop_encoder()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::StartDecoder, boost::json::object,
                function_start_decoder(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::StopDecoder,
                function_stop_decoder()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::FeedDecoderData, RawBuffer,
                function_feed_decoder_data(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::SetAFE_Config, boost::json::object,
                function_set_afe_config(PARAM)
            ),
        };
    }

    template<DataType type>
    constexpr auto &get_data()
    {
        if constexpr (type == DataType::PlayerVolume) {
            return data_player_volume_;
        } else {
            static_assert(false, "Invalid data type");
        }
    }
    template<DataType type>
    void set_data(const auto &data)
    {
        if constexpr (type == DataType::PlayerVolume) {
            data_player_volume_ = data;
        } else {
            static_assert(false, "Invalid data type");
        }
    }
    void try_load_data();
    void try_save_data(DataType type);
    void try_erase_data();

    bool start_encoder(const AudioEncoderDynamicConfig &config);
    void stop_encoder();
    bool is_encoder_started() const
    {
        return is_encoder_started_;
    }
    bool start_decoder(const AudioDecoderDynamicConfig &config);
    void stop_decoder();
    bool is_decoder_started() const
    {
        return is_decoder_started_;
    }
    void on_recorder_event(void *event);
    void on_recorder_input_data(const uint8_t *data, size_t size);

    void on_playback_event(uint8_t state);
    void process_play_url_queue();
    bool play_url_internal(const std::string &url, const AudioPlayUrlConfig &config);
    void handle_loop_playback_complete();
    void schedule_next_loop_iteration();

    void cancel_loop_scheduled_task();
    void suspend_loop_scheduled_task();
    void resume_loop_scheduled_task();

    bool is_data_loaded_ = false;
    int data_player_volume_ = -1;

    /* Codec related */
    bool is_encoder_started_ = false;
    bool is_decoder_started_ = false;
    AudioPeripheralConfig peripheral_config_{};
    AudioPlaybackConfig playback_config_{};
    AudioEncoderStaticConfig encoder_static_config_{};
    AudioDecoderStaticConfig decoder_static_config_{};
    AudioAFE_Config afe_config_{};
    std::string afe_model_partition_label_;
    std::string afe_mn_language_;
    boost::thread recorder_fetcher_thread_;

    /* Playback related */
    AudioPlayState play_state_ = AudioPlayState::Idle;
    // Play URL queue for non-interrupt playback
    struct PlayUrlRequest {
        std::string url;
        AudioPlayUrlConfig config;
    };
    std::queue<PlayUrlRequest> play_url_queue_;
    std::mutex play_url_queue_mutex_;
    bool is_processing_queue_ = false;
    // Loop playback state for async loop handling
    struct LoopPlaybackState {
        std::string url;
        AudioPlayUrlConfig config;
        uint32_t current_iteration = 0;
        uint32_t total_iterations = 0;
        int64_t start_time_ms = 0;
        bool is_active = false;
        lib_utils::TaskScheduler::TaskId scheduled_task_id = 0;
        uint32_t session_id = 0;  // Used to distinguish different loop sessions
        int64_t pause_start_ms = 0;      // Time when pause started (0 if not paused)
        int64_t paused_duration_ms = 0;  // Total accumulated pause duration
    };
    LoopPlaybackState loop_state_;
    std::mutex loop_state_mutex_;
    uint32_t loop_session_counter_ = 0;  // Monotonically increasing session counter
};

BROOKESIA_DESCRIBE_ENUM(Audio::DataType, PlayerVolume, Max)

} // namespace esp_brookesia::service
