/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <list>
#include "core/esp_brookesia_template.hpp"
#include "esp_brookesia_phone_home.hpp"
#include "esp_brookesia_phone_manager.hpp"
#include "esp_brookesia_phone_app.hpp"
#include "esp_brookesia_phone_type.h"

#include "stylesheets/default/dark/stylesheet.h"
#include "stylesheets/320_240/dark/stylesheet.h"
#include "stylesheets/480_480/dark/stylesheet.h"
#include "stylesheets/800_480/dark/stylesheet.h"
#include "stylesheets/1024_600/dark/stylesheet.h"

using ESP_Brookesia_TemplatePhone = ESP_Brookesia_Template<ESP_Brookesia_PhoneStylesheet_t>;

// *INDENT-OFF*
class ESP_Brookesia_Phone: public ESP_Brookesia_TemplatePhone {
public:
    ESP_Brookesia_Phone(lv_disp_t *display = nullptr);
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

    bool calibrateStylesheet(const ESP_Brookesia_StyleSize_t &screen_size, ESP_Brookesia_PhoneStylesheet_t &sheetstyle) override;

    ESP_Brookesia_PhoneHome &getHome(void)         { return _home; }
    ESP_Brookesia_PhoneManager &getManager(void)   { return _manager; }

private:
    ESP_Brookesia_PhoneHome _home;
    ESP_Brookesia_PhoneManager _manager;

    static const ESP_Brookesia_PhoneStylesheet_t _default_stylesheet_dark;
};
// *INDENT-OFF*
