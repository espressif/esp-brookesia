/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <memory>
#include "boost/signals2/signal.hpp"
#include "systems/base/esp_brookesia_base_context.hpp"
#include "widgets/app_launcher/esp_brookesia_app_launcher.hpp"
#include "widgets/quick_settings/esp_brookesia_speaker_quick_settings.hpp"
#include "widgets/keyboard/esp_brookesia_keyboard.hpp"
#include "anim_player/esp_brookesia_anim_player.hpp"

namespace esp_brookesia::systems::speaker {

class Display: public base::Display {
public:
    friend class Speaker;
    friend class Manager;

    struct Data {
        struct {
            gui::AnimPlayerData data;
        } boot_animation;
        struct {
            AppLauncherData data;
            gui::StyleImage default_image;
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

    using OnDummyDrawSignal = boost::signals2::signal<void(bool)>;

    Display(base::Context &core, const Data &data);
    ~Display();

    Display(const Display &) = delete;
    Display(Display &&) = delete;
    Display &operator=(const Display &) = delete;
    Display &operator=(Display &&) = delete;

    bool checkInitialized(void) const
    {
        return  _app_launcher.checkInitialized();
    }
    const Data &getData(void) const
    {
        return _data;
    }
    AppLauncher *getAppLauncher(void)
    {
        return &_app_launcher;
    }
    QuickSettings &getQuickSettings(void)
    {
        return _quick_settings;
    }
    Keyboard &getKeyboard(void)
    {
        return _keyboard;
    }
    gui::LvContainer *getDummyDrawMask(void)
    {
        return _dummy_draw_mask.get();
    }

    bool startBootAnimation(void);
    bool waitBootAnimationStop(void);

    bool calibrateData(const gui::StyleSize &screen_size, Data &data);

    static OnDummyDrawSignal on_dummy_draw_signal;

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

    bool processDummyDraw(bool is_visible);

    // Core
    const Data &_data;
    std::unique_ptr<gui::AnimPlayer> _boot_animation;
    gui::AnimPlayer::EventFuture _boot_animation_future;
    // Widgets
    AppLauncher _app_launcher;
    QuickSettings _quick_settings;
    Keyboard _keyboard;
    gui::LvContainerUniquePtr _dummy_draw_mask;
};

/* Keep compatibility with old code */
using DisplayData [[deprecated("Use `esp_brookesia::systems::speaker::Display::Data` instead")]] =
    Display::Data;

} // namespace esp_brookesia::systems::speaker
