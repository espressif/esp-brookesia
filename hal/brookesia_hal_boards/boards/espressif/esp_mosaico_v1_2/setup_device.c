/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_lcd_touch_cst9220.h"
#include "esp_log.h"
#include "brookesia/hal_custom/board/display.h"

static const char *TAG = "MOSAICO_SETUP";

esp_err_t lcd_panel_factory_entry_t(esp_lcd_panel_io_handle_t io,
                                    const esp_lcd_panel_dev_config_t *panel_dev_config,
                                    esp_lcd_panel_handle_t *ret_panel)
{
    return esp_mosaico_new_lcd_panel(io, panel_dev_config, ret_panel, GPIO_NUM_42);
}

esp_err_t lcd_touch_factory_entry_t(esp_lcd_panel_io_handle_t io,
                                    const esp_lcd_touch_config_t *touch_dev_config,
                                    esp_lcd_touch_handle_t *ret_touch)
{
    esp_err_t ret = esp_lcd_touch_new_i2c_cst9220(io, touch_dev_config, ret_touch);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create CST9220 touch: %s", esp_err_to_name(ret));
    }
    return ret;
}
