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
#include "systems/core/esp_brookesia_core.hpp"
#include "widgets/gesture/esp_brookesia_gesture.hpp"
#include "esp_brookesia_speaker_ai_buddy.hpp"
#include "esp_brookesia_speaker_display.hpp"
#include "esp_brookesia_speaker_app.hpp"

// *INDENT-OFF*

namespace esp_brookesia::speaker {

constexpr const char *SETTINGS_NVS_KEY_VOLUME = "volume";
constexpr const char *SETTINGS_NVS_KEY_BRIGHTNESS = "brightness";
constexpr const char *SETTINGS_NVS_KEY_WLAN_SWITCH = "wlan_switch";
constexpr const char *SETTINGS_NVS_KEY_WLAN_SSID = "wlan_ssid";
constexpr const char *SETTINGS_NVS_KEY_WLAN_PASSWORD = "wlan_password";

struct ManagerData {
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

enum ManagerScreen {
    ESP_BROOKESIA_SPEAKER_MANAGER_SCREEN_MAIN = 0,
    ESP_BROOKESIA_SPEAKER_MANAGER_SCREEN_APP,
    ESP_BROOKESIA_SPEAKER_MANAGER_SCREEN_DRAW_DUMMY,
    ESP_BROOKESIA_SPEAKER_MANAGER_SCREEN_MAX,
};

class Manager: public ESP_Brookesia_CoreManager {
public:
    friend class Speaker;

    Manager(ESP_Brookesia_Core &core_in, Display &display_in, const ManagerData &data_in);
    ~Manager();

    Manager(const Manager&) = delete;
    Manager(Manager&&) = delete;
    Manager& operator=(const Manager&) = delete;
    Manager& operator=(Manager&&) = delete;

    bool processQuickSettingsMoveTop();
    bool processQuickSettingsScrollTop();
    bool processQuickSettingsScrollBottom();
    bool processDisplayScreenChange(ManagerScreen screen, void *param);

    bool checkInitialized(void) const   { return _flags.is_initialized; }
    Gesture *getGesture(void)    { return _gesture.get(); }

    static bool calibrateData(
        const ESP_Brookesia_StyleSize_t screen_size, Display &display, ManagerData &data
    );

    Display &display;
    const ManagerData &data;

private:
    // Core
    bool processAppRunExtra(ESP_Brookesia_CoreApp *app) override;
    bool processAppResumeExtra(ESP_Brookesia_CoreApp *app) override;
    bool processAppCloseExtra(ESP_Brookesia_CoreApp *app) override;
    bool processNavigationEvent(ESP_Brookesia_CoreNavigateType_t type) override;
    // Main
    bool begin(void);
    bool del(void);
    // Display
    bool processGestureScreenChange(ManagerScreen screen, void *param);
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

    ManagerScreen getDisplayActiveScreen(void) const   { return _display_active_screen; }

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
    ManagerScreen _display_active_screen = ESP_BROOKESIA_SPEAKER_MANAGER_SCREEN_MAX;
    // Gesture
    std::unique_ptr<Gesture> _gesture;
    // Timer
    gui::LvTimerUniquePtr _draw_dummy_timer;
    gui::LvTimerUniquePtr _quick_settings_update_clock_timer;
    gui::LvTimerUniquePtr _quick_settings_update_memory_timer;
};
// *INDENT-OFF*

} // namespace esp_brookesia::speaker
