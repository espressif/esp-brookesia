/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "phone_app_simple_conf.hpp"
#include "phone_app_simple_conf_main.h"

using namespace std;


PhoneAppSimpleConf::PhoneAppSimpleConf(bool use_status_bar, bool use_navigation_bar):
    ESP_Brookesia_PhoneApp("Simple Conf", &esp_brookesia_image_large_app_launcher_default_112_112, true, use_status_bar, use_navigation_bar)
{
}

PhoneAppSimpleConf::PhoneAppSimpleConf():
    ESP_Brookesia_PhoneApp("Simple Conf", &esp_brookesia_image_large_app_launcher_default_112_112, true)
{
}

PhoneAppSimpleConf::~PhoneAppSimpleConf()
{
    ESP_BROOKESIA_LOGD("Destroy(@0x%p)", this);
}

bool PhoneAppSimpleConf::run(void)
{
    ESP_BROOKESIA_LOGD("Run");

    // Create all UI resources here
    ESP_BROOKESIA_CHECK_FALSE_RETURN(phone_app_simple_conf_main_init(), false, "Main init failed");

    return true;
}

bool PhoneAppSimpleConf::back(void)
{
    ESP_BROOKESIA_LOGD("Back");

    // If the app needs to exit, call notifyCoreClosed() to notify the core to close the app
    ESP_BROOKESIA_CHECK_FALSE_RETURN(notifyCoreClosed(), false, "Notify core closed failed");

    return true;
}

// bool PhoneAppSimpleConf::close(void)
// {
//     ESP_BROOKESIA_LOGD("Close");

//     /* Do some operations here if needed */

//     return true;
// }

// bool PhoneAppSimpleConf::init()
// {
//     ESP_BROOKESIA_LOGD("Init");

//     /* Do some initialization here if needed */

//     return true;
// }

// bool PhoneAppSimpleConf::deinit()
// {
//     ESP_BROOKESIA_LOGD("Deinit");

//     /* Do some deinitialization here if needed */

//     return true;
// }

// bool PhoneAppSimpleConf::pause()
// {
//     ESP_BROOKESIA_LOGD("Pause");

//     /* Do some operations here if needed */

//     return true;
// }

// bool PhoneAppSimpleConf::resume()
// {
//     ESP_BROOKESIA_LOGD("Resume");

//     /* Do some operations here if needed */

//     return true;
// }

// bool PhoneAppSimpleConf::cleanResource()
// {
//     ESP_BROOKESIA_LOGD("Clean resource");

//     /* Do some cleanup here if needed */

//     return true;
// }
