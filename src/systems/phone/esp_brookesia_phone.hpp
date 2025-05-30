/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <list>
#include <memory>
#include "esp_brookesia_conf_internal.h"
#include "systems/core/esp_brookesia_core_stylesheet_manager.hpp"
#include "esp_brookesia_phone_home.hpp"
#include "esp_brookesia_phone_manager.hpp"
#include "esp_brookesia_phone_app.hpp"

// *INDENT-OFF*

typedef struct {
    ESP_Brookesia_CoreData_t core;
    ESP_Brookesia_PhoneHomeData_t home;
    ESP_Brookesia_PhoneManagerData_t manager;
} ESP_Brookesia_PhoneStylesheet_t;

using ESP_Brookesia_PhoneStylesheetManager = ESP_Brookesia_CoreStylesheetManager<ESP_Brookesia_PhoneStylesheet_t>;

class ESP_Brookesia_Phone: public ESP_Brookesia_Core, public ESP_Brookesia_PhoneStylesheetManager {
public:
    ESP_Brookesia_Phone(lv_display_t *display = nullptr);
    ~ESP_Brookesia_Phone();

    int installApp(ESP_Brookesia_PhoneApp &app)    { return _core_manager.installApp(app); }
    int installApp(ESP_Brookesia_PhoneApp *app)    { return _core_manager.installApp(app); }
    int uninstallApp(ESP_Brookesia_PhoneApp &app)  { return _core_manager.uninstallApp(app); }
    int uninstallApp(ESP_Brookesia_PhoneApp *app)  { return _core_manager.uninstallApp(app); }
    bool uninstallApp(int id)               { return _core_manager.uninstallApp(id); }

    bool begin(void);
    bool del(void);
    bool addStylesheet(const ESP_Brookesia_PhoneStylesheet_t &stylesheet);
    bool addStylesheet(const ESP_Brookesia_PhoneStylesheet_t *stylesheet);
    bool activateStylesheet(const ESP_Brookesia_PhoneStylesheet_t &stylesheet);
    bool activateStylesheet(const ESP_Brookesia_PhoneStylesheet_t *stylesheet);

    bool calibrateScreenSize(ESP_Brookesia_StyleSize_t &size) override;

    ESP_Brookesia_PhoneHome &getHome(void)         { return _home; }
    ESP_Brookesia_PhoneManager &getManager(void)   { return _manager; }

private:
    bool calibrateStylesheet(const ESP_Brookesia_StyleSize_t &screen_size, ESP_Brookesia_PhoneStylesheet_t &sheetstyle) override;

    ESP_Brookesia_PhoneHome _home;
    ESP_Brookesia_PhoneManager _manager;

    static const ESP_Brookesia_PhoneStylesheet_t _default_stylesheet_dark;
};

// *INDENT-ON*
