/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the ESP-Mosaico USB CDC console.
 *
 * The function is idempotent after a successful initialization. When
 * CONFIG_BSP_USB_AUTO_DOWNLOAD is enabled, the CDC control-line callback also
 * supports the DTR/RTS sequence emitted by esptool.
 *
 * @return ESP_OK on success, otherwise an ESP-IDF error code.
 */
esp_err_t esp_mosaico_usb_console_init(void);

#ifdef __cplusplus
}
#endif
