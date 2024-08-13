/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <cmath>
#include "esp_ui_phone_manager.hpp"
#include "esp_ui_phone.hpp"

#if !ESP_UI_LOG_ENABLE_DEBUG_PHONE_MANAGER
#undef ESP_UI_LOGD
#define ESP_UI_LOGD(...)
#endif

using namespace std;

ESP_UI_PhoneManager::ESP_UI_PhoneManager(ESP_UI_Core &core, ESP_UI_PhoneHome &home,
        const ESP_UI_PhoneManagerData_t &data):
    ESP_UI_CoreManager(core, core.getCoreData().manager),
    _home(home),
    _data(data),
    _is_initialized(false),
    _home_active_screen(ESP_UI_PHONE_MANAGER_SCREEN_MAX),
    _is_app_launcher_gesture_disabled(false),
    _app_launcher_gesture_dir(ESP_UI_GESTURE_DIR_NONE),
    _enable_navigation_bar_gesture(false),
    _is_navigation_bar_gesture_disabled(false),
    _navigation_bar_gesture_dir(ESP_UI_GESTURE_DIR_NONE),
    _enable_gesture_navigation(false),
    _enable_gesture_navigation_back(false),
    _enable_gesture_navigation_home(false),
    _enable_gesture_navigation_recents_app(false),
    _is_gesture_navigation_disabled(false),
    _gesture(nullptr),
    _recents_screen_pressed(0),
    _recents_screen_snapshot_move_hor(0),
    _recents_screen_snapshot_move_ver(0),
    _recents_screen_drag_tan_threshold(0),
    _recents_screen_start_point{},
    _recents_screen_last_point{},
    _recents_screen_active_app(nullptr)
{
}

ESP_UI_PhoneManager::~ESP_UI_PhoneManager()
{
    ESP_UI_LOGD("Destroy(0x%p)", this);
    if (!del()) {
        ESP_UI_LOGE("Failed to delete");
    }
}

bool ESP_UI_PhoneManager::calibrateData(const ESP_UI_CoreData_t core_data, ESP_UI_PhoneManagerData_t &data)
{
    ESP_UI_LOGD("Calibrate data");

    if (data.flags.enable_gesture) {
        ESP_UI_CHECK_FALSE_RETURN(ESP_UI_Gesture::calibrateData(core_data, data.gesture), false,
                                  "Calibrate gesture data failed");
    }

    return true;
}

bool ESP_UI_PhoneManager::begin(void)
{
    const ESP_UI_RecentsScreen *recents_screen = _home.getRecentsScreen();
    unique_ptr<ESP_UI_Gesture> gesture = nullptr;
    lv_indev_t *touch = nullptr;

    ESP_UI_LOGD("Begin(@0x%p)", this);
    ESP_UI_CHECK_FALSE_RETURN(!checkInitialized(), false, "Already initialized");

    // Gesture
    if (_data.flags.enable_gesture) {
        // Get the touch device
        touch = _core.getTouchDevice();
        if (touch == nullptr) {
            ESP_UI_LOGW("No touch device is set, try to use default touch device");

            touch = esp_ui_core_utils_get_input_dev(_core.getDisplayDevice(), LV_INDEV_TYPE_POINTER);
            ESP_UI_CHECK_NULL_RETURN(touch, false, "No touch device is initialized");
            ESP_UI_LOGW("Using default touch device(@0x%p)", touch);

            ESP_UI_CHECK_FALSE_RETURN(_core.setTouchDevice(touch), false, "Core set touch device failed");
        }

        // Create and begin gesture
        gesture = make_unique<ESP_UI_Gesture>(_core, _data.gesture);
        ESP_UI_CHECK_NULL_RETURN(gesture, false, "Create gesture failed");
        ESP_UI_CHECK_FALSE_RETURN(gesture->begin(_home.getSystemScreenObject()), false, "Gesture begin failed");

        _enable_gesture_navigation = true;
        lv_obj_add_event_cb(gesture->getEventObj(), onGestureNavigationPressingEventCallback,
                            gesture->getPressingEventCode(), this);
        lv_obj_add_event_cb(gesture->getEventObj(), onGestureNavigationReleaseEventCallback,
                            gesture->getReleaseEventCode(), this);
        lv_obj_add_event_cb(_home.getMainScreen(), onHomeMainScreenLoadEventCallback, LV_EVENT_SCREEN_LOADED, this);
    }

    if (gesture != nullptr) {
        // App Launcher
        lv_obj_add_event_cb(gesture->getEventObj(), onAppLauncherGestureEventCallback,
                            gesture->getPressingEventCode(), this);
        lv_obj_add_event_cb(gesture->getEventObj(), onAppLauncherGestureEventCallback,
                            gesture->getReleaseEventCode(), this);

        // Navigation Bar
        if (_home.getNavigationBar() != nullptr) {
            lv_obj_add_event_cb(gesture->getEventObj(), onNavigationBarGestureEventCallback,
                                gesture->getPressingEventCode(), this);
            lv_obj_add_event_cb(gesture->getEventObj(), onNavigationBarGestureEventCallback,
                                gesture->getReleaseEventCode(), this);
        }
    }

    // Recents Screen
    if (recents_screen != nullptr) {
        // Hide recents_screen by default
        ESP_UI_CHECK_FALSE_RETURN(processRecentsScreenHide(), false, "Hide recents_screen failed");
        _recents_screen_drag_tan_threshold = tan(_data.recents_screen.drag_snapshot_angle_threshold * M_PI / 180);
        lv_obj_add_event_cb(recents_screen->getEventObject(), onRecentsScreenSnapshotDeletedEventCallback,
                            recents_screen->getSnapshotDeletedEventCode(), this);
        // Register gesture event
        if (gesture != nullptr) {
            ESP_UI_LOGD("Enable recents_screen gesture");
            lv_obj_add_event_cb(gesture->getEventObj(), onRecentsScreenGesturePressEventCallback,
                                gesture->getPressEventCode(), this);
            lv_obj_add_event_cb(gesture->getEventObj(), onRecentsScreenGesturePressingEventCallback,
                                gesture->getPressingEventCode(), this);
            lv_obj_add_event_cb(gesture->getEventObj(), onRecentsScreenGestureReleaseEventCallback,
                                gesture->getReleaseEventCode(), this);
        }
    }

    ESP_UI_CHECK_FALSE_RETURN(processHomeScreenChange(ESP_UI_PHONE_MANAGER_SCREEN_MAIN, nullptr), false,
                              "Process screen change failed");

    if (gesture != nullptr) {
        _gesture = std::move(gesture);
    }
    _is_initialized = true;

    return true;
}

