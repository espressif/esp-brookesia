/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <cmath>
#include <algorithm>
#include "boost/format.hpp"
#include "boost/thread.hpp"
#include "brookesia/service_audio/macro_configs.h"
#if !BROOKESIA_SERVICE_AUDIO_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/lib_utils/plugin.hpp"
#include "brookesia/lib_utils/function_guard.hpp"
#include "brookesia/service_manager/macro_configs.h"
#include "brookesia/service_helper/nvs.hpp"
#include "brookesia/service_audio/service_audio.hpp"
#include "esp_bit_defs.h"
#include "esp_codec_dev.h"
#include "audio_processor.h"

namespace esp_brookesia::service {

using NVSHelper = helper::NVS;

constexpr lib_utils::ThreadConfig RECORDER_FETCH_THREAD_CONFIG = {
    .name = "am_rec_fetch",
    .core_id = 1,
    .priority = 12,
    .stack_size = 6 * 1024,
};
constexpr size_t RECORDER_FETCH_INTERVAL_MS = 10;
constexpr size_t DEFAULT_ENCODER_READ_DATA_SIZE = 4096;

constexpr size_t ENCODER_AFE_FETCH_TASK_STACK_SIZE_MIN = 6 * 1024;

constexpr uint32_t NVS_SAVE_DATA_TIMEOUT_MS = 20;
constexpr uint32_t NVS_ERASE_DATA_TIMEOUT_MS = 20;

bool Audio::configure_peripheral(const PeripheralConfig &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGI("Params: config(%1%)", BROOKESIA_DESCRIBE_TO_STR(config));

    BROOKESIA_CHECK_FALSE_RETURN(!is_running(), false, "Should be called before start");

    peripheral_config_ = config;

    return true;
}

bool Audio::configure_player(const PlayerConfig &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_FALSE_RETURN(!is_running(), false, "Should be called before start");

    player_config_ = config;

    return true;
}

bool Audio::configure_recorder(const RecorderConfig &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_FALSE_RETURN(!is_running(), false, "Should be called before start");

    recorder_config_ = config;

    return true;
}

bool Audio::configure_feeder(const FeederConfig &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_FALSE_RETURN(!is_running(), false, "Should be called before start");

    feeder_config_ = config;

    return true;
}

bool Audio::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGI(
        "Version: %1%.%2%.%3%", BROOKESIA_SERVICE_AUDIO_VER_MAJOR, BROOKESIA_SERVICE_AUDIO_VER_MINOR,
        BROOKESIA_SERVICE_AUDIO_VER_PATCH
    );

    return true;
}

void Audio::on_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGI("Deinitialized");
}

bool Audio::on_start()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    // Try to load data from NVS
    try_load_data();

    auto &manager_config = peripheral_config_.manager_config;

    // Open audio dac and audio_adc
    esp_codec_dev_sample_info_t fs = {
        .bits_per_sample = static_cast<uint8_t>(manager_config.board_bits),
        .channel = static_cast<uint8_t>(manager_config.board_channels),
        .sample_rate = static_cast<uint32_t>(manager_config.board_sample_rate),
    };
    BROOKESIA_LOGI(
        "Board sample info: sample_rate(%1%) channel(%2%) bits_per_sample(%3%)", fs.sample_rate, fs.channel,
        fs.bits_per_sample
    );
    BROOKESIA_CHECK_ESP_ERR_RETURN(
        esp_codec_dev_open(manager_config.play_dev, &fs), false, "Failed to open audio dac"
    );
    BROOKESIA_CHECK_ESP_ERR_RETURN(
        esp_codec_dev_open(manager_config.rec_dev, &fs), false, "Failed to open audio_adc"
    );

    // Initialize audio manager
    BROOKESIA_CHECK_ESP_ERR_RETURN(
        audio_manager_init(&manager_config), false, "Failed to initialize audio manager"
    );

    // Set player volume
    auto init_volume = peripheral_config_.player_volume_default;
    if (is_data_loaded_ && data_player_volume_ > 0) {
        init_volume = std::clamp(
                          data_player_volume_, peripheral_config_.player_volume_min, peripheral_config_.player_volume_max
                      );
        if (init_volume != data_player_volume_) {
            set_data<DataType::PlayerVolume>(init_volume);
            try_save_data(DataType::PlayerVolume);
        }
    }
    set_data<DataType::PlayerVolume>(init_volume);
    BROOKESIA_CHECK_FALSE_RETURN(
        esp_codec_dev_set_out_vol(manager_config.play_dev, init_volume) == ESP_CODEC_DEV_OK,
        false, "Failed to set play volume"
    );

    // Set recorder gain
    BROOKESIA_CHECK_FALSE_RETURN(
        esp_codec_dev_set_in_gain(manager_config.rec_dev, peripheral_config_.recorder_gain) == ESP_CODEC_DEV_OK,
        false, "Failed to set recorder gain"
    );
    for (const auto &[channel, gain] : peripheral_config_.recorder_channel_gains) {
        esp_codec_dev_set_in_channel_gain(manager_config.rec_dev, BIT(channel), gain);
    }

    // Open playback
    player_config_.event_cb = playback_event_callback;
    player_config_.event_cb_ctx = this;
    BROOKESIA_CHECK_ESP_ERR_RETURN(audio_playback_open(&player_config_), false, "Failed to open playback");

    play_state_ = AudioPlayState::Idle;
    auto function_schema = Helper::get_function_schema(Helper::FunctionId::SetEncoderReadDataSize);
    auto param_index = BROOKESIA_DESCRIBE_ENUM_TO_NUM(Helper::FunctionSetEncoderReadDataSizeParam::Size);
    if (function_schema && !function_schema->parameters.empty() &&
            function_schema->parameters[param_index].default_value.has_value()) {
        auto default_value = std::get_if<double>(&function_schema->parameters[param_index].default_value.value());
        if (default_value && *default_value > 0) {
            encoder_read_data_size_ = static_cast<size_t>(*default_value);
        } else {
            BROOKESIA_LOGW("Invalid default value for encoder read data size");
        }
    }

    return true;
}

