/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <algorithm>
#include "esp_brookesia_phone_type.h"
#include "esp_brookesia_phone.hpp"

#if !ESP_BROOKESIA_LOG_ENABLE_DEBUG_PHONE_PHONE
#undef ESP_BROOKESIA_LOGD
#define ESP_BROOKESIA_LOGD(...)
#endif

using namespace std;

const ESP_Brookesia_PhoneStylesheet_t ESP_Brookesia_Phone::_default_stylesheet_dark = ESP_BROOKESIA_PHONE_DEFAULT_DARK_STYLESHEET();

ESP_Brookesia_Phone::ESP_Brookesia_Phone(lv_disp_t *display):
    ESP_Brookesia_Core(_active_stylesheet.core, _home, _manager, display),
    ESP_Brookesia_PhoneStylesheet(),
    _home(*this, _active_stylesheet.home),
    _manager(*this, _home, _active_stylesheet.manager)
{
}

ESP_Brookesia_Phone::~ESP_Brookesia_Phone()
{
    ESP_BROOKESIA_LOGD("Destroy phone(@0x%p)", this);
    if (!del()) {
        ESP_BROOKESIA_LOGE("Delete failed");
    }
}

bool ESP_Brookesia_Phone::begin(void)
{
    bool ret = true;
    const ESP_Brookesia_PhoneStylesheet_t *default_find_data = nullptr;
    ESP_Brookesia_StyleSize_t display_size = {};

    ESP_BROOKESIA_LOGD("Begin phone(@0x%p)", this);
    ESP_BROOKESIA_CHECK_FALSE_RETURN(!checkCoreInitialized(), false, "Already initialized");

    // Check if any phone stylesheet is added, if not, add default stylesheet
    if (getStylesheetCount() == 0) {
        ESP_BROOKESIA_LOGW("No phone stylesheet is added, adding default dark stylesheet(%s)",
                           _default_stylesheet_dark.core.name);
        ESP_BROOKESIA_CHECK_FALSE_GOTO(ret = addStylesheet(_default_stylesheet_dark), end,
                                       "Failed to add default stylesheet");
    }
    // Check if any phone stylesheet is activated, if not, activate default stylesheet
    if (_active_stylesheet.core.name == nullptr) {
        ESP_BROOKESIA_CHECK_NULL_RETURN(_display, false, "Invalid display");
        display_size.width = lv_disp_get_hor_res(_display);
        display_size.height = lv_disp_get_ver_res(_display);

        ESP_BROOKESIA_LOGW("No phone stylesheet is activated, try to find first stylesheet with display size(%dx%d)",
                           display_size.width, display_size.height);
        default_find_data = getStylesheet(display_size);
        ESP_BROOKESIA_CHECK_NULL_GOTO(default_find_data, end, "Failed to get default stylesheet");

        ret = activateStylesheet(*default_find_data);
        ESP_BROOKESIA_CHECK_FALSE_GOTO(ret, end, "Failed to activate default stylesheet");
    }

    ESP_BROOKESIA_CHECK_FALSE_GOTO(ret = beginCore(), end, "Failed to begin core");
    ESP_BROOKESIA_CHECK_FALSE_GOTO(ret = _home.begin(), end, "Failed to begin home");
    ESP_BROOKESIA_CHECK_FALSE_GOTO(ret = _manager.begin(), end, "Failed to begin manager");

end:
    return ret;
}

bool ESP_Brookesia_Phone::del(void)
{
    ESP_BROOKESIA_LOGD("Delete(@0x%p)", this);

    if (!checkCoreInitialized()) {
        return true;
    }

    if (!_manager.del()) {
        ESP_BROOKESIA_LOGE("Delete manager failed");
    }
    if (!_home.del()) {
        ESP_BROOKESIA_LOGE("Delete home failed");
    }
    if (!ESP_Brookesia_PhoneStylesheet::del()) {
        ESP_BROOKESIA_LOGE("Delete core template failed");
    }
    if (!delCore()) {
        ESP_BROOKESIA_LOGE("Delete core failed");
    }

    return true;
}

