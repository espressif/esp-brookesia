/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <string>

#include "audio_processor.h"
#include "brookesia/lib_utils/thread_config.hpp"
#include "brookesia/hal_adaptor/audio/device.hpp"
#include "brookesia/hal_interface/interfaces/audio/processor.hpp"

#if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32S3) || \
    defined(CONFIG_IDF_TARGET_ESP32P4) || defined(CONFIG_IDF_TARGET_ESP32S31)
#   define BROOKESIA_HAL_ADAPTOR_AUDIO_PROCESSOR_AFE_USE_CUSTOM 0
#else
#   define BROOKESIA_HAL_ADAPTOR_AUDIO_PROCESSOR_AFE_USE_CUSTOM 1
#endif

namespace esp_brookesia::hal {

using audio_playback_config_t = audio_play_config_t;
using audio_playback_handle_t = audio_play_handle_t;

class AudioProcessorTypeConverter {
public:
    static audio_task_config_t convert(const lib_utils::ThreadConfig &config);
    static audio_mixer_gain_config_t convert(const AudioProcessorMixerGainConfig &config);
    static audio_playback_config_t convert(
        const AudioProcessorPlaybackConfig &config, const void *event_cb, void *event_cb_ctx
    );
    static av_processor_format_id_t convert(audio::CodecFormat format);
    static av_processor_audio_info_t convert(const audio::CodecGeneralConfig &config);
    static av_processor_encoder_config_t convert(const audio::EncoderDynamicConfig &config);
    static av_processor_decoder_config_t convert(const audio::DecoderDynamicConfig &config);
    static av_processor_afe_config_t convert(
        const AudioProcessorAFE_Config &config, std::string &model_storage, std::string &mn_language_storage
    );
    static audio_recorder_config_t convert(
        const AudioProcessorEncoderConfig &static_config,
        const audio::EncoderDynamicConfig &dynamic_config,
        const AudioProcessorAFE_Config &afe_config,
        std::string &afe_model_storage,
        std::string &afe_mn_language_storage,
        const void *event_cb,
        void *event_cb_ctx
    );
    static audio_feeder_config_t convert(
        const AudioProcessorDecoderConfig &static_config,
        const audio::DecoderDynamicConfig &dynamic_config
    );

    static audio_feeder_handle_t to_feeder_handle(void *handle)
    {
        return reinterpret_cast<audio_feeder_handle_t>(handle);
    }

    static audio_playback_handle_t to_playback_handle(void *handle)
    {
        return reinterpret_cast<audio_playback_handle_t>(handle);
    }

    static audio_recorder_handle_t to_recorder_handle(void *handle)
    {
        return reinterpret_cast<audio_recorder_handle_t>(handle);
    }
};

} // namespace esp_brookesia::hal
