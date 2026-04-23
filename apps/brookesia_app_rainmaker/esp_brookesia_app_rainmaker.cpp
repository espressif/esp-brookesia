/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "lvgl.h"
#include "esp_brookesia.hpp"
#ifdef ESP_UTILS_LOG_TAG
#   undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "BS:RainMaker"
#include "esp_lib_utils.h"
#include "ui/ui.h"
#include "esp_brookesia_app_rainmaker.hpp"
#include "rainmaker_app_backend.h"

#define APP_NAME "ESP RainMaker"

using namespace std;
using namespace esp_brookesia::systems;

LV_IMG_DECLARE(esp_brookesia_app_icon_launcher_rainmaker_112_112);

namespace esp_brookesia::apps {

RainmakerDemo *RainmakerDemo::_instance = nullptr;

RainmakerDemo *RainmakerDemo::requestInstance(bool use_status_bar, bool use_navigation_bar)
{
    if (_instance == nullptr) {
        _instance = new RainmakerDemo(use_status_bar, use_navigation_bar);
    }
    return _instance;
}

RainmakerDemo::RainmakerDemo(bool use_status_bar, bool use_navigation_bar):
    App(APP_NAME, &esp_brookesia_app_icon_launcher_rainmaker_112_112, false, use_status_bar, use_navigation_bar)
{
}

RainmakerDemo::~RainmakerDemo()
{
}

bool RainmakerDemo::run(void)
{
    ESP_UTILS_LOGD("Run");

    // Create all UI resources here
    phone_app_rainmaker_ui_init();

    return true;
}

bool RainmakerDemo::back(void)
{
    ESP_UTILS_LOGD("Back");
    printf("RainmakerDemo::back\r\n");
    // If the app needs to exit, call notifyCoreClosed() to notify the core to close the app
    ESP_UTILS_CHECK_FALSE_RETURN(notifyCoreClosed(), false, "Notify core closed failed");

    return true;
}

bool RainmakerDemo::cleanResource(void)
{
    phone_app_rainmaker_ui_deinit();
    return true;
}

ESP_UTILS_REGISTER_PLUGIN_WITH_CONSTRUCTOR(systems::base::App, RainmakerDemo, APP_NAME, []()
{
    return std::shared_ptr<RainmakerDemo>(RainmakerDemo::requestInstance(), [](RainmakerDemo * p) {});
})

} // namespace esp_brookesia::apps

/* Avoid duplicate *_Animation symbols with esp-brookesia phone_app_squareline_main.cpp. */
