/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_gmf_oal_mem.h"
#include "driver/i2c_master.h"
#include "driver/i2c.h"
#include "esp_gmf_setup_peripheral.h"
#include "sd_pwr_ctrl_by_on_chip_ldo.h"
#ifdef USE_ESP_GMF_ESP_CODEC_DEV_IO
#include "esp_gmf_io_codec_dev.h"
#include "esp_gmf_io_i2s_pdm.h"
#include "driver/i2s_std.h"
#include "driver/i2s_tdm.h"
#include "driver/i2s_pdm.h"
#include "esp_codec_dev_defaults.h"
#endif  /* USE_ESP_GMF_ESP_CODEC_DEV_IO */

#define WIFI_CONNECTED_BIT     BIT0
#define WIFI_FAIL_BIT          BIT1
#define WIFI_RECONNECT_RETRIES 30

#ifdef USE_ESP_GMF_ESP_CODEC_DEV_IO
i2s_chan_handle_t            rx_handle   = NULL;
const audio_codec_data_if_t *in_data_if  = NULL;
const audio_codec_ctrl_if_t *in_ctrl_if  = NULL;
const audio_codec_if_t      *in_codec_if = NULL;

i2s_chan_handle_t            tx_handle    = NULL;
const audio_codec_data_if_t *out_data_if  = NULL;
const audio_codec_ctrl_if_t *out_ctrl_if  = NULL;
const audio_codec_if_t      *out_codec_if = NULL;

const audio_codec_gpio_if_t *gpio_if = NULL;
#endif  /* USE_ESP_GMF_ESP_CODEC_DEV_IO */

typedef enum {
    I2S_CREATE_MODE_TX_ONLY   = 0,
    I2S_CREATE_MODE_RX_ONLY   = 1,
    I2S_CREATE_MODE_TX_AND_RX = 2,
} i2s_create_mode_t;

static const char        *TAG = "SETUP_PERIPH";
static i2c_master_bus_handle_t   i2c_handle  = NULL;
static esp_gmf_setup_periph_hardware_info hardware_info = {};

#ifdef USE_ESP_GMF_ESP_CODEC_DEV_IO
static esp_err_t setup_periph_i2s_tx_init(esp_gmf_setup_periph_aud_info *aud_info)
{
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(aud_info->sample_rate),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(aud_info->bits_per_sample, aud_info->channel),
        .gpio_cfg = {
            .mclk = aud_info->io_mclk,
            .bclk = aud_info->io_bclk,
            .ws = aud_info->io_ws,
            .dout = aud_info->io_do,
            .din = aud_info->io_di,
        },
    };
    return i2s_channel_init_std_mode(tx_handle, &std_cfg);
}

static esp_err_t setup_periph_i2s_rx_init(esp_gmf_setup_periph_aud_info *aud_info)
{
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(aud_info->sample_rate),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(aud_info->bits_per_sample, aud_info->channel),
        .gpio_cfg = {
            .mclk = aud_info->io_mclk,
            .bclk = aud_info->io_bclk,
            .ws = aud_info->io_ws,
            .dout = aud_info->io_do,
            .din = aud_info->io_di,
        },
    };
    return i2s_channel_init_std_mode(rx_handle, &std_cfg);
}

static esp_err_t setup_periph_create_i2s(i2s_create_mode_t mode, esp_gmf_setup_periph_aud_info *aud_info)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(aud_info->port_num, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;
    esp_err_t ret = ESP_OK;
    if (mode == I2S_CREATE_MODE_TX_ONLY) {
        ret = i2s_new_channel(&chan_cfg, &tx_handle, NULL);
        ESP_GMF_RET_ON_NOT_OK(TAG, ret, { return ret;}, "Failed to new I2S tx handle");
        ret = setup_periph_i2s_tx_init(aud_info);
        ESP_GMF_RET_ON_NOT_OK(TAG, ret, { return ret;}, "Failed to initialize I2S tx");
    } else if (mode == I2S_CREATE_MODE_RX_ONLY) {
        ret = i2s_new_channel(&chan_cfg, NULL, &rx_handle);
        ESP_GMF_RET_ON_NOT_OK(TAG, ret, { return ret;}, "Failed to new I2S rx handle");
        ret = setup_periph_i2s_rx_init(aud_info);
        ESP_GMF_RET_ON_NOT_OK(TAG, ret, { return ret;}, "Failed to initialize I2S rx");
    } else {
        ret = i2s_new_channel(&chan_cfg, &tx_handle, &rx_handle);
        ESP_GMF_RET_ON_NOT_OK(TAG, ret, { return ret;}, "Failed to new I2S tx and rx handle");
        ret = setup_periph_i2s_tx_init(aud_info);
        ESP_GMF_RET_ON_NOT_OK(TAG, ret, { return ret;}, "Failed to initialize I2S tx");
        ret = setup_periph_i2s_rx_init(aud_info);
        ESP_GMF_RET_ON_NOT_OK(TAG, ret, { return ret;}, "Failed to initialize I2S rx");
    }
    return ret;
}

