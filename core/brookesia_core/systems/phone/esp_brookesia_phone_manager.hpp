/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <memory>
#include "lvgl.h"
#include "systems/base/esp_brookesia_base_manager.hpp"
#include "systems/phone/widgets/gesture/esp_brookesia_gesture.hpp"
#include "esp_brookesia_phone_display.hpp"
#include "esp_brookesia_phone_app.hpp"

namespace esp_brookesia::systems::phone {

class Phone;

class Manager: public base::Manager {
public:
    friend class Phone;

    struct Data {
        Gesture::Data gesture;
        uint32_t gesture_mask_indicator_trigger_time_ms;
        struct {
            int drag_snapshot_y_step;
            int drag_snapshot_y_threshold;
            int drag_snapshot_angle_threshold;
            int delete_snapshot_y_threshold;
        } recents_screen;
        struct {
            uint8_t enable_gesture: 1;
            uint8_t enable_gesture_navigation_back: 1;
            uint8_t enable_recents_screen_snapshot_drag: 1;
            uint8_t enable_recents_screen_hide_when_no_snapshot: 1;
        } flags;
    };

    enum Screen {
        MAIN = 0,
        APP,
        RECENTS_SCREEN,
        MAX,
    };

    Manager(base::Context &core_in, Display &display_in, const Data &data_in);
    ~Manager();

    bool checkInitialized(void) const
    {
        return _flags.is_initialized;
    }
    Gesture *getGesture(void)
    {
        return _gesture.get();
    }

    static bool calibrateData(const gui::StyleSize &screen_size, Display &display, Data &data);

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
    bool processStatusBarScreenChange(Screen screen, void *param);
    bool processNavigationBarScreenChange(Screen screen, void *param);
    bool processGestureScreenChange(Screen screen, void *param);
    bool processDisplayScreenChange(Screen screen, void *param);
    static void onDisplayMainScreenLoadEventCallback(lv_event_t *event);
    // base::App Launcher
    static void onAppLauncherGestureEventCallback(lv_event_t *event);
    // Navigation Bar
    static void onNavigationBarGestureEventCallback(lv_event_t *event);
    // Gesture
    static void onGestureNavigationPressingEventCallback(lv_event_t *event);
    static void onGestureNavigationReleaseEventCallback(lv_event_t *event);
    static void onGestureMaskIndicatorPressingEventCallback(lv_event_t *event);
    static void onGestureMaskIndicatorReleaseEventCallback(lv_event_t *event);
    // Recents Screen
    bool processRecentsScreenShow(void);
    bool processRecentsScreenHide(void);
    bool processRecentsScreenMoveLeft(void);
    bool processRecentsScreenMoveRight(void);
    static void onRecentsScreenGesturePressEventCallback(lv_event_t *event);
    static void onRecentsScreenGesturePressingEventCallback(lv_event_t *event);
    static void onRecentsScreenGestureReleaseEventCallback(lv_event_t *event);
    static void onRecentsScreenSnapshotDeletedEventCallback(lv_event_t *event);

    // Flags
    struct {
        // Main
        uint8_t is_initialized: 1;
        // base::App Launcher
        uint8_t is_app_launcher_gesture_disabled: 1;
        // Navigation Bar
        uint8_t enable_navigation_bar_gesture: 1;
        uint8_t is_navigation_bar_gesture_disabled: 1;
        // Gesture
        uint8_t enable_gesture_navigation: 1;
        uint8_t enable_gesture_navigation_back: 1;
        uint8_t enable_gesture_navigation_home: 1;
        uint8_t enable_gesture_navigation_recents_app: 1;
        uint8_t is_gesture_navigation_disabled: 1;
        uint8_t enable_gesture_show_mask_left_right_edge: 1;
        uint8_t enable_gesture_show_mask_bottom_edge: 1;
        uint8_t enable_gesture_show_left_right_indicator_bar: 1;
        uint8_t enable_gesture_show_bottom_indicator_bar: 1;
        // Recents Screen
        uint8_t is_recents_screen_pressed: 1;
        uint8_t is_recents_screen_snapshot_move_hor: 1;
        uint8_t is_recents_screen_snapshot_move_ver: 1;
    } _flags = {};
    // Display
    Screen _display_active_screen = Screen::MAX;
    // base::App Launcher
    Gesture::Direction _app_launcher_gesture_dir = Gesture::DIR_NONE;
    // Navigation Bar
    Gesture::Direction _navigation_bar_gesture_dir = Gesture::DIR_NONE;
    // Gesture
    std::unique_ptr<Gesture> _gesture;
    // RecentsScreen
    float _recents_screen_drag_tan_threshold = 0;
    lv_point_t _recents_screen_start_point = {};
    lv_point_t _recents_screen_last_point = {};
    base::App *_recents_screen_active_app = nullptr;
    base::App *_recents_screen_pause_app = nullptr;
};

} // namespace esp_brookesia::systems::phone

using ESP_Brookesia_PhoneManagerData_t [[deprecated("Use `esp_brookesia::systems::phone::Manager::Data` instead")]] =
    esp_brookesia::systems::phone::Manager::Data;
using ESP_Brookesia_PhoneManager [[deprecated("Use `esp_brookesia::systems::phone::Manager` instead")]] =
    esp_brookesia::systems::phone::Manager;
using ESP_Brookesia_PhoneManagerScreen_t [[deprecated("Use `esp_brookesia::systems::phone::Manager::Screen` instead")]] =
    esp_brookesia::systems::phone::Manager::Screen;
#define ESP_BROOKESIA_PHONE_MANAGER_SCREEN_MAIN            ESP_Brookesia_PhoneManagerScreen_t::MAIN
#define ESP_BROOKESIA_PHONE_MANAGER_SCREEN_APP             ESP_Brookesia_PhoneManagerScreen_t::APP
#define ESP_BROOKESIA_PHONE_MANAGER_SCREEN_RECENTS_SCREEN  ESP_Brookesia_PhoneManagerScreen_t::RECENTS_SCREEN
#define ESP_BROOKESIA_PHONE_MANAGER_SCREEN_MAX             ESP_Brookesia_PhoneManagerScreen_t::MAX