void Audio::on_stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_ESP_ERR_EXECUTE(audio_playback_close(), {}, {
        BROOKESIA_LOGE("Failed to close playback");
    });

    stop_encoder();
    stop_decoder();

    BROOKESIA_CHECK_ESP_ERR_EXECUTE(audio_manager_deinit(), {}, {
        BROOKESIA_LOGE("Failed to deinitialize audio manager");
    });
    BROOKESIA_CHECK_ESP_ERR_EXECUTE(esp_codec_dev_close(peripheral_config_.manager_config.play_dev), {}, {
        BROOKESIA_LOGE("Failed to close playback device");
    });
    BROOKESIA_CHECK_ESP_ERR_EXECUTE(esp_codec_dev_close(peripheral_config_.manager_config.rec_dev), {}, {
        BROOKESIA_LOGE("Failed to close recorder device");
    });
}

std::expected<void, std::string> Audio::function_play_url(const std::string &url)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: url(%1%)", url);

    if (audio_playback_play(url.c_str()) != ESP_OK) {
        return std::unexpected("Failed to play URL: " + url);
    }

    return {};
}

std::expected<void, std::string> Audio::function_play_control(const std::string &action)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: action(%1%)", action);

    AudioPlayControlAction action_enum;
    if (!BROOKESIA_DESCRIBE_STR_TO_ENUM(action, action_enum)) {
        return std::unexpected("Invalid action: " + action);
    }

    switch (action_enum) {
    case AudioPlayControlAction::Pause:
        if (audio_playback_pause() != ESP_OK) {
            return std::unexpected("Failed to pause playback");
        }
        break;
    case AudioPlayControlAction::Resume:
        if (audio_playback_resume() != ESP_OK) {
            return std::unexpected("Failed to resume playback");
        }
        break;
    case AudioPlayControlAction::Stop:
        if (audio_playback_stop() != ESP_OK) {
            return std::unexpected("Failed to stop playback");
        }
        break;
    }

    return {};
}

std::expected<void, std::string> Audio::function_set_volume(double volume)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: volume(%1%)", volume);

    int volume_int = static_cast<int>(volume);
    volume_int = std::clamp(
                     volume_int, static_cast<int>(peripheral_config_.player_volume_min),
                     static_cast<int>(peripheral_config_.player_volume_max)
                 );
    if (data_player_volume_ != volume_int) {
        auto result = esp_codec_dev_set_out_vol(peripheral_config_.manager_config.play_dev, volume_int);
        if (result != ESP_CODEC_DEV_OK) {
            return std::unexpected("Failed to set codec dev out volume");
        }
        set_data<DataType::PlayerVolume>(volume_int);
        try_save_data(DataType::PlayerVolume);
    } else {
        BROOKESIA_LOGD("Volume is the same, skip");
    }

    return {};
}

std::expected<double, std::string> Audio::function_get_volume()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    return static_cast<double>(get_data<DataType::PlayerVolume>());
}

