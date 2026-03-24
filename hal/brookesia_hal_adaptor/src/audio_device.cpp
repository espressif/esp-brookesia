/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "brookesia/hal_adaptor/macro_configs.h"
#undef BROOKESIA_LOG_TAG
#define BROOKESIA_LOG_TAG BROOKESIA_HAL_ADAPTOR_LOG_TAG
#include "audio_device.hpp"
#include "esp_board_device.h"
#include "esp_board_manager_includes.h"
#include "esp_codec_dev.h"

using namespace esp_brookesia::hal;

namespace {
bool is_board_name(const char *board_name, const char *target_name)
{
    if (board_name == nullptr || target_name == nullptr) {
        return false;
    }
    return strcmp(board_name, target_name) == 0;
}
}

bool AudioDevice::check_initialized_intern() const
{
    return AudioPlayerIface::get_handle() != nullptr && AudioRecorderIface::get_handle() != nullptr;
}

bool AudioDevice::check_initialized() const
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard<boost::mutex> lock(mutex_);
    return check_initialized_intern();
}

bool AudioDevice::init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard<boost::mutex> lock(mutex_);

    if (check_initialized_intern()) {
        BROOKESIA_LOGW("Audio device is already initialized");
        return true;
    }

    BROOKESIA_LOGI("Initializing audio dac and audio adc devices...");
#if CONFIG_ESP_BOARD_DEV_AUDIO_CODEC_SUPPORT
    BROOKESIA_CHECK_ESP_ERR_RETURN(
        esp_board_manager_init_device_by_name(
            ESP_BOARD_DEVICE_NAME_AUDIO_DAC), false, "Failed to initialize audio_dac");
    BROOKESIA_CHECK_ESP_ERR_RETURN(
        esp_board_manager_init_device_by_name(
            ESP_BOARD_DEVICE_NAME_AUDIO_ADC), false, "Failed to initialize audio_adc");

    esp_err_t ret = ESP_OK;
    dev_audio_codec_handles_t *play_dev_handles = nullptr;
    dev_audio_codec_handles_t *rec_dev_handles = nullptr;

    ret = esp_board_device_get_handle(ESP_BOARD_DEVICE_NAME_AUDIO_DAC, (void **)&play_dev_handles);
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to get audio_dac handle");
    BROOKESIA_CHECK_NULL_RETURN(play_dev_handles, false, "Failed to get audio_dac handle");
    AudioPlayerIface::handle = play_dev_handles->codec_dev;

    ret = esp_board_device_get_handle(ESP_BOARD_DEVICE_NAME_AUDIO_ADC, (void **)&rec_dev_handles);
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to get audio_adc handle");
    BROOKESIA_CHECK_NULL_RETURN(rec_dev_handles, false, "Failed to get audio_adc handle");
    AudioRecorderIface::handle = rec_dev_handles->codec_dev;
#else
    BROOKESIA_LOGE("Audio codec board devices are not supported in this build");
    return false;
#endif

    AudioRecorderIface::config.general_gain = 32.0;
    AudioRecorderIface::config.channel_gains[2] = 20.0;

    if (is_board_name(g_esp_board_info.name, "esp32_s3_korvo2_v3") ||
            is_board_name(g_esp_board_info.name, "esp_vocat_board_v1_0") ||
            is_board_name(g_esp_board_info.name, "esp_vocat_board_v1_2") ||
            is_board_name(g_esp_board_info.name, "esp_box_3")) {
        AudioRecorderIface::config.mic_layout = "RMNN";
        AudioRecorderIface::config.sample_rate = 16000;
        AudioRecorderIface::config.bits = 32;
        AudioRecorderIface::config.channels = 2;
    } else if (is_board_name(g_esp_board_info.name, "esp32_s3_korvo2l_v1") ||
               is_board_name(g_esp_board_info.name, "esp32_p4_function_ev")) {
        AudioRecorderIface::config.mic_layout = "MR";
        AudioRecorderIface::config.sample_rate = 16000;
        AudioRecorderIface::config.bits = 16;
        AudioRecorderIface::config.channels = 2;
    } else if (is_board_name(g_esp_board_info.name, "esp_sensair_shuttle")) {
        AudioRecorderIface::config.general_gain = 48.0;
        AudioRecorderIface::config.mic_layout = "M";
        AudioRecorderIface::config.sample_rate = 16000;
        AudioRecorderIface::config.bits = 16;
        AudioRecorderIface::config.channels = 1;
    } else {
        BROOKESIA_LOGE("Unsupported board: %s", g_esp_board_info.name);
        return false;
    }

    return true;
}

bool AudioDevice::deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard<boost::mutex> lock(mutex_);

    if (!check_initialized_intern()) {
        BROOKESIA_LOGW("Audio device is not initialized");
        return true;
    }

    esp_err_t ret = ESP_OK;
    if (AudioPlayerIface::handle != nullptr) {
        ret = esp_board_device_deinit(ESP_BOARD_DEVICE_NAME_AUDIO_DAC);
        BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to release audio_dac handle");
    }
    if (AudioRecorderIface::handle != nullptr) {
        ret = esp_board_device_deinit(ESP_BOARD_DEVICE_NAME_AUDIO_ADC);
        BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to release audio_adc handle");
    }
    AudioPlayerIface::handle = nullptr;
    AudioRecorderIface::handle = nullptr;
    return true;
}

