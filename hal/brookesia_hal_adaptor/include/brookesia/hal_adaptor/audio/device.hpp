/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file device.hpp
 * @brief Singleton HAL device that aggregates codec-based audio playback and recording for board-managed audio hardware.
 */
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/lib_utils/thread_config.hpp"
#include "brookesia/hal_interface/device.hpp"
#include "brookesia/hal_interface/interfaces/audio/codec_player.hpp"
#include "brookesia/hal_interface/interfaces/audio/codec_recorder.hpp"
#include "brookesia/hal_interface/interfaces/audio/processor.hpp"

namespace esp_brookesia::hal {

class AudioProcessorCore;

/**
 * @brief Mixer gain transition configuration for the ESP audio processor backend.
 */
struct AudioProcessorMixerGainConfig {
    float initial_gain = 1.0f;
    float target_gain = 1.0f;
    int transition_time = 1500;
};

/**
 * @brief Playback backend configuration for the ESP audio processor.
 */
struct AudioProcessorPlaybackConfig {
    audio::CodecPlayerIface::Config player{
        .bits = 16,
        .channels = 2,
        .sample_rate = 16000,
    };
    lib_utils::ThreadConfig player_task{
        .core_id = 0,
        .priority = 5,
        .stack_size = 4 * 1024,
        .stack_in_ext = false,
    };
    AudioProcessorMixerGainConfig mixer_gain{
        .initial_gain = 0.6f,
        .target_gain = 1.0f,
        .transition_time = 1500,
    };
};

/**
 * @brief Encoder backend configuration for the ESP audio processor.
 */
struct AudioProcessorEncoderConfig {
    lib_utils::ThreadConfig recorder_task{
        .core_id = 0,
        .priority = 10,
        .stack_size = 4 * 1024,
        .stack_in_ext = true,
    };
    lib_utils::ThreadConfig fetcher_task{
        .name = "EncoderFetcher",
        .core_id = 1,
        .priority = 12,
        .stack_size = 6 * 1024,
        .stack_in_ext = true,
    };
};

/**
 * @brief Decoder backend configuration for the ESP audio processor.
 */
struct AudioProcessorDecoderConfig {
    lib_utils::ThreadConfig feeder_task{
        .core_id = 1,
        .priority = 5,
        .stack_size = 4 * 1024,
        .stack_in_ext = true,
    };
    AudioProcessorMixerGainConfig mixer_gain{
        .initial_gain = 0.9f,
        .target_gain = 1.0f,
        .transition_time = 1500,
    };
};

/**
 * @brief Voice activity detection configuration for the ESP AFE backend.
 */
struct AudioProcessorAFE_VAD_Config {
    uint8_t mode = 4;
    uint32_t min_speech_ms = 64;
    uint32_t min_noise_ms = 1000;
};

/**
 * @brief WakeNet configuration for the ESP AFE backend.
 */
struct AudioProcessorAFE_WakeNetConfig {
    std::string model_partition_label = "model";
    std::string mn_language = "cn";
    uint32_t start_timeout_ms = 30000;
    uint32_t end_timeout_ms = 10000;
};

/**
 * @brief AFE backend configuration for the ESP audio processor.
 */
struct AudioProcessorAFE_Config {
    lib_utils::ThreadConfig feeder_task{
        .core_id = 1,
        .priority = 5,
        .stack_size = 40 * 1024,
        .stack_in_ext = true,
    };
    lib_utils::ThreadConfig fetcher_task{
        .core_id = 0,
        .priority = 5,
        .stack_size = 6 * 1024,
        .stack_in_ext = true,
    };
    std::optional<AudioProcessorAFE_VAD_Config> vad = std::nullopt;
    std::optional<AudioProcessorAFE_WakeNetConfig> wakenet = std::nullopt;
};

/**
 * @brief Complete ESP audio processor backend configuration.
 */
struct AudioProcessorConfig {
    AudioProcessorPlaybackConfig playback;
    AudioProcessorEncoderConfig encoder;
    AudioProcessorDecoderConfig decoder;
    AudioProcessorAFE_Config afe;
};

/**
 * @brief Board-backed audio device: registers codec player and recorder HAL interfaces after board bring-up.
 *
 * Obtained via get_instance(). Not copyable or movable.
 */
class AudioDevice : public Device {
public:
    /** @brief Logical device name passed to the base @ref Device constructor. */
    static constexpr const char *DEVICE_NAME = "Audio";
    /** @brief Registry key for the codec player HAL implementation (`"Audio:CodecPlayer"`). */
    static constexpr const char *CODEC_PLAYER_IMPL_NAME = "Audio:CodecPlayer";
    /** @brief Registry key for the codec recorder HAL implementation (`"Audio:CodecRecorder"`). */
    static constexpr const char *CODEC_RECORDER_IMPL_NAME = "Audio:CodecRecorder";
    /** @brief Registry key for the audio playback HAL implementation. */
    static constexpr const char *PLAYBACK_IMPL_NAME = "Audio:Playback";
    /** @brief Registry key for the audio encoder HAL implementation. */
    static constexpr const char *ENCODER_IMPL_NAME = "Audio:Encoder:0";
    /** @brief Registry key for the audio decoder HAL implementation. */
    static constexpr const char *DECODER_IMPL_NAME = "Audio:Decoder:0";

