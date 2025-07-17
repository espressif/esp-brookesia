/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_gmf_err.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Audio information structure of gmf element
 */
typedef struct {
    int      io_mclk;
    int      io_bclk;
    int      io_ws;
    int      io_do;
    int      io_di;
    uint32_t sample_rate;      /*!< The audio sample rate */
    uint8_t  channel;          /*!< The audio channel number */
    uint8_t  bits_per_sample;  /*!< The audio bits per sample */
    uint8_t  port_num;         /*!< The number of i2s prot */
} esp_gmf_setup_periph_aud_info;

typedef enum {
    ESP_GMF_CODEC_TYPE_ES7210_IN_ES8311_OUT = 0,
    ESP_GMF_CODEC_TYPE_ES8311_IN_OUT        = 1,
} esp_gmf_codec_type_t;

typedef struct {
    struct {
        void *handle;
        int   port;
        int   io_sda;
        int   io_scl;
    } i2c;
    struct {
        int   io_pa;
        esp_gmf_codec_type_t type;
        esp_gmf_setup_periph_aud_info dac;
        esp_gmf_setup_periph_aud_info adc;
    } codec;
} esp_gmf_setup_periph_hardware_info;

esp_gmf_err_t esp_gmf_setup_periph(esp_gmf_setup_periph_hardware_info *info);

esp_gmf_err_t esp_gmf_get_periph_info(esp_gmf_setup_periph_hardware_info *info);

#ifdef USE_ESP_GMF_ESP_CODEC_DEV_IO

/**
 * @brief  Set up record and play codec
 *
 * @param[out]  play_dev    Handle of play codec device
 * @param[out]  record_dev  Handle of record codec device
 *
 * @return
 *       - ESP_GMF_ERR_OK    Success
 *       - ESP_GMF_ERR_FAIL  Failed to setup play and record codec
 */
esp_gmf_err_t esp_gmf_setup_periph_codec(void **play_dev, void **record_dev);

/**
 * @brief  Teardown play and record codec
 *
 * @param[in]  play_dev    Handle of play codec device
 * @param[in]  record_dev  Handle of record codec device
 */
void esp_gmf_teardown_periph_codec(void *play_dev, void *record_dev);

#endif  /* USE_ESP_GMF_ESP_CODEC_DEV_IO */
#ifdef __cplusplus
}
#endif  /* __cplusplus */