bool AudioDevice::set_volume(uint8_t volume)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: volume(%1%)", volume);

    boost::lock_guard<boost::mutex> lock(mutex_);

    if (!check_initialized_intern()) {
        BROOKESIA_LOGW("Audio device is not initialized");
        return false;
    }

    if (volume > AudioPlayerIface::config.volume_max) {
        volume = AudioPlayerIface::config.volume_max;
    } else if (volume < AudioPlayerIface::config.volume_min) {
        volume = AudioPlayerIface::config.volume_min;
    }

    esp_err_t ret = esp_codec_dev_set_out_vol(static_cast<esp_codec_dev_handle_t>(AudioPlayerIface::handle), volume);
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to set volume");
    return true;
}

uint8_t AudioDevice::get_volume() const
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard<boost::mutex> lock(mutex_);

    if (!check_initialized_intern()) {
        BROOKESIA_LOGW("Audio device is not initialized");
        return 0;
    }

    int volume = 0;
    esp_err_t ret = esp_codec_dev_get_out_vol(static_cast<esp_codec_dev_handle_t>(AudioPlayerIface::handle), &volume);
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, 0, "Failed to get volume");
    return static_cast<uint8_t>(volume);
}

bool AudioDevice::open_player()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard<boost::mutex> lock(mutex_);

    if (!check_initialized_intern()) {
        BROOKESIA_LOGW("Audio device is not initialized");
        return false;
    }

    esp_codec_dev_sample_info_t fs {};
    fs.bits_per_sample = static_cast<uint8_t>(AudioRecorderIface::config.bits);
    fs.channel = static_cast<uint8_t>(AudioRecorderIface::config.channels);
    fs.sample_rate = static_cast<uint32_t>(AudioRecorderIface::config.sample_rate);
    BROOKESIA_LOGI(
        "Board sample info: sample_rate(%1%) channel(%2%) bits_per_sample(%3%)", fs.sample_rate, fs.channel,
        fs.bits_per_sample
    );

    esp_err_t ret = esp_codec_dev_open(static_cast<esp_codec_dev_handle_t>(AudioPlayerIface::handle), &fs);
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to open audio dac");

    return true;
}

bool AudioDevice::close_player()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard<boost::mutex> lock(mutex_);

    if (!check_initialized_intern()) {
        BROOKESIA_LOGW("Audio device is not initialized");
        return false;
    }

    esp_err_t ret = esp_codec_dev_close(static_cast<esp_codec_dev_handle_t>(AudioPlayerIface::handle));
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to close audio dac");

    return true;
}

bool AudioDevice::open_recorder()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard<boost::mutex> lock(mutex_);

    if (!check_initialized_intern()) {
        BROOKESIA_LOGW("Audio device is not initialized");
        return false;
    }

    esp_codec_dev_sample_info_t fs {};
    fs.bits_per_sample = static_cast<uint8_t>(AudioRecorderIface::config.bits);
    fs.channel = static_cast<uint8_t>(AudioRecorderIface::config.channels);
    fs.sample_rate = static_cast<uint32_t>(AudioRecorderIface::config.sample_rate);
    BROOKESIA_LOGI(
        "Board sample info: sample_rate(%1%) channel(%2%) bits_per_sample(%3%)", fs.sample_rate, fs.channel,
        fs.bits_per_sample
    );

    esp_err_t ret = esp_codec_dev_open(static_cast<esp_codec_dev_handle_t>(AudioRecorderIface::handle), &fs);
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to open audio adc");

    return true;
}

bool AudioDevice::close_recorder()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard<boost::mutex> lock(mutex_);

    if (!check_initialized_intern()) {
        BROOKESIA_LOGW("Audio device is not initialized");
        return false;
    }

    esp_err_t ret = esp_codec_dev_close(static_cast<esp_codec_dev_handle_t>(AudioRecorderIface::handle));
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to close audio adc");

    return true;
}

bool AudioDevice::set_recorder_gain()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard<boost::mutex> lock(mutex_);

    if (!check_initialized_intern()) {
        BROOKESIA_LOGW("Audio device is not initialized");
        return false;
    }

    BROOKESIA_CHECK_FALSE_RETURN(
        esp_codec_dev_set_in_gain(static_cast<esp_codec_dev_handle_t>(AudioRecorderIface::handle),
                                  AudioRecorderIface::config.general_gain) == ESP_CODEC_DEV_OK,
        false, "Failed to set recorder gain"
    );
    for (const auto &[channel, gain] : AudioRecorderIface::config.channel_gains) {
        esp_codec_dev_set_in_channel_gain(
            static_cast<esp_codec_dev_handle_t>(AudioRecorderIface::handle), BIT(channel), gain);
    }
    return true;
}

#if CONFIG_BROOKESIA_HAL_ADAPTOR_AUDIO_DEVICE_ENABLE_AUTO_REGISTER
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    Device, AudioDevice, AudioDevice::get_instance().get_name(), AudioDevice::get_instance(),
    BROOKESIA_HAL_ADAPTOR_AUDIO_DEVICE_PLUGIN_SYMBOL);
#endif