static const audio_codec_data_if_t *setup_periph_new_i2s_data(void *tx_hd, void *rx_hd)
{
    audio_codec_i2s_cfg_t i2s_cfg = {
        .rx_handle = rx_hd,
        .tx_handle = tx_hd,
    };
    return audio_codec_new_i2s_data(&i2s_cfg);
}

static void setup_periph_new_play_codec()
{
    audio_codec_i2c_cfg_t i2c_ctrl_cfg = {.addr = ES8311_CODEC_DEFAULT_ADDR, .port = 0, .bus_handle = i2c_handle};
    out_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_ctrl_cfg);
    gpio_if = audio_codec_new_gpio();
    // New output codec interface
    es8311_codec_cfg_t es8311_cfg = {
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC,
        .ctrl_if = out_ctrl_if,
        .gpio_if = gpio_if,
        .pa_pin = hardware_info.codec.io_pa,
        .use_mclk = false,
    };
    out_codec_if = es8311_codec_new(&es8311_cfg);
}

static void setup_periph_new_record_codec()
{
    if (hardware_info.codec.type == ESP_GMF_CODEC_TYPE_ES8311_IN_OUT) {
        audio_codec_i2c_cfg_t i2c_ctrl_cfg = {.addr = ES8311_CODEC_DEFAULT_ADDR, .port = 0, .bus_handle = i2c_handle};
        in_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_ctrl_cfg);
        gpio_if = audio_codec_new_gpio();
        // New output codec interface
        es8311_codec_cfg_t es8311_cfg = {
            .codec_mode = ESP_CODEC_DEV_WORK_MODE_BOTH,
            .ctrl_if = in_ctrl_if,
            .gpio_if = gpio_if,
            .pa_pin = hardware_info.codec.io_pa,
            .use_mclk = false,
        };
        in_codec_if = es8311_codec_new(&es8311_cfg);
    } else {
        audio_codec_i2c_cfg_t i2c_ctrl_cfg = {.addr = ES7210_CODEC_DEFAULT_ADDR, .port = 0, .bus_handle = i2c_handle};
        in_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_ctrl_cfg);
        es7210_codec_cfg_t es7210_cfg = {
            .ctrl_if = in_ctrl_if,
            .mic_selected = ES7120_SEL_MIC1 | ES7120_SEL_MIC2 | ES7120_SEL_MIC3,
        };
        in_codec_if = es7210_codec_new(&es7210_cfg);
    }
}

static esp_codec_dev_handle_t setup_periph_create_codec_dev(esp_codec_dev_type_t dev_type, esp_gmf_setup_periph_aud_info *aud_info)
{
    esp_codec_dev_sample_info_t fs = {
        .sample_rate = aud_info->sample_rate,
        .channel = aud_info->channel,
        .bits_per_sample = aud_info->bits_per_sample,
    };
    esp_codec_dev_cfg_t dev_cfg = {0};
    esp_codec_dev_handle_t codec_dev = NULL;
    if (dev_type == ESP_CODEC_DEV_TYPE_OUT) {
        // New output codec device
        dev_cfg.codec_if = out_codec_if;
        dev_cfg.data_if = out_data_if;
        dev_cfg.dev_type = ESP_CODEC_DEV_TYPE_OUT;
        codec_dev = esp_codec_dev_new(&dev_cfg);
        esp_codec_dev_set_out_vol(codec_dev, 80.0);
        esp_codec_dev_open(codec_dev, &fs);
    } else {
        // New input codec device
        dev_cfg.codec_if = in_codec_if;
        dev_cfg.data_if = in_data_if;
        dev_cfg.dev_type = ESP_CODEC_DEV_TYPE_IN;
        codec_dev = esp_codec_dev_new(&dev_cfg);
        esp_codec_dev_set_in_gain(codec_dev, 30.0);
        esp_codec_dev_open(codec_dev, &fs);
    }
    return codec_dev;
}

static void setup_periph_play_codec(esp_gmf_setup_periph_aud_info *aud_info, void **play_dev)
{
    out_data_if = setup_periph_new_i2s_data(tx_handle, NULL);
    setup_periph_new_play_codec();
    *play_dev = setup_periph_create_codec_dev(ESP_CODEC_DEV_TYPE_OUT, aud_info);
}

static void setup_periph_record_codec(esp_gmf_setup_periph_aud_info *aud_info, void **record_dev)
{
    in_data_if = setup_periph_new_i2s_data(NULL, rx_handle);
    setup_periph_new_record_codec();
    *record_dev = setup_periph_create_codec_dev(ESP_CODEC_DEV_TYPE_IN, aud_info);
}

