/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "phone_app_complex_conf.hpp"
#include "phone_app_complex_conf_main.h"

using namespace std;

LV_IMG_DECLARE(esp_ui_phone_app_launcher_image_default);

// *INDENT-OFF*
PhoneAppComplexConf::PhoneAppComplexConf(bool use_status_bar, bool use_navigation_bar):
    ESP_UI_PhoneApp(
        {
            .name = "Complex Conf",
            .launcher_icon = ESP_UI_STYLE_IMAGE(&esp_ui_phone_app_launcher_image_default),
            .screen_size = ESP_UI_STYLE_SIZE_RECT_PERCENT(100, 100),
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
                        ESP_UI_STYLE_IMAGE(&esp_ui_phone_app_launcher_image_default),
                    },
                },
            },
            .status_bar_visual_mode = (use_status_bar) ? ESP_UI_STATUS_BAR_VISUAL_MODE_SHOW_FIXED :
                                                         ESP_UI_STATUS_BAR_VISUAL_MODE_HIDE,
            .navigation_bar_visual_mode = (use_navigation_bar) ? ESP_UI_NAVIGATION_BAR_VISUAL_MODE_SHOW_FLEX :
                                                                 ESP_UI_NAVIGATION_BAR_VISUAL_MODE_HIDE,
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
    ESP_UI_LOGD("Destroy(@0x%p)", this);
}

bool PhoneAppComplexConf::run(void)
{
    ESP_UI_LOGD("Run");

    // Create all UI resources here
    ESP_UI_CHECK_FALSE_RETURN(phone_app_complex_conf_main_init(), false, "Main init failed");

    return true;
}

bool PhoneAppComplexConf::back(void)
{
    ESP_UI_LOGD("Back");

    // If the app needs to exit, call notifyCoreClosed() to notify the core to close the app
    ESP_UI_CHECK_FALSE_RETURN(notifyCoreClosed(), false, "Notify core closed failed");

    return true;
}

// bool PhoneAppComplexConf::close(void)
// {
//     ESP_UI_LOGD("Close");

//     /* Do some operations here if needed */

//     return true;
// }

// bool PhoneAppComplexConf::init()
// {
//     ESP_UI_LOGD("Init");

//     /* Do some initialization here if needed */

//     return true;
// }

// bool PhoneAppComplexConf::deinit()
// {
//     ESP_UI_LOGD("Deinit");

//     /* Do some deinitialization here if needed */

//     return true;
// }

// bool PhoneAppComplexConf::pause()
// {
//     ESP_UI_LOGD("Pause");

//     /* Do some operations here if needed */

//     return true;
// }

// bool PhoneAppComplexConf::resume()
// {
//     ESP_UI_LOGD("Resume");

//     /* Do some operations here if needed */

//     return true;
// }

// bool PhoneAppComplexConf::cleanResource()
// {
//     ESP_UI_LOGD("Clean resource");

//     /* Do some cleanup here if needed */

//     return true;
// }