bool ESP_UI_PhoneManager::del(void)
{
    lv_obj_t *temp_obj = nullptr;

    ESP_UI_LOGD("Delete phone manager(0x%p)", this);

    if (!checkInitialized()) {
        return true;
    }

    if (_gesture != nullptr) {
        _gesture.reset();
    }
    if (_home.getRecentsScreen() != nullptr) {
        temp_obj = _home.getRecentsScreen()->getEventObject();
        if (temp_obj != nullptr && lv_obj_is_valid(temp_obj)) {
            lv_obj_remove_event_cb(temp_obj, onRecentsScreenSnapshotDeletedEventCallback);
        }
    }
    _is_initialized = false;
    _recents_screen_active_app = nullptr;

    return true;
}

bool ESP_UI_PhoneManager::processAppRunExtra(ESP_UI_CoreApp *app)
{
    ESP_UI_PhoneApp *phone_app = static_cast<ESP_UI_PhoneApp *>(app);

    ESP_UI_CHECK_NULL_RETURN(phone_app, false, "Invalid phone app");
    ESP_UI_LOGD("Process app(%p) run extra", phone_app);

    ESP_UI_CHECK_FALSE_RETURN(processHomeScreenChange(ESP_UI_PHONE_MANAGER_SCREEN_APP, phone_app), false,
                              "Process screen change failed");

    return true;
}

bool ESP_UI_PhoneManager::processAppResumeExtra(ESP_UI_CoreApp *app)
{
    ESP_UI_PhoneApp *phone_app = static_cast<ESP_UI_PhoneApp *>(app);

    ESP_UI_CHECK_NULL_RETURN(phone_app, false, "Invalid phone app");
    ESP_UI_LOGD("Process app(%p) resume extra", phone_app);

    ESP_UI_CHECK_FALSE_RETURN(processHomeScreenChange(ESP_UI_PHONE_MANAGER_SCREEN_APP, phone_app), false,
                              "Process screen change failed");

    return true;
}

bool ESP_UI_PhoneManager::processAppCloseExtra(ESP_UI_CoreApp *app)
{
    ESP_UI_PhoneApp *phone_app = static_cast<ESP_UI_PhoneApp *>(app);

    ESP_UI_CHECK_NULL_RETURN(phone_app, false, "Invalid phone app");
    ESP_UI_LOGD("Process app(%p) close extra", phone_app);

    if (getActiveApp() == app) {
        ESP_UI_CHECK_FALSE_RETURN(processHomeScreenChange(ESP_UI_PHONE_MANAGER_SCREEN_MAIN, nullptr), false,
                                  "Process screen change failed");
    }

    return true;
}

