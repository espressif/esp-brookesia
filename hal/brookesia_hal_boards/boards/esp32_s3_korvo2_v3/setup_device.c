/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <string.h>
#include <fcntl.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_board_device.h"
#include "esp_board_periph.h"
#include "esp_lcd_ili9341.h"
#include "esp_lcd_touch_tt21100.h"
#include "dev_gpio_expander.h"
#include "esp_io_expander_tca9554.h"
#include "esp_video_init.h"
#include "dev_camera.h"

#define CAM_DEV_PATH "/dev/video2"

static const char *TAG = "KORVO2_V3_SETUP_DEVICE";

#define LCD_CTRL_GPIO (IO_EXPANDER_PIN_NUM_1)  // TCA9554_GPIO_NUM_1
#define LCD_RST_GPIO  (IO_EXPANDER_PIN_NUM_2)  // TCA9554_GPIO_NUM_2
#define LCD_CS_GPIO   (IO_EXPANDER_PIN_NUM_3)  // TCA9554_GPIO_NUM_3

esp_err_t io_expander_factory_entry_t(i2c_master_bus_handle_t i2c_handle, const uint16_t dev_addr, esp_io_expander_handle_t *handle_ret)
{
    esp_err_t ret = esp_io_expander_new_i2c_tca9554(i2c_handle, dev_addr, handle_ret);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create IO expander handle\n");
        return ret;
    }
    return ESP_OK;
}

esp_err_t lcd_panel_factory_entry_t(esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config, esp_lcd_panel_handle_t *ret_panel)
{
    ESP_LOGI(TAG, "Creating ili9341 panel...");

    esp_lcd_panel_dev_config_t panel_dev_cfg = {0};
    memcpy(&panel_dev_cfg, panel_dev_config, sizeof(esp_lcd_panel_dev_config_t));

    esp_err_t ret = esp_lcd_new_panel_ili9341(io, &panel_dev_cfg, ret_panel);
    if (ret != ESP_OK) {
        ESP_LOGE("lcd_panel_factory_entry_t", "New ili9341 panel failed");
        return ret;
    }

    esp_io_expander_handle_t *io_expander = NULL;
    ret = esp_board_device_get_handle("gpio_expander", (void **)&io_expander);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get io_expander device handle: %s", esp_err_to_name(ret));
        return ESP_FAIL;
    }

    ESP_RETURN_ON_ERROR(esp_io_expander_set_level(*io_expander, LCD_CTRL_GPIO, 1), TAG, "Set IO expander pin level failed");
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_RETURN_ON_ERROR(esp_io_expander_set_level(*io_expander, LCD_CS_GPIO, 1), TAG, "Set IO expander pin level failed");
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_RETURN_ON_ERROR(esp_io_expander_set_level(*io_expander, LCD_CS_GPIO, 0), TAG, "Set IO expander pin level failed");

    return ESP_OK;
}

esp_err_t lcd_touch_factory_entry_t(esp_lcd_panel_io_handle_t io, const esp_lcd_touch_config_t *touch_dev_config, esp_lcd_touch_handle_t *ret_touch)
{
    esp_lcd_touch_config_t touch_cfg = {0};
    memcpy(&touch_cfg, touch_dev_config, sizeof(esp_lcd_touch_config_t));
    if (touch_cfg.int_gpio_num != GPIO_NUM_NC) {
        ESP_LOGW(TAG, "Touch interrupt supported!");
        touch_cfg.interrupt_callback = NULL;
    }
    esp_err_t ret = esp_lcd_touch_new_i2c_tt21100(io, &touch_cfg, ret_touch);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create TT21100 touch driver: %s", esp_err_to_name(ret));
        return ret;
    }
    return ESP_OK;
}
