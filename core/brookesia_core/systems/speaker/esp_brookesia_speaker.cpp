/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <algorithm>
#include "esp_brookesia_systems_internal.h"
#if !ESP_BROOKESIA_SPEAKER_SPEAKER_ENABLE_DEBUG_LOG
#   define ESP_BROOKESIA_UTILS_DISABLE_DEBUG_LOG
#endif
#include "private/esp_brookesia_speaker_utils.hpp"
#include "stylesheets/esp_brookesia_speaker_stylesheets.hpp"
#include "esp_brookesia_speaker.hpp"

#define MUSIC_FILE_BOOT          "file://spiffs/boot.mp3"

using namespace std;

namespace esp_brookesia::systems::speaker {

// const Stylesheet Speaker::_default_stylesheet_dark = ESP_BROOKESIA_SPEAKER_DEFAULT_DARK_STYLESHEET;

Speaker::Speaker(lv_disp_t *display_device):
    base::Context(_active_stylesheet.core, _display, _manager, display_device),
    StylesheetManager(),
    _display(*this, _active_stylesheet.display),
    _manager(*this, _display, _active_stylesheet.manager)
{
}

Speaker::~Speaker()
{
    ESP_UTILS_LOGD("Destroy speaker(@0x%p)", this);
    if (!del()) {
        ESP_UTILS_LOGE("Delete failed");
    }
}

int Speaker::installApp(App &app)
{
    return base::Context::getManager().installApp(app);
}

int Speaker::installApp(App *app)
{
    return base::Context::getManager().installApp(app);
}

bool Speaker::uninstallApp(App &app)
{
    return base::Context::getManager().uninstallApp(app);
}

bool Speaker::uninstallApp(App *app)
{
    return base::Context::getManager().uninstallApp(app);
}

bool Speaker::uninstallApp(int id)
{
    return base::Context::getManager().uninstallApp(id);
}

bool Speaker::begin(void)
{
    const Stylesheet *default_find_data = nullptr;
    gui::StyleSize display_size = {};

    ESP_UTILS_LOGD("Begin speaker(@0x%p)", this);
    ESP_UTILS_CHECK_FALSE_RETURN(!checkCoreInitialized(), false, "Already initialized");

    // // Check if any speaker stylesheet is added, if not, add default stylesheet
    // if (getStylesheetCount() == 0) {
    //     ESP_UTILS_LOGW(
    //         "No speaker stylesheet is added, adding default dark stylesheet(%s)", _default_stylesheet_dark.core.name
    //     );
    //     ESP_UTILS_CHECK_FALSE_RETURN(addStylesheet(_default_stylesheet_dark), false, "Failed to add default stylesheet");
    // }
    // // Check if any speaker stylesheet is activated, if not, activate default stylesheet
    if (_active_stylesheet.core.name == nullptr) {
        ESP_UTILS_CHECK_NULL_RETURN(_display_device, false, "Invalid display");
        display_size.width = lv_disp_get_hor_res(_display_device);
        display_size.height = lv_disp_get_ver_res(_display_device);

        ESP_UTILS_LOGW(
            "No speaker stylesheet is activated, try to find first stylesheet with display size(%dx%d)",
            display_size.width, display_size.height
        );
        default_find_data = getStylesheet(display_size);
        ESP_UTILS_CHECK_NULL_RETURN(default_find_data, false, "Failed to get default stylesheet");

        ESP_UTILS_CHECK_FALSE_RETURN(
            activateStylesheet(*default_find_data), false, "Failed to activate default stylesheet"
        );
    }
    ESP_UTILS_CHECK_NULL_RETURN(_active_stylesheet.core.name, false, "Invalid active stylesheet");

    ESP_UTILS_CHECK_FALSE_RETURN(base::Context::begin(), false, "Failed to begin core");
    ESP_UTILS_CHECK_FALSE_RETURN(_display.begin(), false, "Failed to begin display");

    // Initialize agent before boot animation to prevent waiting for boot animation if crash happens
    auto agent = ai_framework::Agent::requestInstance();
    ESP_UTILS_CHECK_NULL_RETURN(agent, false, "Failed to request agent instance");
    ESP_UTILS_CHECK_FALSE_RETURN(agent->begin(), false, "Agent begin failed");

    // Show boot animation after agent begin
    ESP_UTILS_CHECK_FALSE_RETURN(_display.processDummyDraw(true), false, "Process dummy draw failed");
    ESP_UTILS_CHECK_FALSE_RETURN(_display.startBootAnimation(), false, "Start boot animation failed");
    audio_prompt_play_with_block(MUSIC_FILE_BOOT, -1);
    ESP_UTILS_CHECK_FALSE_RETURN(_display.waitBootAnimationStop(), false, "Wait boot animation stop failed");

    auto ai_buddy = AI_Buddy::requestInstance();
    ESP_UTILS_CHECK_NULL_RETURN(ai_buddy, false, "Failed to request ai buddy instance");
    ESP_UTILS_CHECK_FALSE_RETURN(ai_buddy->begin(_active_stylesheet.ai_buddy), false, "Failed to begin ai buddy");
    ESP_UTILS_CHECK_FALSE_RETURN(_manager.begin(), false, "Failed to begin manager");

    return true;
}

bool Speaker::del(void)
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
        ESP_UTILS_LOGE("Delete core template failed");
    }
    if (!base::Context::del()) {
        ESP_UTILS_LOGE("Delete core failed");
    }

    return true;
}