bool ESP_UI_PhoneManager::processHomeScreenChange(ESP_UI_PhoneManagerScreen_t screen, void *data)
{
    ESP_UI_StatusBar *status_bar = _home._status_bar.get();
    ESP_UI_NavigationBar *navigation_bar = _home._navigation_bar.get();
    ESP_UI_StatusBarVisualMode_t status_bar_visual_mode = ESP_UI_STATUS_BAR_VISUAL_MODE_HIDE;
    ESP_UI_NavigationBarVisualMode_t navigation_bar_visual_mode = ESP_UI_NAVIGATION_BAR_VISUAL_MODE_HIDE;
    const ESP_UI_PhoneAppData_t *app_data = nullptr;

    ESP_UI_LOGD("Process Screen Change(%d)", screen);
    ESP_UI_CHECK_FALSE_RETURN(screen < ESP_UI_PHONE_MANAGER_SCREEN_MAX, false, "Invalid screen");

    switch (screen) {
    case ESP_UI_PHONE_MANAGER_SCREEN_MAIN:
        navigation_bar_visual_mode = _home.getData().navigation_bar.visual_mode;
        status_bar_visual_mode = _home.getData().status_bar.visual_mode;
        _enable_gesture_navigation = ((navigation_bar == nullptr) ||
                                      (navigation_bar_visual_mode == ESP_UI_NAVIGATION_BAR_VISUAL_MODE_HIDE));
        _enable_gesture_navigation_back = false;
        _enable_gesture_navigation_home = false;
        _enable_gesture_navigation_recents_app = _enable_gesture_navigation;
        break;
    case ESP_UI_PHONE_MANAGER_SCREEN_APP:
        ESP_UI_CHECK_NULL_RETURN(data, false, "Invalid data");
        app_data = &((ESP_UI_PhoneApp *)data)->getActiveData();
        navigation_bar_visual_mode = app_data->navigation_bar_visual_mode;
        status_bar_visual_mode = app_data->status_bar_visual_mode;
        _enable_gesture_navigation = (app_data->flags.enable_navigation_gesture &&
                                      (navigation_bar_visual_mode != ESP_UI_NAVIGATION_BAR_VISUAL_MODE_SHOW_FIXED));
        _enable_gesture_navigation_back = _enable_gesture_navigation;
        _enable_gesture_navigation_home = (_enable_gesture_navigation &&
                                           (navigation_bar_visual_mode == ESP_UI_NAVIGATION_BAR_VISUAL_MODE_HIDE));
        _enable_gesture_navigation_recents_app = _enable_gesture_navigation_home;
        break;
    case ESP_UI_PHONE_MANAGER_SCREEN_RECENTS_SCREEN:
        navigation_bar_visual_mode = _home.getData().recents_screen.navigation_bar_visual_mode;
        status_bar_visual_mode = _home.getData().recents_screen.status_bar_visual_mode;
        _enable_gesture_navigation = false;
        break;
    default:
        ESP_UI_CHECK_FALSE_RETURN(false, false, "Invalid screen");
        break;
    }
    ESP_UI_LOGD("Visual Mode: status bar(%d), navigation bar(%d)", status_bar_visual_mode, navigation_bar_visual_mode);
    ESP_UI_LOGD("Gesture: all(%d), back(%d), home(%d), recents(%d)", _enable_gesture_navigation,
                _enable_gesture_navigation_back, _enable_gesture_navigation_home, _enable_gesture_navigation_recents_app);

    // Process gesture
    // if (_gesture != nullptr) {
    // ESP_UI_CHECK_FALSE_RETURN(_gesture->enableMaskObject(_enable_gesture_navigation), false,
    //   "Gesture enable mask object failed");
    // }
    // Process status bar
    if (status_bar != nullptr) {
        ESP_UI_CHECK_FALSE_RETURN(status_bar->setVisualMode(status_bar_visual_mode), false,
                                  "Status bar set visual mode failed");
    }
    // Process navigation bar
    if (navigation_bar != nullptr) {
        _enable_navigation_bar_gesture = (navigation_bar_visual_mode == ESP_UI_NAVIGATION_BAR_VISUAL_MODE_SHOW_FLEX);
        ESP_UI_CHECK_FALSE_RETURN(navigation_bar->setVisualMode(navigation_bar_visual_mode), false,
                                  "Navigation bar set visual mode failed");
    }

    if (screen == ESP_UI_PHONE_MANAGER_SCREEN_MAIN) {
        ESP_UI_CHECK_FALSE_RETURN(_home.processMainScreenLoad(), false, "Home load main screen failed");
    }

    _home_active_screen = screen;

    return true;
}

void ESP_UI_PhoneManager::onHomeMainScreenLoadEventCallback(lv_event_t *event)
{
    ESP_UI_PhoneManager *manager = nullptr;
    ESP_UI_RecentsScreen *recents_screen = nullptr;

    ESP_UI_CHECK_NULL_EXIT(event, "Invalid event");

    manager = static_cast<ESP_UI_PhoneManager *>(lv_event_get_user_data(event));
    ESP_UI_CHECK_NULL_EXIT(manager, "Invalid manager");
    recents_screen = manager->_home.getRecentsScreen();

    // Only process the screen change if the recents_screen is not visible
    if ((recents_screen == nullptr) || !recents_screen->checkVisible()) {
        ESP_UI_CHECK_FALSE_EXIT(manager->processHomeScreenChange(ESP_UI_PHONE_MANAGER_SCREEN_MAIN, nullptr),
                                "Process screen change failed");
    }
}

void ESP_UI_PhoneManager::onAppLauncherGestureEventCallback(lv_event_t *event)
{
    lv_event_code_t event_code = _LV_EVENT_LAST;
    ESP_UI_PhoneManager *manager = nullptr;
    ESP_UI_AppLauncher *app_launcher = nullptr;
    ESP_UI_RecentsScreen *recents_screen = nullptr;
    ESP_UI_GestureInfo_t *gesture_info = nullptr;
    ESP_UI_GestureDirection_t dir_type = ESP_UI_GESTURE_DIR_NONE;

    ESP_UI_CHECK_NULL_GOTO(event, end, "Invalid event");

    manager = static_cast<ESP_UI_PhoneManager *>(lv_event_get_user_data(event));
    ESP_UI_CHECK_NULL_GOTO(manager, end, "Invalid manager");
    recents_screen = manager->_home._recents_screen.get();
    app_launcher = &manager->_home._app_launcher;
    ESP_UI_CHECK_NULL_GOTO(app_launcher, end, "Invalid app launcher");
    event_code = lv_event_get_code(event);
    ESP_UI_CHECK_FALSE_GOTO((event_code == manager->_gesture->getPressingEventCode()) ||
                            (event_code == manager->_gesture->getReleaseEventCode()), end, "Invalid event code");

    // Here is to prevent detecting gestures when the app exits, which could trigger unexpected behaviors
    if (event_code == manager->_gesture->getReleaseEventCode()) {
        if (manager->_is_app_launcher_gesture_disabled) {
            manager->_is_app_launcher_gesture_disabled = false;
            return;
        }
    }

    // Check if the app launcher and recents_screen are visible
    if (!app_launcher->checkVisible() || manager->_is_app_launcher_gesture_disabled ||
            ((recents_screen != nullptr) && recents_screen->checkVisible())) {
        return;
    }

    dir_type = manager->_app_launcher_gesture_dir;
    // Check if the dir type is already set. If so, just ignore and return
    if (dir_type != ESP_UI_GESTURE_DIR_NONE) {
        // Check if the gesture is released
        if (event_code == manager->_gesture->getReleaseEventCode()) {   // If so, reset the navigation type
            dir_type = ESP_UI_GESTURE_DIR_NONE;
            goto end;
        }
        return;
    }

    gesture_info = (ESP_UI_GestureInfo_t *)lv_event_get_param(event);
    ESP_UI_CHECK_NULL_GOTO(gesture_info, end, "Invalid gesture info");
    // Check if there is a gesture
    if (gesture_info->direction == ESP_UI_GESTURE_DIR_NONE) {
        return;
    }

    dir_type = gesture_info->direction;
    switch (dir_type) {
    case ESP_UI_GESTURE_DIR_LEFT:
        ESP_UI_LOGD("App table gesture left");
        ESP_UI_CHECK_FALSE_GOTO(app_launcher->scrollToRightPage(), end, "App table scroll to right page failed");
        break;
    case ESP_UI_GESTURE_DIR_RIGHT:
        ESP_UI_LOGD("App table gesture right");
        ESP_UI_CHECK_FALSE_GOTO(app_launcher->scrollToLeftPage(), end, "App table scroll to left page failed");
        break;
    default:
        break;
    }

end:
    manager->_app_launcher_gesture_dir = dir_type;
}

