/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "esp_log.h"
#include "hal/gpio_hal.h"
#include "esp_rom_gpio.h"
#include "hooks.h"

/* Function used to tell the linker to include this file
 * with all its symbols.
 */
void bootloader_hooks_include(void)
{
}

void bootloader_before_init(void)
{

}

void bootloader_after_init(void)
{
    esp_rom_gpio_pad_select_gpio(ECHOEAR_BSP_HEAD_LED);
    gpio_ll_output_enable(GPIO_HAL_GET_HW(GPIO_PORT_0), ECHOEAR_BSP_HEAD_LED);
    gpio_ll_set_level(GPIO_HAL_GET_HW(GPIO_PORT_0), ECHOEAR_BSP_HEAD_LED, 0);
}
