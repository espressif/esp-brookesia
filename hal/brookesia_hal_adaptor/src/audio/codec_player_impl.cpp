/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/hal_adaptor/macro_configs.h"
#if !BROOKESIA_HAL_ADAPTOR_AUDIO_CODEC_PLAYER_IMPL_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include <algorithm>
#include <utility>
#include "esp_board_manager_includes.h"
#include "private/utils.hpp"
#include "codec_player_impl.hpp"

#if BROOKESIA_HAL_ADAPTOR_AUDIO_ENABLE_CODEC_PLAYER_IMPL
#include "driver/gpio.h"
#include "esp_codec_dev.h"

namespace esp_brookesia::hal {

constexpr uint8_t VOLUME_DEFAULT = 75;
constexpr uint8_t VOLUME_MIN = 0;
constexpr uint8_t VOLUME_MAX = 100;
constexpr bool PA_CONTROL_STATE_DEFAULT = false;

namespace {
esp_codec_dev_handle_t get_codec_handle(void *handles)
{
    return reinterpret_cast<dev_audio_codec_handles_t *>(handles)->codec_dev;
}

periph_gpio_handle_t *get_gpio_handle(void *handle)
{
    return reinterpret_cast<periph_gpio_handle_t *>(handle);
}
} // namespace

AudioCodecPlayerImpl::AudioCodecPlayerImpl()
    : AudioCodecPlayerIface()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard<boost::mutex> lock(mutex_);

    volume_ = VOLUME_DEFAULT;

    BROOKESIA_CHECK_FALSE_EXIT(setup_codec(), "Failed to setup codec");
    BROOKESIA_CHECK_FALSE_EXIT(setup_pa_control(), "Failed to setup PA control");
}

AudioCodecPlayerImpl::~AudioCodecPlayerImpl()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    close();

    if (is_codec_valid_internal()) {
        auto ret = esp_board_manager_deinit_device_by_name(ESP_BOARD_DEVICE_NAME_AUDIO_DAC);
        BROOKESIA_CHECK_ESP_ERR_EXECUTE(ret, {}, { BROOKESIA_LOGE("Failed to deinit codec DAC"); });
    }
    if (is_pa_control_valid_internal()) {
        auto ret = esp_board_periph_deinit(ESP_BOARD_PERIPH_NAME_GPIO_PA_CONTROL);
        BROOKESIA_CHECK_ESP_ERR_EXECUTE(ret, {}, { BROOKESIA_LOGE("Failed to deinit PA control GPIO"); });
    }
}

bool AudioCodecPlayerImpl::open(const AudioCodecPlayerIface::Config &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: config(%1%)", config);

    boost::lock_guard<boost::mutex> lock(mutex_);

    if (is_codec_opened_internal()) {
        BROOKESIA_LOGD("Player is already opened, skip");
        return true;
    }

    BROOKESIA_CHECK_FALSE_RETURN(is_codec_valid_internal(), false, "Codec is not initialized");

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    esp_codec_dev_sample_info_t sample_info = {
        .bits_per_sample = config.bits,
        .channel = config.channels,
        .sample_rate = config.sample_rate,
    };
#pragma GCC diagnostic pop
    auto ret = esp_codec_dev_open(get_codec_handle(codec_handles_), &sample_info);
    BROOKESIA_CHECK_FALSE_RETURN(ret == ESP_CODEC_DEV_OK, false, "Failed to open codec: %1%", ret);

    is_codec_opened_ = true;

    BROOKESIA_CHECK_FALSE_RETURN(set_volume_internal(volume_, true), false, "Failed to set volume after open");

    return true;
}

void AudioCodecPlayerImpl::close()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard<boost::mutex> lock(mutex_);

    if (!is_codec_opened_internal()) {
        BROOKESIA_LOGD("Codec is not opened, skip");
        return;
    }

    auto ret = esp_codec_dev_close(get_codec_handle(codec_handles_));
    BROOKESIA_CHECK_FALSE_EXECUTE(ret == ESP_CODEC_DEV_OK, {}, { BROOKESIA_LOGE("Failed to close codec: %1%", ret); });

    is_codec_opened_ = false;
}

bool AudioCodecPlayerImpl::set_volume(uint8_t volume)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: volume(%1%)", volume);

    boost::lock_guard<boost::mutex> lock(mutex_);

    return set_volume_internal(volume, false);
}

bool AudioCodecPlayerImpl::set_pa_on_off(bool on)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: on(%1%)", on);

    boost::lock_guard<boost::mutex> lock(mutex_);

    return set_pa_on_off_internal(on, false);
}

