/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_ui.h"
#include "ui/ui.h"
#include "phone_app_squareline_main.h"

bool phone_app_squareline_main_init(void)
{
    phone_app_squareline_ui_init();

    return true;
}
