/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_blockdev.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t fs_nand_get_bdl_handle(void *device_handle, esp_blockdev_handle_t *out_handle);

#ifdef __cplusplus
}
#endif
