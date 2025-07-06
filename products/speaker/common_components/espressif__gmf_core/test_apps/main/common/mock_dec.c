/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "esp_log.h"
#include "esp_gmf_oal_mem.h"
#include "mock_dec.h"
#include "esp_gmf_err.h"

static const char *TAG = "MOCK_DEC";

typedef struct mock_decoder {
    uint32_t     sample_rate;
    uint16_t     channel;
    uint16_t     bits;
    uint32_t     para_size;
    mock_para_t  para;
} mock_decoder_t;

esp_err_t mock_dec_open(mock_dec_handle_t *handle)
{
    ESP_LOGI(TAG, "Open, %p", handle);
    *handle = esp_gmf_oal_calloc(1, sizeof(mock_decoder_t));
    ESP_GMF_MEM_CHECK(TAG, handle, return ESP_ERR_NO_MEM);
    return ESP_OK;
}

esp_err_t mock_dec_process(mock_dec_handle_t handle)
{
    ESP_LOGI(TAG, "Process, %p", handle);
    return ESP_OK;
}

esp_err_t mock_dec_close(mock_dec_handle_t handle)
{
    ESP_LOGW(TAG, "Closed, %p", handle);
    esp_gmf_oal_free(handle);
    return ESP_OK;
}

esp_err_t mock_dec_set_para(mock_dec_handle_t handle, uint8_t para_size, mock_para_t *para)
{
    mock_decoder_t *dec = (mock_decoder_t *)handle;
    dec->para_size = para_size;
    memcpy(&dec->para, para, sizeof(mock_para_t));
    return ESP_OK;
}

esp_err_t mock_dec_get_para(mock_dec_handle_t handle, uint8_t para_size, mock_para_t *para)
{
    mock_decoder_t *dec = (mock_decoder_t *)handle;
    dec->para_size = para_size;
    memcpy(para, &dec->para, sizeof(mock_para_t));
    return ESP_OK;
}

esp_err_t mock_dec_set_info(mock_dec_handle_t handle, uint32_t sample_rate, uint16_t ch, uint16_t bits)
{
    mock_decoder_t *dec = (mock_decoder_t *)handle;
    dec->sample_rate = sample_rate;
    dec->channel = ch;
    dec->bits = bits;
    return ESP_OK;
}

esp_err_t mock_dec_get_info(mock_dec_handle_t handle, uint32_t *sample_rate, uint16_t *ch, uint16_t *bits)
{
    mock_decoder_t *dec = (mock_decoder_t *)handle;
    *sample_rate = dec->sample_rate;
    *ch = dec->channel;
    *bits = dec->bits;
    return ESP_OK;
}