void ESP_UI_PhoneManager::onNavigationBarGestureEventCallback(lv_event_t *event)
{
    lv_event_code_t event_code = _LV_EVENT_LAST;
    ESP_UI_PhoneManager *manager = nullptr;
    ESP_UI_NavigationBar *navigation_bar = nullptr;
    ESP_UI_GestureInfo_t *gesture_info = nullptr;
    ESP_UI_GestureDirection_t dir_type = ESP_UI_GESTURE_DIR_NONE;

    ESP_UI_CHECK_NULL_EXIT(event, "Invalid event");

    manager = static_cast<ESP_UI_PhoneManager *>(lv_event_get_user_data(event));
    ESP_UI_CHECK_NULL_EXIT(manager, "Invalid manager");
    navigation_bar = manager->_home.getNavigationBar();
    ESP_UI_CHECK_NULL_EXIT(navigation_bar, "Invalid navigation bar");
    event_code = lv_event_get_code(event);
    ESP_UI_CHECK_FALSE_EXIT((event_code == manager->_gesture->getPressingEventCode()) ||
                            (event_code == manager->_gesture->getReleaseEventCode()), "Invalid event code");

    // Here is to prevent detecting gestures when the app exits, which could trigger unexpected behaviors
    if (manager->_is_navigation_bar_gesture_disabled && (event_code == manager->_gesture->getReleaseEventCode())) {
        manager->_is_navigation_bar_gesture_disabled = false;
        return;
    }

    // Check if the gesture is enabled or the app is running
    if (manager->_is_navigation_bar_gesture_disabled || (!manager->_enable_navigation_bar_gesture)) {
        return;
    }

    dir_type = manager->_navigation_bar_gesture_dir;
    // Check if the dir type is already set. If so, just ignore and return
    if (dir_type != ESP_UI_GESTURE_DIR_NONE) {
        // Check if the gesture is released
        if (event_code == manager->_gesture->getReleaseEventCode()) {   // If so, reset the navigation type
            dir_type = ESP_UI_GESTURE_DIR_NONE;
            goto end;
        }
        return;
    }

    gesture_info = (ESP_UI_GestureInfo_t *)lv_event_get_param(event);
    ESP_UI_CHECK_NULL_EXIT(gesture_info, "Invalid gesture info");

    // Check if there is a valid gesture
    dir_type = gesture_info->direction;
    if ((dir_type == ESP_UI_GESTURE_DIR_UP) &&
            (gesture_info->start_area & ESP_UI_GESTURE_AREA_BOTTOM_EDGE)) {
        ESP_UI_LOGD("Navigation bar gesture up");
        ESP_UI_CHECK_FALSE_EXIT(navigation_bar->triggerVisualFlexShow(), "Navigation bar trigger visual flex show failed");
    }

end:
    manager->_navigation_bar_gesture_dir = dir_type;
}

