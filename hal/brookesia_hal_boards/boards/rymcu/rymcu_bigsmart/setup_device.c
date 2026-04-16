/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "RYMCU_BIGSMART_SETUP";

#define BOARD_I2C_PORT           (1)
#define BOARD_I2C_SDA_GPIO       (1)
#define BOARD_I2C_SCL_GPIO       (2)
#define BOARD_I2C_FREQ_HZ        (400000)

#define PCA9557_I2C_ADDR         (0x19)
#define PCA9557_REG_OUTPUT_PORT  (0x01)
#define PCA9557_REG_CONFIG       (0x03)
#define PCA9557_OUTPUT_DEFAULT   (0x03) /* Match old board init state before enabling LCD */
#define PCA9557_OUTPUT_LCD_ON    (0x02) /* bit0: LCD on, bit1: amp on, bit2: camera on */
#define PCA9557_CONFIG_DEFAULT   (0xF8) /* bit0~2 output, bit3~7 input */

static esp_err_t pca9557_write_reg(i2c_master_dev_handle_t dev_handle, uint8_t reg, uint8_t value)
{
    uint8_t data[2] = {reg, value};
    return i2c_master_transmit(dev_handle, data, sizeof(data), -1);
}

static esp_err_t configure_board_expander(uint8_t output_state)
{
    i2c_master_bus_handle_t bus_handle = NULL;
    i2c_master_dev_handle_t dev_handle = NULL;

    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = BOARD_I2C_PORT,
        .sda_io_num = BOARD_I2C_SDA_GPIO,
        .scl_io_num = BOARD_I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags = {
            .enable_internal_pullup = 1,
        },
    };

    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_cfg, &bus_handle), TAG, "Create temp I2C bus failed");

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = PCA9557_I2C_ADDR,
        .scl_speed_hz = BOARD_I2C_FREQ_HZ,
    };
    esp_err_t ret = i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle);
    if (ret != ESP_OK) {
        i2c_del_master_bus(bus_handle);
        ESP_LOGE(TAG, "Add PCA9557 device failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Configure PCA9557 output state: 0x%02X", output_state);
    ret = pca9557_write_reg(dev_handle, PCA9557_REG_OUTPUT_PORT, output_state);
    if (ret == ESP_OK) {
        ret = pca9557_write_reg(dev_handle, PCA9557_REG_CONFIG, PCA9557_CONFIG_DEFAULT);
    }

    i2c_master_bus_rm_device(dev_handle);
    i2c_del_master_bus(bus_handle);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Configure PCA9557 failed: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}

esp_err_t lcd_panel_factory_entry_t(
    esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config,
    esp_lcd_panel_handle_t *ret_panel
)
{
    ESP_RETURN_ON_ERROR(
        configure_board_expander(PCA9557_OUTPUT_DEFAULT), TAG, "Configure board expander default state failed"
    );
    ESP_LOGI(TAG, "Creating ST7789 panel");

    esp_lcd_panel_dev_config_t panel_dev_cfg = {0};
    memcpy(&panel_dev_cfg, panel_dev_config, sizeof(esp_lcd_panel_dev_config_t));

    esp_err_t ret = esp_lcd_new_panel_st7789(io, &panel_dev_cfg, ret_panel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "New ST7789 panel failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Resetting ST7789 panel before enabling LCD power");
    ret = esp_lcd_panel_reset(*ret_panel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Reset ST7789 panel failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Enabling LCD power through PCA9557");
    ret = configure_board_expander(PCA9557_OUTPUT_LCD_ON);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Enable LCD power via PCA9557 failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "Waiting for LCD power to stabilize");
    vTaskDelay(pdMS_TO_TICKS(500));

    return ESP_OK;
}

esp_err_t lcd_touch_factory_entry_t(
    esp_lcd_panel_io_handle_t io, const esp_lcd_touch_config_t *touch_dev_config,
    esp_lcd_touch_handle_t *ret_touch
)
{
    esp_lcd_touch_config_t touch_cfg = {0};
    memcpy(&touch_cfg, touch_dev_config, sizeof(esp_lcd_touch_config_t));
    if (touch_cfg.int_gpio_num != GPIO_NUM_NC) {
        ESP_LOGW(TAG, "Touch interrupt supported");
        touch_cfg.interrupt_callback = NULL;
    }

    esp_err_t ret = esp_lcd_touch_new_i2c_gt911(io, &touch_cfg, ret_touch);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create GT911 touch driver: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}
