/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <list>
#include "core/esp_ui_template.hpp"
#include "esp_ui_phone_home.hpp"
#include "esp_ui_phone_manager.hpp"
#include "esp_ui_phone_app.hpp"
#include "esp_ui_phone_type.h"

using ESP_UI_TemplatePhone = ESP_UI_Template<ESP_UI_PhoneStylesheet_t>;

// *INDENT-OFF*
class ESP_UI_Phone: public ESP_UI_TemplatePhone {
public:
    ESP_UI_Phone(lv_disp_t *display = nullptr);
    ~ESP_UI_Phone();

    int installApp(ESP_UI_PhoneApp &app)    { return _core_manager.installApp(app); }
    int installApp(ESP_UI_PhoneApp *app)    { return _core_manager.installApp(app); }
    int uninstallApp(ESP_UI_PhoneApp &app)  { return _core_manager.uninstallApp(app); }
    int uninstallApp(ESP_UI_PhoneApp *app)  { return _core_manager.uninstallApp(app); }
    bool uninstallApp(int id)               { return _core_manager.uninstallApp(id); }

    bool begin(void);
    bool del(void);
    bool addStylesheet(const ESP_UI_PhoneStylesheet_t &stylesheet);
    bool addStylesheet(const ESP_UI_PhoneStylesheet_t *stylesheet);
    bool activateStylesheet(const ESP_UI_PhoneStylesheet_t &stylesheet);
    bool activateStylesheet(const ESP_UI_PhoneStylesheet_t *stylesheet);

    bool calibrateStylesheet(const ESP_UI_StyleSize_t &screen_size, ESP_UI_PhoneStylesheet_t &sheetstyle) override;

    ESP_UI_PhoneHome &getHome(void)         { return _home; }
    ESP_UI_PhoneManager &getManager(void)   { return _manager; }

private:
    ESP_UI_PhoneHome _home;
    ESP_UI_PhoneManager _manager;
};
// *INDENT-OFF*
