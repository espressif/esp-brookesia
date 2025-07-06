/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <list>
#include "boost/thread.hpp"
#include "systems/core/esp_brookesia_core_stylesheet_manager.hpp"
#include "esp_brookesia_speaker_ai_buddy.hpp"
#include "esp_brookesia_speaker_display.hpp"
#include "esp_brookesia_speaker_manager.hpp"
#include "esp_brookesia_speaker_app.hpp"

namespace esp_brookesia::speaker {

typedef struct {
    ESP_Brookesia_CoreData_t core;
    DisplayData display;
    ManagerData manager;
    AI_BuddyData ai_buddy;
} SpeakerStylesheet_t;

using SpeakerStylesheet = ESP_Brookesia_CoreStylesheetManager<SpeakerStylesheet_t>;

// *INDENT-OFF*
class Speaker: public ESP_Brookesia_Core, public SpeakerStylesheet {
public:
    Speaker(lv_disp_t *display_device = nullptr);
    ~Speaker();

    Speaker(const Speaker&) = delete;
    Speaker(Speaker&&) = delete;
    Speaker& operator=(const Speaker&) = delete;
    Speaker& operator=(Speaker&&) = delete;

    int installApp(App &app)    { return _core_manager.installApp(app); }
    int installApp(App *app)    { return _core_manager.installApp(app); }
    int uninstallApp(App &app)  { return _core_manager.uninstallApp(app); }
    int uninstallApp(App *app)  { return _core_manager.uninstallApp(app); }
    bool uninstallApp(int id)               { return _core_manager.uninstallApp(id); }

    bool begin(void);
    bool del(void);
    bool addStylesheet(const SpeakerStylesheet_t &stylesheet);
    bool addStylesheet(const SpeakerStylesheet_t *stylesheet);
    bool activateStylesheet(const SpeakerStylesheet_t &stylesheet);
    bool activateStylesheet(const SpeakerStylesheet_t *stylesheet);

    bool calibrateScreenSize(ESP_Brookesia_StyleSize_t &size) override;

    Display display;
    Manager manager;

private:
    bool calibrateStylesheet(const ESP_Brookesia_StyleSize_t &screen_size, SpeakerStylesheet_t &sheetstyle) override;

    static const SpeakerStylesheet_t _default_stylesheet_dark;
};
// *INDENT-OFF*

} // namespace esp_brookesia::speaker