bool ESP_Brookesia_Phone::addStylesheet(const ESP_Brookesia_PhoneStylesheet_t &stylesheet)
{
    ESP_BROOKESIA_LOGD("Add phone(0x%p) stylesheet", this);

    ESP_BROOKESIA_CHECK_FALSE_RETURN(
        ESP_Brookesia_PhoneStylesheet::addStylesheet(stylesheet.core.name, stylesheet.core.screen_size, stylesheet),
        false, "Failed to add phone stylesheet"
    );

    return true;
}

bool ESP_Brookesia_Phone::addStylesheet(const ESP_Brookesia_PhoneStylesheet_t *stylesheet)
{
    ESP_BROOKESIA_LOGD("Add phone(0x%p) stylesheet", this);

    ESP_BROOKESIA_CHECK_FALSE_RETURN(addStylesheet(*stylesheet), false, "Failed to add phone stylesheet");

    return true;
}

bool ESP_Brookesia_Phone::activateStylesheet(const ESP_Brookesia_PhoneStylesheet_t &stylesheet)
{
    ESP_BROOKESIA_LOGD("Activate phone(0x%p) stylesheet", this);

    ESP_BROOKESIA_CHECK_FALSE_RETURN(
        ESP_Brookesia_PhoneStylesheet::activateStylesheet(stylesheet.core.name, stylesheet.core.screen_size),
        false, "Failed to activate phone stylesheet"
    );

    if (checkCoreInitialized() && !sendDataUpdateEvent()) {
        ESP_BROOKESIA_LOGE("Send update data event failed");
    }

    return true;
}

bool ESP_Brookesia_Phone::activateStylesheet(const ESP_Brookesia_PhoneStylesheet_t *stylesheet)
{
    ESP_BROOKESIA_LOGD("Activate phone(0x%p) stylesheet", this);

    ESP_BROOKESIA_CHECK_FALSE_RETURN(activateStylesheet(*stylesheet), false, "Failed to activate phone stylesheet");

    return true;
}

bool ESP_Brookesia_Phone::calibrateStylesheet(const ESP_Brookesia_StyleSize_t &screen_size, ESP_Brookesia_PhoneStylesheet_t &stylesheet)
{
    ESP_BROOKESIA_LOGD("Calibrate phone(0x%p) stylesheet", this);

    // Core
    ESP_BROOKESIA_CHECK_FALSE_RETURN(calibrateCoreData(stylesheet.core), false, "Invalid core data");

    // Home
    if (!_active_stylesheet.manager.flags.enable_gesture && _active_stylesheet.home.flags.enable_recents_screen) {
        ESP_BROOKESIA_LOGW("Gesture is disabled, but recents_screen is enabled, disable recents_screen automatically");
        _active_stylesheet.home.flags.enable_recents_screen = 0;
    }
    ESP_BROOKESIA_CHECK_FALSE_RETURN(_home.calibrateData(screen_size, stylesheet.home), false, "Invalid home data");
    ESP_BROOKESIA_CHECK_FALSE_RETURN(_manager.calibrateData(screen_size, _home, stylesheet.manager), false,
                                     "Invalid manager data");

    return true;
}

bool ESP_Brookesia_Phone::calibrateScreenSize(ESP_Brookesia_StyleSize_t &size)
{
    ESP_BROOKESIA_LOGD("Calibrate phone(0x%p) screen size", this);

    ESP_Brookesia_StyleSize_t display_size = {};
    ESP_BROOKESIA_CHECK_FALSE_RETURN(getDisplaySize(display_size), false, "Get display size failed");
    ESP_BROOKESIA_CHECK_FALSE_RETURN(_core_home.calibrateCoreObjectSize(display_size, size), false, "Invalid screen size");

    return true;
}
