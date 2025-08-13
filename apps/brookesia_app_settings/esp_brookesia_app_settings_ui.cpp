/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "private/esp_brookesia_app_settings_utils.hpp"
#include "esp_brookesia_app_settings_ui.hpp"

using namespace esp_brookesia::systems::speaker;

namespace esp_brookesia::apps {

SettingsUI::SettingsUI(App &ui_app, const SettingsUI_Data &ui_data):
    app(ui_app),
    data(ui_data),
    screen_settings(ui_app, ui_data.screen_base, ui_data.screen_settings),
    screen_wlan(ui_app, ui_data.screen_base, ui_data.screen_wlan),
    screen_wlan_verification(ui_app, ui_data.screen_base, ui_data.screen_wlan_verification),
    screen_wlan_softap(ui_app, ui_data.screen_base, ui_data.screen_wlan_softap),
    screen_about(ui_app, ui_data.screen_base, ui_data.screen_about),
    screen_sound(ui_app, ui_data.screen_base, ui_data.screen_sound),
    screen_display(ui_app, ui_data.screen_base, ui_data.screen_display)
{
}

SettingsUI::~SettingsUI()
{
    ESP_UTILS_LOGD("Destroy(0x%p)", this);

    ESP_UTILS_CHECK_FALSE_EXIT(del(), "Delete failed");
}

bool SettingsUI::begin()
{
    ESP_UTILS_LOGD("Begin(@0x%p)", this);
    ESP_UTILS_CHECK_FALSE_RETURN(app.checkInitialized(), false, "Core app not initialized");

    ESP_UTILS_CHECK_FALSE_RETURN(screen_settings.begin(), false, "Screen settings begin failed");
    ESP_UTILS_CHECK_FALSE_RETURN(screen_wlan.begin(), false, "Screen wlan begin failed");
    ESP_UTILS_CHECK_FALSE_RETURN(
        screen_wlan_verification.begin(), false, "Screen wlan connect begin failed"
    );
    ESP_UTILS_CHECK_FALSE_RETURN(
        screen_wlan_softap.begin(), false, "Screen wlan softap begin failed"
    );
    ESP_UTILS_CHECK_FALSE_RETURN(screen_about.begin(), false, "Screen about begin failed");
    ESP_UTILS_CHECK_FALSE_RETURN(screen_sound.begin(), false, "Screen about sound failed");
    ESP_UTILS_CHECK_FALSE_RETURN(screen_display.begin(), false, "Screen display begin failed");

    lv_scr_load(screen_settings.getScreenObject());
    // lv_scr_load(screen_about.getScreenObject());
    // lv_scr_load(screen_wlan.getScreenObject());
    // lv_scr_load(screen_wlan_verification.getScreenObject());

    _is_initialized = true;

    return true;
}

bool SettingsUI::del()
{
    ESP_UTILS_LOGD("Delete(@0x%p)", this);

    _is_initialized = false;

    ESP_UTILS_CHECK_FALSE_RETURN(screen_settings.del(), false, "Screen settings delete failed");
    ESP_UTILS_CHECK_FALSE_RETURN(screen_wlan.del(), false, "Screen wlan delete failed");
    ESP_UTILS_CHECK_FALSE_RETURN(screen_wlan_verification.del(), false, "Screen wlan connect delete failed");
    ESP_UTILS_CHECK_FALSE_RETURN(screen_wlan_softap.del(), false, "Screen wlan softap delete failed");
    ESP_UTILS_CHECK_FALSE_RETURN(screen_about.del(), false, "Screen about delete failed");
    ESP_UTILS_CHECK_FALSE_RETURN(screen_sound.del(), false, "Screen sound delete failed");
    ESP_UTILS_CHECK_FALSE_RETURN(screen_display.del(), false, "Screen display delete failed");

    return true;
}

bool SettingsUI::calibrateData(const gui::StyleSize &parent_size, SettingsUI_Data &ui_data)
{
    ESP_UTILS_LOGD("Calibrate data");
    const systems::base::Display &core_home = app.getSystemContext()->getDisplay();
    ESP_UTILS_CHECK_FALSE_RETURN(
        SettingsUI_ScreenBase::calibrateData(parent_size, core_home, ui_data.screen_base),
        false, "Screen base calibrate data failed"
    );
    ESP_UTILS_CHECK_FALSE_RETURN(
        SettingsUI_ScreenWlanVerification::calibrateData(parent_size, core_home, ui_data.screen_wlan_verification),
        false, "Screen WLAN connect calibrate data failed"
    );
    ESP_UTILS_CHECK_FALSE_RETURN(
        SettingsUI_ScreenWlanSoftAP::calibrateData(parent_size, core_home, ui_data.screen_wlan_softap),
        false, "Screen WLAN softap calibrate data failed"
    );

    return true;
}

bool SettingsUI::processStylesheetUpdate()
{
    ESP_UTILS_LOGD("Process stylesheet update");
    if (!checkInitialized()) {
        return true;
    }

    ESP_UTILS_CHECK_FALSE_RETURN(screen_settings.processDataUpdate(), false, "Screen settings process data update failed");
    ESP_UTILS_CHECK_FALSE_RETURN(screen_wlan.processDataUpdate(), false, "Screen wlan process data update failed");
    ESP_UTILS_CHECK_FALSE_RETURN(screen_wlan_verification.processDataUpdate(), false, "Screen wlan connect process data update failed");
    ESP_UTILS_CHECK_FALSE_RETURN(screen_wlan_softap.processDataUpdate(), false, "Screen wlan softap process data update failed");
    ESP_UTILS_CHECK_FALSE_RETURN(screen_about.processDataUpdate(), false, "Screen about process data update failed");
    ESP_UTILS_CHECK_FALSE_RETURN(screen_sound.processDataUpdate(), false, "Screen sound process data update failed");
    ESP_UTILS_CHECK_FALSE_RETURN(screen_display.processDataUpdate(), false, "Screen display process data update failed");

    return true;
}

}; // namespace esp_brookesia::apps
