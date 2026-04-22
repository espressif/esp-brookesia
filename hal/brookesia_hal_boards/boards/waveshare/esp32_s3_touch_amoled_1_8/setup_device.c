/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <string.h>
#include "esp_log.h"
#include "esp_check.h"
#include "esp_io_expander_tca9554.h"
#include "esp_lcd_sh8601.h"
#include "esp_lcd_touch_ft5x06.h"
#include "esp_board_device.h"
#include "esp_board_periph.h"
#include "dev_gpio_expander.h"
#include "esp_io_expander_tca9554.h"

static const char *TAG = "SETUP_DEVICE";

#define LCD_RST_GPIO (IO_EXPANDER_PIN_NUM_0)  // TCA9554_GPIO_NUM_0
#define DSI_PWR_GPIO (IO_EXPANDER_PIN_NUM_1)  // TCA9554_GPIO_NUM_1
#define TP_RST_GPIO  (IO_EXPANDER_PIN_NUM_2)  // TCA9554_GPIO_NUM_2

static const sh8601_lcd_init_cmd_t vendor_specific_init_default[] = {
    {0x11, (uint8_t[]){0x00}, 0, 120},
    {0x44, (uint8_t[]){0x01, 0xD1}, 2, 0},
    {0x35, (uint8_t[]){0x00}, 1, 0},
    {0x53, (uint8_t[]){0x20}, 1, 10},
    {0x2A, (uint8_t[]){0x00, 0x00, 0x01, 0x6F}, 4, 0},
    {0x2B, (uint8_t[]){0x00, 0x00, 0x01, 0xBF}, 4, 0},
    {0x51, (uint8_t[]){0x00}, 1, 10},
    {0x29, (uint8_t[]){0x00}, 0, 10},
    {0x51, (uint8_t[]){0xFF}, 1, 0},
};
// define sh8601_vendor_config_t
static const sh8601_vendor_config_t vendor_config = {
    .init_cmds      = vendor_specific_init_default,
    .init_cmds_size = sizeof(vendor_specific_init_default) / sizeof(vendor_specific_init_default[0]),
    .flags          = {
        .use_qspi_interface = 1,  // QSPI
    }};

esp_err_t io_expander_factory_entry_t(i2c_master_bus_handle_t i2c_handle, const uint16_t dev_addr, esp_io_expander_handle_t *handle_ret)
{
    esp_err_t ret = esp_io_expander_new_i2c_tca9554(i2c_handle, dev_addr, handle_ret);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create IO expander handle\n");
        return ret;
    }
    return ret;
}

esp_err_t lcd_panel_factory_entry_t(esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config, esp_lcd_panel_handle_t *ret_panel)
{
    esp_lcd_panel_dev_config_t panel_dev_cfg = {0};
    memcpy(&panel_dev_cfg, panel_dev_config, sizeof(esp_lcd_panel_dev_config_t));

    panel_dev_cfg.vendor_config = (void *)&vendor_config;
    int ret = esp_lcd_new_panel_sh8601(io, &panel_dev_cfg, ret_panel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "New sh8601 panel failed");
    }

    esp_io_expander_handle_t *io_expander = NULL;
    ret = esp_board_device_get_handle("gpio_expander", (void **)&io_expander);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get io_expander device handle: %s", esp_err_to_name(ret));
        return ESP_FAIL;
    }

    ESP_RETURN_ON_ERROR(esp_io_expander_set_level(*io_expander, DSI_PWR_GPIO, 0), TAG, "Set IO expander pin level failed");
    ESP_RETURN_ON_ERROR(esp_io_expander_set_level(*io_expander, LCD_RST_GPIO, 0), TAG, "Set IO expander pin level failed");
    vTaskDelay(pdMS_TO_TICKS(300));
    ESP_RETURN_ON_ERROR(esp_io_expander_set_level(*io_expander, DSI_PWR_GPIO, 1), TAG, "Set IO expander pin level failed");
    ESP_RETURN_ON_ERROR(esp_io_expander_set_level(*io_expander, LCD_RST_GPIO, 1), TAG, "Set IO expander pin level failed");
    vTaskDelay(pdMS_TO_TICKS(100));

    return ret;
}

esp_err_t lcd_touch_factory_entry_t(esp_lcd_panel_io_handle_t io, const esp_lcd_touch_config_t *touch_dev_config, esp_lcd_touch_handle_t *ret_touch)
{
    esp_log_level_set("lcd_panel.io.i2c", ESP_LOG_NONE);
    esp_log_level_set("FT5x06", ESP_LOG_NONE);

    esp_io_expander_handle_t *io_expander = NULL;
    esp_err_t ret = ESP_OK;

    ret = esp_board_device_get_handle("gpio_expander", (void **)&io_expander);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get io_expander device handle: %s", esp_err_to_name(ret));
        return ESP_FAIL;
    }

    ESP_RETURN_ON_ERROR(esp_io_expander_set_level(*io_expander, TP_RST_GPIO, 0), TAG, "Set IO expander pin level failed");
    vTaskDelay(pdMS_TO_TICKS(300));
    ESP_RETURN_ON_ERROR(esp_io_expander_set_level(*io_expander, TP_RST_GPIO, 1), TAG, "Set IO expander pin level failed");
    vTaskDelay(pdMS_TO_TICKS(100));

    ret = esp_lcd_touch_new_i2c_ft5x06(io, touch_dev_config, ret_touch);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create ft5x06 touch driver: %s", esp_err_to_name(ret));
    }

    return ret;
}
