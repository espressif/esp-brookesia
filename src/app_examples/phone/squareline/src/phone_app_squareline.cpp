/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "phone_app_squareline.hpp"
#include "phone_app_squareline_main.h"

using namespace std;

LV_IMG_DECLARE(ui_img_sls_logo_png);

PhoneAppSquareline::PhoneAppSquareline(bool use_status_bar, bool use_navigation_bar):
    ESP_UI_PhoneApp("Squareline", &ui_img_sls_logo_png, false, use_status_bar, use_navigation_bar)
{
}

PhoneAppSquareline::~PhoneAppSquareline()
{
}

bool PhoneAppSquareline::run(void)
{
    ESP_UI_LOGD("Run");

    // Create all UI resources here
    ESP_UI_CHECK_FALSE_RETURN(phone_app_squareline_main_init(), false, "Main init failed");

    return true;
}

bool PhoneAppSquareline::back(void)
{
    ESP_UI_LOGD("Back");

    // If the app needs to exit, call notifyCoreClosed() to notify the core to close the app
    ESP_UI_CHECK_FALSE_RETURN(notifyCoreClosed(), false, "Notify core closed failed");

    return true;
}

// bool PhoneAppSquareline::close(void)
// {
//     /* Do some operations here if needed */

//     return true;
// }

// bool PhoneAppSquareline::init()
// {
//     /* Do some initialization here if needed */

//     return true;
// }

// bool PhoneAppSquareline::pause()
// {
//     /* Do some operations here if needed */

//     return true;
// }

// bool PhoneAppSquareline::resume()
// {
//     /* Do some operations here if needed */

//     return true;
// }

// bool PhoneAppSquareline::cleanResource()
// {
//     /* Do some cleanup here if needed */

//     return true;
// }
