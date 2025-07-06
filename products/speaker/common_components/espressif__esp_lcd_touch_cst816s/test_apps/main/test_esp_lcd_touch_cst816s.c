/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <inttypes.h>
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "unity.h"
#include "unity_test_runner.h"

#include "esp_lcd_touch_cst816s.h"

#define TEST_TOUCH_I2C_NUM       (0)
#define TEST_TOUCH_I2C_CLK_HZ    (400000)

#define TEST_LCD_H_RES              (240)
#define TEST_LCD_V_RES              (240)

/* LCD touch pins */
#define TEST_TOUCH_I2C_SCL       (GPIO_NUM_18)
#define TEST_TOUCH_I2C_SDA       (GPIO_NUM_8)
#define TEST_TOUCH_GPIO_INT      (GPIO_NUM_3)

TEST_CASE("test cst816s to initialize touch", "[cst816s][i2c]")
{
    /* Initilize I2C */
    const i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = TEST_TOUCH_I2C_SDA,
        .sda_pullup_en = GPIO_PULLUP_DISABLE,
        .scl_io_num = TEST_TOUCH_I2C_SCL,
        .scl_pullup_en = GPIO_PULLUP_DISABLE,
        .master.clk_speed = TEST_TOUCH_I2C_CLK_HZ
    };
    TEST_ESP_OK(i2c_param_config(TEST_TOUCH_I2C_NUM, &i2c_conf));
    TEST_ESP_OK(i2c_driver_install(TEST_TOUCH_I2C_NUM, i2c_conf.mode, 0, 0, 0));

    /* Initialize touch HW */
    const esp_lcd_touch_config_t tp_cfg = {
        .x_max = TEST_LCD_H_RES,
        .y_max = TEST_LCD_V_RES,
        .rst_gpio_num = GPIO_NUM_NC, // Shared with LCD reset
        .int_gpio_num = TEST_TOUCH_GPIO_INT,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 0,
            .mirror_x = 1,
            .mirror_y = 0,
        },
    };

    esp_lcd_touch_handle_t touch_handle;
    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    const esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_CST816S_CONFIG();
    TEST_ESP_OK(esp_lcd_new_panel_io_i2c((esp_lcd_i2c_bus_handle_t)TEST_TOUCH_I2C_NUM, &tp_io_config, &tp_io_handle));
    TEST_ESP_OK(esp_lcd_touch_new_i2c_cst816s(tp_io_handle, &tp_cfg, &touch_handle));
}

void app_main(void)
{
    printf("CST816S Test\r\n");
    unity_run_menu();
}
