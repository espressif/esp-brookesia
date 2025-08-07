/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <memory>
#include "boost/signals2/signal.hpp"
#include "systems/core/esp_brookesia_core.hpp"
#include "widgets/app_launcher/esp_brookesia_app_launcher.hpp"
#include "widgets/quick_settings/esp_brookesia_speaker_quick_settings.hpp"
#include "widgets/keyboard/esp_brookesia_keyboard.hpp"
#include "anim_player/esp_brookesia_anim_player.hpp"

// *INDENT-OFF*

namespace esp_brookesia::speaker {

struct DisplayData {
    struct {
        gui::AnimPlayerData data;
    } boot_animation;
    struct {
        AppLauncherData data;
        ESP_Brookesia_StyleImage_t default_image;
    } app_launcher;
    struct {
        QuickSettingsData data;
    } quick_settings;
    struct {
        KeyboardData data;
    } keyboard;
    struct {
        uint8_t enable_app_launcher_flex_size: 1;
    } flags;
};

class Display: public ESP_Brookesia_CoreDisplay {
public:
    friend class Speaker;
    friend class Manager;

    using OnDummyDrawSignal = boost::signals2::signal<void(bool)>;

    Display(ESP_Brookesia_Core &core, const DisplayData &data);
    ~Display();

    Display(const Display&) = delete;
    Display(Display&&) = delete;
    Display& operator=(const Display&) = delete;
    Display& operator=(Display&&) = delete;

    bool checkInitialized(void) const               { return  _app_launcher.checkInitialized(); }
    const DisplayData &getData(void) const        { return _data; }
    AppLauncher *getAppLauncher(void)               { return &_app_launcher; }
    QuickSettings &getQuickSettings(void)           { return _quick_settings; }
    Keyboard &getKeyboard(void)                     { return _keyboard; }
    gui::LvContainer *getDummyDrawMask(void)        { return _dummy_draw_mask.get(); }

    bool startBootAnimation(void);
    bool waitBootAnimationStop(void);

    bool calibrateData(const ESP_Brookesia_StyleSize_t &screen_size, DisplayData &data);

    static OnDummyDrawSignal on_dummy_draw_signal;

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

    bool processDummyDraw(bool is_visible);

    // Core
    const DisplayData &_data;
    std::unique_ptr<gui::AnimPlayer> _boot_animation;
    gui::AnimPlayer::EventFuture _boot_animation_future;
    // Widgets
    AppLauncher _app_launcher;
    QuickSettings _quick_settings;
    Keyboard _keyboard;
    gui::LvContainerUniquePtr _dummy_draw_mask;
};
// *INDENT-OFF*

} // namespace esp_brookesia::speaker
