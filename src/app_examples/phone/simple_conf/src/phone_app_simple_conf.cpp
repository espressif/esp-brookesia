/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "phone_app_simple_conf.hpp"
#include "phone_app_simple_conf_main.h"

using namespace std;

LV_IMG_DECLARE(esp_ui_phone_app_launcher_image_default);

PhoneAppSimpleConf::PhoneAppSimpleConf(bool use_status_bar, bool use_navigation_bar):
    ESP_UI_PhoneApp("Simple Conf", &esp_ui_phone_app_launcher_image_default, true, use_status_bar, use_navigation_bar)
{
}

PhoneAppSimpleConf::~PhoneAppSimpleConf()
{
    ESP_UI_LOGD("Destroy(@0x%p)", this);
}

bool PhoneAppSimpleConf::run(void)
{
    ESP_UI_LOGD("Run");

    // Create all UI resources here
    ESP_UI_CHECK_FALSE_RETURN(phone_app_simple_conf_main_init(), false, "Main init failed");

    return true;
}

bool PhoneAppSimpleConf::back(void)
{
    ESP_UI_LOGD("Back");

    // If the app needs to exit, call notifyCoreClosed() to notify the core to close the app
    ESP_UI_CHECK_FALSE_RETURN(notifyCoreClosed(), false, "Notify core closed failed");

    return true;
}

// bool PhoneAppSimpleConf::close(void)
// {
//     ESP_UI_LOGD("Close");

//     /* Do some operations here if needed */

//     return true;
// }

// bool PhoneAppSimpleConf::init()
// {
//     ESP_UI_LOGD("Init");

//     /* Do some initialization here if needed */

//     return true;
// }

// bool PhoneAppSimpleConf::deinit()
// {
//     ESP_UI_LOGD("Deinit");

//     /* Do some deinitialization here if needed */

//     return true;
// }

// bool PhoneAppSimpleConf::pause()
// {
//     ESP_UI_LOGD("Pause");

//     /* Do some operations here if needed */

//     return true;
// }

// bool PhoneAppSimpleConf::resume()
// {
//     ESP_UI_LOGD("Resume");

//     /* Do some operations here if needed */

//     return true;
// }

// bool PhoneAppSimpleConf::cleanResource()
// {
//     ESP_UI_LOGD("Clean resource");

//     /* Do some cleanup here if needed */

//     return true;
// }
