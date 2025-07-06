/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

typedef void *mock_dec_handle_t;

typedef struct {
    uint8_t      a;
    uint32_t     b;
    uint16_t     c;
} mock_args_ldata_t;

#pragma pack(push, 1)
typedef struct {
    uint8_t      d;
    uint32_t     e;
    uint16_t     f;
} mock_args_hdata_t;
#pragma pack(pop)

typedef struct {
    mock_args_ldata_t    first;
    mock_args_hdata_t    second;
    uint16_t             value;
} mock_dec_desc_t;

typedef struct {
    mock_dec_desc_t     desc;
    char                label[16];
} mock_dec_el_args_t;

typedef struct {
    uint32_t  type;
    uint32_t  fc;
    float     q;
    float     gain;
} mock_para_t;

esp_err_t mock_dec_open(mock_dec_handle_t *handle);
esp_err_t mock_dec_process(mock_dec_handle_t handle);
esp_err_t mock_dec_close(mock_dec_handle_t handle);

esp_err_t mock_dec_set_para(mock_dec_handle_t handle, uint8_t index, mock_para_t *para);

esp_err_t mock_dec_get_para(mock_dec_handle_t handle, uint8_t index, mock_para_t *para);

esp_err_t mock_dec_set_info(mock_dec_handle_t handle, uint32_t sample_rate, uint16_t ch, uint16_t bits);

esp_err_t mock_dec_get_info(mock_dec_handle_t handle, uint32_t *sample_rate, uint16_t *ch, uint16_t *bits);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
