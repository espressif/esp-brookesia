/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <string>
#include <map>
#include "boost/format.hpp"
#include "boost/thread/thread.hpp"
#include "audio_processor.h"
#include "brookesia/service_helper/audio.hpp"
#include "brookesia/service_manager/service/base.hpp"
#include "brookesia/service_manager/macro_configs.h"
#include "brookesia/lib_utils/describe_helpers.hpp"

namespace esp_brookesia::service {

using AudioCodecFormat = helper::Audio::CodecFormat;
using AudioEncoderConfig = helper::Audio::EncoderConfig;
using AudioDecoderConfig = helper::Audio::DecoderConfig;
using AudioPlayControlAction = helper::Audio::PlayControlAction;
using AudioPlayState = helper::Audio::PlayState;

class Audio : public ServiceBase {
public:
    enum class DataType {
        PlayerVolume,
        Max,
    };

    struct PeripheralConfig {
        audio_manager_config_t manager_config;
        int player_volume_default;
        int player_volume_min;
        int player_volume_max;
        float recorder_gain;
        std::map<uint8_t, float> recorder_channel_gains;
    };

    using PlayerConfig = audio_playback_config_t;
    using RecorderConfig = audio_recorder_config_t;
    using FeederConfig = audio_feeder_config_t;

    bool configure_peripheral(const PeripheralConfig &config);
    bool configure_player(const PlayerConfig &config);
    bool configure_recorder(const RecorderConfig &config);
    bool configure_feeder(const FeederConfig &config);

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

    std::expected<void, std::string> function_play_url(const std::string &url);
    std::expected<void, std::string> function_play_control(const std::string &action);
    std::expected<void, std::string> function_set_volume(double volume);
    std::expected<double, std::string> function_get_volume();
    std::expected<void, std::string> function_start_encoder(const boost::json::object &config);
    std::expected<void, std::string> function_stop_encoder();
    std::expected<void, std::string> function_set_encoder_read_data_size(double size);
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
                Helper, Helper::FunctionId::PlayUrl, std::string,
                function_play_url(PARAM)
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
                Helper, Helper::FunctionId::SetEncoderReadDataSize, double,
                function_set_encoder_read_data_size(PARAM)
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
            )
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

    bool parse_encoder_config(const boost::json::object &json_data, av_processor_encoder_config_t &config);
    bool parse_decoder_config(const boost::json::object &json_data, av_processor_decoder_config_t &config);

    bool start_encoder(const av_processor_encoder_config_t &config);
    void stop_encoder();
    bool start_decoder(const av_processor_decoder_config_t &config);
    void stop_decoder();

    void on_playback_event(audio_player_state_t state);
    void on_recorder_event(void *event);

    static void playback_event_callback(audio_player_state_t state, void *ctx);
    static void recorder_event_callback(void *event, void *ctx);

    PeripheralConfig peripheral_config_{
        .manager_config = DEFAULT_AUDIO_MANAGER_CONFIG(),
        .player_volume_default = 70,
        .player_volume_min = 0,
        .player_volume_max = 100,
        .recorder_gain = 32.0,
        .recorder_channel_gains = {{2, 20.0}}
    };
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    PlayerConfig player_config_ = DEFAULT_AUDIO_PLAYBACK_CONFIG();
    RecorderConfig recorder_config_ = DEFAULT_AUDIO_RECORDER_CONFIG();
    FeederConfig feeder_config_ = DEFAULT_AUDIO_FEEDER_CONFIG();
#pragma GCC diagnostic pop

    bool is_data_loaded_ = false;
    int data_player_volume_ = 0;

    bool is_encoder_started_ = false;
    bool is_decoder_started_ = false;

    AudioPlayState play_state_ = AudioPlayState::Idle;

    size_t encoder_read_data_size_ = 0;
    boost::thread recorder_fetch_thread_;
};

BROOKESIA_DESCRIBE_ENUM(Audio::DataType, PlayerVolume, Max)
BROOKESIA_DESCRIBE_STRUCT(
    audio_manager_config_t, (), (
        play_dev, rec_dev, mic_layout, board_sample_rate, board_bits, board_channels
    )
)
BROOKESIA_DESCRIBE_STRUCT(
    Audio::PeripheralConfig, (), (
        manager_config, player_volume_default, player_volume_min, player_volume_max, recorder_gain,
        recorder_channel_gains
    )
)

} // namespace esp_brookesia::service
