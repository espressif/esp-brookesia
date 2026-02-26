/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <string.h>
#include "esp_board_manager_includes.h"
#include "gen_board_device_custom.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_ili9341.h"
#include "esp_lcd_touch_cst816s.h"
#include "esp_log.h"
#include "esp_check.h"
#include "driver/i2s_pdm.h"
#include "soc/gpio_sig_map.h"
#include "soc/io_mux_reg.h"
#include "hal/rtc_io_hal.h"
#include "hal/gpio_ll.h"
#include "adc_mic.h"

static const char *TAG = "SETUP_DEVICE";

static const ili9341_lcd_init_cmd_t vendor_specific_init[] = {
    {0x11, NULL, 0, 120},                                                                         // Sleep Out
    {0x36, (uint8_t []){0x00}, 1, 0},                                                             // Memory Data Access Control
    {0x3A, (uint8_t []){0x05}, 1, 0},                                                             // Interface Pixel Format (16-bit)
    {0xB2, (uint8_t []){0x0C, 0x0C, 0x00, 0x33, 0x33}, 5, 0},                                     // Porch Setting
    {0xB7, (uint8_t []){0x05}, 1, 0},                                                             // Gate Control
    {0xBB, (uint8_t []){0x21}, 1, 0},                                                             // VCOM Setting
    {0xC0, (uint8_t []){0x2C}, 1, 0},                                                             // LCM Control
    {0xC2, (uint8_t []){0x01}, 1, 0},                                                             // VDV and VRH Command Enable
    {0xC3, (uint8_t []){0x15}, 1, 0},                                                             // VRH Set
    {0xC6, (uint8_t []){0x0F}, 1, 0},                                                             // Frame Rate Control
    {0xD0, (uint8_t []){0xA7}, 1, 0},                                                             // Power Control 1
    {0xD0, (uint8_t []){0xA4, 0xA1}, 2, 0},                                                       // Power Control 1
    {0xD6, (uint8_t []){0xA1}, 1, 0},                                                             // Gate output GND in sleep mode
    {
        0xE0, (uint8_t [])
        {
            0xF0, 0x05, 0x0E, 0x08, 0x0A, 0x17, 0x39, 0x54,
            0x4E, 0x37, 0x12, 0x12, 0x31, 0x37
        }, 14, 0
    },                              // Positive Gamma Control
    {
        0xE1, (uint8_t [])
        {
            0xF0, 0x10, 0x14, 0x0D, 0x0B, 0x05, 0x39, 0x44,
            0x4D, 0x38, 0x14, 0x14, 0x2E, 0x35
        }, 14, 0
    },                              // Negative Gamma Control
    {0xE4, (uint8_t []){0x23, 0x00, 0x00}, 3, 0},                                                 // Gate position control
    {0x21, NULL, 0, 0},                                                                           // Display Inversion On
    {0x29, NULL, 0, 0},                                                                           // Display On
    {0x2C, NULL, 0, 0},                                                                           // Memory Write
};

// define st77916_vendor_config_t
static const ili9341_vendor_config_t vendor_config = {
    .init_cmds = vendor_specific_init,
    .init_cmds_size = sizeof(vendor_specific_init) / sizeof(vendor_specific_init[0]),
};

esp_err_t lcd_panel_factory_entry_t(esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config, esp_lcd_panel_handle_t *ret_panel)
{
    esp_lcd_panel_dev_config_t panel_dev_cfg = {0};
    memcpy(&panel_dev_cfg, panel_dev_config, sizeof(esp_lcd_panel_dev_config_t));

    panel_dev_cfg.vendor_config = (void *)&vendor_config;
    int ret = esp_lcd_new_panel_ili9341(io, &panel_dev_cfg, ret_panel);
    if (ret != ESP_OK) {
        ESP_LOGE("lcd_panel_factory_entry_t", "New ili9341 panel failed");
        return ret;
    }
    // esp_lcd_panel_set_gap(*ret_panel, 36, 0);
    return ESP_OK;
}

