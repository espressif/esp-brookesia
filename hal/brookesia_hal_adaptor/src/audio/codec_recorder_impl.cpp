/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <cstring>
#include <string>
#include <utility>
#include "esp_board_manager_includes.h"
#include "brookesia/hal_adaptor/macro_configs.h"
#if !BROOKESIA_HAL_ADAPTOR_AUDIO_CODEC_RECORDER_IMPL_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "codec_recorder_impl.hpp"

#if BROOKESIA_HAL_ADAPTOR_AUDIO_ENABLE_CODEC_RECORDER_IMPL
#include "esp_codec_dev.h"

namespace esp_brookesia::hal {

namespace {
esp_codec_dev_handle_t get_codec_handle(void *handles)
{
    return reinterpret_cast<dev_audio_codec_handles_t *>(handles)->codec_dev;
}

AudioCodecRecorderIface::Info generate_info()
{
    AudioCodecRecorderIface::Info info = {
        .bits = BROOKESIA_HAL_ADAPTOR_AUDIO_CODEC_RECORDER_BITS,
        .channels = BROOKESIA_HAL_ADAPTOR_AUDIO_CODEC_RECORDER_CHANNELS,
        .sample_rate = BROOKESIA_HAL_ADAPTOR_AUDIO_CODEC_RECORDER_SAMPLE_RATE,
        .mic_layout = BROOKESIA_HAL_ADAPTOR_AUDIO_CODEC_RECORDER_MIC_LAYOUT,
        .general_gain = std::stof(BROOKESIA_HAL_ADAPTOR_AUDIO_CODEC_RECORDER_GENERAL_GAIN),
        .channel_gains = {},
    };
    auto result = BROOKESIA_DESCRIBE_JSON_DESERIALIZE(
                      BROOKESIA_HAL_ADAPTOR_AUDIO_CODEC_RECORDER_CHANNEL_GAINS, info.channel_gains
                  );
    if (!result) {
        BROOKESIA_LOGE("Invalid channel gains config: %1%", BROOKESIA_HAL_ADAPTOR_AUDIO_CODEC_RECORDER_CHANNEL_GAINS);
    }

    return info;
}
} // namespace

AudioCodecRecorderImpl::AudioCodecRecorderImpl(std::optional<AudioCodecRecorderIface::Info> info)
    : AudioCodecRecorderIface(info.has_value() ? info.value() : generate_info())
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: info(%1%)", get_info());

    auto ret = esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_AUDIO_ADC);
    BROOKESIA_CHECK_ESP_ERR_EXIT(ret, "Failed to init codec ADC");

    ret = esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_AUDIO_ADC, &handles_);
    BROOKESIA_CHECK_ESP_ERR_EXIT(ret, "Failed to get handles");
    BROOKESIA_CHECK_NULL_EXIT(handles_, "Failed to get handles");
}

AudioCodecRecorderImpl::~AudioCodecRecorderImpl()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    close();

    esp_board_manager_deinit_device_by_name(ESP_BOARD_DEVICE_NAME_AUDIO_ADC);
}

bool AudioCodecRecorderImpl::open()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard<boost::mutex> lock(mutex_);

    if (is_opened_internal()) {
        BROOKESIA_LOGD("Recorder is already opened, skip");
        return true;
    }

    BROOKESIA_CHECK_FALSE_RETURN(is_valid_internal(), false, "Recorder is not initialized");

    auto &info = get_info();
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    esp_codec_dev_sample_info_t sample_info = {
        .bits_per_sample = info.bits,
        .channel = info.channels,
        .sample_rate = info.sample_rate,
    };
#pragma GCC diagnostic pop
    auto ret = esp_codec_dev_open(get_codec_handle(handles_), &sample_info);
    BROOKESIA_CHECK_FALSE_RETURN(ret == ESP_CODEC_DEV_OK, false, "Failed to open recorder: %1%", ret);

    is_opened_ = true;

    BROOKESIA_CHECK_FALSE_EXECUTE(
        set_general_gain_internal(info.general_gain), {}, { BROOKESIA_LOGE("Failed to set general gain: %1%", ret); }
    );
    BROOKESIA_CHECK_FALSE_EXECUTE(
        set_channel_gains_internal(info.channel_gains), {}, { BROOKESIA_LOGE("Failed to set channel gains: %1%", ret); }
    );

    return true;
}

void AudioCodecRecorderImpl::close()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard<boost::mutex> lock(mutex_);

    if (!is_opened_internal()) {
        BROOKESIA_LOGD("Recorder is not opened, skip");
        return;
    }

    auto ret = esp_codec_dev_close(get_codec_handle(handles_));
    BROOKESIA_CHECK_FALSE_EXECUTE(ret == ESP_CODEC_DEV_OK, {}, { BROOKESIA_LOGE("Failed to close recorder: %1%", ret); });

    is_opened_ = false;
}

bool AudioCodecRecorderImpl::read_data(uint8_t *data, size_t size)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard<boost::mutex> lock(mutex_);

    BROOKESIA_CHECK_FALSE_RETURN(is_opened_internal(), false, "Recorder is not opened");
    BROOKESIA_CHECK_NULL_RETURN(data, false, "Invalid audio data");

    auto ret = esp_codec_dev_read(get_codec_handle(handles_), data, size);
    BROOKESIA_CHECK_FALSE_RETURN(ret == ESP_CODEC_DEV_OK, false, "Failed to read audio data: %1%", ret);

    return true;
}

bool AudioCodecRecorderImpl::set_general_gain(float gain)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard<boost::mutex> lock(mutex_);

    return set_general_gain_internal(gain);
}

bool AudioCodecRecorderImpl::set_channel_gains(const std::map<uint8_t, float> &gains)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard<boost::mutex> lock(mutex_);

    return set_channel_gains_internal(gains);
}

bool AudioCodecRecorderImpl::set_general_gain_internal(float gain)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_FALSE_RETURN(is_opened_internal(), false, "Recorder is not opened");

    auto ret = esp_codec_dev_set_in_gain(get_codec_handle(handles_), gain);
    BROOKESIA_CHECK_FALSE_RETURN(ret == ESP_CODEC_DEV_OK, false, "Failed to set general gain: %1%", ret);

    return true;
}

bool AudioCodecRecorderImpl::set_channel_gains_internal(const std::map<uint8_t, float> &gains)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_FALSE_RETURN(is_opened_internal(), false, "Recorder is not opened");

    for (const auto &[channel, gain] : gains) {
        auto ret = esp_codec_dev_set_in_channel_gain(get_codec_handle(handles_), 1UL << channel, gain);
        BROOKESIA_CHECK_FALSE_EXECUTE(ret == ESP_CODEC_DEV_OK, {}, {
            BROOKESIA_LOGE("Failed to set channel(%1%) gain(%2%): %3%", channel, gain, ret);
        });
    }

    return true;
}

} // namespace esp_brookesia::hal
#endif // BROOKESIA_HAL_ADAPTOR_AUDIO_ENABLE_CODEC_RECORDER_IMPL