std::expected<void, std::string> Audio::function_start_encoder(const boost::json::object &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: config(%1%)", BROOKESIA_DESCRIBE_TO_STR(config));

    av_processor_encoder_config_t encoder_config = {};
    if (!parse_encoder_config(config, encoder_config)) {
        return std::unexpected("Failed to parse encoder config: " + BROOKESIA_DESCRIBE_TO_STR(config));
    }

    if (!start_encoder(encoder_config)) {
        return std::unexpected("Failed to start encoder");
    }

    return {};
}

std::expected<void, std::string> Audio::function_stop_encoder()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    stop_encoder();

    return {};
}

std::expected<void, std::string> Audio::function_start_decoder(const boost::json::object &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: config(%1%)", BROOKESIA_DESCRIBE_TO_STR(config));

    av_processor_decoder_config_t decoder_config = {};
    if (!parse_decoder_config(config, decoder_config)) {
        return std::unexpected("Failed to parse decoder config: " + BROOKESIA_DESCRIBE_TO_STR(config));
    }

    if (!start_decoder(decoder_config)) {
        return std::unexpected("Failed to start decoder");
    }

    return {};
}

std::expected<void, std::string> Audio::function_stop_decoder()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    stop_decoder();

    return {};
}

std::expected<void, std::string> Audio::function_feed_decoder_data(const RawBuffer &data)
{
    // BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    // BROOKESIA_LOGD("Params: data(%1%)", BROOKESIA_DESCRIBE_TO_STR(data));

    if (!is_decoder_started_) {
        return std::unexpected("Decoder is not running");
    }

    auto result = audio_feeder_feed_data(const_cast<uint8_t *>(data.data_ptr), data.data_size);
    if (result != ESP_OK) {
        return std::unexpected((boost::format("Failed to feed decoder data: %1%") % esp_err_to_name(result)).str());
    }

    return {};
}

std::expected<void, std::string> Audio::function_set_encoder_read_data_size(double size)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: size(%1%)", size);

    if (size <= 0) {
        return std::unexpected("Invalid size: " + std::to_string(size));
    }

    encoder_read_data_size_ = static_cast<size_t>(size);

    return {};
}

void Audio::try_load_data()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_data_loaded_) {
        BROOKESIA_LOGD("Data is already loaded, skip");
        return;
    }

    if (!NVSHelper::is_available()) {
        BROOKESIA_LOGD("NVS is not available, skip");
        return;
    }

    auto binding = service::ServiceManager::get_instance().bind(NVSHelper::get_name().data());
    BROOKESIA_CHECK_FALSE_EXIT(binding.is_valid(), "Failed to bind NVS service");

    auto nvs_namespace = get_attributes().name;

    {
        auto key = BROOKESIA_DESCRIBE_TO_STR(DataType::PlayerVolume);
        auto result = NVSHelper::get_key_value<int>(nvs_namespace, key);
        if (!result) {
            BROOKESIA_LOGD("Failed to load '%1%' from NVS: %2%", key, result.error());
        } else {
            set_data<DataType::PlayerVolume>(result.value());
            auto set_result = esp_codec_dev_set_out_vol(peripheral_config_.manager_config.play_dev, result.value());
            BROOKESIA_CHECK_FALSE_EXECUTE(set_result == ESP_CODEC_DEV_OK, {
                BROOKESIA_LOGE("Failed to set codec dev out volume");
            });
            BROOKESIA_LOGD("Loaded '%1%' from NVS", key);
        }
    }

    is_data_loaded_ = true;

    BROOKESIA_LOGI("Loaded all data from NVS");
}

void Audio::try_save_data(DataType type)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto key = BROOKESIA_DESCRIBE_TO_STR(type);
    BROOKESIA_LOGD("Params: type(%1%)", key);

    if (!NVSHelper::is_available()) {
        BROOKESIA_LOGD("NVS is not available, skip");
        return;
    }

    auto nvs_namespace = get_attributes().name;

    auto save_function = [this, &nvs_namespace, &key](const auto & data_value) {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        auto result = NVSHelper::save_key_value(nvs_namespace, key, data_value, NVS_SAVE_DATA_TIMEOUT_MS);
        if (!result) {
            BROOKESIA_LOGE("Failed to save '%1%' to NVS: %2%", key, result.error());
        } else {
            BROOKESIA_LOGI("Saved '%1%' to NVS", key);
        }
    };

    if (type == DataType::PlayerVolume) {
        save_function(get_data<DataType::PlayerVolume>());
    } else {
        BROOKESIA_LOGE("Invalid data type for saving to NVS");
    }
}

