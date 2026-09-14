/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 *
 * The CO5300 initialization sequence is adapted from esp-mosaico-bsp commit
 * bef99672e411101489ed19c40527cca1c1dd5bb1.
 */

#include <string.h>
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_lcd_co5300.h"
#include "esp_lcd_touch_cst9217.h"
#include "esp_log.h"

static const char *TAG = "MOSAICO_SETUP";

static const co5300_lcd_init_cmd_t vendor_specific_init[] = {
    {0x11, NULL, 0, 600},
    {0xFE, (uint8_t[]){0x20}, 1, 0},
    {0x19, (uint8_t[]){0x10}, 1, 0},
    {0x1C, (uint8_t[]){0xA0}, 1, 0},
    {0xFE, (uint8_t[]){0x00}, 1, 0},
    {0xC4, (uint8_t[]){0x80}, 1, 0},
    {0x3A, (uint8_t[]){0x55}, 1, 0},
    {0x53, (uint8_t[]){0x20}, 1, 0},
    {0x51, (uint8_t[]){0xFF}, 1, 0},
    {0x63, (uint8_t[]){0xFF}, 1, 0},
    {0x2A, (uint8_t[]){0x00, 0x00, 0x01, 0xDF}, 4, 0},
    {0x2B, (uint8_t[]){0x00, 0x00, 0x01, 0xDF}, 4, 0},
    {0x29, NULL, 0, 600},
};

static const co5300_vendor_config_t vendor_config = {
    .init_cmds = vendor_specific_init,
    .init_cmds_size = sizeof(vendor_specific_init) / sizeof(vendor_specific_init[0]),
    .flags = {
        .use_qspi_interface = true,
    },
};

static esp_err_t configure_display_drive_capability(void)
{
    static const gpio_num_t display_pins[] = {
        GPIO_NUM_44,
        GPIO_NUM_36,
        GPIO_NUM_51,
        GPIO_NUM_35,
        GPIO_NUM_9,
    };

    for (size_t i = 0; i < sizeof(display_pins) / sizeof(display_pins[0]); i++) {
        esp_err_t ret = gpio_set_drive_capability(display_pins[i], GPIO_DRIVE_CAP_0);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to configure GPIO%d drive capability: %s",
                     display_pins[i], esp_err_to_name(ret));
            return ret;
        }
    }
    return ESP_OK;
}

esp_err_t lcd_panel_factory_entry_t(esp_lcd_panel_io_handle_t io,
                                    const esp_lcd_panel_dev_config_t *panel_dev_config,
                                    esp_lcd_panel_handle_t *ret_panel)
{
    esp_err_t ret = configure_display_drive_capability();
    if (ret != ESP_OK) {
        return ret;
    }

    esp_lcd_panel_dev_config_t config = {};
    memcpy(&config, panel_dev_config, sizeof(config));
    config.vendor_config = (void *)&vendor_config;

    ret = esp_lcd_new_panel_co5300(io, &config, ret_panel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create CO5300 panel: %s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t lcd_touch_factory_entry_t(esp_lcd_panel_io_handle_t io,
                                    const esp_lcd_touch_config_t *touch_dev_config,
                                    esp_lcd_touch_handle_t *ret_touch)
{
    esp_err_t ret = esp_lcd_touch_new_i2c_cst9217(io, touch_dev_config, ret_touch);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create CST9217 touch: %s", esp_err_to_name(ret));
    }
    return ret;
}
