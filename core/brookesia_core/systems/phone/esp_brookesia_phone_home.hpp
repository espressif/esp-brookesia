/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <memory>
#include "systems/core/esp_brookesia_core.hpp"
#include "systems/phone/widgets/status_bar/esp_brookesia_status_bar.hpp"
#include "systems/phone/widgets/navigation_bar/esp_brookesia_navigation_bar.hpp"
#include "systems/phone/widgets/app_launcher/esp_brookesia_app_launcher.hpp"
#include "systems/phone/widgets/recents_screen/esp_brookesia_recents_screen.hpp"

// *INDENT-OFF*

typedef struct {
    struct {
        ESP_Brookesia_StatusBarData_t data;
        ESP_Brookesia_StatusBarVisualMode_t visual_mode;
    } status_bar;
    struct {
        ESP_Brookesia_NavigationBarData_t data;
        ESP_Brookesia_NavigationBarVisualMode_t visual_mode;
    } navigation_bar;
    struct {
        ESP_Brookesia_AppLauncherData_t data;
        ESP_Brookesia_StyleImage_t default_image;
    } app_launcher;
    struct {
        ESP_Brookesia_RecentsScreenData_t data;
        ESP_Brookesia_StatusBarVisualMode_t status_bar_visual_mode;
        ESP_Brookesia_NavigationBarVisualMode_t navigation_bar_visual_mode;
    } recents_screen;
    struct {
        uint8_t enable_status_bar: 1;
        uint8_t enable_navigation_bar: 1;
        uint8_t enable_app_launcher_flex_size: 1;
        uint8_t enable_recents_screen: 1;
        uint8_t enable_recents_screen_flex_size: 1;
        uint8_t enable_recents_screen_hide_when_no_snapshot: 1; /* Deprecated, use flag in manager instead */
    } flags;
} ESP_Brookesia_PhoneHomeData_t;

class ESP_Brookesia_PhoneHome: public ESP_Brookesia_CoreHome {
public:
    friend class ESP_Brookesia_Phone;
    friend class ESP_Brookesia_PhoneManager;

    ESP_Brookesia_PhoneHome(ESP_Brookesia_Core &core, const ESP_Brookesia_PhoneHomeData_t &data);
    ~ESP_Brookesia_PhoneHome();

    bool checkInitialized(void) const                   { return  _app_launcher.checkInitialized(); }
    const ESP_Brookesia_PhoneHomeData_t &getData(void) const   { return _data; }
    ESP_Brookesia_StatusBar *getStatusBar(void)                { return _status_bar.get(); }
    ESP_Brookesia_NavigationBar *getNavigationBar(void)        { return _navigation_bar.get(); }
    ESP_Brookesia_RecentsScreen *getRecentsScreen(void)        { return _recents_screen.get(); }
    ESP_Brookesia_AppLauncher *getAppLauncher(void)            { return &_app_launcher; }

    bool calibrateData(const ESP_Brookesia_StyleSize_t &screen_size, ESP_Brookesia_PhoneHomeData_t &data);

private:
    bool begin(void);
    bool del(void);
    bool processAppInstall(ESP_Brookesia_CoreApp *app) override;
    bool processAppUninstall(ESP_Brookesia_CoreApp *app) override;
    bool processAppRun(ESP_Brookesia_CoreApp *app) override;
    bool processAppResume(ESP_Brookesia_CoreApp *app) override;
    bool processAppClose(ESP_Brookesia_CoreApp *app) override;
    bool processMainScreenLoad(void) override;
    bool getAppVisualArea(ESP_Brookesia_CoreApp *app, lv_area_t &app_visual_area) const override;

    bool processRecentsScreenShow(void);

    // Core
    const ESP_Brookesia_PhoneHomeData_t &_data;
    // Widgets
    ESP_Brookesia_AppLauncher _app_launcher;
    std::shared_ptr<ESP_Brookesia_StatusBar> _status_bar;
    std::shared_ptr<ESP_Brookesia_NavigationBar> _navigation_bar;
    std::shared_ptr<ESP_Brookesia_RecentsScreen> _recents_screen;
};

// *INDENT-ON*
