/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_lcd_panel_dev.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a Mosaico CO5300 panel with the shared initialization sequence.
 * @param io Existing QSPI panel IO handle; its ownership stays with the caller.
 * @param panel_dev_config Panel configuration copied during the call.
 * @param ret_panel Receives the panel owned by the caller on success.
 * @param clock_gpio_num Clock GPIO selected by the CoreBoard hardware revision.
 * @return ESP_OK on success or the GPIO/panel creation error.
 */
esp_err_t esp_mosaico_new_lcd_panel(esp_lcd_panel_io_handle_t io,
                                    const esp_lcd_panel_dev_config_t *panel_dev_config,
                                    esp_lcd_panel_handle_t *ret_panel,
                                    gpio_num_t clock_gpio_num);

#ifdef __cplusplus
}
#endif