void Audio::try_erase_data()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!NVSHelper::is_available()) {
        BROOKESIA_LOGD("NVS is not available, skip");
        return;
    }

    auto result = NVSHelper::erase_keys(get_attributes().name, {}, NVS_ERASE_DATA_TIMEOUT_MS);
    if (!result) {
        BROOKESIA_LOGE("Failed to erase NVS data: %1%", result.error());
    } else {
        BROOKESIA_LOGI("Erased NVS data");
    }
}

bool Audio::parse_encoder_config(const boost::json::object &json_data, av_processor_encoder_config_t &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: json_data(%1%)", BROOKESIA_DESCRIBE_TO_STR(json_data));

    AudioEncoderConfig encoder_config;
    BROOKESIA_CHECK_FALSE_RETURN(
        BROOKESIA_DESCRIBE_FROM_JSON(json_data, encoder_config), false,
        "Failed to parse encoder config from json data: %1%", BROOKESIA_DESCRIBE_TO_STR(json_data)
    );

    auto &general_config = encoder_config.general;
    switch (encoder_config.type) {
    case AudioCodecFormat::PCM: {
        config.format = AV_PROCESSOR_FORMAT_ID_PCM;
        config.params.pcm = av_processor_pcm_config_t{
            .audio_info = {
                .sample_rate = general_config.sample_rate,
                .sample_bits = general_config.sample_bits,
                .channels = general_config.channels,
                .frame_duration = general_config.frame_duration,
            }
        };
        break;
    }
    case AudioCodecFormat::OPUS: {
        auto extra_config = std::get_if<Helper::EncoderExtraConfigOpus>(&encoder_config.extra);
        BROOKESIA_CHECK_NULL_RETURN(extra_config, false, "Opus encoder is missing extra config");

        config.format = AV_PROCESSOR_FORMAT_ID_OPUS;
        config.params.opus = {
            .audio_info = {
                .sample_rate = general_config.sample_rate,
                .sample_bits = general_config.sample_bits,
                .channels = general_config.channels,
                .frame_duration = general_config.frame_duration,
            },
            .enable_vbr = extra_config->enable_vbr,
            .bitrate = static_cast<int>(extra_config->bitrate),
        };
        break;
    }
    case AudioCodecFormat::G711A: {
        config.format = AV_PROCESSOR_FORMAT_ID_G711A;
        config.params.g711 = {
            .audio_info = {
                .sample_rate = general_config.sample_rate,
                .sample_bits = general_config.sample_bits,
                .channels = general_config.channels,
                .frame_duration = general_config.frame_duration,
            }
        };
        break;
    }
    default:
        BROOKESIA_CHECK_FALSE_RETURN(
            false, false, "Invalid encoder format type: %1%", BROOKESIA_DESCRIBE_TO_STR(encoder_config.type)
        );
    }

    return true;
}

bool Audio::parse_decoder_config(const boost::json::object &json_data, av_processor_decoder_config_t &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: json_data(%1%)", BROOKESIA_DESCRIBE_TO_STR(json_data));

    AudioDecoderConfig decoder_config;
    BROOKESIA_CHECK_FALSE_RETURN(
        BROOKESIA_DESCRIBE_FROM_JSON(json_data, decoder_config), false,
        "Failed to parse decoder config from json data: %1%", BROOKESIA_DESCRIBE_TO_STR(json_data)
    );

    auto &general_config = decoder_config.general;
    switch (decoder_config.type) {
    case AudioCodecFormat::PCM: {
        BROOKESIA_LOGD("Got PCM decoder config");
        config.format = AV_PROCESSOR_FORMAT_ID_PCM;
        config.params.pcm = {
            .audio_info = {
                .sample_rate = general_config.sample_rate,
                .sample_bits = general_config.sample_bits,
                .channels = general_config.channels,
                .frame_duration = general_config.frame_duration,
            }
        };
        break;
    }
    case AudioCodecFormat::OPUS: {
        BROOKESIA_LOGD("Got OPUS decoder config");
        config.format = AV_PROCESSOR_FORMAT_ID_OPUS;
        config.params.opus = {
            .audio_info = {
                .sample_rate = general_config.sample_rate,
                .sample_bits = general_config.sample_bits,
                .channels = general_config.channels,
                .frame_duration = general_config.frame_duration,
            },
        };
        break;
    }
    case AudioCodecFormat::G711A: {
        BROOKESIA_LOGD("Got G711A decoder config");
        config.format = AV_PROCESSOR_FORMAT_ID_G711A;
        config.params.g711 = {
            .audio_info = {
                .sample_rate = general_config.sample_rate,
                .sample_bits = general_config.sample_bits,
                .channels = general_config.channels,
                .frame_duration = general_config.frame_duration,
            }
        };
        break;
    }
    default:
        BROOKESIA_CHECK_FALSE_RETURN(
            false, false, "Invalid decoder format type: %1%", BROOKESIA_DESCRIBE_TO_STR(decoder_config.type)
        );
    }

    return true;
}

