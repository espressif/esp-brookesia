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

namespace esp_brookesia::systems::speaker {

App::App(const base::App::Config &core_data, const Config &speaker_data):
    base::App(core_data),
    _init_config(speaker_data)
{
}

App::App(
    const char *name, const void *launcher_icon, bool use_default_screen, bool enable_gesture_navigation
):
    base::App(name, launcher_icon, use_default_screen),
    _init_config(App::Config::SIMPLE_CONSTRUCTOR(enable_gesture_navigation))
{
}

App::App(
    const char *name, const void *launcher_icon, bool use_default_screen
):
    base::App(name, launcher_icon, use_default_screen),
    _init_config(App::Config::SIMPLE_CONSTRUCTOR(true))
{
}

App::~App()
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();

    // Uninstall the app if it is initialized
    if (checkInitialized()) {
        if (!getSystem()->getManager().uninstallApp(this)) {
            ESP_UTILS_LOGE("Uninstall app failed");
        }
    }

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
}

Speaker *App::getSystem(void)
{
    return static_cast<Speaker *>(getSystemContext());
}

bool App::beginExtra(void)
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();

    Gesture *gesture = getSystem()->getManager().getGesture();

    _active_config = _init_config;

    // Check navigation bar and gesture
    if (_active_config.flags.enable_navigation_gesture && (gesture == nullptr)) {
        ESP_UTILS_LOGE("Navigation gesture is enabled but not provided, disable it");
        _active_config.flags.enable_navigation_gesture = false;
    }

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();

    return true;
}

bool App::delExtra(void)
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();

    _active_config = {};

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();

    return true;
}

} // namespace esp_brookesia::systems::speaker
