/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <string.h>
#include <fcntl.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_board_device.h"
#include "esp_board_periph.h"
#include "dev_gpio_expander.h"
#if __has_include(<esp_lcd_ili9341.h>)
#define HAS_ILI9341  1
#include "esp_lcd_ili9341.h"
#endif  /* __has_include(<esp_lcd_ili9341.h>) */
#if __has_include(<esp_lcd_touch_gt911.h>)
#define HAS_GT911  1
#include "esp_lcd_touch_gt911.h"
#endif  /* __has_include(<esp_lcd_touch_gt911.h>) */
#if __has_include(<esp_lcd_touch_tt21100.h>)
#define HAS_TT21100  1
#include "esp_lcd_touch_tt21100.h"
#endif  /* __has_include(<esp_lcd_touch_tt21100.h>) */
#if __has_include(<esp_io_expander_tca9554.h>)
#define HAS_TCA9554  1
#include "esp_io_expander_tca9554.h"
#endif  /* __has_include(<esp_io_expander_tca9554.h>) */

static const char *TAG = "KORVO2_V3_SETUP_DEVICE";

#define LCD_CTRL_GPIO  (IO_EXPANDER_PIN_NUM_1)  // TCA9554_GPIO_NUM_1
#define LCD_RST_GPIO   (IO_EXPANDER_PIN_NUM_2)  // TCA9554_GPIO_NUM_2
#define LCD_CS_GPIO    (IO_EXPANDER_PIN_NUM_3)  // TCA9554_GPIO_NUM_3

#define TOUCH_ADDR_TT21100  0x48
#define TOUCH_ADDR_GT911    0x28

#if defined(HAS_TCA9554)
__attribute__((weak)) esp_err_t io_expander_factory_entry_t(i2c_master_bus_handle_t i2c_handle, const uint16_t dev_addr, esp_io_expander_handle_t *handle_ret)
{
    esp_err_t ret = esp_io_expander_new_i2c_tca9554(i2c_handle, dev_addr, handle_ret);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create IO expander handle\n");
        return ret;
    }
    return ESP_OK;
}
#endif  /* defined(HAS_TCA9554) */

#if defined(HAS_ILI9341)
__attribute__((weak)) esp_err_t lcd_panel_factory_entry_t(esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config, esp_lcd_panel_handle_t *ret_panel)
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
#endif  /* defined(HAS_ILI9341) */

#if defined(HAS_TT21100) || defined(HAS_GT911)
__attribute__((weak)) esp_err_t lcd_touch_factory_entry_t(esp_lcd_panel_io_handle_t io, const esp_lcd_touch_config_t *touch_dev_config, esp_lcd_touch_handle_t *ret_touch)
{
    esp_lcd_touch_config_t touch_cfg = {0};
    memcpy(&touch_cfg, touch_dev_config, sizeof(esp_lcd_touch_config_t));
    if (touch_cfg.int_gpio_num != GPIO_NUM_NC) {
        ESP_LOGW(TAG, "Touch interrupt supported!");
        touch_cfg.interrupt_callback = NULL;
    }

    uint16_t touch_addr = 0;
    esp_err_t ret = esp_board_device_get_i2c_effective_addr(ESP_BOARD_DEVICE_NAME_LCD_TOUCH, &touch_addr);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get LCD touch I2C address: %s", esp_err_to_name(ret));
        return ret;
    }

#if defined(HAS_TT21100)
    if (touch_addr == TOUCH_ADDR_TT21100) {
        touch_cfg.flags.mirror_x = true;
        ret = esp_lcd_touch_new_i2c_tt21100(io, &touch_cfg, ret_touch);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create TT21100 touch driver: %s", esp_err_to_name(ret));
            return ret;
        }
        ESP_LOGI(TAG, "Created TT21100 touch driver, addr: 0x%02x, mirror_x: %d", touch_addr, touch_cfg.flags.mirror_x);
        return ESP_OK;
    }
#endif  /* defined(HAS_TT21100) */

#if defined(HAS_GT911)
    if (touch_addr == TOUCH_ADDR_GT911) {
        touch_cfg.flags.mirror_x = false;
        ret = esp_lcd_touch_new_i2c_gt911(io, &touch_cfg, ret_touch);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create GT911 touch driver: %s", esp_err_to_name(ret));
            return ret;
        }
        ESP_LOGI(TAG, "Created GT911 touch driver, addr: 0x%02x, mirror_x: %d", touch_addr, touch_cfg.flags.mirror_x);
        return ESP_OK;
    }
#endif  /* defined(HAS_GT911) */

    ESP_LOGE(TAG, "Unsupported LCD touch I2C address: 0x%02x", touch_addr);
    return ESP_ERR_NOT_SUPPORTED;
}
#endif  /* defined(HAS_TT21100) || defined(HAS_GT911) */
