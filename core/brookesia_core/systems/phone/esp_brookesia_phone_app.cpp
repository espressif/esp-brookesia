/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_brookesia_systems_internal.h"
#if !ESP_BROOKESIA_PHONE_APP_ENABLE_DEBUG_LOG
#   define ESP_BROOKESIA_UTILS_DISABLE_DEBUG_LOG
#endif
#include "private/esp_brookesia_phone_utils.hpp"
#include "esp_brookesia_phone.hpp"
#include "esp_brookesia_phone_app.hpp"

using namespace std;
using namespace esp_brookesia::gui;

namespace esp_brookesia::systems::phone {

App::App(const base::App::Config &core_data, const Config &phone_data):
    base::App(core_data),
    _init_config(phone_data),
    _recents_screen_snapshot_conf{}
{
}

App::App(const char *name, const void *launcher_icon, bool use_default_screen,
         bool use_status_bar, bool use_navigation_bar):
    base::App(name, launcher_icon, use_default_screen),
    _init_config(Config::SIMPLE_CONSTRUCTOR(launcher_icon, use_status_bar, use_navigation_bar)),
    _recents_screen_snapshot_conf{}
{
}

App::App(const char *name, const void *launcher_icon, bool use_default_screen):
    base::App(name, launcher_icon, use_default_screen),
    _init_config(Config::SIMPLE_CONSTRUCTOR(launcher_icon, true, false)),
    _recents_screen_snapshot_conf{}
{
}

App::~App()
{
    ESP_UTILS_LOGD("Destroy(@0x%p)", this);

    // Uninstall the app if it is initialized
    if (checkInitialized()) {
        if (!getSystem()->getManager().uninstallApp(this)) {
            ESP_UTILS_LOGE("Uninstall app failed");
        }
    }
}

bool App::setStatusIconState(int state)
{
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "base::App is not initialized");

    StatusBar *status_bar = getSystem()->getDisplay().getStatusBar();
    ESP_UTILS_CHECK_NULL_RETURN(status_bar, false, "Status bar is invalid");

    ESP_UTILS_CHECK_FALSE_RETURN(status_bar->setIconState(getId(), state), false, "Failed to set status icon state");

    return true;
}

Phone *App::getSystem(void)
{
    return static_cast<Phone *>(getSystemContext());
}

bool App::beginExtra(void)
{
    ESP_UTILS_LOGD("Begin extra(@0x%p)", this);

    NavigationBar *navigation_bar = getSystem()->getDisplay().getNavigationBar();
    Gesture *gesture = getSystem()->getManager().getGesture();

    _active_config = _init_config;

    // Check navigation bar and gesture
    if ((_active_config.navigation_bar_visual_mode != NavigationBar::VisualMode::HIDE) &&
            (navigation_bar == nullptr)) {
        ESP_UTILS_LOGE("Navigation bar is enabled but not provided, disable it");
        _active_config.navigation_bar_visual_mode = NavigationBar::VisualMode::HIDE;
    }
    if (_active_config.flags.enable_navigation_gesture && (gesture == nullptr)) {
        ESP_UTILS_LOGE("Navigation gesture is enabled but not provided, disable it");
        _active_config.flags.enable_navigation_gesture = false;
    }
    if ((_active_config.navigation_bar_visual_mode == NavigationBar::VisualMode::SHOW_FIXED) &&
            _active_config.flags.enable_navigation_gesture) {
        ESP_UTILS_LOGW("Both navigation bar(fixed) and gesture are enabled, only bar will be used");
        _active_config.flags.enable_navigation_gesture = false;
    }

    // Check status icon
    if ((_active_config.status_icon_data.icon.image_num > 0) &&
            (_active_config.status_icon_data.icon.images[0].resource == nullptr)) {
        ESP_UTILS_LOGW("No status icon provided, use launcher icon");
        _active_config.status_icon_data.icon.images[0].resource = getLauncherIcon().resource;
    }

    return true;
}

bool App::delExtra(void)
{
    ESP_UTILS_LOGD("Delete extra(@0x%p)", this);

    _active_config = {};
    _recents_screen_snapshot_conf = {};

    return true;
}

bool App::updateRecentsScreenSnapshotConf(const void *image_resource)
{
    ESP_UTILS_LOGD("Update recents_screen snapshot conf");
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "base::App is not initialized");

    _recents_screen_snapshot_conf = {
        .name = getName(),
        .icon_image_resource = getLauncherIcon().resource,
        .snapshot_image_resource = (image_resource == nullptr) ? getLauncherIcon().resource : image_resource,
        .id = getId()
    };

    return true;
}

} // namespace esp_brookesia::systems::phone