bool Audio::start_encoder(const av_processor_encoder_config_t &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_encoder_started_) {
        BROOKESIA_LOGD("Encoder is already running");
        return true;
    }

    recorder_config_.recorder_event_cb = recorder_event_callback;
    recorder_config_.recorder_ctx = this;
    recorder_config_.encoder_cfg = config;

    if (recorder_config_.afe_fetch_task_config.task_stack < ENCODER_AFE_FETCH_TASK_STACK_SIZE_MIN) {
        recorder_config_.afe_fetch_task_config.task_stack = ENCODER_AFE_FETCH_TASK_STACK_SIZE_MIN;
    }

#if (BROOKESIA_SERVICE_AUDIO_ENABLE_WORKER && BROOKESIA_SERVICE_AUDIO_WORKER_STACK_IN_EXT) || \
    (!BROOKESIA_SERVICE_MANAGER_WORKER_STACK_IN_EXT)
    BROOKESIA_CHECK_ESP_ERR_RETURN(audio_recorder_open(&recorder_config_), false, "Failed to open recorder");
#else
    {
        // Since initializing SR in `audio_recorder_open()` operates on flash,
        // a separate thread with its stack located in SRAM needs to be created to prevent a crash.
        BROOKESIA_THREAD_CONFIG_GUARD({
            .stack_in_ext = false,
        });
        auto recorder_open_future = std::async(std::launch::async, [this, recorder_config = recorder_config_]() mutable {
            BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
            BROOKESIA_CHECK_ESP_ERR_RETURN(audio_recorder_open(&recorder_config), false, "Failed to open recorder");
            return true;
        });
        BROOKESIA_CHECK_FALSE_RETURN(recorder_open_future.get(), false, "Failed to open recorder");
    }
#endif

    auto recorder_fetch_thread_func = [this, encoder_read_data_size = encoder_read_data_size_]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

        BROOKESIA_LOGI("recorder fetch thread started (encoder read data size: %1%)", encoder_read_data_size);

        std::unique_ptr<uint8_t[]> data(new (std::nothrow) uint8_t[encoder_read_data_size]);
        BROOKESIA_CHECK_NULL_EXIT(data, "Failed to allocate memory");

        int ret_size = 0;
        try {
            while (!boost::this_thread::interruption_requested()) {
                ret_size = audio_recorder_read_data(data.get(), encoder_read_data_size);
                // BROOKESIA_LOGD("Reading data from recorder (%1%)", ret_size);
                if (ret_size > 0) {
                    BROOKESIA_CHECK_FALSE_EXIT(
                    publish_event(BROOKESIA_DESCRIBE_TO_STR(Helper::EventId::EncoderDataReady), {
                        RawBuffer(static_cast<const uint8_t *>(data.get()), ret_size)
                    }), "Failed to publish recorder data ready event"
                    );
                } else {
                    boost::this_thread::sleep_for(boost::chrono::milliseconds(RECORDER_FETCH_INTERVAL_MS));
                }
            }
        } catch (const boost::thread_interrupted &e) {
            BROOKESIA_LOGI("recorder fetch thread interrupted");
        }

        BROOKESIA_LOGI("recorder fetch thread stopped");
    };
    {
        BROOKESIA_THREAD_CONFIG_GUARD(RECORDER_FETCH_THREAD_CONFIG);
        BROOKESIA_CHECK_EXCEPTION_RETURN(
            recorder_fetch_thread_ = boost::thread(recorder_fetch_thread_func), false,
            "Failed to create recorder fetch thread"
        );
    }

    is_encoder_started_ = true;

    BROOKESIA_LOGI("Encoder started");

    return true;
}

