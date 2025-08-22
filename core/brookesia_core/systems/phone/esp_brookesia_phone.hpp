/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <list>
#include <memory>
#include "esp_brookesia_systems_internal.h"
#include "gui/style/esp_brookesia_gui_stylesheet_manager.hpp"
#include "esp_brookesia_phone_display.hpp"
#include "esp_brookesia_phone_manager.hpp"
#include "esp_brookesia_phone_app.hpp"

namespace esp_brookesia::systems::phone {

struct Stylesheet {
    base::Context::Data core;
    Display::Data display;
    Manager::Data manager;
};

using StylesheetManager = gui::StylesheetManager<Stylesheet>;

class Phone: public base::Context, public StylesheetManager {
public:
    Phone(lv_display_t *display = nullptr);
    ~Phone();

    Phone(const Phone &) = delete;
    Phone(Phone &&) = delete;
    Phone &operator=(const Phone &) = delete;
    Phone &operator=(Phone &&) = delete;

    int installApp(App &app);
    int installApp(App *app);
    bool uninstallApp(App &app);
    bool uninstallApp(App *app);
    bool uninstallApp(int id);

    bool begin(void);
    bool del(void);

    bool addStylesheet(const Stylesheet &stylesheet);
    bool addStylesheet(const Stylesheet *stylesheet);
    bool activateStylesheet(const Stylesheet &stylesheet);
    bool activateStylesheet(const Stylesheet *stylesheet);

    bool calibrateScreenSize(gui::StyleSize &size) override;

    Display &getDisplay(void)
    {
        return _display;
    }
    Manager &getManager(void)
    {
        return _manager;
    }

private:
    bool calibrateStylesheet(const gui::StyleSize &screen_size, Stylesheet &sheetstyle) override;

    Display _display;
    Manager _manager;

    static const Stylesheet _default_stylesheet_dark;
};

} // namespace esp_brookesia::systems::phone

using ESP_Brookesia_PhoneStylesheet_t [[deprecated("Use `esp_brookesia::systems::phone::Stylesheet` instead")]] =
    esp_brookesia::systems::phone::Stylesheet;
using ESP_Brookesia_PhoneStylesheetManager [[deprecated("Use `esp_brookesia::systems::phone::StylesheetManager` instead")]] =
    esp_brookesia::systems::phone::StylesheetManager;
using ESP_Brookesia_Phone [[deprecated("Use `esp_brookesia::systems::phone::Phone` instead")]] =
    esp_brookesia::systems::phone::Phone;
