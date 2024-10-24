/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_brookesia_phone.hpp"
#include "esp_brookesia_phone_app.hpp"

#if !ESP_BROOKESIA_LOG_ENABLE_DEBUG_PHONE_APP
#undef ESP_BROOKESIA_LOGD
#define ESP_BROOKESIA_LOGD(...)
#endif

using namespace std;

ESP_Brookesia_PhoneApp::ESP_Brookesia_PhoneApp(const ESP_Brookesia_CoreAppData_t &core_data, const ESP_Brookesia_PhoneAppData_t &phone_data):
    ESP_Brookesia_CoreApp(core_data),
    _init_data(phone_data),
    _recents_screen_snapshot_conf{}
{
}

ESP_Brookesia_PhoneApp::ESP_Brookesia_PhoneApp(const char *name, const void *launcher_icon, bool use_default_screen,
        bool use_status_bar, bool use_navigation_bar):
    ESP_Brookesia_CoreApp(name, launcher_icon, use_default_screen),
    _init_data(ESP_BROOKESIA_PHONE_APP_DATA_DEFAULT(launcher_icon, use_status_bar, use_navigation_bar)),
    _recents_screen_snapshot_conf{}
{
}

ESP_Brookesia_PhoneApp::ESP_Brookesia_PhoneApp(const char *name, const void *launcher_icon, bool use_default_screen):
    ESP_Brookesia_CoreApp(name, launcher_icon, use_default_screen),
    _init_data(ESP_BROOKESIA_PHONE_APP_DATA_DEFAULT(launcher_icon, true, false)),
    _recents_screen_snapshot_conf{}
{
}

ESP_Brookesia_PhoneApp::~ESP_Brookesia_PhoneApp()
{
    ESP_BROOKESIA_LOGD("Destroy(@0x%p)", this);

    // Uninstall the app if it is initialized
    if (checkInitialized()) {
        if (!getPhone()->getManager().uninstallApp(this)) {
            ESP_BROOKESIA_LOGE("Uninstall app failed");
        }
    }
}

bool ESP_Brookesia_PhoneApp::setStatusIconState(uint8_t state)
{
    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkInitialized(), false, "App is not initialized");

    ESP_Brookesia_StatusBar *status_bar = getPhone()->getHome().getStatusBar();
    ESP_BROOKESIA_CHECK_NULL_RETURN(status_bar, false, "Status bar is invalid");

    ESP_BROOKESIA_CHECK_FALSE_RETURN(status_bar->setIconState(getId(), state), false, "Failed to set status icon state");

    return true;
}

ESP_Brookesia_Phone *ESP_Brookesia_PhoneApp::getPhone(void)
{
    return static_cast<ESP_Brookesia_Phone *>(getCore());
}

bool ESP_Brookesia_PhoneApp::beginExtra(void)
{
    ESP_BROOKESIA_LOGD("Begin extra(@0x%p)", this);

    ESP_Brookesia_NavigationBar *navigation_bar = getPhone()->getHome().getNavigationBar();
    ESP_Brookesia_Gesture *gesture = getPhone()->getManager().getGesture();

    _active_data = _init_data;

    // Check navigation bar and gesture
    if ((_active_data.navigation_bar_visual_mode != ESP_BROOKESIA_NAVIGATION_BAR_VISUAL_MODE_HIDE) &&
            (navigation_bar == nullptr)) {
        ESP_BROOKESIA_LOGE("Navigation bar is enabled but not provided, disable it");
        _active_data.navigation_bar_visual_mode = ESP_BROOKESIA_NAVIGATION_BAR_VISUAL_MODE_HIDE;
    }
    if (_active_data.flags.enable_navigation_gesture && (gesture == nullptr)) {
        ESP_BROOKESIA_LOGE("Navigation gesture is enabled but not provided, disable it");
        _active_data.flags.enable_navigation_gesture = false;
    }
    if ((_active_data.navigation_bar_visual_mode == ESP_BROOKESIA_NAVIGATION_BAR_VISUAL_MODE_SHOW_FIXED) &&
            _active_data.flags.enable_navigation_gesture) {
        ESP_BROOKESIA_LOGW("Both navigation bar(fixed) and gesture are enabled, only bar will be used");
        _active_data.flags.enable_navigation_gesture = false;
    }

    // Check status icon
    if ((_active_data.status_icon_data.icon.image_num > 0) &&
            (_active_data.status_icon_data.icon.images[0].resource == nullptr)) {
        ESP_BROOKESIA_LOGW("No status icon provided, use launcher icon");
        _active_data.status_icon_data.icon.images[0].resource = getLauncherIcon().resource;
    }

    return true;
}

bool ESP_Brookesia_PhoneApp::delExtra(void)
{
    ESP_BROOKESIA_LOGD("Delete extra(@0x%p)", this);

    _active_data = {};
    _recents_screen_snapshot_conf = {};

    return true;
}

bool ESP_Brookesia_PhoneApp::updateRecentsScreenSnapshotConf(const void *image_resource)
{
    ESP_BROOKESIA_LOGD("Update recents_screen snapshot conf");
    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkInitialized(), false, "App is not initialized");

    _recents_screen_snapshot_conf = (ESP_Brookesia_RecentsScreenSnapshotConf_t) {
        .name = getName(),
        .icon_image_resource = getLauncherIcon().resource,
        .snapshot_image_resource = (image_resource == nullptr) ? getLauncherIcon().resource : image_resource,
        .id = getId(),
    };

    return true;
}
