/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_brookesia_systems_internal.h"
#if !ESP_BROOKESIA_SPEAKER_APP_ENABLE_DEBUG_LOG
#   define ESP_BROOKESIA_UTILS_DISABLE_DEBUG_LOG
#endif
#include "private/esp_brookesia_speaker_utils.hpp"
#include "esp_brookesia_speaker.hpp"
#include "esp_brookesia_speaker_app.hpp"

using namespace std;

namespace esp_brookesia::speaker {

App::App(const ESP_Brookesia_CoreAppData_t &core_data, const AppData_t &speaker_data):
    ESP_Brookesia_CoreApp(core_data),
    _init_data(speaker_data)
{
}

App::App(
    const char *name, const void *launcher_icon, bool use_default_screen, bool enable_gesture_navigation
):
    ESP_Brookesia_CoreApp(name, launcher_icon, use_default_screen),
    _init_data(ESP_BROOKESIA_SPEAKER_APP_DATA_DEFAULT(enable_gesture_navigation))
{
}

App::App(
    const char *name, const void *launcher_icon, bool use_default_screen
):
    ESP_Brookesia_CoreApp(name, launcher_icon, use_default_screen),
    _init_data(ESP_BROOKESIA_SPEAKER_APP_DATA_DEFAULT(true))
{
}

App::~App()
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();

    // Uninstall the app if it is initialized
    if (checkInitialized()) {
        if (!getSystem()->manager.uninstallApp(this)) {
            ESP_UTILS_LOGE("Uninstall app failed");
        }
    }

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
}

Speaker *App::getSystem(void)
{
    return static_cast<Speaker *>(getCore());
}

bool App::beginExtra(void)
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();

    Gesture *gesture = getSystem()->manager.getGesture();

    _active_data = _init_data;

    // Check navigation bar and gesture
    if (_active_data.flags.enable_navigation_gesture && (gesture == nullptr)) {
        ESP_UTILS_LOGE("Navigation gesture is enabled but not provided, disable it");
        _active_data.flags.enable_navigation_gesture = false;
    }

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();

    return true;
}

bool App::delExtra(void)
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();

    _active_data = {};

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();

    return true;
}

} // namespace esp_brookesia::speaker
