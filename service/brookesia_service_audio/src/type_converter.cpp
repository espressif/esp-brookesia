/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <cstring>
#include "private/utils.hpp"
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

audio_playback_config_t TypeConverter::convert(
    const AudioHelper::PlaybackConfig &config, const void *event_cb, void *event_cb_ctx
)
{
    return {
        .type = AUDIO_PLAY_TYPE_PLAYBACK,
        .event_cb = reinterpret_cast<playback_event_callback_t>(event_cb),
        .event_cb_ctx = event_cb_ctx,
        .task_config = convert(config.player_task),
        .mixer_gain = convert(config.mixer_gain),
        .block_until_done = false,
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

av_processor_afe_config_t TypeConverter::convert(
    const AudioHelper::AFE_Config &config, std::string &model_storage, std::string &mn_language_storage
)
{
#if !AV_PROCESSOR_AFE_USE_CUSTOM
    av_processor_afe_config_t result = DEFAULT_AV_PROCESSOR_AFE_CONFIG();
    auto &afe_config = result.mode.afe;
#else
    av_processor_afe_config_t result = DEFAULT_AV_PROCESSOR_CUSTOM_CONFIG();
    auto &afe_config = result.mode.custom;
#endif

    if (config.vad.has_value()) {
#if !AV_PROCESSOR_AFE_USE_CUSTOM
        afe_config.algo_mask |= AV_PROCESSOR_AFE_FLAG_AEC_ENABLE |
                                AV_PROCESSOR_AFE_FLAG_NS_ENABLE |
                                AV_PROCESSOR_AFE_FLAG_VAD_ENABLE |
                                AV_PROCESSOR_AFE_FLAG_AGC_ENABLE;
#else
        BROOKESIA_LOGW("VAD is not supported on this target");
#endif
    }

    if (config.wakenet.has_value()) {
        afe_config.algo_mask |= AV_PROCESSOR_CUSTOM_FLAG_WAKENET_ENABLE;
#if !AV_PROCESSOR_AFE_USE_CUSTOM
        const auto &wakenet = config.wakenet.value();
        afe_config.wakeup_time_ms = wakenet.start_timeout_ms;
        afe_config.wakeup_end_time_ms = wakenet.end_timeout_ms;
        // Store strings in persistent storage
        model_storage = wakenet.model_partition_label;
        mn_language_storage = wakenet.mn_language;
        afe_config.mn_language = mn_language_storage.c_str();
        result.model = model_storage.c_str();
#endif
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
    void *event_cb_ctx
)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    audio_recorder_config_t result = DEFAULT_AUDIO_RECORDER_CONFIG();
#pragma GCC diagnostic pop
    result.afe_feed_task_config = convert(afe_config.feeder_task);
    result.afe_config = convert(afe_config, afe_model_storage, afe_mn_language_storage);
    result.afe_fetch_task_config = convert(afe_config.fetcher_task);
    result.recorder_task_config = convert(static_config.recorder_task);
    result.encoder_cfg = convert(dynamic_config);
    result.recorder_event_cb = reinterpret_cast<recorder_event_callback_t>(event_cb);
    result.recorder_ctx = event_cb_ctx;

    return result;
}

audio_feeder_config_t TypeConverter::convert(
    const AudioHelper::DecoderStaticConfig &static_config,
    const AudioHelper::DecoderDynamicConfig &dynamic_config
)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    audio_feeder_config_t result = DEFAULT_AUDIO_FEEDER_CONFIG();
#pragma GCC diagnostic pop

    result.feeder_task_config = convert(static_config.feeder_task);
    result.decoder_cfg = convert(dynamic_config);
    result.feeder_mixer_gain = convert(static_config.mixer_gain);

    return result;
}

} // namespace esp_brookesia::service