void teardown_periph_play_codec(void *play_dev)
{
    esp_codec_dev_close(play_dev);
    esp_codec_dev_delete(play_dev);
    audio_codec_delete_codec_if(out_codec_if);
    audio_codec_delete_ctrl_if(out_ctrl_if);
    audio_codec_delete_gpio_if(gpio_if);
    audio_codec_delete_data_if(out_data_if);
    i2s_channel_disable(tx_handle);
    i2s_del_channel(tx_handle);
    tx_handle = NULL;
}

void teardown_periph_record_codec(void *record_dev)
{
    esp_codec_dev_close(record_dev);
    esp_codec_dev_delete(record_dev);
    audio_codec_delete_codec_if(in_codec_if);
    audio_codec_delete_ctrl_if(in_ctrl_if);
    audio_codec_delete_data_if(in_data_if);
    i2s_channel_disable(rx_handle);
    i2s_del_channel(rx_handle);
    rx_handle = NULL;
}
#endif  /* USE_ESP_GMF_ESP_CODEC_DEV_IO */

esp_gmf_err_t esp_gmf_setup_periph(esp_gmf_setup_periph_hardware_info *info)
{
    if (info == NULL) {
        ESP_LOGE(TAG, "Invalid hardware info");
        return ESP_GMF_ERR_INVALID_ARG;
    }

    if (info->i2c.handle != NULL) {
        i2c_handle = (i2c_master_bus_handle_t)info->i2c.handle;
    } else {
        i2c_master_bus_config_t i2c_config = {
            .i2c_port = info->i2c.port,
            .sda_io_num = info->i2c.io_sda,
            .scl_io_num = info->i2c.io_scl,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .flags.enable_internal_pullup = true,
            .glitch_ignore_cnt = 7,
        };
        ESP_GMF_RET_ON_NOT_OK(TAG, i2c_new_master_bus(&i2c_config, &i2c_handle), { return ESP_GMF_ERR_FAIL;}, "Failed to create I2C master bus");
    }

    hardware_info = *info;

    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_get_periph_info(esp_gmf_setup_periph_hardware_info *info)
{
    if (info == NULL) {
        ESP_LOGE(TAG, "Invalid hardware info");
        return ESP_GMF_ERR_INVALID_ARG;
    }

    *info = hardware_info;

    return ESP_GMF_ERR_OK;
}

#ifdef USE_ESP_GMF_ESP_CODEC_DEV_IO
esp_gmf_err_t esp_gmf_setup_periph_codec(void **play_dev, void **record_dev)
{
    if ((play_dev != NULL) && (record_dev != NULL)) {
        if (hardware_info.codec.dac.port_num == hardware_info.codec.adc.port_num) {
            ESP_GMF_RET_ON_NOT_OK(TAG, setup_periph_create_i2s(I2S_CREATE_MODE_TX_AND_RX, &hardware_info.codec.dac), { return ESP_GMF_ERR_FAIL;}, "Failed to create I2S tx and rx");
        } else {
            ESP_GMF_RET_ON_NOT_OK(TAG, setup_periph_create_i2s(I2S_CREATE_MODE_TX_ONLY, &hardware_info.codec.dac), { return ESP_GMF_ERR_FAIL;}, "Failed to create I2S tx");
            ESP_GMF_RET_ON_NOT_OK(TAG, setup_periph_create_i2s(I2S_CREATE_MODE_RX_ONLY, &hardware_info.codec.adc), { return ESP_GMF_ERR_FAIL;}, "Failed to create I2S rx");
        }
        setup_periph_play_codec(&hardware_info.codec.dac, play_dev);
        setup_periph_record_codec(&hardware_info.codec.adc, record_dev);
    } else if (play_dev != NULL) {
        ESP_GMF_RET_ON_NOT_OK(TAG, setup_periph_create_i2s(I2S_CREATE_MODE_TX_ONLY, &hardware_info.codec.dac), { return ESP_GMF_ERR_FAIL;}, "Failed to create I2S tx");
        setup_periph_play_codec(&hardware_info.codec.dac, play_dev);
    } else if (record_dev != NULL) {
        ESP_GMF_RET_ON_NOT_OK(TAG, setup_periph_create_i2s(I2S_CREATE_MODE_RX_ONLY, &hardware_info.codec.adc), { return ESP_GMF_ERR_FAIL;}, "Failed to create I2S rx");
        setup_periph_record_codec(&hardware_info.codec.adc, record_dev);
    } else {
        return ESP_GMF_ERR_FAIL;
    }
    return ESP_GMF_ERR_OK;
}

void esp_gmf_teardown_periph_codec(void *play_dev, void *record_dev)
{
    if (play_dev != NULL) {
        teardown_periph_play_codec(play_dev);
    }
    if (record_dev != NULL) {
        teardown_periph_record_codec(record_dev);
    }
}
#endif  /* USE_ESP_GMF_ESP_CODEC_DEV_IO */
