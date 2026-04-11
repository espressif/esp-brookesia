/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <string>
#include "brookesia/service_helper/audio.hpp"
#include "brookesia/lib_utils/thread_config.hpp"
#include "audio_processor.h"

namespace esp_brookesia::service {

using audio_playback_config_t = audio_play_config_t;
using audio_playback_handle_t = audio_play_handle_t;

/**
 * @brief  Type converter class for converting helper types to audio_processor types
 */
class TypeConverter {
public:
    using AudioHelper = helper::Audio;

    /**
     * @brief  Convert ThreadConfig to audio_task_config_t
     */
    static audio_task_config_t convert(const lib_utils::ThreadConfig &config);

    /**
     * @brief  Convert MixerGainConfig to audio_mixer_gain_config_t
     */
    static audio_mixer_gain_config_t convert(const AudioHelper::MixerGainConfig &config);

    /**
     * @brief  Convert PlaybackConfig to audio_playback_config_t
     */
    static audio_playback_config_t convert(
        const AudioHelper::PlaybackConfig &config, const void *event_cb, void *event_cb_ctx
    );

    /**
     * @brief  Convert CodecFormat to av_processor_format_id_t
     */
    static av_processor_format_id_t convert(AudioHelper::CodecFormat format);

    /**
     * @brief  Convert CodecGeneralConfig to av_processor_audio_info_t
     */
    static av_processor_audio_info_t convert(const AudioHelper::CodecGeneralConfig &config);

    /**
     * @brief  Convert EncoderDynamicConfig to av_processor_encoder_config_t
     */
    static av_processor_encoder_config_t convert(const AudioHelper::EncoderDynamicConfig &config);

    /**
     * @brief  Convert DecoderDynamicConfig to av_processor_decoder_config_t
     */
    static av_processor_decoder_config_t convert(const AudioHelper::DecoderDynamicConfig &config);

    /**
     * @brief  Convert AFE_Config to av_processor_afe_config_t with persistent string storage
     *
     * @param config            The AFE config to convert
     * @param model_storage     Reference to string for storing model partition label
     * @param mn_language_storage Reference to string for storing mn_language
     */
    static av_processor_afe_config_t convert(
        const AudioHelper::AFE_Config &config, std::string &model_storage, std::string &mn_language_storage
    );

    /**
     * @brief  Convert EncoderStaticConfig and EncoderDynamicConfig to audio_recorder_config_t
     */
    static audio_recorder_config_t convert(
        const AudioHelper::EncoderStaticConfig &static_config,
        const AudioHelper::EncoderDynamicConfig &dynamic_config,
        const AudioHelper::AFE_Config &afe_config,
        std::string &afe_model_storage,
        std::string &afe_mn_language_storage,
        const void *event_cb,
        void *event_cb_ctx
    );

    /**
     * @brief  Convert DecoderStaticConfig and DecoderDynamicConfig to audio_feeder_config_t
     */
    static audio_feeder_config_t convert(
        const AudioHelper::DecoderStaticConfig &static_config,
        const AudioHelper::DecoderDynamicConfig &dynamic_config
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

} // namespace esp_brookesia::service
