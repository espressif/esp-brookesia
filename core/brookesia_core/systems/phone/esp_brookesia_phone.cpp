/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <algorithm>
#include "esp_brookesia_systems_internal.h"
#if !ESP_BROOKESIA_PHONE_PHONE_ENABLE_DEBUG_LOG
#   define ESP_BROOKESIA_UTILS_DISABLE_DEBUG_LOG
#endif
#include "private/esp_brookesia_phone_utils.hpp"
#include "stylesheets/esp_brookesia_phone_stylesheets.hpp"
#include "esp_brookesia_phone.hpp"

using namespace std;
using namespace esp_brookesia::gui;

namespace esp_brookesia::systems::phone {

const Stylesheet Phone::_default_stylesheet_dark = ESP_BROOKESIA_PHONE_DEFAULT_DARK_STYLESHEET();

Phone::Phone(lv_display_t *display):
    base::Context(_active_stylesheet.core, _display, _manager, display),
    StylesheetManager(),
    _display(*this, _active_stylesheet.display),
    _manager(*this, _display, _active_stylesheet.manager)
{
}

Phone::~Phone()
{
    ESP_UTILS_LOGD("Destroy phone(@0x%p)", this);
    if (!del()) {
        ESP_UTILS_LOGE("Delete failed");
    }
}

bool Phone::begin(void)
{
    bool ret = true;
    const Stylesheet *default_find_data = nullptr;
    StyleSize display_size = {};

    ESP_UTILS_LOGD("Begin phone(@0x%p)", this);
    ESP_UTILS_CHECK_FALSE_RETURN(!checkCoreInitialized(), false, "Already initialized");

    // Check if any phone stylesheet is added, if not, add default stylesheet
    if (getStylesheetCount() == 0) {
        ESP_UTILS_LOGW("No phone stylesheet is added, adding default dark stylesheet(%s)",
                       _default_stylesheet_dark.core.name);
        ESP_UTILS_CHECK_FALSE_GOTO(ret = addStylesheet(_default_stylesheet_dark), end,
                                   "Failed to add default stylesheet");
    }
    // Check if any phone stylesheet is activated, if not, activate default stylesheet
    if (_active_stylesheet.core.name == nullptr) {
        ESP_UTILS_CHECK_NULL_RETURN(_display_device, false, "Invalid display");
        display_size.width = lv_disp_get_hor_res(_display_device);
        display_size.height = lv_disp_get_ver_res(_display_device);

        ESP_UTILS_LOGW("No phone stylesheet is activated, try to find first stylesheet with display size(%dx%d)",
                       display_size.width, display_size.height);
        default_find_data = getStylesheet(display_size);
        ESP_UTILS_CHECK_NULL_GOTO(default_find_data, end, "Failed to get default stylesheet");

        ret = activateStylesheet(*default_find_data);
        ESP_UTILS_CHECK_FALSE_GOTO(ret, end, "Failed to activate default stylesheet");
    }

    ESP_UTILS_CHECK_FALSE_GOTO(ret = base::Context::begin(), end, "Failed to begin core");
    ESP_UTILS_CHECK_FALSE_GOTO(ret = _display.begin(), end, "Failed to begin display");
    ESP_UTILS_CHECK_FALSE_GOTO(ret = _manager.begin(), end, "Failed to begin manager");

end:
    return ret;
}

bool Phone::del(void)
{
    ESP_UTILS_LOGD("Delete(@0x%p)", this);

    if (!checkCoreInitialized()) {
        return true;
    }

    if (!_manager.del()) {
        ESP_UTILS_LOGE("Delete manager failed");
    }
    if (!_display.del()) {
        ESP_UTILS_LOGE("Delete display failed");
    }
    if (!StylesheetManager::del()) {
        ESP_UTILS_LOGE("Delete stylesheet manager failed");
    }
    if (!base::Context::del()) {
        ESP_UTILS_LOGE("Delete core failed");
    }

    return true;
}

int Phone::installApp(App &app)
{
    return base::Context::getManager().installApp(app);
}

int Phone::installApp(App *app)
{
    return base::Context::getManager().installApp(app);
}

bool Phone::uninstallApp(App &app)
{
    return base::Context::getManager().uninstallApp(app);
}

bool Phone::uninstallApp(App *app)
{
    return base::Context::getManager().uninstallApp(app);
}

bool Phone::uninstallApp(int id)
{
    return base::Context::getManager().uninstallApp(id);
}

bool Phone::addStylesheet(const Stylesheet &stylesheet)
{
    ESP_UTILS_LOGD("Add phone(0x%p) stylesheet", this);

    ESP_UTILS_CHECK_FALSE_RETURN(
        StylesheetManager::addStylesheet(stylesheet.core.name, stylesheet.core.screen_size, stylesheet),
        false, "Failed to add phone stylesheet"
    );

    return true;
}

bool Phone::addStylesheet(const Stylesheet *stylesheet)
{
    ESP_UTILS_LOGD("Add phone(0x%p) stylesheet", this);

    ESP_UTILS_CHECK_FALSE_RETURN(addStylesheet(*stylesheet), false, "Failed to add phone stylesheet");

    return true;
}

bool Phone::activateStylesheet(const Stylesheet &stylesheet)
{
    ESP_UTILS_LOGD("Activate phone(0x%p) stylesheet", this);

    ESP_UTILS_CHECK_FALSE_RETURN(
        StylesheetManager::activateStylesheet(stylesheet.core.name, stylesheet.core.screen_size),
        false, "Failed to activate phone stylesheet"
    );

    if (checkCoreInitialized() && !sendDataUpdateEvent()) {
        ESP_UTILS_LOGE("Send update data event failed");
    }

    return true;
}

bool Phone::activateStylesheet(const Stylesheet *stylesheet)
{
    ESP_UTILS_LOGD("Activate phone(0x%p) stylesheet", this);

    ESP_UTILS_CHECK_FALSE_RETURN(activateStylesheet(*stylesheet), false, "Failed to activate phone stylesheet");

    return true;
}

bool Phone::calibrateStylesheet(const gui::StyleSize &screen_size, Stylesheet &stylesheet)
{
    ESP_UTILS_LOGD("Calibrate phone(0x%p) stylesheet", this);

    // Core
    ESP_UTILS_CHECK_FALSE_RETURN(calibrateCoreData(stylesheet.core), false, "Invalid core data");

    // Display
    if (!_active_stylesheet.manager.flags.enable_gesture && _active_stylesheet.display.flags.enable_recents_screen) {
        ESP_UTILS_LOGW("Gesture is disabled, but recents_screen is enabled, disable recents_screen automatically");
        _active_stylesheet.display.flags.enable_recents_screen = 0;
    }
    ESP_UTILS_CHECK_FALSE_RETURN(_display.calibrateData(screen_size, stylesheet.display), false, "Invalid display data");
    ESP_UTILS_CHECK_FALSE_RETURN(_manager.calibrateData(screen_size, _display, stylesheet.manager), false,
                                 "Invalid manager data");

    return true;
}

bool Phone::calibrateScreenSize(gui::StyleSize &size)
{
    ESP_UTILS_LOGD("Calibrate phone(0x%p) screen size", this);

    gui::StyleSize display_size = {};
    ESP_UTILS_CHECK_FALSE_RETURN(getDisplaySize(display_size), false, "Get display size failed");
    ESP_UTILS_CHECK_FALSE_RETURN(base::Context::getDisplay().calibrateCoreObjectSize(display_size, size), false, "Invalid screen size");

    return true;
}

} // namespace esp_brookesia::systems::phone
