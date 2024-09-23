/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_brookesia.h"
#include "ui/ui.h"
#include "phone_app_complex_conf_main.h"

bool phone_app_complex_conf_main_init(void)
{
    ESP_BROOKESIA_CHECK_FALSE_RETURN(phone_app_complex_conf_ui_init(), false, "Failed to initialize UI");

    return true;
}
