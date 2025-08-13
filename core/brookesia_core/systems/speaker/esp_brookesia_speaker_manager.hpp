/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include "lvgl.h"
#include "esp_brookesia_systems_internal.h"
#include "systems/base/esp_brookesia_base_context.hpp"
#include "widgets/gesture/esp_brookesia_gesture.hpp"
#include "esp_brookesia_speaker_ai_buddy.hpp"
#include "esp_brookesia_speaker_display.hpp"
#include "esp_brookesia_speaker_app.hpp"

namespace esp_brookesia::systems::speaker {

class Manager: public base::Manager {
public:
    friend class Speaker;

    struct Data {
        GestureData gesture;
        int gesture_mask_indicator_trigger_time_ms;
        int ai_buddy_resume_time_ms;
        struct {
            int top_threshold;
            int bottom_threshold;
            int top_threshold_percent;
            int bottom_threshold_percent;
        } quick_settings;
        struct {
            int enable_gesture: 1;
            int enable_gesture_navigation_back: 1;
            int enable_app_launcher_gesture_navigation: 1;
            int enable_quick_settings_top_threshold_percent: 1;
            int enable_quick_settings_bottom_threshold_percent: 1;
        } flags;
    };

    enum class Screen {
        MAIN,
        APP,
        DRAW_DUMMY,
        MAX,
    };

    Manager(base::Context &core_in, Display &display_in, const Data &data_in);
    ~Manager();

    Manager(const Manager &) = delete;
    Manager(Manager &&) = delete;
    Manager &operator=(const Manager &) = delete;
    Manager &operator=(Manager &&) = delete;

    bool processQuickSettingsMoveTop();
    bool processQuickSettingsScrollTop();
    bool processQuickSettingsScrollBottom();
    bool processDisplayScreenChange(Screen screen, void *param);

    bool checkInitialized(void) const
    {
        return _flags.is_initialized;
    }
    Gesture *getGesture(void)
    {
        return _gesture.get();
    }

    static bool calibrateData(
        const gui::StyleSize screen_size, Display &display, Data &data
    );

    static constexpr const char *SETTINGS_VOLUME = "volume";
    static constexpr const char *SETTINGS_BRIGHTNESS = "brightness";
    static constexpr const char *SETTINGS_WLAN_SWITCH = "wlan_switch";
    static constexpr const char *SETTINGS_WLAN_SSID = "wlan_ssid";
    static constexpr const char *SETTINGS_WLAN_PASSWORD = "wlan_password";

    Display &display;
    const Data &data;

private:
    // Core
    bool processAppRunExtra(base::App *app) override;
    bool processAppResumeExtra(base::App *app) override;
    bool processAppCloseExtra(base::App *app) override;
    bool processNavigationEvent(base::Manager::NavigateType type) override;
    // Main
    bool begin(void);
    bool del(void);
    // Display
    bool processGestureScreenChange(Screen screen, void *param);
    bool processAI_BuddyResumeTimer(void);
    bool processAppLauncherGestureEvent(lv_event_t *event);
    bool processQuickSettingsEventSignal(QuickSettings::EventData event_data);
    bool processQuickSettingsStorageServiceEventSignal(std::string key);
    bool processQuickSettingsGesturePressEvent(lv_event_t *event);
    bool processQuickSettingsGesturePressingEvent(lv_event_t *event);
    bool processQuickSettingsGestureReleaseEvent(lv_event_t *event);
    // Navigation
    bool processNavigationGesturePressingEvent(lv_event_t *event);
    bool processNavigationGestureReleaseEvent(lv_event_t *event);
    // Mask & Indicator Bar
    bool processMaskIndicatorBarGesturePressingEvent(lv_event_t *event);
    bool processMaskIndicatorBarGestureReleaseEvent(lv_event_t *event);

    Screen getDisplayActiveScreen(void) const
    {
        return _display_active_screen;
    }

    // Flags
    struct {
        // Main
        int is_initialized: 1;
        // App Launcher
        int is_app_launcher_gesture_disabled: 1;
        // Quick Settings
        int is_quick_settings_enabled: 1;
        // Gesture
        int enable_gesture_navigation: 1;
        int enable_gesture_navigation_back: 1;
        int enable_gesture_navigation_home: 1;
        int enable_gesture_navigation_recents_app: 1;
        int is_gesture_navigation_disabled: 1;
        int enable_gesture_show_mask_left_right_edge: 1;
        int enable_gesture_show_mask_bottom_edge: 1;
        int enable_gesture_show_left_right_indicator_bar: 1;
        int enable_gesture_show_bottom_indicator_bar: 1;
    } _flags = {};
    // AI Buddy
    std::shared_ptr<AI_Buddy> _ai_buddy;
    // App Launcher
    GestureDirection _app_launcher_gesture_dir = GESTURE_DIR_NONE;
    // Display
    Screen _display_active_screen = Screen::MAX;
    // Gesture
    std::unique_ptr<Gesture> _gesture;
    // Timer
    gui::LvTimerUniquePtr _draw_dummy_timer;
    gui::LvTimerUniquePtr _quick_settings_update_clock_timer;
    gui::LvTimerUniquePtr _quick_settings_update_memory_timer;
};

/* Keep compatibility with old code */
using ManagerScreen [[deprecated("Use `esp_brookesia::systems::speaker::Manager::Screen` instead")]] =
    Manager::Screen;

} // namespace esp_brookesia::systems::speaker

/* Keep compatibility with old code */
#define ESP_BROOKESIA_SPEAKER_MANAGER_SCREEN_MAIN       esp_brookesia::systems::speaker::ManagerScreen::MAIN
#define ESP_BROOKESIA_SPEAKER_MANAGER_SCREEN_APP        esp_brookesia::systems::speaker::ManagerScreen::APP
#define ESP_BROOKESIA_SPEAKER_MANAGER_SCREEN_DRAW_DUMMY esp_brookesia::systems::speaker::ManagerScreen::DRAW_DUMMY
#define ESP_BROOKESIA_SPEAKER_MANAGER_SCREEN_MAX        esp_brookesia::systems::speaker::ManagerScreen::MAX
#define SETTINGS_NVS_KEY_VOLUME        esp_brookesia::systems::speaker::Manager::SETTINGS_VOLUME
#define SETTINGS_NVS_KEY_BRIGHTNESS    esp_brookesia::systems::speaker::Manager::SETTINGS_BRIGHTNESS
#define SETTINGS_NVS_KEY_WLAN_SWITCH   esp_brookesia::systems::speaker::Manager::SETTINGS_WLAN_SWITCH
#define SETTINGS_NVS_KEY_WLAN_SSID     esp_brookesia::systems::speaker::Manager::SETTINGS_WLAN_SSID
#define SETTINGS_NVS_KEY_WLAN_PASSWORD esp_brookesia::systems::speaker::Manager::SETTINGS_WLAN_PASSWORD