esp_err_t lcd_touch_factory_entry_t(esp_lcd_panel_io_handle_t io, const esp_lcd_touch_config_t *touch_dev_config, esp_lcd_touch_handle_t *ret_touch)
{
    esp_lcd_touch_config_t touch_cfg = {0};
    memcpy(&touch_cfg, touch_dev_config, sizeof(esp_lcd_touch_config_t));
    if (touch_cfg.int_gpio_num != GPIO_NUM_NC) {
        ESP_LOGW("lcd_touch_factory_entry_t", "Touch interrupt supported!");
        touch_cfg.interrupt_callback = NULL;
    }
    esp_err_t ret = esp_lcd_touch_new_i2c_cst816s(io, &touch_cfg, ret_touch);
    if (ret != ESP_OK) {
        ESP_LOGE("lcd_touch_factory_entry_t", "Failed to create gt911 touch driver: %s", esp_err_to_name(ret));
        return ret;
    }
    return ESP_OK;
}

static int _set_mic_gain(const audio_codec_if_t *h, float db)
{
    printf("Audio codec set mic gain to %.2f dB\n", db);
    return ESP_OK;
}

static int _set_vol(const audio_codec_if_t *h, float db)
{
    printf("Audio codec set volume to %.2f dB\n", db);
    return ESP_OK;
}

static int _open(const audio_codec_if_t *h, void *cfg, int cfg_size)
{
    printf("Audio codec open\n");
    return ESP_OK;
}

static bool is_open(const audio_codec_if_t *h)
{
    printf("Audio codec is_open\n");
    return true;
}


static int adc_audio_in_init(void *config, int cfg_size, void **device_handle)
{
    dev_custom_audio_adc_config_t *dev_cfg = (dev_custom_audio_adc_config_t *)config;
    ESP_LOGI(TAG, "Initializing %s device", dev_cfg->name);
    dev_audio_codec_handles_t *codec_handles = (dev_audio_codec_handles_t *)calloc(1, sizeof(dev_audio_codec_handles_t));
    ESP_RETURN_ON_FALSE(codec_handles != NULL, ESP_FAIL, TAG, "Failed to allocate memory for codec handles");

    esp_err_t ret = ESP_OK;
    audio_codec_adc_cfg_t cfg = DEFAULT_AUDIO_CODEC_ADC_MONO_CFG(dev_cfg->adc_channel, dev_cfg->sample_rate_hz);
    const audio_codec_data_if_t *adc_if = audio_codec_new_adc_data(&cfg);
    ESP_GOTO_ON_FALSE(adc_if != NULL, ESP_FAIL, err, TAG, "Failed to create adc data interface");

    static audio_codec_if_t codec_if = {
        .open = _open,
        .is_open = is_open,
        .set_mic_gain = _set_mic_gain,
        .set_vol = _set_vol,
    };

    esp_codec_dev_cfg_t codec_dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_IN,
        .data_if = adc_if,
        .codec_if = &codec_if,
    };
    esp_codec_dev_handle_t codec_hdl = esp_codec_dev_new(&codec_dev_cfg);
    ESP_GOTO_ON_FALSE(codec_hdl != NULL, ESP_FAIL, err, TAG, "Failed to create codec device");

    codec_handles->codec_dev = codec_hdl;
    codec_handles->data_if = adc_if;
    codec_handles->codec_if = &codec_if;
    *device_handle = codec_handles;
    return ret;

err:
    if (codec_handles != NULL) {
        free(codec_handles);
    }
    assert(0);
    return ret;
}

static int adc_audio_in_deinit(void *device_handle)
{
    assert(0);
    return ESP_OK;
}

CUSTOM_DEVICE_IMPLEMENT(audio_adc, adc_audio_in_init, adc_audio_in_deinit);

#define BSP_I2S_GPIO_CFG(_dout)       \
    {                          \
        .clk = GPIO_NUM_NC,    \
        .dout = _dout,  \
        .invert_flags = {      \
            .clk_inv = false, \
        },                     \
    }
#define BSP_I2S_DUPLEX_MONO_CFG(_sample_rate, _dout)                                                         \
    {                                                                                                 \
        .clk_cfg = I2S_PDM_TX_CLK_DEFAULT_CONFIG(_sample_rate),                                          \
        .slot_cfg = I2S_PDM_TX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),        \
        .gpio_cfg = BSP_I2S_GPIO_CFG(_dout),                                                                 \
    }