void Audio::stop_encoder()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_encoder_started_) {
        BROOKESIA_LOGD("Encoder is not running");
        return;
    }

    if (recorder_fetch_thread_.joinable()) {
        recorder_fetch_thread_.interrupt();
        if (!recorder_fetch_thread_.try_join_for(boost::chrono::milliseconds(0))) {
            recorder_fetch_thread_.join();
        }
    }

    BROOKESIA_CHECK_ESP_ERR_EXECUTE(audio_recorder_close(), {}, {
        BROOKESIA_LOGE("Failed to close recorder");
    });
    is_encoder_started_ = false;

    BROOKESIA_LOGI("Encoder stopped");
}

bool Audio::start_decoder(const av_processor_decoder_config_t &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_decoder_started_) {
        BROOKESIA_LOGD("Decoder is already running");
        return true;
    }

    is_decoder_started_ = true;

    lib_utils::FunctionGuard stop_guard([this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        stop_decoder();
    });

    feeder_config_.decoder_cfg = config;
    BROOKESIA_CHECK_ESP_ERR_RETURN(audio_feeder_open(&feeder_config_), false, "Failed to open feeder");
    BROOKESIA_CHECK_ESP_ERR_RETURN(audio_processor_mixer_open(), false, "Failed to open mixer");
    BROOKESIA_CHECK_ESP_ERR_RETURN(audio_feeder_run(), false, "Failed to run feeder");

    stop_guard.release();

    BROOKESIA_LOGI("Decoder started");

    return true;
}

void Audio::stop_decoder()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_decoder_started_) {
        BROOKESIA_LOGD("Decoder is not running");
        return;
    }

    BROOKESIA_CHECK_ESP_ERR_EXECUTE(audio_processor_mixer_close(), {}, {
        BROOKESIA_LOGE("Failed to close mixer");
    });
    BROOKESIA_CHECK_ESP_ERR_EXECUTE(audio_feeder_close(), {}, {
        BROOKESIA_LOGE("Failed to close feeder");
    });
    is_decoder_started_ = false;

    BROOKESIA_LOGI("Decoder stopped");
}

void Audio::on_playback_event(audio_player_state_t state)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: state(%1%)", state);

    AudioPlayState new_state;
    switch (state) {
    case AUDIO_PLAYER_STATE_IDLE:
        new_state = AudioPlayState::Idle;
        break;
    case AUDIO_PLAYER_STATE_PLAYING:
        new_state = AudioPlayState::Playing;
        break;
    case AUDIO_PLAYER_STATE_PAUSED:
        new_state = AudioPlayState::Paused;
        break;
    case AUDIO_PLAYER_STATE_FINISHED:
    case AUDIO_PLAYER_STATE_STOPPED:
        new_state = AudioPlayState::Idle;
        break;
    default:
        BROOKESIA_LOGE("Invalid playback state: %1%", BROOKESIA_DESCRIBE_TO_STR(state));
        return;
    }

    if (new_state != play_state_) {
        play_state_ = new_state;
        BROOKESIA_LOGI("Play state changed to: %1%", BROOKESIA_DESCRIBE_TO_STR(play_state_));
        auto result = publish_event(BROOKESIA_DESCRIBE_TO_STR(Helper::EventId::PlayStateChanged), {
            BROOKESIA_DESCRIBE_TO_STR(new_state)
        });
        BROOKESIA_CHECK_FALSE_EXECUTE(result, {}, {
            BROOKESIA_LOGE("Failed to publish play state changed event");
        });
    }
}

void Audio::on_recorder_event(void *event)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: event(%1%)", BROOKESIA_DESCRIBE_TO_STR(event));

    auto result = publish_event(BROOKESIA_DESCRIBE_TO_STR(Helper::EventId::EncoderEventHappened), {
        RawBuffer(static_cast<uint8_t *>(event), 0)
    });
    BROOKESIA_CHECK_FALSE_EXIT(result, "Failed to publish encoder event happened event");
}

void Audio::playback_event_callback(audio_player_state_t state, void *ctx)
{
    auto self = static_cast<Audio *>(ctx);
    BROOKESIA_CHECK_NULL_EXIT(self, "Invalid context");

    self->on_playback_event(state);
}

void Audio::recorder_event_callback(void *event, void *ctx)
{
    BROOKESIA_LOG_TRACE_GUARD();

    auto self = static_cast<Audio *>(ctx);
    BROOKESIA_CHECK_NULL_EXIT(self, "Invalid context");

    self->on_recorder_event(event);
}

BROOKESIA_PLUGIN_REGISTER_SINGLETON(
    ServiceBase, Audio, Audio::get_instance().get_attributes().name, Audio::get_instance()
);

} // namespace esp_brookesia::service