    AudioDevice(const AudioDevice &) = delete;
    AudioDevice &operator=(const AudioDevice &) = delete;
    AudioDevice(AudioDevice &&) = delete;
    AudioDevice &operator=(AudioDevice &&) = delete;

    /**
     * @brief Overrides default static recording capability information used when constructing the codec recorder implementation.
     *
     * @param[in] info Codec recorder capability descriptor (format, channels, gains, etc.).
     *
     * @return `true` if the value was stored; `false` on invalid input or if the recorder is already initialized.
     */
    bool set_codec_recorder_info(audio::CodecRecorderIface::Info info);

    /**
     * @brief Set the ESP audio processor backend configuration.
     *
     * This must be called before the audio processor playback, encoder, or decoder HAL interfaces are initialized.
     *
     * @param[in] config Complete processor configuration.
     * @return `true` if the value was stored; `false` after processor initialization.
     */
    bool set_processor_config(AudioProcessorConfig config);

    /**
     * @brief Returns the process-wide singleton audio device.
     *
     * @return Reference to the unique @ref AudioDevice instance.
     */
    static AudioDevice &get_instance()
    {
        static AudioDevice instance;
        return instance;
    }

private:
    AudioDevice()
        : Device(std::string(DEVICE_NAME))
    {
    }
    ~AudioDevice() = default;

    bool probe() override;
    std::vector<InterfaceSpec> get_interface_specs() const override;
    bool on_init() override;
    void on_deinit() override;

    bool init_codec_player();
    void deinit_codec_player();

    bool init_codec_recorder();
    void deinit_codec_recorder();

    bool init_processor();
    void deinit_processor();

    std::optional<audio::CodecRecorderIface::Info> codec_recorder_info_ = std::nullopt;
    AudioProcessorConfig processor_config_{};
    std::shared_ptr<AudioProcessorCore> processor_core_;
};

BROOKESIA_DESCRIBE_STRUCT(AudioProcessorMixerGainConfig, (), (initial_gain, target_gain, transition_time));
BROOKESIA_DESCRIBE_STRUCT(AudioProcessorPlaybackConfig, (), (player, player_task, mixer_gain));
BROOKESIA_DESCRIBE_STRUCT(AudioProcessorEncoderConfig, (), (recorder_task, fetcher_task));
BROOKESIA_DESCRIBE_STRUCT(AudioProcessorDecoderConfig, (), (feeder_task, mixer_gain));
BROOKESIA_DESCRIBE_STRUCT(AudioProcessorAFE_VAD_Config, (), (mode, min_speech_ms, min_noise_ms));
BROOKESIA_DESCRIBE_STRUCT(AudioProcessorAFE_WakeNetConfig, (), (
                              model_partition_label, mn_language, start_timeout_ms, end_timeout_ms
                          ));
BROOKESIA_DESCRIBE_STRUCT(AudioProcessorAFE_Config, (), (feeder_task, fetcher_task, vad, wakenet));
BROOKESIA_DESCRIBE_STRUCT(AudioProcessorConfig, (), (playback, encoder, decoder, afe));

} // namespace esp_brookesia::hal
