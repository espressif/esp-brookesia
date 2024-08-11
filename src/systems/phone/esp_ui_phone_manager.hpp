/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <memory>
#include "core/esp_ui_core_manager.hpp"
#include "widgets/gesture/esp_ui_gesture.hpp"
#include "esp_ui_phone_home.hpp"

class ESP_UI_Phone;

// *INDENT-OFF*
class ESP_UI_PhoneManager: public ESP_UI_CoreManager {
public:
    friend class ESP_UI_Phone;

    ESP_UI_PhoneManager(ESP_UI_Core &core, ESP_UI_PhoneHome &home, const ESP_UI_PhoneManagerData_t &data);
    ~ESP_UI_PhoneManager();

    bool checkInitialized(void) const   { return _is_initialized; }
    ESP_UI_Gesture *getGesture(void)    { return _gesture.get(); }

    static bool calibrateData(const ESP_UI_CoreData_t core_data, ESP_UI_PhoneManagerData_t &data);

protected:
    ESP_UI_PhoneHome &_home;
    const ESP_UI_PhoneManagerData_t &_data;

private:
    // Main
    bool begin(void);
    bool del(void);
    bool processAppRunExtra(ESP_UI_CoreApp *app) override;
    bool processAppResumeExtra(ESP_UI_CoreApp *app) override;
    bool processAppCloseExtra(ESP_UI_CoreApp *app) override;
    // Home
    static void onHomeMainScreenLoadEventCallback(lv_event_t *event);
    // App Launcher
    static void onAppLauncherGestureEventCallback(lv_event_t *event);
    // Navigation
    bool processNavigationEvent(ESP_UI_CoreNavigateType_t type) override;
    static void onNavigationGesturePressingEventCallback(lv_event_t *event);
    static void onNavigationGestureReleaseEventCallback(lv_event_t *event);
    // RecentsScreen
    bool processRecentsScreenShow(void);
    bool processRecentsScreenHide(void);
    bool processRecentsScreenMoveLeft(void);
    bool processRecentsScreenMoveRight(void);
    static void onRecentsScreenGesturePressEventCallback(lv_event_t *event);
    static void onRecentsScreenGesturePressingEventCallback(lv_event_t *event);
    static void onRecentsScreenGestureReleaseEventCallback(lv_event_t *event);
    static void onRecentsScreenSnapshotDeletedEventCallback(lv_event_t *event);

    // Main
    bool _is_initialized;
    // App Launcher
    // The following variables are used to prevent the app launcher from being operated when the app is closed
    bool _is_app_closed;
    bool _is_app_launcher_gesture_disabled;
    ESP_UI_GestureDirection_t _app_launcher_gesture_dir;
    // Navigation
    bool _enable_navigation_gesture;
    bool _is_navigation_gesture_released;
    std::unique_ptr<ESP_UI_Gesture> _gesture;
    // RecentsScreen
    bool _recents_screen_pressed;
    bool _recents_screen_snapshot_move_hor;
    bool _recents_screen_snapshot_move_ver;
    float _recents_screen_drag_tan_threshold;
    lv_point_t _recents_screen_start_point;
    lv_point_t _recents_screen_last_point;
    ESP_UI_CoreApp *_recents_screen_active_app;
};
// *INDENT-OFF*
