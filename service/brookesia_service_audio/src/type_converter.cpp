/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <cstring>
#include "private/type_converter.hpp"

namespace esp_brookesia::service {

audio_task_config_t TypeConverter::convert(const lib_utils::ThreadConfig &config)
{
    return {
        .task_stack = static_cast<int>(config.stack_size),
        .task_prio = static_cast<int>(config.priority),
        .task_core = static_cast<int>(config.core_id),
        .task_stack_in_ext = config.stack_in_ext,
    };
}

audio_mixer_gain_config_t TypeConverter::convert(const AudioHelper::MixerGainConfig &config)
{
    return {
        .initial_gain = config.initial_gain,
        .target_gain = config.target_gain,
        .transition_time = config.transition_time,
    };
}

audio_manager_config_t TypeConverter::convert(const AudioHelper::PeripheralConfig &config)
{
    audio_manager_config_t result = {
        .play_dev = config.play_dev,
        .rec_dev = config.rec_dev,
        .mic_layout = {0},
        .board_sample_rate = static_cast<int>(config.board_sample_rate),
        .board_bits = static_cast<int>(config.board_bits),
        .board_channels = static_cast<int>(config.board_channels),
    };

    // Copy mic_layout string safely
    if (!config.mic_layout.empty()) {
        std::strncpy(result.mic_layout, config.mic_layout.c_str(), sizeof(result.mic_layout) - 1);
        result.mic_layout[sizeof(result.mic_layout) - 1] = '\0';
    }

    return result;
}

audio_playback_config_t TypeConverter::convert(
    const AudioHelper::PlaybackConfig &config, const void *event_cb, void *event_cb_ctx
)
{
    return {
        .event_cb = reinterpret_cast<playback_event_callback_t>(event_cb),
        .event_cb_ctx = event_cb_ctx,
        .playback_task_config = convert(config.player_task),
        .playback_mixer_gain = convert(config.mixer_gain),
    };
}

av_processor_format_id_t TypeConverter::convert(AudioHelper::CodecFormat format)
{
    switch (format) {
    case AudioHelper::CodecFormat::PCM:
        return AV_PROCESSOR_FORMAT_ID_PCM;
    case AudioHelper::CodecFormat::OPUS:
        return AV_PROCESSOR_FORMAT_ID_OPUS;
    case AudioHelper::CodecFormat::G711A:
        return AV_PROCESSOR_FORMAT_ID_G711A;
    default:
        return AV_PROCESSOR_FORMAT_ID_PCM;
    }
}

av_processor_audio_info_t TypeConverter::convert(const AudioHelper::CodecGeneralConfig &config)
{
    return {
        .sample_rate = config.sample_rate,
        .sample_bits = config.sample_bits,
        .channels = config.channels,
        .frame_duration = config.frame_duration,
    };
}

av_processor_encoder_config_t TypeConverter::convert(const AudioHelper::EncoderDynamicConfig &config)
{
    av_processor_encoder_config_t result = {};
    result.format = convert(config.type);

    auto audio_info = convert(config.general);
    switch (config.type) {
    case AudioHelper::CodecFormat::PCM:
        result.params.pcm.audio_info = audio_info;
        break;
    case AudioHelper::CodecFormat::OPUS: {
        result.params.opus.audio_info = audio_info;
        // Try to get extra config for OPUS
        if (auto *opus_extra = std::get_if<AudioHelper::EncoderExtraConfigOpus>(&config.extra)) {
            result.params.opus.enable_vbr = opus_extra->enable_vbr;
            result.params.opus.bitrate = static_cast<int>(opus_extra->bitrate);
        }
        break;
    }
    case AudioHelper::CodecFormat::G711A:
        result.params.g711.audio_info = audio_info;
        break;
    default:
        result.params.pcm.audio_info = audio_info;
        break;
    }

    return result;
}

av_processor_decoder_config_t TypeConverter::convert(const AudioHelper::DecoderDynamicConfig &config)
{
    av_processor_decoder_config_t result = {};
    result.format = convert(config.type);

    auto audio_info = convert(config.general);
    switch (config.type) {
    case AudioHelper::CodecFormat::PCM:
        result.params.pcm.audio_info = audio_info;
        break;
    case AudioHelper::CodecFormat::OPUS:
        result.params.opus.audio_info = audio_info;
        break;
    case AudioHelper::CodecFormat::G711A:
        result.params.g711.audio_info = audio_info;
        break;
    default:
        result.params.pcm.audio_info = audio_info;
        break;
    }

    return result;
}

av_processor_afe_config_t TypeConverter::convert(const AudioHelper::AFE_Config &config)
{
    av_processor_afe_config_t result = DEFAULT_AV_PROCESSOR_AFE_CONFIG();

    if (config.vad.has_value()) {
        const auto &vad = config.vad.value();
        result.vad_enable = true;
        result.vad_mode = vad.mode;
        result.vad_min_speech_ms = vad.min_speech_ms;
        result.vad_min_noise_ms = vad.min_noise_ms;
    } else {
        result.vad_enable = false;
    }

    if (config.wakenet.has_value()) {
        const auto &wakenet = config.wakenet.value();
        result.ai_mode_wakeup = true;
        // Note: These point to temporary storage, use the overload with storage parameters for persistent use
        result.wakeup_time_ms = wakenet.start_timeout_ms;
        result.wakeup_end_time_ms = wakenet.end_timeout_ms;
    } else {
        result.ai_mode_wakeup = false;
    }

    return result;
}

av_processor_afe_config_t TypeConverter::convert(
    const AudioHelper::AFE_Config &config,
    std::string &model_storage,
    std::string &mn_language_storage
)
{
    av_processor_afe_config_t result = convert(config);

    if (config.wakenet.has_value()) {
        const auto &wakenet = config.wakenet.value();
        // Store strings in persistent storage
        model_storage = wakenet.model_partition_label;
        mn_language_storage = wakenet.mn_language;
        result.model = model_storage.c_str();
        result.mn_language = mn_language_storage.c_str();
    }

    return result;
}

audio_recorder_config_t TypeConverter::convert(
    const AudioHelper::EncoderStaticConfig &static_config,
    const AudioHelper::EncoderDynamicConfig &dynamic_config,
    const AudioHelper::AFE_Config &afe_config,
    std::string &afe_model_storage,
    std::string &afe_mn_language_storage,
    const void *event_cb,
    void *event_cb_ctx,
    const void *raw_data_cb,
    void *raw_data_cb_ctx
)
{
    audio_recorder_config_t result = DEFAULT_AUDIO_RECORDER_CONFIG();
    result.afe_feed_task_config = convert(afe_config.feeder_task);
    result.afe_config = convert(afe_config, afe_model_storage, afe_mn_language_storage);
    result.afe_fetch_task_config = convert(afe_config.fetcher_task);
    result.recorder_task_config = convert(static_config.recorder_task);
    result.encoder_cfg = convert(dynamic_config);
    result.recorder_event_cb = reinterpret_cast<recorder_event_callback_t>(event_cb);
    result.recorder_ctx = event_cb_ctx;
    result.input_cb = reinterpret_cast<recorder_input_callback_t>(raw_data_cb);
    result.input_ctx = raw_data_cb_ctx;

    return result;
}

audio_feeder_config_t TypeConverter::convert(
    const AudioHelper::DecoderStaticConfig &static_config,
    const AudioHelper::DecoderDynamicConfig &dynamic_config
)
{
    audio_feeder_config_t result = DEFAULT_AUDIO_FEEDER_CONFIG();

    result.feeder_task_config = convert(static_config.feeder_task);
    result.decoder_cfg = convert(dynamic_config);
    result.feeder_mixer_gain = convert(static_config.mixer_gain);

    return result;
}

} // namespace esp_brookesia::service
