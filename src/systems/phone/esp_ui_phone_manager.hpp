/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <memory>
#include "lvgl.h"
#include "core/esp_ui_core_manager.hpp"
#include "widgets/gesture/esp_ui_gesture.hpp"
#include "esp_ui_phone_home.hpp"
#include "esp_ui_phone_app.hpp"

// *INDENT-OFF*
class ESP_UI_PhoneManager: public ESP_UI_CoreManager {
public:
    friend class ESP_UI_Phone;

    ESP_UI_PhoneManager(ESP_UI_Core &core_in, ESP_UI_PhoneHome &home_in, const ESP_UI_PhoneManagerData_t &data_in);
    ~ESP_UI_PhoneManager();

    bool checkInitialized(void) const   { return _flags.is_initialized; }
    ESP_UI_Gesture *getGesture(void)    { return _gesture.get(); }

    static bool calibrateData(const ESP_UI_StyleSize_t screen_size, ESP_UI_PhoneHome &home,
                              ESP_UI_PhoneManagerData_t &data);

    ESP_UI_PhoneHome &home;
    const ESP_UI_PhoneManagerData_t &data;

private:
    // Core
    bool processAppRunExtra(ESP_UI_CoreApp *app) override;
    bool processAppResumeExtra(ESP_UI_CoreApp *app) override;
    bool processAppCloseExtra(ESP_UI_CoreApp *app) override;
    bool processNavigationEvent(ESP_UI_CoreNavigateType_t type) override;
    // Main
    bool begin(void);
    bool del(void);
    // Home
    bool processStatusBarScreenChange(ESP_UI_PhoneManagerScreen_t screen, void *param);
    bool processNavigationBarScreenChange(ESP_UI_PhoneManagerScreen_t screen, void *param);
    bool processGestureScreenChange(ESP_UI_PhoneManagerScreen_t screen, void *param);
    bool processHomeScreenChange(ESP_UI_PhoneManagerScreen_t screen, void *param);
    static void onHomeMainScreenLoadEventCallback(lv_event_t *event);
    // App Launcher
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
        // App Launcher
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
    } _flags;
    // Home
    ESP_UI_PhoneManagerScreen_t _home_active_screen;
    // App Launcher
    ESP_UI_GestureDirection_t _app_launcher_gesture_dir;
    // Navigation Bar
    ESP_UI_GestureDirection_t _navigation_bar_gesture_dir;
    // Gesture
    std::unique_ptr<ESP_UI_Gesture> _gesture;
    // RecentsScreen
    float _recents_screen_drag_tan_threshold;
    lv_point_t _recents_screen_start_point;
    lv_point_t _recents_screen_last_point;
    ESP_UI_CoreApp *_recents_screen_active_app;
    ESP_UI_CoreApp *_recents_screen_pause_app;
};
// *INDENT-OFF*
