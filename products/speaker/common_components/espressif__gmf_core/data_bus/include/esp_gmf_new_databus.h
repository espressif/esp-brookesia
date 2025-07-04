/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_gmf_data_bus.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Create a new ring buffer with the specified item count and size
 *
 * @param[in]   num       Size of each item
 * @param[in]   item_cnt  Number of items
 * @param[out]  h         Pointer to store the handle of the GMF data bus
 *
 * @return
 *       - 0    On success
 *       - < 0  Negative value if an error occurs
 */
int esp_gmf_db_new_ringbuf(int num, int item_cnt, esp_gmf_db_handle_t *h);

/**
 * @brief  Create a new block buffer with the specified item count and size
 *
 * @param[in]   num       Size of each item
 * @param[in]   item_cnt  Number of items
 * @param[out]  h         Pointer to store the handle of the GMF data bus
 *
 * @return
 *       - 0    On success
 *       - < 0  Negative value if an error occurs
 */
int esp_gmf_db_new_block(int num, int item_cnt, esp_gmf_db_handle_t *h);

/**
 * @brief  Create a new pointer buffer (pbuf) with the specified item count and size
 *
 * @param[in]   num       Size of each item
 * @param[in]   item_cnt  Number of items
 * @param[out]  h         Pointer to store the handle of the GMF data bus
 *
 * @return
 *       - 0    On success
 *       - < 0  Negative value if an error occurs
 */
int esp_gmf_db_new_pbuf(int num, int item_cnt, esp_gmf_db_handle_t *h);

/**
 * @brief  Create a new FIFO buffer with the specified item count and size
 *
 * @param[in]   num       num Maximum number of items
 * @param[in]   item_cnt  Reserved for future use (currently unused)
 * @param[out]  h         Pointer to store the handle of the GMF data bus
 *
 * @return
 *       - 0    On success
 *       - < 0  Negative value if an error occurs
 */
int esp_gmf_db_new_fifo(int num, int item_cnt, esp_gmf_db_handle_t *h);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
