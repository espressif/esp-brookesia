/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <iterator>
#include <thread>
#include <unistd.h>
#include "private/esp_brookesia_app_settings_utils.hpp"
#include "esp_brookesia_app_settings.hpp"

#define APP_NAME "Settings"

#define MANAGER_THREAD_NAME                     "manager_run"
#define MANAGER_THREAD_STACK_SIZE_BIG           (12 * 1024)
#define MANAGER_THREAD_STACK_CAPS_EXT           (true)

using namespace std;
using namespace esp_brookesia::gui;
using namespace esp_brookesia::systems::speaker;
using namespace esp_brookesia::ai_framework;

namespace esp_brookesia::apps {

Settings::Settings():
    App({
    .name = APP_NAME,
    .launcher_icon = gui::StyleImage::IMAGE(&esp_brookesia_app_icon_launcher_settings_112_112),
    .screen_size = gui::StyleSize::RECT_PERCENT(100, 100),
    .flags = {
        .enable_default_screen = 0,
        .enable_recycle_resource = 0,
        .enable_resize_visual_area = 1,
    },
},
{
    .app_launcher_page_index = 0,
    .flags = {
        .enable_navigation_gesture = 1,
    },
}),
ui(*this, getStylesheet()->ui),
manager(*this, ui, getStylesheet()->manager)
{
}

Settings::~Settings()
{
    ESP_UTILS_LOGD("Destroy(@0x%p)", this);
}

bool Settings::addStylesheet(const SettingsStylesheetData *data)
{
    ESP_UTILS_CHECK_NULL_RETURN(data, false, "Invalid data");
    ESP_UTILS_LOGD("Add stylesheet");

    ESP_UTILS_CHECK_FALSE_RETURN(
        SettingsStylesheet::addStylesheet(data->name, data->screen_size, *data), false, "Failed to add stylesheet"
    );

    return true;
}

bool Settings::addStylesheet(Speaker *speaker, const SettingsStylesheetData *data)
{
    ESP_UTILS_CHECK_NULL_RETURN(speaker, false, "Invalid speaker");
    ESP_UTILS_CHECK_NULL_RETURN(data, false, "Invalid data");
    ESP_UTILS_LOGD("Add stylesheet with speaker");

    _system_context = static_cast<systems::base::Context *>(speaker);
    ESP_UTILS_CHECK_FALSE_RETURN(addStylesheet(data), false, "Failed to add stylesheet");

    return true;
}

bool Settings::activateStylesheet(const SettingsStylesheetData *data)
{
    ESP_UTILS_CHECK_NULL_RETURN(data, false, "Invalid data");
    ESP_UTILS_LOGD("Activate stylesheet");

    ESP_UTILS_CHECK_FALSE_RETURN(
        SettingsStylesheet::activateStylesheet(data->screen_size, *data), false, "Failed to activate stylesheet"
    );

    return true;
}

bool Settings::activateStylesheet(const char *name, const gui::StyleSize &screen_size)
{
    ESP_UTILS_CHECK_NULL_RETURN(name, false, "Invalid name");
    ESP_UTILS_LOGD("Activate stylesheet");

    ESP_UTILS_CHECK_FALSE_RETURN(
        SettingsStylesheet::activateStylesheet(name, screen_size), false, "Failed to activate stylesheet"
    );

    return true;
}

Settings *Settings::requestInstance()
{
    if (_instance == nullptr) {
        ESP_UTILS_CHECK_EXCEPTION_RETURN(
            _instance = new Settings(), nullptr, "Failed to create instance"
        );
    }
    return _instance;
}

bool Settings::run()
{
    ESP_UTILS_LOGD("Run");

    Speaker *speaker = getSystem();
    ESP_UTILS_CHECK_NULL_RETURN(speaker, false, "Invalid speaker");

    while (isStopping()) {
        usleep(10 * 1000);
    }
    _is_starting = true;

    // Due to the visual area of the app may be changed, recalibrate the screen size
    ESP_UTILS_CHECK_FALSE_RETURN(
        speaker->getDisplaySize(_active_stylesheet.screen_size), false, "Failed to get screen size"
    );
    ESP_UTILS_CHECK_FALSE_RETURN(activateStylesheet(&_active_stylesheet), false, "Failed to activate stylesheet");

    ESP_UTILS_CHECK_FALSE_RETURN(ui.begin(), false, "UI begin failed");

    {
        esp_utils::thread_config_guard thread_config(esp_utils::ThreadConfig{
            .name = MANAGER_THREAD_NAME,
            .stack_size = MANAGER_THREAD_STACK_SIZE_BIG,
            .stack_in_ext = MANAGER_THREAD_STACK_CAPS_EXT,
        });
        boost::thread([this]() {
            LvLockGuard gui_guard;
            ESP_UTILS_CHECK_FALSE_EXIT(manager.processRun(), "Manager process run failed");
            _is_starting = false;
        }).detach();
    }

    return true;
}

bool Settings::back()
{
    ESP_UTILS_LOGD("Back");

    if (!manager.processBack()) {
        ESP_UTILS_CHECK_FALSE_RETURN(notifyCoreClosed(), false, "Notify core closed failed");
    }

    return true;
}

bool Settings::close()
{
    ESP_UTILS_LOGD("Close");

    _is_stopping = true;

    boost::thread([this]() {
        while (isStarting()) {
            boost::this_thread::sleep_for(boost::chrono::milliseconds(10));
        }
        LvLockGuard gui_guard;
        ESP_UTILS_CHECK_FALSE_EXIT(manager.processClose(), "Manager process close failed");
        ESP_UTILS_CHECK_FALSE_EXIT(ui.del(), "UI delete failed");
        _is_stopping = false;
    }).detach();

    return true;
}

bool Settings::init()
{
    ESP_UTILS_LOGD("Init");

    // Check if any speaker stylesheet is added, if not, add default stylesheet
    if (getStylesheetCount() == 0) {
        ESP_UTILS_LOGW("No stylesheet is added, adding default stylesheet(%s)", _default_stylesheet_dark.name);
        ESP_UTILS_CHECK_FALSE_RETURN(
            addStylesheet(&_default_stylesheet_dark), false, "Failed to add default stylesheet"
        );
    }

    Speaker *speaker = getSystem();
    ESP_UTILS_CHECK_NULL_RETURN(speaker, false, "Invalid speaker");
    gui::StyleSize display_size = {};
    ESP_UTILS_CHECK_FALSE_RETURN(speaker->getDisplaySize(display_size), false, "Failed to get display size");

    // Check if any speaker stylesheet is activated, if not, activate default stylesheet
    if (_active_stylesheet.name == nullptr) {
        ESP_UTILS_LOGW("No stylesheet is activated, try to find first stylesheet with display size(%dx%d)",
                       display_size.width, display_size.height);

        auto default_find_data = getStylesheet(display_size);
        ESP_UTILS_CHECK_NULL_RETURN(default_find_data, false, "Failed to get default stylesheet");

        ESP_UTILS_CHECK_FALSE_RETURN(activateStylesheet(default_find_data), false, "Failed to activate stylesheet");
    }

    ESP_UTILS_CHECK_FALSE_RETURN(manager.processInit(), false, "Manager process init failed");

    return true;
}

bool Settings::deinit()
{
    ESP_UTILS_LOGD("Deinit");

    return true;
}

bool Settings::calibrateStylesheet(const gui::StyleSize &screen_size, SettingsStylesheetData &sheetstyle)
{
    ESP_UTILS_LOGD("Calibrate stylesheet");

    ESP_UTILS_CHECK_FALSE_RETURN(ui.calibrateData(screen_size, sheetstyle.ui), false, "UI calibrate data failed");

    return true;
}

bool Settings::calibrateScreenSize(gui::StyleSize &size)
{
    ESP_UTILS_LOGD("Calibrate screen size");

    Speaker *speaker = getSystem();
    ESP_UTILS_CHECK_NULL_RETURN(speaker, false, "Invalid speaker");

    ESP_UTILS_CHECK_FALSE_RETURN(speaker->calibrateScreenSize(size), false, "Calibrate screen size failed");

    return true;
}

ESP_UTILS_REGISTER_PLUGIN_WITH_CONSTRUCTOR(systems::base::App, Settings, APP_NAME, []()
{
    return std::shared_ptr<Settings>(Settings::requestInstance(), [](Settings * p) {});
})

} // namespace esp_brookesia::apps