static int adc_audio_out_init(void *config, int cfg_size, void **device_handle)
{
    esp_err_t ret = ESP_OK;
    static i2s_chan_handle_t tx_handle_ = NULL;
    dev_custom_audio_dac_config_t *dev_cfg = (dev_custom_audio_dac_config_t *)config;
    ESP_LOGI(TAG, "Initializing %s device", dev_cfg->name);
    dev_audio_codec_handles_t *codec_handles = (dev_audio_codec_handles_t *)calloc(1, sizeof(dev_audio_codec_handles_t));
    ESP_RETURN_ON_FALSE(codec_handles != NULL, ESP_FAIL, TAG, "Failed to allocate memory for codec handles");

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true; // Auto clear the legacy data in the DMA buffer
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle_, NULL));

    i2s_pdm_tx_config_t pdm_cfg_default = BSP_I2S_DUPLEX_MONO_CFG(dev_cfg->sample_rate_hz, dev_cfg->speaker_p_io);
    pdm_cfg_default.clk_cfg.up_sample_fs = 480; // ;AUDIO_OUTPUT_SAMPLE_RATE / 100;
    pdm_cfg_default.slot_cfg.sd_scale = I2S_PDM_SIG_SCALING_MUL_4;
    pdm_cfg_default.slot_cfg.hp_scale = I2S_PDM_SIG_SCALING_MUL_4;
    pdm_cfg_default.slot_cfg.lp_scale = I2S_PDM_SIG_SCALING_MUL_4;
    pdm_cfg_default.slot_cfg.sinc_scale = I2S_PDM_SIG_SCALING_MUL_4;
    const i2s_pdm_tx_config_t *p_i2s_cfg = &pdm_cfg_default;

    ESP_ERROR_CHECK(i2s_channel_init_pdm_tx_mode(tx_handle_, p_i2s_cfg));

    static audio_codec_if_t codec_if = {
        .open = _open,
        .is_open = is_open,
        .set_mic_gain = _set_mic_gain,
        .set_vol = _set_vol,
    };

    audio_codec_i2s_cfg_t i2s_cfg = {
        .port = I2S_NUM_0,
        .rx_handle = NULL,
        .tx_handle = tx_handle_,
    };
    const audio_codec_data_if_t *i2s_data_if = audio_codec_new_i2s_data(&i2s_cfg);

    i2s_channel_enable(tx_handle_);

    gpio_set_drive_capability(dev_cfg->speaker_p_io, GPIO_DRIVE_CAP_0);

    if (dev_cfg->speaker_n_io != GPIO_NUM_NC) {
        PIN_FUNC_SELECT(IO_MUX_GPIO10_REG, PIN_FUNC_GPIO);
        gpio_set_direction(dev_cfg->speaker_n_io, GPIO_MODE_OUTPUT);
        esp_rom_gpio_connect_out_signal(dev_cfg->speaker_n_io, I2SO_SD_OUT_IDX, 1, 0);
        gpio_set_drive_capability(dev_cfg->speaker_n_io, GPIO_DRIVE_CAP_0);
    }

    esp_codec_dev_cfg_t codec_dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_OUT,
        .data_if = i2s_data_if,
        .codec_if = &codec_if,
    };
    esp_codec_dev_handle_t codec_hdl = esp_codec_dev_new(&codec_dev_cfg);
    ESP_GOTO_ON_FALSE(codec_hdl != NULL, ESP_FAIL, err, TAG, "Failed to create codec device");

    codec_handles->codec_dev = codec_hdl;
    codec_handles->data_if = i2s_data_if;
    codec_handles->codec_if = &codec_if;
    *device_handle = codec_handles;
    printf("codec_handles: %p\n", codec_handles);
    return ret;

err:
    if (codec_handles != NULL) {
        free(codec_handles);
    }
    assert(0);
    return ret;
}

static int adc_audio_out_deinit(void *device_handle)
{
    assert(0);
    return ESP_OK;
}

CUSTOM_DEVICE_IMPLEMENT(audio_dac, adc_audio_out_init, adc_audio_out_deinit);

