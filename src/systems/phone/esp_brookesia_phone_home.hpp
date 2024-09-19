/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <memory>
#include "core/esp_brookesia_core.hpp"
#include "widgets/status_bar/esp_brookesia_status_bar.hpp"
#include "widgets/navigation_bar/esp_brookesia_navigation_bar.hpp"
#include "widgets/app_launcher/esp_brookesia_app_launcher.hpp"
#include "widgets/recents_screen/esp_brookesia_recents_screen.hpp"
#include "esp_brookesia_phone_type.h"

// *INDENT-OFF*
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
// *INDENT-OFF*
