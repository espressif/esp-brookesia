/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <memory>
#include "systems/base/esp_brookesia_base_context.hpp"
#include "systems/phone/widgets/status_bar/esp_brookesia_status_bar.hpp"
#include "systems/phone/widgets/navigation_bar/esp_brookesia_navigation_bar.hpp"
#include "systems/phone/widgets/app_launcher/esp_brookesia_app_launcher.hpp"
#include "systems/phone/widgets/recents_screen/esp_brookesia_recents_screen.hpp"

namespace esp_brookesia::systems::phone {

class Display: public base::Display {
public:
    friend class Phone;
    friend class Manager;

    struct Data {
        struct {
            StatusBar::Data data;
            StatusBar::VisualMode visual_mode;
        } status_bar;
        struct {
            NavigationBar::Data data;
            NavigationBar::VisualMode visual_mode;
        } navigation_bar;
        struct {
            AppLauncherData data;
            gui::StyleImage default_image;
        } app_launcher;
        struct {
            RecentsScreen::Data data;
            StatusBar::VisualMode status_bar_visual_mode;
            NavigationBar::VisualMode navigation_bar_visual_mode;
        } recents_screen;
        struct {
            uint8_t enable_status_bar: 1;
            uint8_t enable_navigation_bar: 1;
            uint8_t enable_app_launcher_flex_size: 1;
            uint8_t enable_recents_screen: 1;
            uint8_t enable_recents_screen_flex_size: 1;
            uint8_t enable_recents_screen_hide_when_no_snapshot: 1; /* Deprecated, use flag in manager instead */
        } flags;
    };

    Display(base::Context &core, const Data &data);
    ~Display();

    bool checkInitialized(void) const
    {
        return  _app_launcher.checkInitialized();
    }
    const Data &getData(void) const
    {
        return _data;
    }
    StatusBar *getStatusBar(void)
    {
        return _status_bar.get();
    }
    NavigationBar *getNavigationBar(void)
    {
        return _navigation_bar.get();
    }
    RecentsScreen *getRecentsScreen(void)
    {
        return _recents_screen.get();
    }
    AppLauncher *getAppLauncher(void)
    {
        return &_app_launcher;
    }

    bool calibrateData(const gui::StyleSize &screen_size, Data &data);

private:
    bool begin(void);
    bool del(void);
    bool processAppInstall(base::App *app) override;
    bool processAppUninstall(base::App *app) override;
    bool processAppRun(base::App *app) override;
    bool processAppResume(base::App *app) override;
    bool processAppClose(base::App *app) override;
    bool processMainScreenLoad(void) override;
    bool getAppVisualArea(base::App *app, lv_area_t &app_visual_area) const override;

    bool processRecentsScreenShow(void);

    // Core
    const Data &_data;
    // Widgets
    AppLauncher _app_launcher;
    std::shared_ptr<StatusBar> _status_bar;
    std::shared_ptr<NavigationBar> _navigation_bar;
    std::shared_ptr<RecentsScreen> _recents_screen;
};

} // namespace esp_brookesia::systems::phone

using ESP_Brookesia_PhoneDisplayData_t [[deprecated("Use `esp_brookesia::systems::phone::Display::Data` instead")]] =
    esp_brookesia::systems::phone::Display::Data;
using ESP_Brookesia_PhoneDisplay [[deprecated("Use `esp_brookesia::systems::phone::Display` instead")]] =
    esp_brookesia::systems::phone::Display;
