/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "phone_app_squareline.hpp"
#include "phone_app_squareline_main.h"

using namespace std;

LV_IMG_DECLARE(esp_brookesia_app_icon_launcher_squareline_112_112);

// This is a static variable to check if the app is already inited
static bool is_inited = false;

PhoneAppSquareline::PhoneAppSquareline(bool use_status_bar, bool use_navigation_bar):
    ESP_Brookesia_PhoneApp(
        "Squareline", &esp_brookesia_app_icon_launcher_squareline_112_112, false, use_status_bar, use_navigation_bar
    )
{
}

PhoneAppSquareline::PhoneAppSquareline():
    ESP_Brookesia_PhoneApp("Squareline", &esp_brookesia_app_icon_launcher_squareline_112_112, false)
{
}

PhoneAppSquareline::PhoneAppSquareline(
    const ESP_Brookesia_CoreAppData_t &core_data, const ESP_Brookesia_PhoneAppData_t &phone_data
):
    ESP_Brookesia_PhoneApp(core_data, phone_data)
{
}

PhoneAppSquareline::~PhoneAppSquareline()
{
}

bool PhoneAppSquareline::run(void)
{
    ESP_BROOKESIA_LOGD("Run");

    // Create all UI resources here
    ESP_BROOKESIA_CHECK_FALSE_RETURN(phone_app_squareline_main_init(this), false, "Main init failed");

    return true;
}

bool PhoneAppSquareline::back(void)
{
    ESP_BROOKESIA_LOGD("Back");

    // If the app needs to exit, call notifyCoreClosed() to notify the core to close the app
    ESP_BROOKESIA_CHECK_FALSE_RETURN(notifyCoreClosed(), false, "Notify core closed failed");

    return true;
}

// bool PhoneAppSquareline::close(void)
// {
//     ESP_BROOKESIA_LOGD("Close");

//     /* Do some operations here if needed */

//     return true;
// }

bool PhoneAppSquareline::init()
{
    ESP_BROOKESIA_LOGD("Init");

    /* Do some initialization here if needed */
    ESP_BROOKESIA_CHECK_FALSE_RETURN(!is_inited, false, "Already inited");
    is_inited = true;

    return true;
}

bool PhoneAppSquareline::deinit()
{
    ESP_BROOKESIA_LOGD("Deinit");

    /* Do some deinitialization here if needed */
    is_inited = false;

    return true;
}

// bool PhoneAppSquareline::pause()
// {
//     ESP_BROOKESIA_LOGD("Pause");

//     /* Do some operations here if needed */

//     return true;
// }

// bool PhoneAppSquareline::resume()
// {
//     ESP_BROOKESIA_LOGD("Resume");

//     /* Do some operations here if needed */

//     return true;
// }

// bool PhoneAppSquareline::cleanResource()
// {
//     ESP_BROOKESIA_LOGD("Clean resource");

//     /* Do some cleanup here if needed */

//     return true;
// }
