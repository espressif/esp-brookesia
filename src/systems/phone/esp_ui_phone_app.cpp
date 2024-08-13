/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_ui_phone.hpp"
#include "esp_ui_phone_app.hpp"

#if !ESP_UI_LOG_ENABLE_DEBUG_PHONE_APP
#undef ESP_UI_LOGD
#define ESP_UI_LOGD(...)
#endif

using namespace std;

ESP_UI_PhoneApp::ESP_UI_PhoneApp(const ESP_UI_CoreAppData_t &core_data, const ESP_UI_PhoneAppData_t &phone_data):
    ESP_UI_CoreApp(core_data),
    _init_data(phone_data),
    _recents_screen_snapshot_conf{}
{
}

ESP_UI_PhoneApp::ESP_UI_PhoneApp(const char *name, const void *launcher_icon, bool use_default_screen,
                                 bool use_status_bar, bool use_navigation_bar):
    ESP_UI_CoreApp(name, launcher_icon, use_default_screen),
    _init_data(ESP_UI_PHONE_APP_DATA_DEFAULT(launcher_icon, use_status_bar, use_navigation_bar)),
    _recents_screen_snapshot_conf{}
{
}

ESP_UI_PhoneApp::~ESP_UI_PhoneApp()
{
    ESP_UI_LOGD("Destroy(@0x%p)", this);

    // Uninstall the app if it is initialized
    if (checkInitialized()) {
        if (!getPhone()->getManager().uninstallApp(this)) {
            ESP_UI_LOGE("Uninstall app failed");
        }
    }
}

bool ESP_UI_PhoneApp::setStatusIconState(uint8_t state)
{
    ESP_UI_CHECK_FALSE_RETURN(checkInitialized(), false, "App is not initialized");

    ESP_UI_StatusBar *status_bar = getPhone()->getHome().getStatusBar();
    ESP_UI_CHECK_NULL_RETURN(status_bar, false, "Status bar is invalid");

    ESP_UI_CHECK_FALSE_RETURN(status_bar->setIconState(getId(), state), false, "Failed to set status icon state");

    return true;
}

ESP_UI_Phone *ESP_UI_PhoneApp::getPhone(void)
{
    return static_cast<ESP_UI_Phone *>(static_cast<ESP_UI_TemplatePhone *>(getCore()));
}

bool ESP_UI_PhoneApp::beginExtra(void)
{
    ESP_UI_LOGD("Begin extra(@0x%p)", this);

    ESP_UI_NavigationBar *navigation_bar = getPhone()->getHome().getNavigationBar();
    ESP_UI_Gesture *gesture = getPhone()->getManager().getGesture();

    _active_data = _init_data;

    // Check navigation bar and gesture
    if ((_active_data.navigation_bar_visual_mode != ESP_UI_NAVIGATION_BAR_VISUAL_MODE_HIDE) &&
            (navigation_bar == nullptr)) {
        ESP_UI_LOGE("Navigation bar is enabled but not provided, disable it");
        _active_data.navigation_bar_visual_mode = ESP_UI_NAVIGATION_BAR_VISUAL_MODE_HIDE;
    }
    if (_active_data.flags.enable_navigation_gesture && (gesture == nullptr)) {
        ESP_UI_LOGE("Navigation gesture is enabled but not provided, disable it");
        _active_data.flags.enable_navigation_gesture = false;
    }
    if ((_active_data.navigation_bar_visual_mode == ESP_UI_NAVIGATION_BAR_VISUAL_MODE_SHOW_FIXED) &&
            _active_data.flags.enable_navigation_gesture) {
        ESP_UI_LOGW("Both navigation bar(fixed) and gesture are enabled, only bar will be used");
        _active_data.flags.enable_navigation_gesture = false;
    }

    // Check status icon
    if ((_active_data.status_icon_data.icon.image_num > 0) &&
            (_active_data.status_icon_data.icon.images[0].resource == nullptr)) {
        ESP_UI_LOGW("No status icon provided, use launcher icon");
        _active_data.status_icon_data.icon.images[0].resource = getLauncherIcon().resource;
    }

    return true;
}

bool ESP_UI_PhoneApp::delExtra(void)
{
    ESP_UI_LOGD("Delete extra(@0x%p)", this);

    _active_data = {};
    _recents_screen_snapshot_conf = {};

    return true;
}

bool ESP_UI_PhoneApp::updateRecentsScreenSnapshotConf(const void *image_resource)
{
    ESP_UI_LOGD("Update recents_screen snapshot conf");
    ESP_UI_CHECK_FALSE_RETURN(checkInitialized(), false, "App is not initialized");

    _recents_screen_snapshot_conf = (ESP_UI_RecentsScreenSnapshotConf_t) {
        .name = getName(),
        .icon_image_resource = getLauncherIcon().resource,
        .snapshot_image_resource = (image_resource == nullptr) ? getLauncherIcon().resource : image_resource,
        .id = getId(),
    };

    return true;
}
