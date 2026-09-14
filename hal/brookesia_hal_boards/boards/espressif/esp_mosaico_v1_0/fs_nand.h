/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdbool.h>
#include "esp_blockdev.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t fs_nand_get_bdl_handle(void *device_handle, esp_blockdev_handle_t *out_handle);

/** @brief Report whether a failed initialization or deinitialization still owns resources. */
bool fs_nand_cleanup_pending(void);

/** @brief Finish pending cleanup; an unknown peripheral release result requires a restart. */
esp_err_t fs_nand_cleanup(void);

/** @brief Return the most recent cleanup result, including errors hidden by Board Manager. */
esp_err_t fs_nand_cleanup_error(void);

#ifdef __cplusplus
}
#endif
