/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <atomic>
#include "esp_brookesia.hpp"
#include "ui/widgets/cell_container.hpp"
#include "ui/screens/settings.hpp"
#include "ui/screens/wlan.hpp"
#include "ui/screens/wlan_verification.hpp"
#include "ui/screens/about.hpp"
#include "ui/screens/sound.hpp"
#include "ui/screens/display.hpp"

namespace esp_brookesia::speaker_apps {

struct SettingsUI_Data {
    SettingsUI_ScreenBaseData screen_base;
    SettingsUI_ScreenSettingsData screen_settings;
    SettingsUI_ScreenWlanData screen_wlan;
    SettingsUI_ScreenWlanVerificationData screen_wlan_verification;
    SettingsUI_ScreenAboutData screen_about;
    SettingsUI_ScreenSoundData screen_sound;
    SettingsUI_ScreenDisplayData screen_display;
};

class SettingsUI {
public:
    SettingsUI(speaker::App &ui_app, const SettingsUI_Data &ui_data);
    ~SettingsUI();

    SettingsUI(const SettingsUI &) = delete;
    SettingsUI(SettingsUI &&) = delete;
    SettingsUI &operator=(const SettingsUI &) = delete;
    SettingsUI &operator=(SettingsUI &&) = delete;

    bool begin();
    bool del();

    bool calibrateData(const ESP_Brookesia_StyleSize_t &parent_size, SettingsUI_Data &ui_data);
    bool processStylesheetUpdate();

    bool checkInitialized() const
    {
        return _is_initialized;
    }

    speaker::App &app;
    const SettingsUI_Data &data;
    SettingsUI_ScreenSettings screen_settings;
    SettingsUI_ScreenWlan screen_wlan;
    SettingsUI_ScreenWlanVerification screen_wlan_verification;
    SettingsUI_ScreenAbout screen_about;
    SettingsUI_ScreenSound screen_sound;
    SettingsUI_ScreenDisplay screen_display;

    std::atomic<bool> _is_initialized = false;
};

} // namespace esp_brookesia::speaker
