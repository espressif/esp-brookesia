/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <algorithm>
#include <utility>
#include "esp_board_manager_includes.h"
#include "brookesia/hal_adaptor/macro_configs.h"
#if !BROOKESIA_HAL_ADAPTOR_AUDIO_CODEC_PLAYER_IMPL_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "codec_player_impl.hpp"

#if BROOKESIA_HAL_ADAPTOR_AUDIO_ENABLE_CODEC_PLAYER_IMPL
#include "esp_codec_dev.h"

namespace esp_brookesia::hal {

namespace {
esp_codec_dev_handle_t get_codec_handle(void *handles)
{
    return reinterpret_cast<dev_audio_codec_handles_t *>(handles)->codec_dev;
}

AudioCodecPlayerIface::Info generate_info()
{
    AudioCodecPlayerIface::Info info = {
        .volume_default = BROOKESIA_HAL_ADAPTOR_AUDIO_CODEC_PLAYER_VOLUME_DEFAULT,
        .volume_min = BROOKESIA_HAL_ADAPTOR_AUDIO_CODEC_PLAYER_VOLUME_MIN,
        .volume_max = BROOKESIA_HAL_ADAPTOR_AUDIO_CODEC_PLAYER_VOLUME_MAX,
    };

    return info;
}
} // namespace

AudioCodecPlayerImpl::AudioCodecPlayerImpl(std::optional<AudioCodecPlayerIface::Info> info)
    : AudioCodecPlayerIface(info.has_value() ? info.value() : generate_info())
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: info(%1%)", get_info());

    auto ret = esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_AUDIO_DAC);
    BROOKESIA_CHECK_ESP_ERR_EXIT(ret, "Failed to init codec DAC");

    ret = esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_AUDIO_DAC, &handles_);
    BROOKESIA_CHECK_ESP_ERR_EXIT(ret, "Failed to get handles");
    BROOKESIA_CHECK_NULL_EXIT(handles_, "Failed to get handles");
}

AudioCodecPlayerImpl::~AudioCodecPlayerImpl()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    close();

    esp_board_manager_deinit_device_by_name(ESP_BOARD_DEVICE_NAME_AUDIO_DAC);
}

bool AudioCodecPlayerImpl::open(const AudioCodecPlayerIface::Config &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: config(%1%)", config);

    boost::lock_guard<boost::mutex> lock(mutex_);

    if (is_opened_internal()) {
        BROOKESIA_LOGD("Player is already opened, skip");
        return true;
    }

    BROOKESIA_CHECK_FALSE_RETURN(is_valid_internal(), false, "Player is not initialized");

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    esp_codec_dev_sample_info_t sample_info = {
        .bits_per_sample = config.bits,
        .channel = config.channels,
        .sample_rate = config.sample_rate,
    };
#pragma GCC diagnostic pop
    auto ret = esp_codec_dev_open(get_codec_handle(handles_), &sample_info);
    BROOKESIA_CHECK_FALSE_RETURN(ret == ESP_CODEC_DEV_OK, false, "Failed to open player: %1%", ret);

    is_opened_ = true;

    BROOKESIA_CHECK_FALSE_EXECUTE(mute_internal(), {}, { BROOKESIA_LOGE("Failed to mute player"); });

    return true;
}

void AudioCodecPlayerImpl::close()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard<boost::mutex> lock(mutex_);

    if (!is_opened_internal()) {
        BROOKESIA_LOGD("Player is not opened, skip");
        return;
    }

    auto ret = esp_codec_dev_close(get_codec_handle(handles_));
    BROOKESIA_CHECK_FALSE_EXECUTE(ret == ESP_CODEC_DEV_OK, {}, { BROOKESIA_LOGE("Failed to close player: %1%", ret); });

    is_opened_ = false;
    volume_ = 0;
    volume_before_mute_ = 0;
}

bool AudioCodecPlayerImpl::set_volume(uint8_t volume)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard<boost::mutex> lock(mutex_);

    return set_volume_internal(volume);
}

bool AudioCodecPlayerImpl::get_volume(uint8_t &volume)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard<boost::mutex> lock(mutex_);

    return get_volume_internal(volume);
}