bool Speaker::addStylesheet(const Stylesheet &stylesheet)
{
    ESP_UTILS_LOGD("Add speaker(0x%p) stylesheet", this);

    ESP_UTILS_CHECK_FALSE_RETURN(
        StylesheetManager::addStylesheet(stylesheet.core.name, stylesheet.core.screen_size, stylesheet),
        false, "Failed to add speaker stylesheet"
    );

    return true;
}

bool Speaker::addStylesheet(const Stylesheet *stylesheet)
{
    ESP_UTILS_LOGD("Add speaker(0x%p) stylesheet", this);

    ESP_UTILS_CHECK_FALSE_RETURN(addStylesheet(*stylesheet), false, "Failed to add speaker stylesheet");

    return true;
}

bool Speaker::activateStylesheet(const Stylesheet &stylesheet)
{
    ESP_UTILS_LOGD("Activate speaker(0x%p) stylesheet", this);

    ESP_UTILS_CHECK_FALSE_RETURN(
        StylesheetManager::activateStylesheet(stylesheet.core.name, stylesheet.core.screen_size),
        false, "Failed to activate speaker stylesheet"
    );

    if (checkCoreInitialized() && !sendDataUpdateEvent()) {
        ESP_UTILS_LOGE("Send update data event failed");
    }

    return true;
}

bool Speaker::activateStylesheet(const Stylesheet *stylesheet)
{
    ESP_UTILS_LOGD("Activate speaker(0x%p) stylesheet", this);

    ESP_UTILS_CHECK_FALSE_RETURN(activateStylesheet(*stylesheet), false, "Failed to activate speaker stylesheet");

    return true;
}

bool Speaker::calibrateStylesheet(const gui::StyleSize &screen_size, Stylesheet &stylesheet)
{
    ESP_UTILS_LOGD("Calibrate speaker(0x%p) stylesheet", this);

    // Core
    ESP_UTILS_CHECK_FALSE_RETURN(calibrateCoreData(stylesheet.core), false, "Invalid core data");
    // Display
    ESP_UTILS_CHECK_FALSE_RETURN(_display.calibrateData(screen_size, stylesheet.display), false, "Invalid display data");
    // Manager
    ESP_UTILS_CHECK_FALSE_RETURN(_manager.calibrateData(screen_size, _display, stylesheet.manager), false,
                                 "Invalid manager data");

    return true;
}

bool Speaker::calibrateScreenSize(gui::StyleSize &size)
{
    ESP_UTILS_LOGD("Calibrate speaker(0x%p) screen size", this);

    gui::StyleSize display_size = {};
    ESP_UTILS_CHECK_FALSE_RETURN(getDisplaySize(display_size), false, "Get display size failed");
    ESP_UTILS_CHECK_FALSE_RETURN(base::Context::getDisplay().calibrateCoreObjectSize(display_size, size), false, "Invalid screen size");

    return true;
}

} // namespace esp_brookesia::systems::speaker
