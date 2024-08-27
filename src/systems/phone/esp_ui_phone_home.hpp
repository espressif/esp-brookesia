/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <memory>
#include "core/esp_ui_core.hpp"
#include "widgets/status_bar/esp_ui_status_bar.hpp"
#include "widgets/navigation_bar/esp_ui_navigation_bar.hpp"
#include "widgets/app_launcher/esp_ui_app_launcher.hpp"
#include "widgets/recents_screen/esp_ui_recents_screen.hpp"
#include "esp_ui_phone_type.h"

// *INDENT-OFF*
class ESP_UI_PhoneHome: public ESP_UI_CoreHome {
public:
    friend class ESP_UI_Phone;
    friend class ESP_UI_PhoneManager;

    ESP_UI_PhoneHome(ESP_UI_Core &core, const ESP_UI_PhoneHomeData_t &data);
    ~ESP_UI_PhoneHome();

    bool checkInitialized(void) const                   { return  _app_launcher.checkInitialized(); }
    const ESP_UI_PhoneHomeData_t &getData(void) const   { return _data; }
    ESP_UI_StatusBar *getStatusBar(void)                { return _status_bar.get(); }
    ESP_UI_NavigationBar *getNavigationBar(void)        { return _navigation_bar.get(); }
    ESP_UI_RecentsScreen *getRecentsScreen(void)        { return _recents_screen.get(); }
    ESP_UI_AppLauncher *getAppLauncher(void)            { return &_app_launcher; }

    bool calibrateData(const ESP_UI_StyleSize_t &screen_size, ESP_UI_PhoneHomeData_t &data);

private:
    bool begin(void);
    bool del(void);
    bool processAppInstall(ESP_UI_CoreApp *app) override;
    bool processAppUninstall(ESP_UI_CoreApp *app) override;
    bool processAppRun(ESP_UI_CoreApp *app) override;
    bool processAppResume(ESP_UI_CoreApp *app) override;
    bool processAppClose(ESP_UI_CoreApp *app) override;
    bool processMainScreenLoad(void) override;
    bool getAppVisualArea(ESP_UI_CoreApp *app, lv_area_t &app_visual_area) const override;

    bool processRecentsScreenShow(void);

    // Core
    const ESP_UI_PhoneHomeData_t &_data;
    // Widgets
    ESP_UI_AppLauncher _app_launcher;
    std::shared_ptr<ESP_UI_StatusBar> _status_bar;
    std::shared_ptr<ESP_UI_NavigationBar> _navigation_bar;
    std::shared_ptr<ESP_UI_RecentsScreen> _recents_screen;
};
// *INDENT-OFF*
