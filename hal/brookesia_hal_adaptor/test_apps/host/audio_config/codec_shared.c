/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <assert.h>
#include <stddef.h>
#include <string.h>
#include "esp_codec_dev.h"
#include "codec_shared.h"

/* Only the IDF driver/OS and analogue codec controls are faked. The SDK's
 * pairing, format validation/compatibility, set_fmt and codec open functions
 * below are extracted unchanged by suite.py from the resolved dependency. */
typedef int i2s_clock_src_t;
typedef void *i2s_chan_handle_t;
typedef void *esp_codec_dev_mutex_handle_t;
#define ESP_IDF_VERSION_VAL(major, minor, patch) (((major) << 16) | ((minor) << 8) | (patch))
#define ESP_IDF_VERSION ESP_IDF_VERSION_VAL(6, 1, 0)
#define VOL_TRANSITION_TIME 50
#define ESP_LOGE(...) ((void)0)
#define ESP_LOGI(...) ((void)0)
#include "codec_types.inc"

static i2s_data_t shared_data;
static i2s_data_keep_t shared_keeper;
static codec_dev_t player;
static codec_dev_t recorder;
static esp_codec_dev_sample_info_t driver_tx;
static esp_codec_dev_sample_info_t driver_rx;
static int format_rejections;
static int last_format_result;

static int set_fs(i2s_data_t *data, bool playback, bool skip)
{
    (void)skip;
    if (playback) {
        driver_tx = data->out_fs;
    } else {
        driver_rx = data->in_fs;
    }
    return ESP_CODEC_DEV_OK;
}

static int set_drv_fs(i2s_chan_handle_t channel, bool playback, int slot_bits, i2s_clock_src_t clock,
                      esp_codec_dev_sample_info_t *format)
{
    (void)channel;
    (void)slot_bits;
    (void)clock;
    if (playback) {
        driver_tx = *format;
    } else {
        driver_rx = *format;
    }
    return ESP_CODEC_DEV_OK;
}

static int _i2s_drv_enable(i2s_data_t *data, bool playback, bool enable)
{
    if (playback) {
        data->out_enable = enable;
    } else {
        data->in_enable = enable;
    }
    return ESP_CODEC_DEV_OK;
}

static void _i2s_lock(i2s_data_t *data, const char *function)
{
    (void)data;
    (void)function;
}

static void _i2s_unlock(i2s_data_t *data)
{
    (void)data;
}

static void _update_codec_setting(codec_dev_t *device)
{
    (void)device;
}

const audio_codec_vol_if_t *audio_codec_new_sw_vol(void)
{
    assert(false); /* Codec has hardware volume; this path must stay unused. */
    return NULL;
}

#include "codec_functions.inc"

static int record_set_fmt(const audio_codec_data_if_t *interface, esp_codec_dev_type_t type,
                          esp_codec_dev_sample_info_t *format)
{
    last_format_result = _i2s_data_set_fmt(interface, type, format);
    if (last_format_result != ESP_CODEC_DEV_OK) {
        ++format_rejections;
    }
    return last_format_result;
}

static bool data_is_open(const audio_codec_data_if_t *interface)
{
    return ((const i2s_data_t *)interface)->is_open;
}

static int data_enable(const audio_codec_data_if_t *interface, esp_codec_dev_type_t type, bool enable)
{
    i2s_data_t *data = (i2s_data_t *)interface;
    return _i2s_drv_enable(data, (type & ESP_CODEC_DEV_TYPE_OUT) != 0, enable);
}

static int data_io(const audio_codec_data_if_t *interface, uint8_t *data, int size)
{
    (void)interface;
    (void)data;
    (void)size;
    return ESP_CODEC_DEV_OK;
}

static int hardware_volume(const audio_codec_if_t *interface, float volume)
{
    (void)interface;
    (void)volume;
    return ESP_CODEC_DEV_OK;
}

void shared_codec_reset(void)
{
    memset(&shared_data, 0, sizeof(shared_data));
    memset(&player, 0, sizeof(player));
    memset(&recorder, 0, sizeof(recorder));
    shared_data.is_open = true;
    shared_data.in_handle = &driver_rx;
    shared_data.out_handle = &driver_tx;
    shared_data.base.is_open = data_is_open;
    shared_data.base.set_fmt = record_set_fmt;
    shared_data.base.enable = data_enable;
    shared_data.base.read = data_io;
    shared_data.base.write = data_io;
    shared_keeper.i2s_data = &shared_data;
    shared_keeper.next = NULL;
    i2s_data_list = &shared_keeper;
    static const audio_codec_if_t codec = {.set_vol = hardware_volume};
    player.codec_if = recorder.codec_if = &codec;
    player.data_if = recorder.data_if = &shared_data.base;
    player.dev_caps = ESP_CODEC_DEV_TYPE_OUT;
    recorder.dev_caps = ESP_CODEC_DEV_TYPE_IN;
    driver_rx = driver_tx = (esp_codec_dev_sample_info_t) {
        .bits_per_sample = 16, .channel = 2, .sample_rate = 16000,
    };
    format_rejections = 0;
    last_format_result = ESP_CODEC_DEV_OK;
}

int shared_codec_open(bool playback, uint8_t bits, uint8_t channels, uint32_t rate)
{
    esp_codec_dev_sample_info_t format = {
        .bits_per_sample = bits, .channel = channels, .sample_rate = rate,
    };
    return esp_codec_dev_open(playback ? &player : &recorder, &format);
}

void shared_codec_close(bool playback)
{
    /* Closing is the fake device boundary; open/format behaviour above is SDK code. */
    if (playback) {
        shared_data.out_enable = player.output_opened = false;
    } else {
        shared_data.in_enable = recorder.input_opened = false;
    }
}

int shared_codec_format_rejections(void)
{
    return format_rejections;
}

int shared_codec_last_format_result(void)
{
    return last_format_result;
}

esp_codec_dev_sample_info_t shared_codec_driver_format(bool playback)
{
    return playback ? driver_tx : driver_rx;
}
