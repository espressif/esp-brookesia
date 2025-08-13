/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <cmath>
#include "esp_brookesia_systems_internal.h"
#if !ESP_BROOKESIA_PHONE_MANAGER_ENABLE_DEBUG_LOG
#   define ESP_BROOKESIA_UTILS_DISABLE_DEBUG_LOG
#endif
#include "private/esp_brookesia_phone_utils.hpp"
#include "esp_brookesia_phone_manager.hpp"
#include "esp_brookesia_phone.hpp"

using namespace std;
using namespace esp_brookesia::gui;

namespace esp_brookesia::systems::phone {

Manager::Manager(base::Context &core_in, Display &display_in, const Data &data_in)
    : base::Manager(core_in, core_in.getData().manager)
    , display(display_in)
    , data(data_in)
{
}

Manager::~Manager()
{
    ESP_UTILS_LOGD("Destroy(0x%p)", this);
    if (!del()) {
        ESP_UTILS_LOGE("Failed to delete");
    }
}

bool Manager::calibrateData(const gui::StyleSize &screen_size, Display &display, Data &data)
{
    ESP_UTILS_LOGD("Calibrate data");

    if (data.flags.enable_gesture) {
        ESP_UTILS_CHECK_FALSE_RETURN(Gesture::calibrateData(screen_size, display, data.gesture), false,
                                     "Calibrate gesture data failed");
    }

    return true;
}

bool Manager::begin(void)
{
    const RecentsScreen *recents_screen = display.getRecentsScreen();
    unique_ptr<Gesture> gesture = nullptr;
    lv_indev_t *touch = nullptr;

    ESP_UTILS_LOGD("Begin(@0x%p)", this);
    ESP_UTILS_CHECK_FALSE_RETURN(!checkInitialized(), false, "Already initialized");

    // base::Display
    lv_obj_add_event_cb(display.getMainScreen(), onDisplayMainScreenLoadEventCallback, LV_EVENT_SCREEN_LOADED, this);

    // Gesture
    if (data.flags.enable_gesture) {
        // Get the touch device
        touch = _system_context.getTouchDevice();
        if (touch == nullptr) {
            ESP_UTILS_LOGW("No touch device is set, try to use default touch device");

            touch = esp_brookesia_core_utils_get_input_dev(_system_context.getDisplayDevice(), LV_INDEV_TYPE_POINTER);
            ESP_UTILS_CHECK_NULL_RETURN(touch, false, "No touch device is initialized");
            ESP_UTILS_LOGW("Using default touch device(@0x%p)", touch);

            ESP_UTILS_CHECK_FALSE_RETURN(_system_context.setTouchDevice(touch), false, "Core set touch device failed");
        }

        // Create and begin gesture
        gesture = make_unique<Gesture>(_system_context, data.gesture);
        ESP_UTILS_CHECK_NULL_RETURN(gesture, false, "Create gesture failed");
        ESP_UTILS_CHECK_FALSE_RETURN(gesture->begin(display.getSystemScreenObject()), false, "Gesture begin failed");
        ESP_UTILS_CHECK_FALSE_RETURN(gesture->setMaskObjectVisible(false), false, "Hide mask object failed");
        ESP_UTILS_CHECK_FALSE_RETURN(
            gesture->setIndicatorBarVisible(Gesture::IndicatorBarType::LEFT, false),
            false, "Set left indicator bar visible failed"
        );
        ESP_UTILS_CHECK_FALSE_RETURN(
            gesture->setIndicatorBarVisible(Gesture::IndicatorBarType::RIGHT, false),
            false, "Set right indicator bar visible failed"
        );
        ESP_UTILS_CHECK_FALSE_RETURN(
            gesture->setIndicatorBarVisible(Gesture::IndicatorBarType::BOTTOM, true),
            false, "Set bottom indicator bar visible failed"
        );

        _flags.enable_gesture_navigation = true;
        // Navigation events
        lv_obj_add_event_cb(gesture->getEventObj(), onGestureNavigationPressingEventCallback,
                            gesture->getPressingEventCode(), this);
        lv_obj_add_event_cb(gesture->getEventObj(), onGestureNavigationReleaseEventCallback,
                            gesture->getReleaseEventCode(), this);
        // Mask object and indicator bar events
        lv_obj_add_event_cb(gesture->getEventObj(), onGestureMaskIndicatorPressingEventCallback,
                            gesture->getPressingEventCode(), this);
        lv_obj_add_event_cb(gesture->getEventObj(), onGestureMaskIndicatorReleaseEventCallback,
                            gesture->getReleaseEventCode(), this);
    }

    if (gesture != nullptr) {
        // App Launcher
        lv_obj_add_event_cb(gesture->getEventObj(), onAppLauncherGestureEventCallback,
                            gesture->getPressingEventCode(), this);
        lv_obj_add_event_cb(gesture->getEventObj(), onAppLauncherGestureEventCallback,
                            gesture->getReleaseEventCode(), this);

        // Navigation Bar
        if (display.getNavigationBar() != nullptr) {
            lv_obj_add_event_cb(gesture->getEventObj(), onNavigationBarGestureEventCallback,
                                gesture->getPressingEventCode(), this);
            lv_obj_add_event_cb(gesture->getEventObj(), onNavigationBarGestureEventCallback,
                                gesture->getReleaseEventCode(), this);
        }
    }

    // Recents Screen
    if (recents_screen != nullptr) {
        // Hide recents_screen by default
        ESP_UTILS_CHECK_FALSE_RETURN(recents_screen->setVisible(false), false, "Recents screen set visible failed");
        _recents_screen_drag_tan_threshold = tan(data.recents_screen.drag_snapshot_angle_threshold * M_PI / 180);
        lv_obj_add_event_cb(recents_screen->getEventObject(), onRecentsScreenSnapshotDeletedEventCallback,
                            recents_screen->getSnapshotDeletedEventCode(), this);
        // Register gesture event
        if (gesture != nullptr) {
            ESP_UTILS_LOGD("Enable recents_screen gesture");
            lv_obj_add_event_cb(gesture->getEventObj(), onRecentsScreenGesturePressEventCallback,
                                gesture->getPressEventCode(), this);
            lv_obj_add_event_cb(gesture->getEventObj(), onRecentsScreenGesturePressingEventCallback,
                                gesture->getPressingEventCode(), this);
            lv_obj_add_event_cb(gesture->getEventObj(), onRecentsScreenGestureReleaseEventCallback,
                                gesture->getReleaseEventCode(), this);
        }
    }

    if (gesture != nullptr) {
        _gesture = std::move(gesture);
    }
    _flags.is_initialized = true;

    ESP_UTILS_CHECK_FALSE_RETURN(processDisplayScreenChange(Screen::MAIN, nullptr), false,
                                 "Process screen change failed");

    return true;
}

bool Manager::del(void)
{
    lv_obj_t *temp_obj = nullptr;

    ESP_UTILS_LOGD("Delete phone manager(0x%p)", this);

    if (!checkInitialized()) {
        return true;
    }

    if (_gesture != nullptr) {
        _gesture.reset();
    }
    if (display.getRecentsScreen() != nullptr) {
        temp_obj = display.getRecentsScreen()->getEventObject();
        if (temp_obj != nullptr && checkLvObjIsValid(temp_obj)) {
            lv_obj_remove_event_cb(temp_obj, onRecentsScreenSnapshotDeletedEventCallback);
        }
    }
    _flags.is_initialized = false;
    _recents_screen_active_app = nullptr;

    return true;
}

bool Manager::processAppRunExtra(base::App *app)
{
    App *phone_app = static_cast<App *>(app);

    ESP_UTILS_CHECK_NULL_RETURN(phone_app, false, "Invalid phone app");
    ESP_UTILS_LOGD("Process app(%p) run extra", phone_app);

    ESP_UTILS_CHECK_FALSE_RETURN(processDisplayScreenChange(Screen::APP, phone_app), false,
                                 "Process screen change failed");

    return true;
}

bool Manager::processAppResumeExtra(base::App *app)
{
    App *phone_app = static_cast<App *>(app);

    ESP_UTILS_CHECK_NULL_RETURN(phone_app, false, "Invalid phone app");
    ESP_UTILS_LOGD("Process app(%p) resume extra", phone_app);

    ESP_UTILS_CHECK_FALSE_RETURN(processDisplayScreenChange(Screen::APP, phone_app), false,
                                 "Process screen change failed");

    return true;
}

bool Manager::processAppCloseExtra(base::App *app)
{
    App *phone_app = static_cast<App *>(app);

    ESP_UTILS_CHECK_NULL_RETURN(phone_app, false, "Invalid phone app");
    ESP_UTILS_LOGD("Process app(%p) close extra", phone_app);

    if (getActiveApp() == app) {
        // Switch to the main screen to release the app resources
        ESP_UTILS_CHECK_FALSE_RETURN(processDisplayScreenChange(Screen::MAIN, nullptr), false,
                                     "Process screen change failed");
        // If the recents_screen is visible, change back to the recents_screen
        if (display.getRecentsScreen()->checkVisible()) {
            ESP_UTILS_CHECK_FALSE_RETURN(processDisplayScreenChange(Screen::RECENTS_SCREEN, nullptr), false,
                                         "Process screen change failed");
        }
    }

    return true;
}

bool Manager::processDisplayScreenChange(Screen screen, void *param)
{
    ESP_UTILS_LOGD("Process Screen Change(%d)", screen);
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_UTILS_CHECK_FALSE_RETURN(screen < Screen::MAX, false, "Invalid screen");

    ESP_UTILS_CHECK_FALSE_RETURN(processStatusBarScreenChange(screen, param), false, "Process status bar failed");
    ESP_UTILS_CHECK_FALSE_RETURN(processNavigationBarScreenChange(screen, param), false, "Process navigation bar failed");
    ESP_UTILS_CHECK_FALSE_RETURN(processGestureScreenChange(screen, param), false, "Process gesture failed");

    if (screen == Screen::MAIN) {
        ESP_UTILS_CHECK_FALSE_RETURN(display.processMainScreenLoad(), false, "base::Display load main screen failed");
    }
    _display_active_screen = screen;

    return true;
}

bool Manager::processStatusBarScreenChange(Screen screen, void *param)
{
    StatusBar *status_bar = display._status_bar.get();
    StatusBar::VisualMode status_bar_visual_mode = StatusBar::VisualMode::HIDE;
    const App::Config *app_data = nullptr;

    ESP_UTILS_LOGD("Process status bar when screen change");
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_UTILS_CHECK_FALSE_RETURN(screen < Screen::MAX, false, "Invalid screen");

    if (status_bar == nullptr) {
        return true;
    }

    switch (screen) {
    case Screen::MAIN:
        status_bar_visual_mode = display.getData().status_bar.visual_mode;
        break;
    case Screen::APP:
        ESP_UTILS_CHECK_NULL_RETURN(param, false, "Invalid param");
        app_data = &((App *)param)->getActiveConfig();
        status_bar_visual_mode = app_data->status_bar_visual_mode;
        break;
    case Screen::RECENTS_SCREEN:
        status_bar_visual_mode = display.getData().recents_screen.status_bar_visual_mode;
        break;
    default:
        ESP_UTILS_CHECK_FALSE_RETURN(false, false, "Invalid screen");
        break;
    }
    ESP_UTILS_LOGD("Visual Mode: status bar(%d)", status_bar_visual_mode);

    ESP_UTILS_CHECK_FALSE_RETURN(status_bar->setVisualMode(status_bar_visual_mode), false,
                                 "Status bar set visual mode failed");

    return true;
}

bool Manager::processNavigationBarScreenChange(Screen screen, void *param)
{
    NavigationBar *navigation_bar = display._navigation_bar.get();
    NavigationBar::VisualMode navigation_bar_visual_mode = NavigationBar::VisualMode::HIDE;
    const App::Config *app_data = nullptr;

    ESP_UTILS_LOGD("Process navigation bar when screen change");
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_UTILS_CHECK_FALSE_RETURN(screen < Screen::MAX, false, "Invalid screen");

    // Process status bar
    if (navigation_bar == nullptr) {
        return true;
    }

    switch (screen) {
    case Screen::MAIN:
        navigation_bar_visual_mode = display.getData().navigation_bar.visual_mode;
        break;
    case Screen::APP:
        ESP_UTILS_CHECK_NULL_RETURN(param, false, "Invalid param");
        app_data = &((App *)param)->getActiveConfig();
        navigation_bar_visual_mode = app_data->navigation_bar_visual_mode;
        break;
    case Screen::RECENTS_SCREEN:
        navigation_bar_visual_mode = display.getData().recents_screen.navigation_bar_visual_mode;
        break;
    default:
        ESP_UTILS_CHECK_FALSE_RETURN(false, false, "Invalid screen");
        break;
    }
    ESP_UTILS_LOGD("Visual Mode: navigation bar(%d)", navigation_bar_visual_mode);

    _flags.enable_navigation_bar_gesture = (navigation_bar_visual_mode == NavigationBar::VisualMode::SHOW_FLEX);
    ESP_UTILS_CHECK_FALSE_RETURN(navigation_bar->setVisualMode(navigation_bar_visual_mode), false,
                                 "Navigation bar set visual mode failed");

    return true;
}

bool Manager::processGestureScreenChange(Screen screen, void *param)
{
    NavigationBar *navigation_bar = display._navigation_bar.get();
    NavigationBar::VisualMode navigation_bar_visual_mode = NavigationBar::VisualMode::HIDE;
    const App::Config *app_data = nullptr;

    ESP_UTILS_LOGD("Process gesture when screen change");
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_UTILS_CHECK_FALSE_RETURN(screen < Screen::MAX, false, "Invalid screen");

    switch (screen) {
    case Screen::MAIN:
        navigation_bar_visual_mode = display.getData().navigation_bar.visual_mode;
        _flags.enable_gesture_navigation = ((navigation_bar == nullptr) ||
                                            (navigation_bar_visual_mode == NavigationBar::VisualMode::HIDE));
        _flags.enable_gesture_navigation_back = false;
        _flags.enable_gesture_navigation_home = false;
        _flags.enable_gesture_navigation_recents_app = _flags.enable_gesture_navigation;
        _flags.enable_gesture_show_mask_left_right_edge = false;
        _flags.enable_gesture_show_mask_bottom_edge = (_flags.enable_gesture_navigation ||
                (navigation_bar_visual_mode == NavigationBar::VisualMode::SHOW_FLEX));
        _flags.enable_gesture_show_left_right_indicator_bar = false;
        _flags.enable_gesture_show_bottom_indicator_bar = _flags.enable_gesture_show_mask_bottom_edge;
        break;
    case Screen::APP:
        ESP_UTILS_CHECK_NULL_RETURN(param, false, "Invalid param");
        app_data = &((App *)param)->getActiveConfig();
        navigation_bar_visual_mode = app_data->navigation_bar_visual_mode;
        _flags.enable_gesture_navigation = (app_data->flags.enable_navigation_gesture &&
                                            (navigation_bar_visual_mode != NavigationBar::VisualMode::SHOW_FIXED));
        _flags.enable_gesture_navigation_back = (_flags.enable_gesture_navigation &&
                                                data.flags.enable_gesture_navigation_back);
        _flags.enable_gesture_navigation_home = (_flags.enable_gesture_navigation &&
                                                (navigation_bar_visual_mode == NavigationBar::VisualMode::HIDE));
        _flags.enable_gesture_navigation_recents_app = _flags.enable_gesture_navigation_home;
        _flags.enable_gesture_show_mask_left_right_edge = (_flags.enable_gesture_navigation ||
                (navigation_bar_visual_mode == NavigationBar::VisualMode::SHOW_FLEX));
        _flags.enable_gesture_show_mask_bottom_edge = (_flags.enable_gesture_navigation ||
                (navigation_bar_visual_mode == NavigationBar::VisualMode::SHOW_FLEX));
        _flags.enable_gesture_show_left_right_indicator_bar = _flags.enable_gesture_show_mask_left_right_edge;
        _flags.enable_gesture_show_bottom_indicator_bar = _flags.enable_gesture_show_mask_bottom_edge;
        break;
    case Screen::RECENTS_SCREEN:
        _flags.enable_gesture_navigation = false;
        _flags.enable_gesture_navigation_back = false;
        _flags.enable_gesture_navigation_home = false;
        _flags.enable_gesture_navigation_recents_app = false;
        _flags.enable_gesture_show_mask_left_right_edge = false;
        _flags.enable_gesture_show_mask_bottom_edge = false;
        _flags.enable_gesture_show_left_right_indicator_bar = false;
        _flags.enable_gesture_show_bottom_indicator_bar = false;
        break;
    default:
        ESP_UTILS_CHECK_FALSE_RETURN(false, false, "Invalid screen");
        break;
    }
    ESP_UTILS_LOGD("Gesture Navigation: all(%d), back(%d), display(%d), recents(%d)", _flags.enable_gesture_navigation,
                   _flags.enable_gesture_navigation_back, _flags.enable_gesture_navigation_home,
                   _flags.enable_gesture_navigation_recents_app);
    ESP_UTILS_LOGD("Gesture Mask & Indicator: mask(left_right: %d, bottom: %d), indicator_left_right(%d), "
                   "indicator_bottom(%d)", _flags.enable_gesture_show_mask_left_right_edge,
                   _flags.enable_gesture_show_mask_bottom_edge, _flags.enable_gesture_show_left_right_indicator_bar,
                   _flags.enable_gesture_show_bottom_indicator_bar);

    if (!_flags.enable_gesture_show_left_right_indicator_bar) {
        ESP_UTILS_CHECK_FALSE_RETURN(
            _gesture->setIndicatorBarVisible(Gesture::IndicatorBarType::LEFT, false), false,
            "Gesture set left indicator bar visible failed"
        );
        ESP_UTILS_CHECK_FALSE_RETURN(
            _gesture->setIndicatorBarVisible(Gesture::IndicatorBarType::RIGHT, false), false,
            "Gesture set right indicator bar visible failed"
        );
    }
    ESP_UTILS_CHECK_FALSE_RETURN(
        _gesture->setIndicatorBarVisible(
            Gesture::IndicatorBarType::BOTTOM, _flags.enable_gesture_show_bottom_indicator_bar
        ), false, "Gesture set bottom indicator bar visible failed"
    );

    return true;
}

void Manager::onDisplayMainScreenLoadEventCallback(lv_event_t *event)
{
    Manager *manager = nullptr;
    RecentsScreen *recents_screen = nullptr;

    ESP_UTILS_LOGD("base::Display main screen load event callback");
    ESP_UTILS_CHECK_NULL_EXIT(event, "Invalid event");

    manager = static_cast<Manager *>(lv_event_get_user_data(event));
    ESP_UTILS_CHECK_NULL_EXIT(manager, "Invalid manager");
    recents_screen = manager->display.getRecentsScreen();

    // Only process the screen change if the recents_screen is not visible
    if ((recents_screen == nullptr) || !recents_screen->checkVisible()) {
        ESP_UTILS_CHECK_FALSE_EXIT(
            manager->processStatusBarScreenChange(Screen::MAIN, nullptr),
            "Process status bar failed");
        ESP_UTILS_CHECK_FALSE_EXIT(
            manager->processNavigationBarScreenChange(Screen::MAIN, nullptr),
            "Process navigation bar failed");
        ESP_UTILS_CHECK_FALSE_EXIT(
            manager->processGestureScreenChange(Screen::MAIN, nullptr),
            "Process gesture failed");
    }
}

void Manager::onAppLauncherGestureEventCallback(lv_event_t *event)
{
    lv_event_code_t event_code = _LV_EVENT_LAST;
    Manager *manager = nullptr;
    AppLauncher *app_launcher = nullptr;
    RecentsScreen *recents_screen = nullptr;
    Gesture *gesture = nullptr;
    Gesture::Info *gesture_info = nullptr;
    Gesture::Direction dir_type = Gesture::DIR_NONE;

    // ESP_UTILS_LOGD("base::App launcher gesture event callback");
    ESP_UTILS_CHECK_NULL_GOTO(event, end, "Invalid event");

    manager = static_cast<Manager *>(lv_event_get_user_data(event));
    ESP_UTILS_CHECK_NULL_GOTO(manager, end, "Invalid manager");
    gesture = manager->_gesture.get();
    ESP_UTILS_CHECK_NULL_GOTO(gesture, end, "Invalid gesture");
    recents_screen = manager->display._recents_screen.get();
    app_launcher = &manager->display._app_launcher;
    ESP_UTILS_CHECK_NULL_GOTO(app_launcher, end, "Invalid app launcher");
    event_code = lv_event_get_code(event);
    ESP_UTILS_CHECK_FALSE_GOTO(
        (event_code == gesture->getPressingEventCode()) || (event_code == gesture->getReleaseEventCode()), end,
        "Invalid event code"
    );

    // Here is to prevent detecting gestures when the app exits, which could trigger unexpected behaviors
    if (event_code == gesture->getReleaseEventCode()) {
        if (manager->_flags.is_app_launcher_gesture_disabled) {
            manager->_flags.is_app_launcher_gesture_disabled = false;
            return;
        }
    }

    // Check if the app launcher and recents_screen are visible
    if (!app_launcher->checkVisible() || manager->_flags.is_app_launcher_gesture_disabled ||
            ((recents_screen != nullptr) && recents_screen->checkVisible())) {
        return;
    }

    dir_type = manager->_app_launcher_gesture_dir;
    // Check if the dir type is already set. If so, just ignore and return
    if (dir_type != Gesture::DIR_NONE) {
        // Check if the gesture is released
        if (event_code == gesture->getReleaseEventCode()) {   // If so, reset the navigation type
            dir_type = Gesture::DIR_NONE;
            goto end;
        }
        return;
    }

    gesture_info = (Gesture::Info *)lv_event_get_param(event);
    ESP_UTILS_CHECK_NULL_GOTO(gesture_info, end, "Invalid gesture info");
    // Check if there is a gesture
    if (gesture_info->direction == Gesture::DIR_NONE) {
        return;
    }

    dir_type = gesture_info->direction;
    switch (dir_type) {
    case Gesture::DIR_LEFT:
        ESP_UTILS_LOGD("base::App table gesture left");
        ESP_UTILS_CHECK_FALSE_GOTO(app_launcher->scrollToRightPage(), end, "base::App table scroll to right page failed");
        break;
    case Gesture::DIR_RIGHT:
        ESP_UTILS_LOGD("base::App table gesture right");
        ESP_UTILS_CHECK_FALSE_GOTO(app_launcher->scrollToLeftPage(), end, "base::App table scroll to left page failed");
        break;
    default:
        break;
    }

end:
    manager->_app_launcher_gesture_dir = dir_type;
}

void Manager::onNavigationBarGestureEventCallback(lv_event_t *event)
{
    lv_event_code_t event_code = _LV_EVENT_LAST;
    Manager *manager = nullptr;
    NavigationBar *navigation_bar = nullptr;
    Gesture::Info *gesture_info = nullptr;
    Gesture::Direction dir_type = Gesture::DIR_NONE;

    // ESP_UTILS_LOGD("Navigation bar gesture event callback");
    ESP_UTILS_CHECK_NULL_EXIT(event, "Invalid event");

    manager = static_cast<Manager *>(lv_event_get_user_data(event));
    ESP_UTILS_CHECK_NULL_EXIT(manager, "Invalid manager");
    navigation_bar = manager->display.getNavigationBar();
    ESP_UTILS_CHECK_NULL_EXIT(navigation_bar, "Invalid navigation bar");
    event_code = lv_event_get_code(event);
    ESP_UTILS_CHECK_FALSE_EXIT((event_code == manager->_gesture->getPressingEventCode()) ||
                               (event_code == manager->_gesture->getReleaseEventCode()), "Invalid event code");

    // Here is to prevent detecting gestures when the app exits, which could trigger unexpected behaviors
    if (manager->_flags.is_navigation_bar_gesture_disabled && (event_code == manager->_gesture->getReleaseEventCode())) {
        manager->_flags.is_navigation_bar_gesture_disabled = false;
        return;
    }

    // Check if the gesture is enabled or the app is running
    if (manager->_flags.is_navigation_bar_gesture_disabled || (!manager->_flags.enable_navigation_bar_gesture)) {
        return;
    }

    dir_type = manager->_navigation_bar_gesture_dir;
    // Check if the dir type is already set. If so, just ignore and return
    if (dir_type != Gesture::DIR_NONE) {
        // Check if the gesture is released
        if (event_code == manager->_gesture->getReleaseEventCode()) {   // If so, reset the navigation type
            dir_type = Gesture::DIR_NONE;
            goto end;
        }
        return;
    }

    gesture_info = (Gesture::Info *)lv_event_get_param(event);
    ESP_UTILS_CHECK_NULL_EXIT(gesture_info, "Invalid gesture info");

    // Check if there is a valid gesture
    dir_type = gesture_info->direction;
    if ((dir_type == Gesture::DIR_UP) &&
            (gesture_info->start_area & Gesture::AREA_BOTTOM_EDGE)) {
        ESP_UTILS_LOGD("Navigation bar gesture up");
        ESP_UTILS_CHECK_FALSE_EXIT(navigation_bar->triggerVisualFlexShow(), "Navigation bar trigger visual flex show failed");
    }

end:
    manager->_navigation_bar_gesture_dir = dir_type;
}

bool Manager::processNavigationEvent(base::Manager::NavigateType type)
{
    bool ret = true;
    RecentsScreen *recents_screen = display._recents_screen.get();
    App *active_app = static_cast<App *>(getActiveApp());
    App *phone_app = nullptr;

    ESP_UTILS_LOGD("Process navigation event type(%d)", type);

    // Disable the gesture function of widgets
    _flags.is_app_launcher_gesture_disabled = true;
    _flags.is_navigation_bar_gesture_disabled = true;

    // Check if the recents_screen is visible
    if ((recents_screen != nullptr) && recents_screen->checkVisible()) {
        // Hide if the recents_screen is visible
        if (!processRecentsScreenHide()) {
            ESP_UTILS_LOGE("Hide recents_screen failed");
            ret = false;
        }
        // Directly return if the type is not display
        if (type != base::Manager::NavigateType::HOME) {
            return ret;
        }
    }

    switch (type) {
    case base::Manager::NavigateType::BACK:
        if (active_app == nullptr) {
            goto end;
        }
        // Call app back function
        ESP_UTILS_CHECK_FALSE_GOTO(ret = (active_app->back()), end, "base::App(%d) back failed", active_app->getId());
        break;
    case base::Manager::NavigateType::HOME:
        if (active_app == nullptr) {
            goto end;
        }
        // Process app pause
        ESP_UTILS_CHECK_FALSE_GOTO(ret = processAppPause(active_app), end, "base::App(%d) pause failed", active_app->getId());
        ESP_UTILS_CHECK_FALSE_RETURN(processDisplayScreenChange(Screen::MAIN, nullptr), false,
                                     "Process screen change failed");
        resetActiveApp();
        break;
    case base::Manager::NavigateType::RECENTS_SCREEN:
        if (recents_screen == nullptr) {
            ESP_UTILS_LOGW("Recents screen is disabled");
            goto end;
        }
        // Save the active app and pause it
        if (active_app != nullptr) {
            ret = processAppPause(active_app);
            ESP_UTILS_CHECK_FALSE_GOTO(ret, end, "Process app pause failed");
        }
        _recents_screen_pause_app = active_app;
        // Show recents_screen
        ESP_UTILS_CHECK_FALSE_GOTO(ret = processRecentsScreenShow(), end, "Process recents_screen show failed");
        // Get the active app for recents screen, if the active app is not set, set the last app as the active app
        _recents_screen_active_app = active_app != nullptr ? active_app : getRunningAppByIdenx(getRunningAppCount() - 1);
        // Scroll to the active app
        if ((_recents_screen_active_app != nullptr) &&
                !recents_screen->scrollToSnapshotById(_recents_screen_active_app->getId())) {
            ESP_UTILS_LOGE("Recents screen scroll to snapshot(%d) failed", _recents_screen_active_app->getId());
            ret = false;
        }
        // Update the snapshot, need to be called after `processAppPause()`
        for (int i = 0; i < getRunningAppCount(); i++) {
            phone_app = static_cast<App *>(getRunningAppByIdenx(i));
            ESP_UTILS_CHECK_FALSE_GOTO(ret = (phone_app != nullptr), end, "Invalid active app");

            // Update snapshot conf and image
            ESP_UTILS_CHECK_FALSE_GOTO(ret = phone_app->updateRecentsScreenSnapshotConf(getAppSnapshot(phone_app->getId())),
                                       end, "base::App update snapshot(%d) conf failed", phone_app->getId());
            ESP_UTILS_CHECK_FALSE_GOTO(ret = recents_screen->updateSnapshotImage(phone_app->getId()), end,
                                       "Recents screen update snapshot(%d) image failed", phone_app->getId());
        }
        break;
    default:
        break;
    }

end:
    return ret;
}

void Manager::onGestureNavigationPressingEventCallback(lv_event_t *event)
{
    Manager *manager = nullptr;
    Gesture::Info *gesture_info = nullptr;
    base::Manager::NavigateType navigation_type = base::Manager::NavigateType::MAX;

    // ESP_UTILS_LOGD("Gesture navigation pressing event callback");
    ESP_UTILS_CHECK_NULL_EXIT(event, "Invalid event");

    manager = static_cast<Manager *>(lv_event_get_user_data(event));
    ESP_UTILS_CHECK_NULL_EXIT(manager, "Invalid manager");
    // Check if the gesture is released and enabled
    if (!manager->_flags.enable_gesture_navigation || manager->_flags.is_gesture_navigation_disabled) {
        return;
    }

    gesture_info = (Gesture::Info *)lv_event_get_param(event);
    ESP_UTILS_CHECK_NULL_EXIT(gesture_info, "Invalid gesture info");
    // Check if there is a gesture
    if (gesture_info->direction == Gesture::DIR_NONE) {
        return;
    }

    // Check if there is a "back" gesture
    if ((gesture_info->start_area & (Gesture::AREA_LEFT_EDGE | Gesture::AREA_RIGHT_EDGE)) &&
            (gesture_info->direction & Gesture::DIR_HOR) && manager->_flags.enable_gesture_navigation_back) {
        navigation_type = base::Manager::NavigateType::BACK;
    } else if ((gesture_info->start_area & Gesture::AREA_BOTTOM_EDGE) && (!gesture_info->flags.short_duration) &&
               (gesture_info->direction & Gesture::DIR_UP) && manager->_flags.enable_gesture_navigation_recents_app) {
        // Check if there is a "recents_screen" gesture
        navigation_type = base::Manager::NavigateType::RECENTS_SCREEN;
    }

    // Only process the navigation event if the navigation type is valid
    if (navigation_type != base::Manager::NavigateType::MAX) {
        manager->_flags.is_gesture_navigation_disabled = true;
        ESP_UTILS_CHECK_FALSE_EXIT(manager->processNavigationEvent(navigation_type), "Process navigation event failed");
    }
}

void Manager::onGestureNavigationReleaseEventCallback(lv_event_t *event)
{
    Manager *manager = nullptr;
    Gesture::Info *gesture_info = nullptr;
    base::Manager::NavigateType navigation_type = base::Manager::NavigateType::MAX;

    ESP_UTILS_LOGD("Gesture navigation release event callback");
    ESP_UTILS_CHECK_NULL_EXIT(event, "Invalid event");

    manager = static_cast<Manager *>(lv_event_get_user_data(event));
    ESP_UTILS_CHECK_NULL_EXIT(manager, "Invalid manager");
    manager->_flags.is_gesture_navigation_disabled = false;
    // Check if the gesture is released and enabled
    if (!manager->_flags.enable_gesture_navigation) {
        return;
    }

    gesture_info = (Gesture::Info *)lv_event_get_param(event);
    ESP_UTILS_CHECK_NULL_EXIT(gesture_info, "Invalid gesture info");
    // Check if there is a gesture
    if (gesture_info->direction == Gesture::DIR_NONE) {
        return;
    }

    // Check if there is a "display" gesture
    if ((gesture_info->start_area & Gesture::AREA_BOTTOM_EDGE) && (gesture_info->flags.short_duration) &&
            (gesture_info->direction & Gesture::DIR_UP) && manager->_flags.enable_gesture_navigation_home) {
        navigation_type = base::Manager::NavigateType::HOME;
    }

    // Only process the navigation event if the navigation type is valid
    if (navigation_type != base::Manager::NavigateType::MAX) {
        ESP_UTILS_CHECK_FALSE_EXIT(manager->processNavigationEvent(navigation_type), "Process navigation event failed");
    }
}

void Manager::onGestureMaskIndicatorPressingEventCallback(lv_event_t *event)
{
    bool is_gesture_mask_enabled = false;
    int gesture_indicator_offset = 0;
    Manager *manager = nullptr;
    NavigationBar *navigation_bar = nullptr;
    Gesture *gesture = nullptr;
    Gesture::Info *gesture_info = nullptr;
    Gesture::IndicatorBarType gesture_indicator_bar_type = Gesture::IndicatorBarType::MAX;

    ESP_UTILS_CHECK_NULL_EXIT(event, "Invalid event");

    manager = static_cast<Manager *>(lv_event_get_user_data(event));
    ESP_UTILS_CHECK_NULL_EXIT(manager, "Invalid manager");
    gesture = manager->getGesture();
    ESP_UTILS_CHECK_NULL_EXIT(gesture, "Invalid gesture");
    gesture_info = (Gesture::Info *)lv_event_get_param(event);
    ESP_UTILS_CHECK_NULL_EXIT(gesture_info, "Invalid gesture info");
    navigation_bar = manager->display.getNavigationBar();

    // Just return if the navigation bar is visible or the gesture duration is less than the trigger time
    if (((navigation_bar != nullptr) && navigation_bar->checkVisible()) ||
            (gesture_info->duration_ms < manager->data.gesture_mask_indicator_trigger_time_ms)) {
        return;
    }

    // Get the type of the indicator bar and the offset of the gesture
    switch (gesture_info->start_area) {
    case Gesture::AREA_LEFT_EDGE:
        if (manager->_flags.enable_gesture_show_left_right_indicator_bar) {
            gesture_indicator_bar_type = Gesture::IndicatorBarType::LEFT;
            gesture_indicator_offset = gesture_info->stop_x - gesture_info->start_x;
        }
        is_gesture_mask_enabled = manager->_flags.enable_gesture_show_mask_left_right_edge;
        break;
    case Gesture::AREA_RIGHT_EDGE:
        if (manager->_flags.enable_gesture_show_left_right_indicator_bar) {
            gesture_indicator_bar_type = Gesture::IndicatorBarType::RIGHT;
            gesture_indicator_offset = gesture_info->start_x - gesture_info->stop_x;
        }
        is_gesture_mask_enabled = manager->_flags.enable_gesture_show_mask_left_right_edge;
        break;
    case Gesture::AREA_BOTTOM_EDGE:
        if (manager->_flags.enable_gesture_show_bottom_indicator_bar) {
            gesture_indicator_bar_type = Gesture::IndicatorBarType::BOTTOM;
            gesture_indicator_offset = gesture_info->start_y - gesture_info->stop_y;
        }
        is_gesture_mask_enabled = manager->_flags.enable_gesture_show_mask_bottom_edge;
        break;
    default:
        break;
    }

    // If the gesture indicator bar type is valid, update the indicator bar
    if (static_cast<int>(gesture_indicator_bar_type) < static_cast<int>(Gesture::IndicatorBarType::MAX)) {
        if (gesture->checkIndicatorBarVisible(gesture_indicator_bar_type)) {
            ESP_UTILS_CHECK_FALSE_EXIT(
                gesture->setIndicatorBarLengthByOffset(gesture_indicator_bar_type, gesture_indicator_offset),
                "Gesture set bottom indicator bar length by offset failed"
            );
        } else {
            if (gesture->checkIndicatorBarScaleBackAnimRunning(gesture_indicator_bar_type)) {
                ESP_UTILS_CHECK_FALSE_EXIT(
                    gesture->controlIndicatorBarScaleBackAnim(gesture_indicator_bar_type, false),
                    "Gesture control indicator bar scale back anim failed"
                );
            }
            ESP_UTILS_CHECK_FALSE_EXIT(
                gesture->setIndicatorBarVisible(gesture_indicator_bar_type, true),
                "Gesture set indicator bar visible failed"
            );
        }
    }

    // If the gesture mask is enabled, show the mask object
    if (is_gesture_mask_enabled && !gesture->checkMaskVisible()) {
        ESP_UTILS_CHECK_FALSE_EXIT(gesture->setMaskObjectVisible(true), "Gesture show mask object failed");
    }
}

void Manager::onGestureMaskIndicatorReleaseEventCallback(lv_event_t *event)
{
    Manager *manager = nullptr;
    Gesture *gesture = nullptr;
    Gesture::Info *gesture_info = nullptr;
    Gesture::IndicatorBarType gesture_indicator_bar_type = Gesture::IndicatorBarType::MAX;

    ESP_UTILS_CHECK_NULL_EXIT(event, "Invalid event");

    manager = static_cast<Manager *>(lv_event_get_user_data(event));
    ESP_UTILS_CHECK_NULL_EXIT(manager, "Invalid manager");
    gesture = manager->getGesture();
    ESP_UTILS_CHECK_NULL_EXIT(gesture, "Invalid gesture");
    gesture_info = (Gesture::Info *)lv_event_get_param(event);
    ESP_UTILS_CHECK_NULL_EXIT(gesture_info, "Invalid gesture info");

    // Update the mask object and indicator bar of the gesture
    ESP_UTILS_CHECK_FALSE_EXIT(gesture->setMaskObjectVisible(false), "Gesture hide mask object failed");
    switch (gesture_info->start_area) {
    case Gesture::AREA_LEFT_EDGE:
        gesture_indicator_bar_type = Gesture::IndicatorBarType::LEFT;
        break;
    case Gesture::AREA_RIGHT_EDGE:
        gesture_indicator_bar_type = Gesture::IndicatorBarType::RIGHT;
        break;
    case Gesture::AREA_BOTTOM_EDGE:
        gesture_indicator_bar_type = Gesture::IndicatorBarType::BOTTOM;
        break;
    default:
        break;
    }
    if (static_cast<int>(gesture_indicator_bar_type) < static_cast<int>(Gesture::IndicatorBarType::MAX) &&
            (gesture->checkIndicatorBarVisible(gesture_indicator_bar_type))) {
        ESP_UTILS_CHECK_FALSE_EXIT(
            gesture->controlIndicatorBarScaleBackAnim(gesture_indicator_bar_type, true),
            "Gesture control indicator bar scale back anim failed"
        );
    }
}

bool Manager::processRecentsScreenShow(void)
{
    ESP_UTILS_LOGD("Process recents_screen show");

    ESP_UTILS_CHECK_FALSE_RETURN(display.processRecentsScreenShow(), false, "Load recents_screen failed");
    ESP_UTILS_CHECK_FALSE_RETURN(processDisplayScreenChange(Screen::RECENTS_SCREEN, nullptr), false,
                                 "Process screen change failed");

    return true;
}

bool Manager::processRecentsScreenHide(void)
{
    RecentsScreen *recents_screen = display.getRecentsScreen();
    App *active_app = static_cast<App *>(getActiveApp());

    ESP_UTILS_LOGD("Process recents_screen hide");
    ESP_UTILS_CHECK_NULL_RETURN(recents_screen, false, "Invalid recents_screen");
    ESP_UTILS_CHECK_FALSE_RETURN(recents_screen->setVisible(false), false, "Hide recents_screen failed");

    // Load the main screen if there is no active app
    if (active_app == nullptr) {
        ESP_UTILS_CHECK_FALSE_RETURN(processDisplayScreenChange(Screen::MAIN, nullptr), false,
                                     "Process screen change failed");
    }

    return true;
}

bool Manager::processRecentsScreenMoveLeft(void)
{
    int recents_screen_active_app_index = getRunningAppIndexByApp(_recents_screen_active_app);
    RecentsScreen *recents_screen = display._recents_screen.get();

    ESP_UTILS_LOGD("Process recents_screen move left");
    ESP_UTILS_CHECK_NULL_RETURN(recents_screen, false, "Invalid recents_screen");
    ESP_UTILS_CHECK_FALSE_RETURN(recents_screen_active_app_index >= 0, false, "Invalid recents_screen active app index");

    // Check if the active app is at the leftmost
    if (++recents_screen_active_app_index >= getRunningAppCount()) {
        ESP_UTILS_LOGD("Recents screen snapshot is at the rightmost");
        return true;
    }

    ESP_UTILS_LOGD("Recents screen scroll snapshot(%d) left(%d)", _recents_screen_active_app->getId(),
                   recents_screen_active_app_index);
    // Move the snapshot to the left
    ESP_UTILS_CHECK_FALSE_RETURN(recents_screen->scrollToSnapshotByIndex(recents_screen_active_app_index), false,
                                 "Recents screen scroll snapshot left failed");
    // Update the active app
    _recents_screen_active_app = getRunningAppByIdenx(recents_screen_active_app_index);

    return true;
}

bool Manager::processRecentsScreenMoveRight(void)
{
    int recents_screen_active_app_index = getRunningAppIndexByApp(_recents_screen_active_app);
    RecentsScreen *recents_screen = display._recents_screen.get();

    ESP_UTILS_LOGD("Process recents_screen move right");
    ESP_UTILS_CHECK_NULL_RETURN(recents_screen, false, "Invalid recents_screen");
    ESP_UTILS_CHECK_FALSE_RETURN(recents_screen_active_app_index >= 0, false, "Invalid recents_screen active app index");

    // Check if the active app is at the rightmost
    if (--recents_screen_active_app_index < 0) {
        ESP_UTILS_LOGD("Recents screen snapshot is at the leftmost");
        return true;
    }

    ESP_UTILS_LOGD("Recents screen scroll snapshot(%d) right(%d)", _recents_screen_active_app->getId(),
                   recents_screen_active_app_index);
    // Move the snapshot to the right
    ESP_UTILS_CHECK_FALSE_RETURN(recents_screen->scrollToSnapshotByIndex(recents_screen_active_app_index), false,
                                 "Recents screen scroll snapshot right failed");
    // Update the active app
    _recents_screen_active_app = getRunningAppByIdenx(recents_screen_active_app_index);

    return true;
}

void Manager::onRecentsScreenGesturePressEventCallback(lv_event_t *event)
{
    Manager *manager = nullptr;
    const RecentsScreen *recents_screen = nullptr;
    Gesture::Info *gesture_info = nullptr;
    lv_point_t start_point = {};

    ESP_UTILS_CHECK_NULL_EXIT(event, "Invalid event");

    manager = static_cast<Manager *>(lv_event_get_user_data(event));
    ESP_UTILS_CHECK_NULL_EXIT(manager, "Invalid manager");

    recents_screen = manager->display.getRecentsScreen();
    ESP_UTILS_CHECK_NULL_EXIT(recents_screen, "Invalid recents_screen");

    // Check if recents_screen is visible
    if (!recents_screen->checkVisible()) {
        return;
    }

    gesture_info = (Gesture::Info *)lv_event_get_param(event);
    ESP_UTILS_CHECK_NULL_EXIT(gesture_info, "Invalid gesture info");

    start_point = (lv_point_t) {
        (lv_coord_t)gesture_info->start_x, (lv_coord_t)gesture_info->start_y
    };

    // Check if the start point is inside the recents_screen
    if (!recents_screen->checkPointInsideMain(start_point)) {
        return;
    }

    manager->_recents_screen_start_point = start_point;
    manager->_recents_screen_last_point = start_point;
    manager->_flags.is_recents_screen_pressed = true;
    manager->_flags.is_recents_screen_snapshot_move_hor = false;
    manager->_flags.is_recents_screen_snapshot_move_ver = false;

    ESP_UTILS_LOGD("Recents screen press(%d, %d)", start_point.x, start_point.y);
}

void Manager::onRecentsScreenGesturePressingEventCallback(lv_event_t *event)
{
    Manager *manager = nullptr;
    RecentsScreen *recents_screen = nullptr;
    Gesture::Info *gesture_info = nullptr;
    const Data *data = nullptr;
    lv_point_t start_point = { 0 };
    int drag_app_id = -1;
    int app_y_max = 0;
    int app_y_min = 0;
    int app_y_current = 0;
    int app_y_target = 0;
    int distance_x = 0;
    int distance_y = 0;
    float tan_value = 0;

    ESP_UTILS_CHECK_NULL_EXIT(event, "Invalid event");

    manager = static_cast<Manager *>(lv_event_get_user_data(event));
    ESP_UTILS_CHECK_NULL_EXIT(manager, "Invalid manager");

    // Check if there is an active app and the recents_screen is pressed
    if ((!manager->_flags.is_recents_screen_pressed) || (manager->_recents_screen_active_app == nullptr)) {
        return;
    }

    recents_screen = manager->display._recents_screen.get();
    ESP_UTILS_CHECK_NULL_EXIT(recents_screen, "Invalid recents_screen");

    gesture_info = (Gesture::Info *)lv_event_get_param(event);
    ESP_UTILS_CHECK_NULL_EXIT(gesture_info, "Invalid gesture info");

    // Check if scroll to the left or right
    if ((gesture_info->direction & Gesture::DIR_LEFT) && !manager->_flags.is_recents_screen_snapshot_move_hor &&
            !manager->_flags.is_recents_screen_snapshot_move_ver) {
        if (!manager->processRecentsScreenMoveLeft()) {
            ESP_UTILS_LOGE("Recents screen app move left failed");
        }
        manager->_flags.is_recents_screen_snapshot_move_hor = true;
    } else if ((gesture_info->direction & Gesture::DIR_RIGHT) &&
               !manager->_flags.is_recents_screen_snapshot_move_hor &&
               !manager->_flags.is_recents_screen_snapshot_move_ver) {
        if (!manager->processRecentsScreenMoveRight()) {
            ESP_UTILS_LOGE("Recents screen app move right failed");
        }
        manager->_flags.is_recents_screen_snapshot_move_hor = true;
    }

    start_point = (lv_point_t) {
        (lv_coord_t)gesture_info->start_x, (lv_coord_t)gesture_info->start_y,
    };
    drag_app_id = recents_screen->getSnapshotIdPointIn(start_point);
    data = &manager->data;
    // Check if the snapshot is dragged
    if (drag_app_id < 0) {
        return;
    }

    app_y_current = recents_screen->getSnapshotCurrentY(drag_app_id);
    distance_x = gesture_info->stop_x - manager->_recents_screen_last_point.x;
    distance_y = gesture_info->stop_y - manager->_recents_screen_last_point.y;
    // If the vertical distance is less than the step, return
    if (abs(distance_y) < data->recents_screen.drag_snapshot_y_step) {
        return;
    }
    if (distance_x != 0) {
        tan_value = fabs((float)distance_y / distance_x);
        if (tan_value < manager->_recents_screen_drag_tan_threshold) {
            distance_y = 0;
        }
    }

    app_y_max = data->recents_screen.drag_snapshot_y_threshold;
    app_y_min = -app_y_max;
    if (data->flags.enable_recents_screen_snapshot_drag && !manager->_flags.is_recents_screen_snapshot_move_hor &&
            (((distance_y > 0) && (app_y_current < app_y_max)) || ((distance_y < 0) && (app_y_current > app_y_min)))) {
        app_y_target = min(max(app_y_current + distance_y, app_y_min), app_y_max);
        ESP_UTILS_CHECK_FALSE_EXIT(recents_screen->moveSnapshotY(drag_app_id, app_y_target),
                                   "Recents screen move snapshot(%d) y failed", drag_app_id);
        manager->_flags.is_recents_screen_snapshot_move_ver = true;
    }

    manager->_recents_screen_last_point = (lv_point_t) {
        (lv_coord_t)gesture_info->stop_x, (lv_coord_t)gesture_info->stop_y
    };
}

void Manager::onRecentsScreenGestureReleaseEventCallback(lv_event_t *event)
{
    enum {
        // Default
        RECENTS_SCREEN_NONE =      0,
        // Operate recents_screen
        RECENTS_SCREEN_HIDE =      (1 << 0),
        // Operate app
        RECENTS_SCREEN_APP_CLOSE = (1 << 1),
        RECENTS_SCREEN_APP_SHOW =  (1 << 2),
        // Operate Snapshot
        RECENTS_SCREEN_SNAPSHOT_MOVE_BACK = (1 << 3),
    };

    int recents_screen_active_snapshot_index = 0;
    int target_app_id = -1;
    int distance_move_up_threshold = 0;
    int distance_move_down_threshold = 0;
    int distance_move_up_exit_threshold = 0;
    int distance_y = 0;
    int state = RECENTS_SCREEN_NONE;
    lv_event_code_t event_code = _LV_EVENT_LAST;
    lv_point_t start_point = { 0 };
    Manager *manager = nullptr;
    RecentsScreen *recents_screen = nullptr;
    Gesture::Info *gesture_info = nullptr;
    base::Context::AppEventData app_event_data = {
        .type = base::Context::AppEventType::MAX,
    };
    const Data *data = nullptr;

    ESP_UTILS_CHECK_NULL_EXIT(event, "Invalid event");

    manager = static_cast<Manager *>(lv_event_get_user_data(event));
    ESP_UTILS_CHECK_NULL_EXIT(manager, "Invalid manager");
    recents_screen = manager->display._recents_screen.get();
    ESP_UTILS_CHECK_NULL_EXIT(recents_screen, "Invalid recents_screen");
    gesture_info = (Gesture::Info *)lv_event_get_param(event);
    ESP_UTILS_CHECK_NULL_EXIT(gesture_info, "Invalid gesture info");
    event_code = manager->_system_context.getAppEventCode();
    ESP_UTILS_CHECK_FALSE_EXIT(esp_brookesia_core_utils_check_event_code_valid(event_code), "Invalid event code");

    // Check if the recents_screen is not pressed or the snapshot is moved
    if (!manager->_flags.is_recents_screen_pressed || manager->_flags.is_recents_screen_snapshot_move_hor) {
        return;
    }

    if (manager->_recents_screen_active_app == nullptr) {
        goto process;
    }

    start_point = (lv_point_t) {
        (lv_coord_t)gesture_info->start_x, (lv_coord_t)gesture_info->start_y,
    };
    target_app_id = recents_screen->getSnapshotIdPointIn(start_point);
    if (target_app_id < 0) {
        if (manager->_recents_screen_pause_app != nullptr) {
            target_app_id = manager->_recents_screen_pause_app->getId();
            state |= RECENTS_SCREEN_APP_SHOW | RECENTS_SCREEN_HIDE;
        }
        goto process;
    }

    if (manager->_flags.is_recents_screen_snapshot_move_ver) {
        state |= RECENTS_SCREEN_SNAPSHOT_MOVE_BACK;
    }

    data = &manager->data;
    distance_y = gesture_info->stop_y - gesture_info->start_y;
    distance_move_up_threshold = -1 * data->recents_screen.drag_snapshot_y_step + 1;
    distance_move_down_threshold = -distance_move_up_threshold;
    distance_move_up_exit_threshold = -1 * data->recents_screen.delete_snapshot_y_threshold;
    if ((distance_y > distance_move_up_threshold) && (distance_y < distance_move_down_threshold)) {
        state |= RECENTS_SCREEN_APP_SHOW | RECENTS_SCREEN_HIDE;
    } else if (distance_y <= distance_move_up_exit_threshold) {
        state |= RECENTS_SCREEN_APP_CLOSE;
    }

process:
    ESP_UTILS_LOGD("Recents screen release");

    if (state == RECENTS_SCREEN_NONE) {
        state = RECENTS_SCREEN_HIDE;
    }

    if (state & RECENTS_SCREEN_SNAPSHOT_MOVE_BACK) {
        recents_screen->moveSnapshotY(target_app_id, recents_screen->getSnapshotOriginY(target_app_id));
        ESP_UTILS_LOGD("Recents screen move snapshot back");
    }

    if (state & RECENTS_SCREEN_APP_CLOSE) {
        ESP_UTILS_LOGD("Recents screen close app(%d)", target_app_id);
        app_event_data.id = target_app_id;
        app_event_data.type = base::Context::AppEventType::STOP;
    } else if (state & RECENTS_SCREEN_APP_SHOW) {
        ESP_UTILS_LOGD("Recents screen start app(%d)", target_app_id);
        app_event_data.id = target_app_id;
        app_event_data.type = base::Context::AppEventType::START;
    }

    if (state & RECENTS_SCREEN_HIDE) {
        ESP_UTILS_LOGD("Hide recents_screen");
        ESP_UTILS_CHECK_FALSE_EXIT(manager->processRecentsScreenHide(), "Hide recents_screen failed");
    }

    manager->_flags.is_recents_screen_pressed = false;
    if (app_event_data.type != base::Context::AppEventType::MAX) {
        // Get the index of the next dragging snapshot before close it
        recents_screen_active_snapshot_index = max(manager->getRunningAppIndexById(target_app_id) - 1, 0);
        // Start or close the dragging app
        ESP_UTILS_CHECK_FALSE_EXIT(manager->_system_context.sendAppEvent(&app_event_data), "Core send app event failed");
        // Scroll to another running app snapshot if the dragging app is closed
        if (app_event_data.type == base::Context::AppEventType::STOP) {
            manager->_recents_screen_active_app = manager->getRunningAppByIdenx(recents_screen_active_snapshot_index);
            if (manager->_recents_screen_active_app != nullptr) {
                // If there are active apps, scroll to the previous app snapshot
                ESP_UTILS_LOGD("Recents screen scroll snapshot(%d) to %d", manager->_recents_screen_active_app->getId(),
                               recents_screen_active_snapshot_index);
                if (!recents_screen->scrollToSnapshotByIndex(recents_screen_active_snapshot_index)) {
                    ESP_UTILS_LOGE("Recents screen scroll snapshot(%d) to %d failed",
                                   manager->_recents_screen_active_app->getId(),
                                   recents_screen_active_snapshot_index);
                }
            } else if (manager->data.flags.enable_recents_screen_hide_when_no_snapshot) {
                // If there are no active apps, hide the recents_screen
                ESP_UTILS_LOGD("No active app, hide recents_screen");
                ESP_UTILS_CHECK_FALSE_EXIT(manager->processRecentsScreenHide(), "Hide recents_screen failed");
            }
        }
    }
}

void Manager::onRecentsScreenSnapshotDeletedEventCallback(lv_event_t *event)
{
    int app_id = -1;
    Manager *manager = nullptr;
    RecentsScreen *recents_screen = nullptr;
    base::Context::AppEventData app_event_data = {
        .type = base::Context::AppEventType::STOP,
    };

    ESP_UTILS_LOGD("Recents screen snapshot deleted event callback");
    ESP_UTILS_CHECK_NULL_EXIT(event, "Invalid event object");

    manager = static_cast<Manager *>(lv_event_get_user_data(event));
    ESP_UTILS_CHECK_NULL_EXIT(manager, "Invalid manager");
    recents_screen = manager->display._recents_screen.get();
    ESP_UTILS_CHECK_NULL_EXIT(recents_screen, "Invalid recents_screen");
    app_id = (intptr_t)lv_event_get_param(event);

    if (app_id > 0) {
        app_event_data.id = app_id;
        ESP_UTILS_CHECK_FALSE_EXIT(manager->_system_context.sendAppEvent(&app_event_data), "Core send app event failed");
    }

    if (recents_screen->getSnapshotCount() == 0) {
        ESP_UTILS_LOGD("No snapshot in the recents_screen");
        manager->_recents_screen_active_app = nullptr;
        if (manager->data.flags.enable_recents_screen_hide_when_no_snapshot) {
            ESP_UTILS_CHECK_FALSE_EXIT(manager->processRecentsScreenHide(), "Manager hide recents_screen failed");
        }
    }
}

} // namespace esp_brookesia::systems::phone