bool ESP_UI_PhoneManager::processNavigationEvent(ESP_UI_CoreNavigateType_t type)
{
    bool ret = true;
    ESP_UI_RecentsScreen *recents_screen = _home._recents_screen.get();
    ESP_UI_PhoneApp *active_app = static_cast<ESP_UI_PhoneApp *>(getActiveApp());
    ESP_UI_PhoneApp *phone_app = nullptr;

    ESP_UI_LOGD("Process navigation event type(%d)", type);

    // Disable the gesture function of widgets
    _is_app_launcher_gesture_disabled = true;
    _is_navigation_bar_gesture_disabled = true;

    // Check if the recents_screen is visible
    if ((recents_screen != nullptr) && recents_screen->checkVisible()) {
        // Hide if the recents_screen is visible
        if (!processRecentsScreenHide()) {
            ESP_UI_LOGE("Hide recents_screen failed");
            ret = false;
        }
        // Directly return if the type is not home
        if (type != ESP_UI_CORE_NAVIGATE_TYPE_HOME) {
            return ret;
        }
    }

    switch (type) {
    case ESP_UI_CORE_NAVIGATE_TYPE_BACK:
        if (active_app == nullptr) {
            goto end;
        }
        // Call app back function
        ESP_UI_CHECK_FALSE_GOTO(ret = (active_app->back()), end, "App(%d) back failed", active_app->getId());
        break;
    case ESP_UI_CORE_NAVIGATE_TYPE_HOME:
        if (active_app == nullptr) {
            goto end;
        }
        // Process app pause
        ESP_UI_CHECK_FALSE_GOTO(ret = processAppPause(active_app), end, "App(%d) pause failed", active_app->getId());
        ESP_UI_CHECK_FALSE_RETURN(processHomeScreenChange(ESP_UI_PHONE_MANAGER_SCREEN_MAIN, nullptr), false,
                                  "Process screen change failed");
        resetActiveApp();
        break;
    case ESP_UI_CORE_NAVIGATE_TYPE_RECENTS_SCREEN:
        if (recents_screen == nullptr) {
            ESP_UI_LOGW("Recents screen is disabled");
            goto end;
        }
        // Show recents_screen
        ESP_UI_CHECK_FALSE_GOTO(ret = processRecentsScreenShow(), end, "Process recents_screen show failed");
        // Check if there is an active app, if not, set the last app as the active app
        if (active_app != nullptr) {
            _recents_screen_active_app = active_app;
            // Pause app
            if (!processAppPause(active_app)) {
                ESP_UI_LOGE("App(%d) pause failed", active_app->getId());
                ret = false;
            }
        } else {
            _recents_screen_active_app = getRunningAppByIdenx(getRunningAppCount() - 1);
        }
        // Scroll to the active app
        if ((_recents_screen_active_app != nullptr) &&
                !recents_screen->scrollToSnapshotById(_recents_screen_active_app->getId())) {
            ESP_UI_LOGE("Recents screen scroll to snapshot(%d) failed", _recents_screen_active_app->getId());
            ret = false;
        }
        // Update the snapshot, need to be called after `processAppPause()`
        for (int i = 0; i < getRunningAppCount(); i++) {
            phone_app = static_cast<ESP_UI_PhoneApp *>(getRunningAppByIdenx(i));
            ESP_UI_CHECK_FALSE_GOTO(ret = (phone_app != nullptr), end, "Invalid active app");

            // Update snapshot conf and image
            ESP_UI_CHECK_FALSE_GOTO(ret = phone_app->updateRecentsScreenSnapshotConf(getAppSnapshot(phone_app->getId())),
                                    end, "App update snapshot(%d) conf failed", phone_app->getId());
            ESP_UI_CHECK_FALSE_GOTO(ret = recents_screen->updateSnapshotImage(phone_app->getId()), end,
                                    "Recents screen update snapshot(%d) image failed", phone_app->getId());
        }
        break;
    default:
        break;
    }

end:
    return ret;
}

void ESP_UI_PhoneManager::onGestureNavigationPressingEventCallback(lv_event_t *event)
{
    ESP_UI_PhoneManager *manager = nullptr;
    ESP_UI_GestureInfo_t *gesture_info = nullptr;
    ESP_UI_CoreNavigateType_t navigation_type = ESP_UI_CORE_NAVIGATE_TYPE_MAX;

    ESP_UI_CHECK_NULL_EXIT(event, "Invalid event");

    manager = static_cast<ESP_UI_PhoneManager *>(lv_event_get_user_data(event));
    ESP_UI_CHECK_NULL_EXIT(manager, "Invalid manager");
    // Check if the gesture is released and enabled
    if (!manager->_enable_gesture_navigation || manager->_is_gesture_navigation_disabled) {
        return;
    }

    gesture_info = (ESP_UI_GestureInfo_t *)lv_event_get_param(event);
    ESP_UI_CHECK_NULL_EXIT(gesture_info, "Invalid gesture info");
    // Check if there is a gesture
    if (gesture_info->direction == ESP_UI_GESTURE_DIR_NONE) {
        return;
    }

    // Check if there is a "back" gesture
    if ((gesture_info->start_area & (ESP_UI_GESTURE_AREA_LEFT_EDGE | ESP_UI_GESTURE_AREA_RIGHT_EDGE)) &&
            (gesture_info->direction & ESP_UI_GESTURE_DIR_HOR) && manager->_enable_gesture_navigation_back) {
        navigation_type = ESP_UI_CORE_NAVIGATE_TYPE_BACK;
    } else if ((gesture_info->start_area & ESP_UI_GESTURE_AREA_BOTTOM_EDGE) && (!gesture_info->flags.short_duration) &&
               (gesture_info->direction & ESP_UI_GESTURE_DIR_UP) && manager->_enable_gesture_navigation_recents_app) {
        // Check if there is a "recents_screen" gesture
        navigation_type = ESP_UI_CORE_NAVIGATE_TYPE_RECENTS_SCREEN;
    }

    // Only process the navigation event if the navigation type is valid
    if (navigation_type != ESP_UI_CORE_NAVIGATE_TYPE_MAX) {
        manager->_is_gesture_navigation_disabled = true;
        ESP_UI_CHECK_FALSE_EXIT(manager->processNavigationEvent(navigation_type), "Process navigation event failed");
    }
}

void ESP_UI_PhoneManager::onGestureNavigationReleaseEventCallback(lv_event_t *event)
{
    ESP_UI_PhoneManager *manager = nullptr;
    ESP_UI_GestureInfo_t *gesture_info = nullptr;
    ESP_UI_CoreNavigateType_t navigation_type = ESP_UI_CORE_NAVIGATE_TYPE_MAX;

    ESP_UI_CHECK_NULL_EXIT(event, "Invalid event");

    manager = static_cast<ESP_UI_PhoneManager *>(lv_event_get_user_data(event));
    ESP_UI_CHECK_NULL_EXIT(manager, "Invalid manager");
    manager->_is_gesture_navigation_disabled = false;
    // Check if the gesture is released and enabled
    if (!manager->_enable_gesture_navigation) {
        return;
    }

    gesture_info = (ESP_UI_GestureInfo_t *)lv_event_get_param(event);
    ESP_UI_CHECK_NULL_EXIT(gesture_info, "Invalid gesture info");
    // Check if there is a gesture
    if (gesture_info->direction == ESP_UI_GESTURE_DIR_NONE) {
        return;
    }

    // Check if there is a "home" gesture
    if ((gesture_info->start_area & ESP_UI_GESTURE_AREA_BOTTOM_EDGE) && (gesture_info->flags.short_duration) &&
            (gesture_info->direction & ESP_UI_GESTURE_DIR_UP) && manager->_enable_gesture_navigation_home) {
        navigation_type = ESP_UI_CORE_NAVIGATE_TYPE_HOME;
    }

    // Only process the navigation event if the navigation type is valid
    if (navigation_type != ESP_UI_CORE_NAVIGATE_TYPE_MAX) {
        ESP_UI_CHECK_FALSE_EXIT(manager->processNavigationEvent(navigation_type), "Process navigation event failed");
    }
}

