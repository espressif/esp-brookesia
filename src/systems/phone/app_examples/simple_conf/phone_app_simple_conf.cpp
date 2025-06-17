/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "lvgl.h"
#include "esp_brookesia.hpp"
#include "private/esp_brookesia_utils.h"
#include "phone_app_simple_conf.hpp"

using namespace std;
using namespace esp_brookesia::gui;


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

    lv_obj_t *label = lv_label_create(lv_scr_act());
    ESP_BROOKESIA_CHECK_NULL_RETURN(label, false, "Create label failed");
    lv_label_set_text(label, "Top Middle");
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_text_font(label, LV_FONT_DEFAULT, 0);

    label = lv_label_create(lv_scr_act());
    ESP_BROOKESIA_CHECK_NULL_RETURN(label, false, "Create label failed");
    lv_label_set_text(label, "Bottom Middle");
    lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_text_font(label, LV_FONT_DEFAULT, 0);

    label = lv_label_create(lv_scr_act());
    ESP_BROOKESIA_CHECK_NULL_RETURN(label, false, "Create label failed");
    lv_label_set_text(label, "Left Middle");
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_text_font(label, LV_FONT_DEFAULT, 0);

    label = lv_label_create(lv_scr_act());
    ESP_BROOKESIA_CHECK_NULL_RETURN(label, false, "Create label failed");
    lv_label_set_text(label, "Right Middle");
    lv_obj_align(label, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_text_font(label, LV_FONT_DEFAULT, 0);

    label = lv_label_create(lv_scr_act());
    ESP_BROOKESIA_CHECK_NULL_RETURN(label, false, "Create label failed");
    lv_label_set_text(label, "Simple Conf");
    lv_obj_center(label);
    lv_obj_set_style_text_font(label, LV_FONT_DEFAULT, 0);

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
