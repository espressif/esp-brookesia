/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "phone_app_complex_conf.hpp"
#include "phone_app_complex_conf_main.h"

using namespace std;


// *INDENT-OFF*
PhoneAppComplexConf::PhoneAppComplexConf(bool use_status_bar, bool use_navigation_bar):
    ESP_Brookesia_PhoneApp(
        {
            .name = "Complex Conf",
            .launcher_icon = ESP_BROOKESIA_STYLE_IMAGE(&esp_brookesia_image_large_app_launcher_default_112_112),
            .screen_size = ESP_BROOKESIA_STYLE_SIZE_RECT_PERCENT(100, 100),
            .flags = {
                .enable_default_screen = 1,
                .enable_recycle_resource = 1,
                .enable_resize_visual_area = 1,
            },
        },
        {
            .app_launcher_page_index = 0,
            .status_icon_area_index = 0,
            .status_icon_data = {
                .size = {},
                .icon = {
                    .image_num = 1,
                    .images = {
                        ESP_BROOKESIA_STYLE_IMAGE(&esp_brookesia_image_large_app_launcher_default_112_112),
                    },
                },
            },
            .status_bar_visual_mode = (use_status_bar) ? ESP_BROOKESIA_STATUS_BAR_VISUAL_MODE_SHOW_FIXED :
                                                         ESP_BROOKESIA_STATUS_BAR_VISUAL_MODE_HIDE,
            .navigation_bar_visual_mode = (use_navigation_bar) ? ESP_BROOKESIA_NAVIGATION_BAR_VISUAL_MODE_SHOW_FLEX :
                                                                 ESP_BROOKESIA_NAVIGATION_BAR_VISUAL_MODE_HIDE,
            .flags = {
                .enable_status_icon_common_size = 1,
                .enable_navigation_gesture = 1,
            },
        }
    )
{
}

PhoneAppComplexConf::PhoneAppComplexConf():
    ESP_Brookesia_PhoneApp(
        {
            .name = "Complex Conf",
            .launcher_icon = ESP_BROOKESIA_STYLE_IMAGE(&esp_brookesia_image_large_app_launcher_default_112_112),
            .screen_size = ESP_BROOKESIA_STYLE_SIZE_RECT_PERCENT(100, 100),
            .flags = {
                .enable_default_screen = 1,
                .enable_recycle_resource = 1,
                .enable_resize_visual_area = 1,
            },
        },
        {
            .app_launcher_page_index = 0,
            .status_icon_area_index = 0,
            .status_icon_data = {
                .size = {},
                .icon = {
                    .image_num = 1,
                    .images = {
                        ESP_BROOKESIA_STYLE_IMAGE(&esp_brookesia_image_large_app_launcher_default_112_112),
                    },
                },
            },
            .status_bar_visual_mode = ESP_BROOKESIA_STATUS_BAR_VISUAL_MODE_SHOW_FIXED,
            .navigation_bar_visual_mode = ESP_BROOKESIA_NAVIGATION_BAR_VISUAL_MODE_HIDE,
            .flags = {
                .enable_status_icon_common_size = 1,
                .enable_navigation_gesture = 1,
            },
        }
    )
{
}
// *INDENT-OFF*

PhoneAppComplexConf::~PhoneAppComplexConf()
{
    ESP_BROOKESIA_LOGD("Destroy(@0x%p)", this);
}

bool PhoneAppComplexConf::run(void)
{
    ESP_BROOKESIA_LOGD("Run");

    // Create all UI resources here
    ESP_BROOKESIA_CHECK_FALSE_RETURN(phone_app_complex_conf_main_init(), false, "Main init failed");

    return true;
}

bool PhoneAppComplexConf::back(void)
{
    ESP_BROOKESIA_LOGD("Back");

    // If the app needs to exit, call notifyCoreClosed() to notify the core to close the app
    ESP_BROOKESIA_CHECK_FALSE_RETURN(notifyCoreClosed(), false, "Notify core closed failed");

    return true;
}

// bool PhoneAppComplexConf::close(void)
// {
//     ESP_BROOKESIA_LOGD("Close");

//     /* Do some operations here if needed */

//     return true;
// }

// bool PhoneAppComplexConf::init()
// {
//     ESP_BROOKESIA_LOGD("Init");

//     /* Do some initialization here if needed */

//     return true;
// }

// bool PhoneAppComplexConf::deinit()
// {
//     ESP_BROOKESIA_LOGD("Deinit");

//     /* Do some deinitialization here if needed */

//     return true;
// }

// bool PhoneAppComplexConf::pause()
// {
//     ESP_BROOKESIA_LOGD("Pause");

//     /* Do some operations here if needed */

//     return true;
// }

// bool PhoneAppComplexConf::resume()
// {
//     ESP_BROOKESIA_LOGD("Resume");

//     /* Do some operations here if needed */

//     return true;
// }

// bool PhoneAppComplexConf::cleanResource()
// {
//     ESP_BROOKESIA_LOGD("Clean resource");

//     /* Do some cleanup here if needed */

//     return true;
// }
