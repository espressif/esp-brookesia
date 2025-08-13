/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <list>
#include "systems/base/esp_brookesia_base_context.hpp"
#include "gui/style/esp_brookesia_gui_stylesheet_manager.hpp"
#include "esp_brookesia_speaker_ai_buddy.hpp"
#include "esp_brookesia_speaker_display.hpp"
#include "esp_brookesia_speaker_manager.hpp"
#include "esp_brookesia_speaker_app.hpp"

namespace esp_brookesia::systems::speaker {

struct Stylesheet {
    base::Context::Data core;
    Display::Data display;
    Manager::Data manager;
    AI_Buddy::Data ai_buddy;
};

using StylesheetManager = gui::StylesheetManager<Stylesheet>;

class Speaker: public base::Context, public StylesheetManager {
public:
    Speaker(lv_disp_t *display_device = nullptr);
    ~Speaker();

    Speaker(const Speaker &) = delete;
    Speaker(Speaker &&) = delete;
    Speaker &operator=(const Speaker &) = delete;
    Speaker &operator=(Speaker &&) = delete;

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

    // static const Stylesheet _default_stylesheet_dark;
};

/* Keep compatibility with old code */
using SpeakerStylesheet_t [[deprecated("Use `esp_brookesia::systems::speaker::Stylesheet` instead")]] =
    esp_brookesia::systems::speaker::Stylesheet;

} // namespace esp_brookesia::systems::speaker

/* Keep compatibility with old code */
namespace esp_brookesia {
namespace speaker = systems::speaker;
}