bool AudioCodecPlayerImpl::mute()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard<boost::mutex> lock(mutex_);

    BROOKESIA_CHECK_FALSE_RETURN(mute_internal(), false, "Failed to mute player");

    return true;
}

bool AudioCodecPlayerImpl::unmute()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard<boost::mutex> lock(mutex_);

    BROOKESIA_CHECK_FALSE_RETURN(is_opened_internal(), false, "Player is not opened");

    uint8_t current_volume = 0;
    BROOKESIA_CHECK_FALSE_RETURN(get_volume_internal(current_volume), false, "Failed to get volume");

    if (current_volume != 0) {
        BROOKESIA_LOGD("Player is not muted, skip");
        return true;
    }

    // Restore pre-mute volume; fall back to default if it was zero
    auto restore_vol = (volume_before_mute_ != 0) ? volume_before_mute_ : get_info().volume_default;

    BROOKESIA_CHECK_FALSE_RETURN(set_volume_internal(restore_vol), false, "Failed to unmute player");

    BROOKESIA_LOGI("Unmuted: %1%", restore_vol);

    volume_before_mute_ = 0;

    return true;
}

bool AudioCodecPlayerImpl::write_data(const uint8_t *data, size_t size)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard<boost::mutex> lock(mutex_);

    BROOKESIA_CHECK_FALSE_RETURN(is_opened_internal(), false, "Player is not opened");
    BROOKESIA_CHECK_NULL_RETURN(data, false, "Invalid audio data");

    auto ret = esp_codec_dev_write(get_codec_handle(handles_), const_cast<uint8_t *>(data), size);
    BROOKESIA_CHECK_FALSE_RETURN(ret == ESP_CODEC_DEV_OK, false, "Failed to write audio data: %1%", ret);

    return true;
}

bool AudioCodecPlayerImpl::set_volume_internal(uint8_t volume, bool need_map)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: volume(%1%)", volume);

    BROOKESIA_CHECK_FALSE_RETURN(is_opened_internal(), false, "Player is not opened");

    // Clamp external input to [0, 100]
    auto volume_clamped = std::clamp<uint8_t>(volume, 0, 100);
    if (volume_clamped != volume) {
        BROOKESIA_LOGW("Target volume(%1%) is out of range[0, 100], clamp to %2%", volume, volume_clamped);
    }

    if (volume_clamped == volume_) {
        BROOKESIA_LOGD("Volume not changed, skip");
        return true;
    }

    // Map external [0, 100] → hardware [volume_min, volume_max]
    uint8_t hardware_vol = volume_clamped;
    if (need_map) {
        auto &info = get_info();
        hardware_vol = static_cast<uint8_t>(
                           info.volume_min + static_cast<int>(volume_clamped) * (info.volume_max - info.volume_min) / 100
                       );
    }

    auto ret = esp_codec_dev_set_out_vol(get_codec_handle(handles_), hardware_vol);
    BROOKESIA_CHECK_FALSE_RETURN(ret == ESP_CODEC_DEV_OK, false, "Failed to set volume: %1%", ret);

    BROOKESIA_LOGI("Set volume: %1% (hardware: %2%)", volume_clamped, hardware_vol);

    volume_ = volume_clamped;

    return true;
}

bool AudioCodecPlayerImpl::get_volume_internal(uint8_t &volume) const
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_FALSE_RETURN(is_opened_internal(), false, "Player is not opened");

    volume = volume_;

    return true;
}

bool AudioCodecPlayerImpl::mute_internal()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_FALSE_RETURN(is_opened_internal(), false, "Player is not opened");

    if (volume_ != std::numeric_limits<uint8_t>::max()) {
        volume_before_mute_ = volume_;
    }

    BROOKESIA_CHECK_FALSE_RETURN(set_volume_internal(0, false), false, "Failed to mute player");

    BROOKESIA_LOGI("Muted");

    return true;
}

} // namespace esp_brookesia::hal
#endif // BROOKESIA_HAL_ADAPTOR_AUDIO_ENABLE_CODEC_PLAYER_IMPL