bool AudioCodecPlayerImpl::write_data(const uint8_t *data, size_t size)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard<boost::mutex> lock(mutex_);

    BROOKESIA_CHECK_FALSE_RETURN(is_codec_opened_internal(), false, "Codec is not opened");
    BROOKESIA_CHECK_NULL_RETURN(data, false, "Invalid audio data");

    auto ret = esp_codec_dev_write(get_codec_handle(codec_handles_), const_cast<uint8_t *>(data), size);
    BROOKESIA_CHECK_FALSE_RETURN(ret == ESP_CODEC_DEV_OK, false, "Failed to write audio data: %1%", ret);

    return true;
}

bool AudioCodecPlayerImpl::setup_codec()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto ret = esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_AUDIO_DAC);
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to init codec DAC");

    ret = esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_AUDIO_DAC, &codec_handles_);
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to get handles");
    BROOKESIA_CHECK_NULL_RETURN(codec_handles_, false, "Failed to get handles");

    return true;
}

bool AudioCodecPlayerImpl::setup_pa_control()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto ret = esp_board_manager_get_periph_handle(ESP_BOARD_PERIPH_NAME_GPIO_PA_CONTROL, &pa_control_handle_);
    if (ret != ESP_OK) {
        BROOKESIA_LOGW("PA control GPIO not found, skip");
        return true;
    }

    BROOKESIA_CHECK_NULL_RETURN(pa_control_handle_, false, "Failed to get PA control GPIO handle");

    dev_audio_codec_config_t *config = nullptr;
    ret = esp_board_manager_get_device_config(ESP_BOARD_DEVICE_NAME_AUDIO_DAC, reinterpret_cast<void **>(&config));
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to get audio DAC config");
    BROOKESIA_CHECK_NULL_RETURN(config, false, "Failed to get audio DAC config");

    pa_control_active_level_ = config->pa_cfg.active_level;

    // Force turn off PA
    BROOKESIA_CHECK_FALSE_RETURN(set_pa_on_off_internal(PA_CONTROL_STATE_DEFAULT, true), false, "Failed to turn off PA");

    return true;
}

bool AudioCodecPlayerImpl::set_volume_internal(uint8_t volume, bool force)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: volume(%1%), force(%2%)", volume, force);

    BROOKESIA_CHECK_FALSE_RETURN(is_codec_valid_internal(), false, "Codec is not initialized");

    // Clamp target volume to [VOLUME_MIN, VOLUME_MAX]
    auto volume_clamped = std::clamp<uint8_t>(volume, VOLUME_MIN, VOLUME_MAX);
    if (volume_clamped != volume) {
        BROOKESIA_LOGW(
            "Target volume(%1%) is out of range [%2%, %3%], clamp to %4%", volume, VOLUME_MIN, VOLUME_MAX, volume_clamped
        );
    }

    if (!force && (volume_clamped == volume_)) {
        BROOKESIA_LOGD("Volume is already %1%, skip", volume_clamped);
        return true;
    }

    if (is_codec_opened_internal()) {
        auto ret = esp_codec_dev_set_out_vol(get_codec_handle(codec_handles_), volume_clamped);
        BROOKESIA_CHECK_FALSE_RETURN(ret == ESP_CODEC_DEV_OK, false, "Failed to set volume value: %1%", ret);
    } else {
        BROOKESIA_LOGW("Codec is not opened, deferred target volume");
    }

    volume_ = volume_clamped;

    BROOKESIA_LOGI("Set volume: %1%", volume_);

    return true;
}

bool AudioCodecPlayerImpl::set_pa_on_off_internal(bool on, bool force)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: on(%1%), force(%2%)", on, force);

    BROOKESIA_CHECK_FALSE_RETURN(is_pa_control_valid_internal(), false, "PA control is not initialized");

    if (!force && (on == is_pa_on_)) {
        BROOKESIA_LOGD("PA state is already %1%, skip", on ? "on" : "off");
        return true;
    }

    auto ret = gpio_set_level(
                   get_gpio_handle(pa_control_handle_)->gpio_num,
                   on ? pa_control_active_level_ : !pa_control_active_level_
               );
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to set PA control GPIO level");

    is_pa_on_ = on;

    BROOKESIA_LOGI("Set PA state: %1%", is_pa_on_ ? "on" : "off");

    return true;
}

} // namespace esp_brookesia::hal
#endif // BROOKESIA_HAL_ADAPTOR_AUDIO_ENABLE_CODEC_PLAYER_IMPL