bool ESP_UI_PhoneManager::processRecentsScreenShow(void)
{
    ESP_UI_LOGD("Process recents_screen show");

    ESP_UI_CHECK_FALSE_RETURN(_home.processRecentsScreenShow(), false, "Load recents_screen failed");
    if (_gesture != nullptr) {
        // Don't show the mask obj of gesture in the recents_screen
        ESP_UI_CHECK_FALSE_RETURN(_gesture->enableMaskObject(false), false, "Gesture enable mask object failed");
    }

    ESP_UI_CHECK_FALSE_RETURN(processHomeScreenChange(ESP_UI_PHONE_MANAGER_SCREEN_RECENTS_SCREEN, nullptr), false,
                              "Process screen change failed");

    return true;
}

bool ESP_UI_PhoneManager::processRecentsScreenHide(void)
{
    ESP_UI_RecentsScreen *recents_screen = _home.getRecentsScreen();
    ESP_UI_PhoneApp *active_app = static_cast<ESP_UI_PhoneApp *>(getActiveApp());

    ESP_UI_LOGD("Process recents_screen hide");
    ESP_UI_CHECK_NULL_RETURN(recents_screen, false, "Invalid recents_screen");
    ESP_UI_CHECK_FALSE_RETURN(recents_screen->setVisible(false), false, "Hide recents_screen failed");

    // Load the main screen if there is no active app
    if (active_app == nullptr) {
        ESP_UI_CHECK_FALSE_RETURN(processHomeScreenChange(ESP_UI_PHONE_MANAGER_SCREEN_MAIN, nullptr), false,
                                  "Process screen change failed");
    }

    return true;
}

bool ESP_UI_PhoneManager::processRecentsScreenMoveLeft(void)
{
    int recents_screen_active_app_index = getRunningAppIndexByApp(_recents_screen_active_app);
    ESP_UI_RecentsScreen *recents_screen = _home._recents_screen.get();

    ESP_UI_LOGD("Process recents_screen move left");
    ESP_UI_CHECK_NULL_RETURN(recents_screen, false, "Invalid recents_screen");
    ESP_UI_CHECK_FALSE_RETURN(recents_screen_active_app_index >= 0, false, "Invalid recents_screen active app index");

    // Check if the active app is at the leftmost
    if (++recents_screen_active_app_index >= getRunningAppCount()) {
        ESP_UI_LOGD("Recents screen snapshot is at the rightmost");
        return true;
    }

    ESP_UI_LOGD("Recents screen scroll snapshot(%d) left(%d)", _recents_screen_active_app->getId(),
                recents_screen_active_app_index);
    // Move the snapshot to the left
    ESP_UI_CHECK_FALSE_RETURN(recents_screen->scrollToSnapshotByIndex(recents_screen_active_app_index), false,
                              "Recents screen scroll snapshot left failed");
    // Update the active app
    _recents_screen_active_app = getRunningAppByIdenx(recents_screen_active_app_index);

    return true;
}

bool ESP_UI_PhoneManager::processRecentsScreenMoveRight(void)
{
    int recents_screen_active_app_index = getRunningAppIndexByApp(_recents_screen_active_app);
    ESP_UI_RecentsScreen *recents_screen = _home._recents_screen.get();

    ESP_UI_LOGD("Process recents_screen move right");
    ESP_UI_CHECK_NULL_RETURN(recents_screen, false, "Invalid recents_screen");
    ESP_UI_CHECK_FALSE_RETURN(recents_screen_active_app_index >= 0, false, "Invalid recents_screen active app index");

    // Check if the active app is at the rightmost
    if (--recents_screen_active_app_index < 0) {
        ESP_UI_LOGD("Recents screen snapshot is at the leftmost");
        return true;
    }

    ESP_UI_LOGD("Recents screen scroll snapshot(%d) right(%d)", _recents_screen_active_app->getId(),
                recents_screen_active_app_index);
    // Move the snapshot to the right
    ESP_UI_CHECK_FALSE_RETURN(recents_screen->scrollToSnapshotByIndex(recents_screen_active_app_index), false,
                              "Recents screen scroll snapshot right failed");
    // Update the active app
    _recents_screen_active_app = getRunningAppByIdenx(recents_screen_active_app_index);

    return true;
}

