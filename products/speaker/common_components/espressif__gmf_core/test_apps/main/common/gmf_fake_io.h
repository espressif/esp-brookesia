/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_gmf_io.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Fake IO configurations
 */
typedef struct {
    int          dir;   /*!< IO direction, reader or writer */
    const char  *name;  /*!< Name for this instance */
} fake_io_cfg_t;

#define FAKE_IO_CFG_DEFAULT() {   \
    .dir  = ESP_GMF_IO_DIR_NONE,  \
    .name = NULL,                 \
}

/**
 * @brief  Initializes the fake stream I/O with the provided configuration
 *
 * @param[in]   config  Pointer to the fake IO configuration
 * @param[out]  io      Pointer to the fake IO handle to be initialized
 *
 * @return
 *       - ESP_GMF_ERR_OK  Success
 *       - other           error codes  Initialization failed
 */
esp_gmf_err_t fake_io_init(fake_io_cfg_t *config, esp_gmf_io_handle_t *io);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