void ESP_UI_PhoneManager::onRecentsScreenGesturePressEventCallback(lv_event_t *event)
{
    ESP_UI_PhoneManager *manager = nullptr;
    const ESP_UI_RecentsScreen *recents_screen = nullptr;
    ESP_UI_GestureInfo_t *gesture_info = nullptr;
    lv_point_t start_point = {};

    ESP_UI_CHECK_NULL_EXIT(event, "Invalid event");

    manager = static_cast<ESP_UI_PhoneManager *>(lv_event_get_user_data(event));
    ESP_UI_CHECK_NULL_EXIT(manager, "Invalid manager");

    recents_screen = manager->_home.getRecentsScreen();
    ESP_UI_CHECK_NULL_EXIT(recents_screen, "Invalid recents_screen");

    // Check if recents_screen is visible
    if (!recents_screen->checkVisible()) {
        return;
    }

    gesture_info = (ESP_UI_GestureInfo_t *)lv_event_get_param(event);
    ESP_UI_CHECK_NULL_EXIT(gesture_info, "Invalid gesture info");

    start_point = (lv_point_t) {
        (lv_coord_t)gesture_info->start_x, (lv_coord_t)gesture_info->start_y
    };

    // Check if the start point is inside the recents_screen
    if (!recents_screen->checkPointInsideMain(start_point)) {
        return;
    }

    manager->_recents_screen_start_point = start_point;
    manager->_recents_screen_last_point = start_point;
    manager->_recents_screen_pressed = true;
    manager->_recents_screen_snapshot_move_hor = false;
    manager->_recents_screen_snapshot_move_ver = false;

    ESP_UI_LOGD("Recents screen press(%d, %d)", start_point.x, start_point.y);
}

void ESP_UI_PhoneManager::onRecentsScreenGesturePressingEventCallback(lv_event_t *event)
{
    ESP_UI_PhoneManager *manager = nullptr;
    ESP_UI_RecentsScreen *recents_screen = nullptr;
    ESP_UI_GestureInfo_t *gesture_info = nullptr;
    const ESP_UI_PhoneManagerData_t *data = nullptr;
    lv_point_t start_point = { 0 };
    int drag_app_id = -1;
    int app_y_max = 0;
    int app_y_min = 0;
    int app_y_current = 0;
    int app_y_target = 0;
    int distance_x = 0;
    int distance_y = 0;
    float tan_value = 0;

    ESP_UI_CHECK_NULL_EXIT(event, "Invalid event");

    manager = static_cast<ESP_UI_PhoneManager *>(lv_event_get_user_data(event));
    ESP_UI_CHECK_NULL_EXIT(manager, "Invalid manager");

    // Check if there is an active app and the recents_screen is pressed
    if ((!manager->_recents_screen_pressed) || (manager->_recents_screen_active_app == nullptr)) {
        return;
    }

    recents_screen = manager->_home._recents_screen.get();
    ESP_UI_CHECK_NULL_EXIT(recents_screen, "Invalid recents_screen");

    gesture_info = (ESP_UI_GestureInfo_t *)lv_event_get_param(event);
    ESP_UI_CHECK_NULL_EXIT(gesture_info, "Invalid gesture info");

    // Check if scroll to the left or right
    if ((gesture_info->direction & ESP_UI_GESTURE_DIR_LEFT) && !manager->_recents_screen_snapshot_move_hor &&
            !manager->_recents_screen_snapshot_move_ver) {
        if (!manager->processRecentsScreenMoveLeft()) {
            ESP_UI_LOGE("Recents screen app move left failed");
        }
        manager->_recents_screen_snapshot_move_hor = true;
    } else if ((gesture_info->direction & ESP_UI_GESTURE_DIR_RIGHT) && !manager->_recents_screen_snapshot_move_hor &&
               !manager->_recents_screen_snapshot_move_ver) {
        if (!manager->processRecentsScreenMoveRight()) {
            ESP_UI_LOGE("Recents screen app move right failed");
        }
        manager->_recents_screen_snapshot_move_hor = true;
    }

    start_point = (lv_point_t) {
        (lv_coord_t)gesture_info->start_x, (lv_coord_t)gesture_info->start_y,
    };
    drag_app_id = recents_screen->getSnapshotIdPointIn(start_point);
    data = &manager->_data;
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
    if (data->flags.enable_recents_screen_snapshot_drag && !manager->_recents_screen_snapshot_move_hor &&
            (((distance_y > 0) && (app_y_current < app_y_max)) || ((distance_y < 0) && (app_y_current > app_y_min)))) {
        app_y_target = min(max(app_y_current + distance_y, app_y_min), app_y_max);
        ESP_UI_CHECK_FALSE_EXIT(recents_screen->moveSnapshotY(drag_app_id, app_y_target),
                                "Recents screen move snapshot(%d) y failed", drag_app_id);
        manager->_recents_screen_snapshot_move_ver = true;
    }

    manager->_recents_screen_last_point = (lv_point_t) {
        (lv_coord_t)gesture_info->stop_x, (lv_coord_t)gesture_info->stop_y
    };
}

void ESP_UI_PhoneManager::onRecentsScreenGestureReleaseEventCallback(lv_event_t *event)
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
    int drag_app_id = -1;
    int distance_move_up_threshold = 0;
    int distance_move_down_threshold = 0;
    int distance_move_up_exit_threshold = 0;
    int distance_y = 0;
    int state = RECENTS_SCREEN_NONE;
    lv_event_code_t event_code = _LV_EVENT_LAST;
    lv_point_t start_point = { 0 };
    ESP_UI_PhoneManager *manager = nullptr;
    ESP_UI_RecentsScreen *recents_screen = nullptr;
    ESP_UI_GestureInfo_t *gesture_info = nullptr;
    ESP_UI_CoreAppEventData_t app_event_data = {
        .type = ESP_UI_CORE_APP_EVENT_TYPE_MAX,
    };
    const ESP_UI_PhoneManagerData_t *data = nullptr;

    ESP_UI_CHECK_NULL_EXIT(event, "Invalid event");

    manager = static_cast<ESP_UI_PhoneManager *>(lv_event_get_user_data(event));
    ESP_UI_CHECK_NULL_EXIT(manager, "Invalid manager");
    recents_screen = manager->_home._recents_screen.get();
    ESP_UI_CHECK_NULL_EXIT(recents_screen, "Invalid recents_screen");
    gesture_info = (ESP_UI_GestureInfo_t *)lv_event_get_param(event);
    ESP_UI_CHECK_NULL_EXIT(gesture_info, "Invalid gesture info");
    event_code = manager->_core.getAppEventCode();
    ESP_UI_CHECK_FALSE_EXIT(esp_ui_core_utils_check_event_code_valid(event_code), "Invalid event code");

    // Check if the recents_screen is not pressed or the snapshot is moved
    if (!manager->_recents_screen_pressed || manager->_recents_screen_snapshot_move_hor) {
        return;
    }

    if (manager->_recents_screen_active_app == nullptr) {
        goto process;
    }

    start_point = (lv_point_t) {
        (lv_coord_t)gesture_info->start_x, (lv_coord_t)gesture_info->start_y,
    };
    drag_app_id = recents_screen->getSnapshotIdPointIn(start_point);
    if (drag_app_id < 0) {
        goto process;
    }

    if (manager->_recents_screen_snapshot_move_ver) {
        state |= RECENTS_SCREEN_SNAPSHOT_MOVE_BACK;
    }

    data = &manager->_data;
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
    ESP_UI_LOGD("Recents screen release");

    if (state == RECENTS_SCREEN_NONE) {
        state = RECENTS_SCREEN_HIDE;
    }

    if (state & RECENTS_SCREEN_SNAPSHOT_MOVE_BACK) {
        recents_screen->moveSnapshotY(drag_app_id, recents_screen->getSnapshotOriginY(drag_app_id));
        ESP_UI_LOGD("Recents screen move snapshot back");
    }

    if (state & RECENTS_SCREEN_APP_CLOSE) {
        ESP_UI_LOGD("Recents screen close app(%d)", drag_app_id);
        app_event_data.id = drag_app_id;
        app_event_data.type = ESP_UI_CORE_APP_EVENT_TYPE_STOP;
    } else if (state & RECENTS_SCREEN_APP_SHOW) {
        ESP_UI_LOGD("Recents screen start app(%d)", drag_app_id);
        app_event_data.id = drag_app_id;
        app_event_data.type = ESP_UI_CORE_APP_EVENT_TYPE_START;
    }

    if (state & RECENTS_SCREEN_HIDE) {
        ESP_UI_LOGD("Hide recents_screen");
        ESP_UI_CHECK_FALSE_EXIT(manager->processRecentsScreenHide(), "Hide recents_screen failed");
    }

    manager->_recents_screen_pressed = false;
    if (app_event_data.type != ESP_UI_CORE_APP_EVENT_TYPE_MAX) {
        // Get the index of the next dragging snapshot before close it
        recents_screen_active_snapshot_index = max(manager->getRunningAppIndexById(drag_app_id) - 1, 0);
        // Start or close the dragging app
        ESP_UI_CHECK_FALSE_EXIT(manager->_core.sendAppEvent(&app_event_data), "Core send app event failed");
        // Scroll to another running app snapshot if the dragging app is closed
        if (app_event_data.type == ESP_UI_CORE_APP_EVENT_TYPE_STOP) {
            manager->_recents_screen_active_app = manager->getRunningAppByIdenx(recents_screen_active_snapshot_index);
            if (manager->_recents_screen_active_app != nullptr) {
                // If there are active apps, scroll to the previous app snapshot
                ESP_UI_LOGD("Recents screen scroll snapshot(%d) to %d", manager->_recents_screen_active_app->getId(),
                            recents_screen_active_snapshot_index);
                if (!recents_screen->scrollToSnapshotByIndex(recents_screen_active_snapshot_index)) {
                    ESP_UI_LOGE("Recents screen scroll snapshot(%d) to %d failed",
                                manager->_recents_screen_active_app->getId(),
                                recents_screen_active_snapshot_index);
                }
            }
        }
    }
}

void ESP_UI_PhoneManager::onRecentsScreenSnapshotDeletedEventCallback(lv_event_t *event)
{
    int app_id = -1;
    ESP_UI_PhoneManager *manager = nullptr;
    ESP_UI_RecentsScreen *recents_screen = nullptr;
    ESP_UI_CoreAppEventData_t app_event_data = {
        .type = ESP_UI_CORE_APP_EVENT_TYPE_STOP,
    };

    ESP_UI_LOGD("Recents screen snapshot deleted event callback");
    ESP_UI_CHECK_NULL_EXIT(event, "Invalid event object");

    manager = static_cast<ESP_UI_PhoneManager *>(lv_event_get_user_data(event));
    ESP_UI_CHECK_NULL_EXIT(manager, "Invalid manager");
    recents_screen = manager->_home._recents_screen.get();
    ESP_UI_CHECK_NULL_EXIT(recents_screen, "Invalid recents_screen");
    app_id = (intptr_t)lv_event_get_param(event);
    app_event_data.id = app_id;

    ESP_UI_CHECK_FALSE_EXIT(manager->_core.sendAppEvent(&app_event_data), "Core send app event failed");

    if (recents_screen->getSnapshotCount() == 0) {
        ESP_UI_LOGD("No snapshot in the recents_screen");
        manager->_recents_screen_active_app = nullptr;
        if (manager->_home.getData().flags.enable_recents_screen_hide_when_no_snapshot) {
            ESP_UI_CHECK_FALSE_EXIT(manager->processRecentsScreenHide(), "Manager hide recents_screen failed");
        }
    }
}
