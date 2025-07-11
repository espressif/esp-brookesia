/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <cmath>
#include "esp_brookesia_systems_internal.h"
#if !ESP_BROOKESIA_SPEAKER_MANAGER_ENABLE_DEBUG_LOG
#   define ESP_BROOKESIA_UTILS_DISABLE_DEBUG_LOG
#endif
#include "private/esp_brookesia_speaker_utils.hpp"
#include "widgets/gesture/esp_brookesia_gesture.hpp"
#include "lvgl/esp_brookesia_lv_helper.hpp"
#include "esp_brookesia_speaker_manager.hpp"
#include "esp_brookesia_speaker.hpp"
#include "boost/thread.hpp"

using namespace std;
using namespace esp_brookesia::gui;
using namespace esp_brookesia::ai_framework;

namespace esp_brookesia::speaker {

Manager::Manager(ESP_Brookesia_Core &core_in, Display &display_in, const ManagerData &data_in):
    ESP_Brookesia_CoreManager(core_in, core_in.getCoreData().manager),
    display(display_in),
    data(data_in)
{
}

Manager::~Manager()
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();

    if (!del()) {
        ESP_UTILS_LOGE("Failed to delete");
    }

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
}

bool Manager::calibrateData(const ESP_Brookesia_StyleSize_t screen_size, Display &display, ManagerData &data)
{
    ESP_UTILS_LOG_TRACE_GUARD();
    ESP_UTILS_LOGD(
        "Param: screen_size(width: %d, height: %d), display(%p), data(%p)",
        screen_size.width, screen_size.height, &display, &data
    );

    if (data.flags.enable_gesture) {
        ESP_UTILS_CHECK_FALSE_RETURN(Gesture::calibrateData(screen_size, display, data.gesture), false,
                                     "Calibrate gesture data failed");
    }

    if (data.flags.enable_quick_settings_top_threshold_percent) {
        ESP_UTILS_CHECK_VALUE_RETURN(
            data.quick_settings.top_threshold_percent, 1, 100, false, "Invalid top threshold percent"
        );
        data.quick_settings.top_threshold = data.quick_settings.top_threshold_percent * screen_size.height / 100;
    }
    if (data.flags.enable_quick_settings_bottom_threshold_percent) {
        ESP_UTILS_CHECK_VALUE_RETURN(
            data.quick_settings.bottom_threshold_percent, 1, 100, false, "Invalid bottom threshold percent"
        );
        data.quick_settings.bottom_threshold = data.quick_settings.bottom_threshold_percent * screen_size.height / 100;
    }
    return true;
}

bool Manager::begin(void)
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();

    ESP_UTILS_CHECK_FALSE_RETURN(!checkInitialized(), false, "Already initialized");

    _ai_buddy = AI_Buddy::requestInstance();
    ESP_UTILS_CHECK_NULL_RETURN(_ai_buddy, false, "Failed to get ai buddy instance");

    // Display
    auto main_screen = display.getMainScreen();
    ESP_UTILS_CHECK_NULL_RETURN(main_screen, false, "Main screen is not initialized");
    lv_obj_add_event_cb(main_screen, [](lv_event_t *event) {
        ESP_UTILS_LOG_TRACE_GUARD();
        Manager *manager = nullptr;

        ESP_UTILS_LOGD("Display main screen load event callback");
        ESP_UTILS_CHECK_NULL_EXIT(event, "Invalid event");

        manager = static_cast<Manager *>(lv_event_get_user_data(event));
        ESP_UTILS_CHECK_NULL_EXIT(manager, "Invalid manager");

        ESP_UTILS_CHECK_FALSE_EXIT(
            manager->processGestureScreenChange(ESP_BROOKESIA_SPEAKER_MANAGER_SCREEN_MAIN, nullptr),
            "Process gesture failed"
        );
    }, LV_EVENT_SCREEN_LOADED, this);

    display.getDummyDrawMask()->addEventCallback([](lv_event_t *event) {
        ESP_UTILS_LOG_TRACE_GUARD();
        ESP_UTILS_LOGD("Param: event(0x%p)", event);

        auto manager = static_cast<Manager *>(lv_event_get_user_data(event));
        ESP_UTILS_CHECK_NULL_EXIT(manager, "Invalid manager");

        ESP_UTILS_CHECK_FALSE_EXIT(
            manager->processDisplayScreenChange(ESP_BROOKESIA_SPEAKER_MANAGER_SCREEN_MAIN, nullptr),
            "Process screen change failed"
        );
    }, LV_EVENT_LONG_PRESSED, this);
    _draw_dummy_timer = std::make_unique<LvTimer>([this](void *) {
        ESP_UTILS_CHECK_FALSE_EXIT(
            this->processDisplayScreenChange(ESP_BROOKESIA_SPEAKER_MANAGER_SCREEN_DRAW_DUMMY, nullptr),
            "Process screen change failed"
        );
    }, data.ai_buddy_resume_time_ms, this);

    // Gesture
    if (data.flags.enable_gesture) {
        // Get the touch device
        lv_indev_t *touch = _core.getTouchDevice();
        if (touch == nullptr) {
            ESP_UTILS_LOGW("No touch device is set, try to use default touch device");

            touch = getLvInputDev(_core.getDisplayDevice(), LV_INDEV_TYPE_POINTER);
            ESP_UTILS_CHECK_NULL_RETURN(touch, false, "No touch device is initialized");
            ESP_UTILS_LOGW("Using default touch device(@0x%p)", touch);

            ESP_UTILS_CHECK_FALSE_RETURN(_core.setTouchDevice(touch), false, "Core set touch device failed");
        }

        // Create and begin gesture
        _gesture = std::make_unique<Gesture>(_core, data.gesture);
        ESP_UTILS_CHECK_NULL_RETURN(_gesture, false, "Create gesture failed");
        ESP_UTILS_CHECK_FALSE_RETURN(_gesture->begin(display.getSystemScreenObject()), false, "Gesture begin failed");
        ESP_UTILS_CHECK_FALSE_RETURN(_gesture->setMaskObjectVisible(false), false, "Hide mask object failed");
        ESP_UTILS_CHECK_FALSE_RETURN(
            _gesture->setIndicatorBarVisible(GESTURE_INDICATOR_BAR_TYPE_LEFT, false),
            false, "Set left indicator bar visible failed"
        );
        ESP_UTILS_CHECK_FALSE_RETURN(
            _gesture->setIndicatorBarVisible(GESTURE_INDICATOR_BAR_TYPE_RIGHT, false),
            false, "Set right indicator bar visible failed"
        );
        ESP_UTILS_CHECK_FALSE_RETURN(
            _gesture->setIndicatorBarVisible(GESTURE_INDICATOR_BAR_TYPE_BOTTOM, true),
            false, "Set bottom indicator bar visible failed"
        );

        _flags.enable_gesture_navigation = true;
        // Navigation
        lv_obj_add_event_cb(_gesture->getEventObj(), [](lv_event_t *event) {
            ESP_UTILS_LOG_TRACE_GUARD();

            ESP_UTILS_LOGD("Param: event(%p)", event);
            ESP_UTILS_CHECK_NULL_EXIT(event, "Invalid event");

            auto manager = static_cast<Manager *>(lv_event_get_user_data(event));
            ESP_UTILS_CHECK_NULL_EXIT(manager, "Invalid manager");
            ESP_UTILS_CHECK_FALSE_EXIT(
                manager->processNavigationGesturePressingEvent(event), "Process navigation gesture pressing event failed"
            );
        }, _gesture->getPressingEventCode(), this);
        lv_obj_add_event_cb(_gesture->getEventObj(), [](lv_event_t *event) {
            ESP_UTILS_LOG_TRACE_GUARD();

            ESP_UTILS_LOGD("Param: event(%p)", event);
            ESP_UTILS_CHECK_NULL_EXIT(event, "Invalid event");

            auto manager = static_cast<Manager *>(lv_event_get_user_data(event));
            ESP_UTILS_CHECK_NULL_EXIT(manager, "Invalid manager");
            ESP_UTILS_CHECK_FALSE_EXIT(
                manager->processNavigationGestureReleaseEvent(event), "Process navigation gesture release event failed"
            );
        }, _gesture->getReleaseEventCode(), this);
        // Mask object and indicator bar
        lv_obj_add_event_cb(_gesture->getEventObj(), [](lv_event_t *event) {
            ESP_UTILS_LOG_TRACE_GUARD();

            ESP_UTILS_LOGD("Param: event(%p)", event);
            ESP_UTILS_CHECK_NULL_EXIT(event, "Invalid event");

            auto manager = static_cast<Manager *>(lv_event_get_user_data(event));
            ESP_UTILS_CHECK_NULL_EXIT(manager, "Invalid manager");
            ESP_UTILS_CHECK_FALSE_EXIT(
                manager->processMaskIndicatorBarGesturePressingEvent(event), "Process mask indicator bar gesture pressing event failed"
            );
        }, _gesture->getPressingEventCode(), this);
        lv_obj_add_event_cb(_gesture->getEventObj(), [](lv_event_t *event) {
            ESP_UTILS_LOG_TRACE_GUARD();

            ESP_UTILS_LOGD("Param: event(%p)", event);
            ESP_UTILS_CHECK_NULL_EXIT(event, "Invalid event");

            auto manager = static_cast<Manager *>(lv_event_get_user_data(event));
            ESP_UTILS_CHECK_NULL_EXIT(manager, "Invalid manager");
            ESP_UTILS_CHECK_FALSE_EXIT(
                manager->processMaskIndicatorBarGestureReleaseEvent(event), "Process mask indicator bar gesture release event failed"
            );
        }, _gesture->getReleaseEventCode(), this);
    }

    if (_gesture != nullptr) {
        auto onAppLauncherGestureEventCallback = [](lv_event_t *event) {
            ESP_UTILS_LOG_TRACE_GUARD();

            ESP_UTILS_LOGD("Param: event(%p)", event);
            ESP_UTILS_CHECK_NULL_EXIT(event, "Invalid event");

            auto manager = static_cast<Manager *>(lv_event_get_user_data(event));
            ESP_UTILS_CHECK_NULL_EXIT(manager, "Invalid manager");
            ESP_UTILS_CHECK_FALSE_EXIT(
                manager->processAppLauncherGestureEvent(event), "Process app launcher gesture event failed"
            );
        };
        // App Launcher
        lv_obj_add_event_cb(
            _gesture->getEventObj(), onAppLauncherGestureEventCallback, _gesture->getPressingEventCode(), this
        );
        lv_obj_add_event_cb(
            _gesture->getEventObj(), onAppLauncherGestureEventCallback, _gesture->getReleaseEventCode(), this
        );

        // // Quick Settings
        // lv_obj_add_event_cb(_gesture->getEventObj(), [](lv_event_t *event) {
        //     ESP_UTILS_LOG_TRACE_GUARD();

        //     ESP_UTILS_LOGD("Param: event(%p)", event);
        //     ESP_UTILS_CHECK_NULL_EXIT(event, "Invalid event");

        //     auto manager = static_cast<Manager *>(lv_event_get_user_data(event));
        //     ESP_UTILS_CHECK_NULL_EXIT(manager, "Invalid manager");
        //     ESP_UTILS_CHECK_FALSE_EXIT(
        //         manager->processQuickSettingsGesturePressEvent(event),
        //         "Process quick settings gesture press event failed"
        //     );
        // }, _gesture->getPressEventCode(), this);
        // lv_obj_add_event_cb(_gesture->getEventObj(), [](lv_event_t *event) {
        //     ESP_UTILS_LOG_TRACE_GUARD();

        //     ESP_UTILS_LOGD("Param: event(%p)", event);
        //     ESP_UTILS_CHECK_NULL_EXIT(event, "Invalid event");

        //     auto manager = static_cast<Manager *>(lv_event_get_user_data(event));
        //     ESP_UTILS_CHECK_NULL_EXIT(manager, "Invalid manager");
        //     ESP_UTILS_CHECK_FALSE_EXIT(
        //         manager->processQuickSettingsGesturePressingEvent(event),
        //         "Process quick settings gesture pressing event failed"
        //     );
        // }, _gesture->getPressingEventCode(), this);
        // lv_obj_add_event_cb(_gesture->getEventObj(), [](lv_event_t *event) {
        //     ESP_UTILS_LOG_TRACE_GUARD();

        //     ESP_UTILS_LOGD("Param: event(%p)", event);
        //     ESP_UTILS_CHECK_NULL_EXIT(event, "Invalid event");

        //     auto manager = static_cast<Manager *>(lv_event_get_user_data(event));
        //     ESP_UTILS_CHECK_NULL_EXIT(manager, "Invalid manager");
        //     ESP_UTILS_CHECK_FALSE_EXIT(
        //         manager->processQuickSettingsGestureReleaseEvent(event),
        //         "Process quick settings gesture release event failed"
        //     );
        // }, _gesture->getReleaseEventCode(), this);
    }
    _flags.is_initialized = true;

    // Then load the ai_buddy screen
    ESP_UTILS_CHECK_FALSE_RETURN(
        processDisplayScreenChange(ESP_BROOKESIA_SPEAKER_MANAGER_SCREEN_DRAW_DUMMY, nullptr), false,
        "Process screen change failed"
    );

    return true;
}

bool Manager::del(void)
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();

    if (!checkInitialized()) {
        goto end;
    }

    if (_gesture != nullptr) {
        _gesture = nullptr;
    }
    if (_draw_dummy_timer != nullptr) {
        _draw_dummy_timer = nullptr;
    }
    _flags.is_initialized = false;

end:
    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

bool Manager::processAppRunExtra(ESP_Brookesia_CoreApp *app)
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();
    ESP_UTILS_LOGD("Param: app(%p)", app);

    App *speaker_app = static_cast<App *>(app);

    ESP_UTILS_CHECK_NULL_RETURN(speaker_app, false, "Invalid speaker app");

    ESP_UTILS_CHECK_FALSE_RETURN(processDisplayScreenChange(ESP_BROOKESIA_SPEAKER_MANAGER_SCREEN_APP, speaker_app), false,
                                 "Process screen change failed");

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

bool Manager::processAppResumeExtra(ESP_Brookesia_CoreApp *app)
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();
    ESP_UTILS_LOGD("Param: app(%p)", app);

    App *speaker_app = static_cast<App *>(app);

    ESP_UTILS_CHECK_NULL_RETURN(speaker_app, false, "Invalid speaker app");

    ESP_UTILS_CHECK_FALSE_RETURN(processDisplayScreenChange(ESP_BROOKESIA_SPEAKER_MANAGER_SCREEN_APP, speaker_app), false,
                                 "Process screen change failed");

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

bool Manager::processAppCloseExtra(ESP_Brookesia_CoreApp *app)
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();
    ESP_UTILS_LOGD("Param: app(%p)", app);

    App *speaker_app = static_cast<App *>(app);

    ESP_UTILS_CHECK_NULL_RETURN(speaker_app, false, "Invalid speaker app");

    if (getActiveApp() == app) {
        // Switch to the main screen to release the app resources
        ESP_UTILS_CHECK_FALSE_RETURN(processDisplayScreenChange(ESP_BROOKESIA_SPEAKER_MANAGER_SCREEN_MAIN, nullptr), false,
                                     "Process screen change failed");
    }

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

bool Manager::processDisplayScreenChange(ManagerScreen screen, void *param)
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();
    ESP_UTILS_LOGD("Param: screen(%d), param(%p)", screen, param);

    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_UTILS_CHECK_FALSE_RETURN(screen < ESP_BROOKESIA_SPEAKER_MANAGER_SCREEN_MAX, false, "Invalid screen");

    if (_display_active_screen == screen) {
        ESP_UTILS_LOGW("Already on the screen");
        goto end;
    }

    ESP_UTILS_CHECK_FALSE_RETURN(processGestureScreenChange(screen, param), false, "Process gesture failed");

    if ((screen != ESP_BROOKESIA_SPEAKER_MANAGER_SCREEN_DRAW_DUMMY) &&
            (_display_active_screen == ESP_BROOKESIA_SPEAKER_MANAGER_SCREEN_DRAW_DUMMY)) {
        _ai_buddy->pause();
        ESP_UTILS_CHECK_FALSE_RETURN(display.processDummyDraw(false), false, "Display load ai_buddy failed");
    }

    switch (screen) {
    case ESP_BROOKESIA_SPEAKER_MANAGER_SCREEN_MAIN:
        ESP_UTILS_CHECK_FALSE_RETURN(display.processMainScreenLoad(), false, "Display load main screen failed");
        if (_draw_dummy_timer != nullptr) {
            ESP_UTILS_CHECK_FALSE_RETURN(_draw_dummy_timer->restart(), false, "Restart ai_buddy resume timer failed");
        }
        break;
    case ESP_BROOKESIA_SPEAKER_MANAGER_SCREEN_APP:
        if (_draw_dummy_timer != nullptr) {
            ESP_UTILS_CHECK_FALSE_RETURN(_draw_dummy_timer->pause(), false, "Pause ai_buddy resume timer failed");
        }
        if (display.getQuickSettings().isVisible()) {
            ESP_UTILS_CHECK_FALSE_RETURN(
                processQuickSettingsMoveTop(), false, "Process quick settings move top failed"
            );
        }
        break;
    case ESP_BROOKESIA_SPEAKER_MANAGER_SCREEN_DRAW_DUMMY:
        ESP_UTILS_CHECK_FALSE_RETURN(display.processDummyDraw(true), false, "Display load ai_buddy failed");
        if (_ai_buddy->isPause()) {
            _ai_buddy->resume();
        }
        if (_draw_dummy_timer != nullptr) {
            ESP_UTILS_CHECK_FALSE_RETURN(_draw_dummy_timer->pause(), false, "Pause ai_buddy resume timer failed");
        }
        break;
    default:
        break;
    }

    _display_active_screen = screen;

end:
    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

bool Manager::processAI_BuddyResumeTimer(void)
{
    ESP_UTILS_LOGD("Process ai_buddy resume timer");
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    if (_draw_dummy_timer != nullptr) {
        ESP_UTILS_CHECK_FALSE_RETURN(_draw_dummy_timer->reset(), false, "Clear ai_buddy resume timer failed");
    }

    return true;
}

bool Manager::processAppLauncherGestureEvent(lv_event_t *event)
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();

    ESP_UTILS_LOGD("Param: event(%p)", event);
    ESP_UTILS_CHECK_NULL_RETURN(event, false, "Invalid event");

    lv_event_code_t event_code = _LV_EVENT_LAST;
    AppLauncher *app_launcher = nullptr;
    Gesture *gesture = nullptr;
    GestureInfo *gesture_info = nullptr;
    GestureDirection dir_type = GESTURE_DIR_NONE;

    gesture = _gesture.get();
    ESP_UTILS_CHECK_NULL_GOTO(gesture, end, "Invalid gesture");
    app_launcher = &display._app_launcher;
    ESP_UTILS_CHECK_NULL_GOTO(app_launcher, end, "Invalid app launcher");
    event_code = lv_event_get_code(event);
    ESP_UTILS_CHECK_FALSE_GOTO(
        (event_code == gesture->getPressingEventCode()) || (event_code == gesture->getReleaseEventCode()), end,
        "Invalid event code"
    );

    // Check if the display active screen is main, if so, clear the ai_buddy_resume_timer
    if (getDisplayActiveScreen() == ESP_BROOKESIA_SPEAKER_MANAGER_SCREEN_MAIN) {
        processAI_BuddyResumeTimer();
    } else {
        goto end;
    }

    // Here is to prevent detecting gestures when the app exits, which could trigger unexpected behaviors
    if (event_code == gesture->getReleaseEventCode()) {
        if (_flags.is_app_launcher_gesture_disabled) {
            _flags.is_app_launcher_gesture_disabled = false;
            goto end;
        }
    }

    // Check if the app launcher is visible
    if (!app_launcher->checkVisible() || _flags.is_app_launcher_gesture_disabled) {
        goto end;
    }

    dir_type = _app_launcher_gesture_dir;
    // Check if the dir type is already set. If so, just ignore and return
    if (dir_type != GESTURE_DIR_NONE) {
        // Check if the gesture is released
        if (event_code == gesture->getReleaseEventCode()) {   // If so, reset the navigation type
            dir_type = GESTURE_DIR_NONE;
            goto next;
        }
        goto end;
    }

    gesture_info = (GestureInfo *)lv_event_get_param(event);
    ESP_UTILS_CHECK_NULL_GOTO(gesture_info, end, "Invalid gesture info");
    // Check if there is a gesture
    if (gesture_info->direction == GESTURE_DIR_NONE) {
        goto end;
    }

    dir_type = gesture_info->direction;
    switch (dir_type) {
    case GESTURE_DIR_LEFT:
        ESP_UTILS_LOGD("App table gesture left");
        ESP_UTILS_CHECK_FALSE_GOTO(app_launcher->scrollToRightPage(), end, "App table scroll to right page failed");
        break;
    case GESTURE_DIR_RIGHT:
        ESP_UTILS_LOGD("App table gesture right");
        ESP_UTILS_CHECK_FALSE_GOTO(app_launcher->scrollToLeftPage(), end, "App table scroll to left page failed");
        break;
    default:
        break;
    }

next:
    _app_launcher_gesture_dir = dir_type;

end:
    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

bool Manager::processGestureScreenChange(ManagerScreen screen, void *param)
{
    const AppData_t *app_data = nullptr;

    ESP_UTILS_LOGD("Process gesture when screen change");
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_UTILS_CHECK_FALSE_RETURN(screen < ESP_BROOKESIA_SPEAKER_MANAGER_SCREEN_MAX, false, "Invalid screen");

    switch (screen) {
    case ESP_BROOKESIA_SPEAKER_MANAGER_SCREEN_MAIN:
        _flags.enable_gesture_navigation = data.flags.enable_app_launcher_gesture_navigation;
        _flags.enable_gesture_navigation_back = 0;
        _flags.enable_gesture_navigation_home = _flags.enable_gesture_navigation;
        _flags.enable_gesture_navigation_recents_app = _flags.enable_gesture_navigation;
        _flags.enable_gesture_show_mask_left_right_edge = false;
        _flags.enable_gesture_show_mask_bottom_edge = false;
        _flags.enable_gesture_show_left_right_indicator_bar = false;
        _flags.enable_gesture_show_bottom_indicator_bar = _flags.enable_gesture_navigation;
        break;
    case ESP_BROOKESIA_SPEAKER_MANAGER_SCREEN_DRAW_DUMMY:
        _flags.enable_gesture_navigation = false;
        _flags.enable_gesture_navigation_back = false;
        _flags.enable_gesture_navigation_home = false;
        _flags.enable_gesture_navigation_recents_app = false;
        _flags.enable_gesture_show_mask_left_right_edge = false;
        _flags.enable_gesture_show_mask_bottom_edge = false;
        _flags.enable_gesture_show_left_right_indicator_bar = false;
        _flags.enable_gesture_show_bottom_indicator_bar = false;
        break;
    case ESP_BROOKESIA_SPEAKER_MANAGER_SCREEN_APP:
        ESP_UTILS_CHECK_NULL_RETURN(param, false, "Invalid param");
        app_data = &((App *)param)->getActiveData();
        _flags.enable_gesture_navigation = app_data->flags.enable_navigation_gesture;
        _flags.enable_gesture_navigation_back = (_flags.enable_gesture_navigation &&
                                                data.flags.enable_gesture_navigation_back);
        _flags.enable_gesture_navigation_home = _flags.enable_gesture_navigation;
        _flags.enable_gesture_navigation_recents_app = _flags.enable_gesture_navigation_home;
        _flags.enable_gesture_show_mask_left_right_edge = _flags.enable_gesture_navigation;
        _flags.enable_gesture_show_mask_bottom_edge = _flags.enable_gesture_navigation;
        _flags.enable_gesture_show_left_right_indicator_bar = _flags.enable_gesture_show_mask_left_right_edge;
        _flags.enable_gesture_show_bottom_indicator_bar = _flags.enable_gesture_show_mask_bottom_edge;
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
            _gesture->setIndicatorBarVisible(GESTURE_INDICATOR_BAR_TYPE_LEFT, false), false,
            "Gesture set left indicator bar visible failed"
        );
        ESP_UTILS_CHECK_FALSE_RETURN(
            _gesture->setIndicatorBarVisible(GESTURE_INDICATOR_BAR_TYPE_RIGHT, false), false,
            "Gesture set right indicator bar visible failed"
        );
    }
    ESP_UTILS_CHECK_FALSE_RETURN(
        _gesture->setIndicatorBarVisible(
            GESTURE_INDICATOR_BAR_TYPE_BOTTOM, _flags.enable_gesture_show_bottom_indicator_bar
        ), false, "Gesture set bottom indicator bar visible failed"
    );

    return true;
}

bool Manager::processNavigationEvent(ESP_Brookesia_CoreNavigateType_t type)
{
    bool ret = true;
    App *active_app = static_cast<App *>(getActiveApp());

    ESP_UTILS_LOGD("Process navigation event type(%d)", type);

    // Disable the gesture function of widgets
    _flags.is_app_launcher_gesture_disabled = true;

    switch (type) {
    case ESP_BROOKESIA_CORE_NAVIGATE_TYPE_BACK:
        if (active_app == nullptr) {
            goto end;
        }
        // Call app back function
        ESP_UTILS_CHECK_FALSE_GOTO(ret = (active_app->back()), end, "App(%d) back failed", active_app->getId());
        break;
    case ESP_BROOKESIA_CORE_NAVIGATE_TYPE_HOME:
    case ESP_BROOKESIA_CORE_NAVIGATE_TYPE_RECENTS_SCREEN:
        if (active_app == nullptr) {
            processDisplayScreenChange(ESP_BROOKESIA_SPEAKER_MANAGER_SCREEN_DRAW_DUMMY, nullptr);
            goto end;
        }
        /* Process app close */
        // Call app close function
        ESP_UTILS_CHECK_FALSE_GOTO(ret = processAppClose(active_app), end, "App(%d) close failed", active_app->getId());
        resetActiveApp();
        break;
    default:
        break;
    }

end:
    return ret;
}

bool Manager::processQuickSettingsGesturePressEvent(lv_event_t *event)
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();

    ESP_UTILS_LOGD("Param: event(%p)", event);
    ESP_UTILS_CHECK_NULL_RETURN(event, false, "Invalid event");

    auto dummy_draw_mask = display.getDummyDrawMask();
    if (!dummy_draw_mask->hasFlags(gui::STYLE_FLAG_HIDDEN)) {
        goto end;
    }

    {
        auto gesture_info = (GestureInfo *)lv_event_get_param(event);
        ESP_UTILS_CHECK_NULL_RETURN(gesture_info, false, "Invalid gesture info");

        auto &quick_settings = display.getQuickSettings();

        if (!quick_settings.isVisible()) {
            if (gesture_info->start_area == GESTURE_AREA_TOP_EDGE) {
                _flags.is_quick_settings_enabled = true;
                ESP_UTILS_CHECK_FALSE_RETURN(
                    quick_settings.setVisible(true), false, "Set quick settings visible failed"
                );
                ESP_UTILS_CHECK_FALSE_RETURN(
                    quick_settings.scrollBack(), false, "Scroll quick settings back to top failed"
                );
            }
        } else {
            if (gesture_info->start_area == GESTURE_AREA_BOTTOM_EDGE) {
                _flags.is_quick_settings_enabled = true;
            }
        }

        if (_flags.is_quick_settings_enabled) {
            ESP_UTILS_CHECK_FALSE_RETURN(
                quick_settings.setScrollable(false), false, "Set quick settings scrollable failed"
            );
        }
    }

end:
    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

bool Manager::processQuickSettingsGesturePressingEvent(lv_event_t *event)
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();

    ESP_UTILS_LOGD("Param: event(%p)", event);
    ESP_UTILS_CHECK_NULL_RETURN(event, true, "Invalid event");

    // Exit if the quick settings is not enabled
    if (!_flags.is_quick_settings_enabled) {
        goto end;
    }

    {
        auto gesture_info = (GestureInfo *)lv_event_get_param(event);
        ESP_UTILS_CHECK_NULL_RETURN(gesture_info, false, "Invalid gesture info");

        auto &quick_settings = display.getQuickSettings();
        ESP_UTILS_CHECK_FALSE_RETURN(
            quick_settings.moveY_To(gesture_info->stop_y - 360), false, "Move quick settings failed"
        );
    }

end:
    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

bool Manager::processQuickSettingsGestureReleaseEvent(lv_event_t *event)
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();

    ESP_UTILS_LOGD("Param: event(%p)", event);
    ESP_UTILS_CHECK_NULL_RETURN(event, true, "Invalid event");

    {
        // Exit if the quick settings is not enabled
        if (!_flags.is_quick_settings_enabled) {
            goto end;
        }

        auto gesture_info = (GestureInfo *)lv_event_get_param(event);
        ESP_UTILS_CHECK_NULL_RETURN(gesture_info, false, "Invalid gesture info");
        auto &quick_settings = display.getQuickSettings();

        if (((gesture_info->start_area == GESTURE_AREA_TOP_EDGE) &&
                (gesture_info->stop_y > data.quick_settings.top_threshold)) ||
                ((gesture_info->start_area == GESTURE_AREA_BOTTOM_EDGE) &&
                 (gesture_info->stop_y > data.quick_settings.bottom_threshold))) {
            ESP_UTILS_CHECK_FALSE_RETURN(
                processQuickSettingsScrollBottom(), false, "Process quick settings scroll bottom failed"
            );
        } else {
            ESP_UTILS_CHECK_FALSE_RETURN(
                processQuickSettingsScrollTop(), false, "Process quick settings scroll top failed"
            );
        }

        _flags.is_quick_settings_enabled = false;
        ESP_UTILS_CHECK_FALSE_RETURN(
            quick_settings.setScrollable(true), false, "Set quick settings scrollable failed"
        );
    }

end:
    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

bool Manager::processQuickSettingsMoveTop(void)
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();

    auto &quick_settings = display.getQuickSettings();
    // Move the quick settings to the top edge and hide it
    ESP_UTILS_CHECK_FALSE_RETURN(quick_settings.moveY_To(
                                     _gesture->data.threshold.horizontal_edge - _core.getCoreData().screen_size.height), false,
                                 "Move quick settings failed"
                                );
    ESP_UTILS_CHECK_FALSE_RETURN(quick_settings.setVisible(false), false, "Set quick settings visible failed");

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

bool Manager::processQuickSettingsScrollTop(void)
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();

    auto &quick_settings = display.getQuickSettings();
    if (quick_settings.isAnimationRunning()) {
        ESP_UTILS_LOGD("Quick settings animation is running, skip");
        return true;
    }

    // Move the quick settings to the bottom edge and hide it
    ESP_UTILS_CHECK_FALSE_RETURN(
        quick_settings.moveY_ToWithAnimation(
            _gesture->data.threshold.horizontal_edge - _core.getCoreData().screen_size.height, false
        ), false, "Move quick settings failed"
    );

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

bool Manager::processQuickSettingsScrollBottom(void)
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();

    auto &quick_settings = display.getQuickSettings();
    if (quick_settings.isAnimationRunning()) {
        ESP_UTILS_LOGD("Quick settings animation is running, skip");
        return true;
    }

    // Move the quick settings to the bottom edge
    ESP_UTILS_CHECK_FALSE_RETURN(
        quick_settings.moveY_ToWithAnimation(0, true), false, "Move quick settings failed"
    );

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

bool Manager::processNavigationGesturePressingEvent(lv_event_t *event)
{
    ESP_UTILS_LOG_TRACE_GUARD();

    ESP_UTILS_LOGD("Param: event(%p)", event);
    ESP_UTILS_CHECK_NULL_RETURN(event, true, "Invalid event");

    GestureInfo *gesture_info = nullptr;
    ESP_Brookesia_CoreNavigateType_t navigation_type = ESP_BROOKESIA_CORE_NAVIGATE_TYPE_MAX;

    // Check if the gesture is released and enabled
    if (!_flags.enable_gesture_navigation || _flags.is_gesture_navigation_disabled || display.getQuickSettings().isVisible()) {
        goto end;
    }

    gesture_info = (GestureInfo *)lv_event_get_param(event);
    ESP_UTILS_CHECK_NULL_RETURN(gesture_info, true, "Invalid gesture info");

    // Check if there is a gesture
    if (gesture_info->direction == GESTURE_DIR_NONE) {
        goto end;
    }

    // Check if there is a "back" gesture
    if ((gesture_info->start_area & (GESTURE_AREA_LEFT_EDGE | GESTURE_AREA_RIGHT_EDGE)) &&
            (gesture_info->direction & GESTURE_DIR_HOR) && _flags.enable_gesture_navigation_back) {
        navigation_type = ESP_BROOKESIA_CORE_NAVIGATE_TYPE_BACK;
    } else if ((gesture_info->start_area & GESTURE_AREA_BOTTOM_EDGE) && (!gesture_info->flags.short_duration) &&
               (gesture_info->direction & GESTURE_DIR_UP) && _flags.enable_gesture_navigation_recents_app) {
        // Check if there is a "recents_screen" gesture
        navigation_type = ESP_BROOKESIA_CORE_NAVIGATE_TYPE_RECENTS_SCREEN;
    }

    // Only process the navigation event if the navigation type is valid
    if (navigation_type != ESP_BROOKESIA_CORE_NAVIGATE_TYPE_MAX) {
        _flags.is_gesture_navigation_disabled = true;
        ESP_UTILS_CHECK_FALSE_RETURN(
            processNavigationEvent(navigation_type), false, "Process navigation event failed"
        );
    }

end:
    return true;
}

bool Manager::processNavigationGestureReleaseEvent(lv_event_t *event)
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();

    GestureInfo *gesture_info = nullptr;
    ESP_Brookesia_CoreNavigateType_t navigation_type = ESP_BROOKESIA_CORE_NAVIGATE_TYPE_MAX;

    _flags.is_gesture_navigation_disabled = false;
    // Check if the gesture is released and enabled
    if (!_flags.enable_gesture_navigation || display.getQuickSettings().isVisible()) {
        goto end;
    }

    gesture_info = (GestureInfo *)lv_event_get_param(event);
    ESP_UTILS_CHECK_NULL_RETURN(gesture_info, true, "Invalid gesture info");
    // Check if there is a gesture
    if (gesture_info->direction == GESTURE_DIR_NONE) {
        goto end;
    }

    // Check if there is a "display" gesture
    if ((gesture_info->start_area & GESTURE_AREA_BOTTOM_EDGE) && (gesture_info->flags.short_duration) &&
            (gesture_info->direction & GESTURE_DIR_UP) && _flags.enable_gesture_navigation_home) {
        navigation_type = ESP_BROOKESIA_CORE_NAVIGATE_TYPE_HOME;
    }

    // Only process the navigation event if the navigation type is valid
    if (navigation_type != ESP_BROOKESIA_CORE_NAVIGATE_TYPE_MAX) {
        ESP_UTILS_CHECK_FALSE_RETURN(
            processNavigationEvent(navigation_type), false, "Process navigation event failed"
        );
    }

end:
    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

bool Manager::processMaskIndicatorBarGesturePressingEvent(lv_event_t *event)
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();

    ESP_UTILS_LOGD("Param: event(%p)", event);
    ESP_UTILS_CHECK_NULL_RETURN(event, true, "Invalid event");

    bool is_gesture_mask_enabled = false;
    int gesture_indicator_offset = 0;
    Gesture *gesture = nullptr;
    GestureInfo *gesture_info = nullptr;
    GestureIndicatorBarType gesture_indicator_bar_type = GESTURE_INDICATOR_BAR_TYPE_MAX;

    gesture = getGesture();
    ESP_UTILS_CHECK_NULL_RETURN(gesture, true, "Invalid gesture");
    gesture_info = (GestureInfo *)lv_event_get_param(event);
    ESP_UTILS_CHECK_NULL_RETURN(gesture_info, true, "Invalid gesture info");

    // Just return if the gesture duration is less than the trigger time
    if (gesture_info->duration_ms < data.gesture_mask_indicator_trigger_time_ms) {
        goto end;
    }

    // Get the type of the indicator bar and the offset of the gesture
    switch (gesture_info->start_area) {
    case GESTURE_AREA_LEFT_EDGE:
        if (_flags.enable_gesture_show_left_right_indicator_bar) {
            gesture_indicator_bar_type = GESTURE_INDICATOR_BAR_TYPE_LEFT;
            gesture_indicator_offset = gesture_info->stop_x - gesture_info->start_x;
        }
        is_gesture_mask_enabled = _flags.enable_gesture_show_mask_left_right_edge;
        break;
    case GESTURE_AREA_RIGHT_EDGE:
        if (_flags.enable_gesture_show_left_right_indicator_bar) {
            gesture_indicator_bar_type = GESTURE_INDICATOR_BAR_TYPE_RIGHT;
            gesture_indicator_offset = gesture_info->start_x - gesture_info->stop_x;
        }
        is_gesture_mask_enabled = _flags.enable_gesture_show_mask_left_right_edge;
        break;
    case GESTURE_AREA_BOTTOM_EDGE:
        if (_flags.enable_gesture_show_bottom_indicator_bar) {
            gesture_indicator_bar_type = GESTURE_INDICATOR_BAR_TYPE_BOTTOM;
            gesture_indicator_offset = gesture_info->start_y - gesture_info->stop_y;
        }
        is_gesture_mask_enabled = _flags.enable_gesture_show_mask_bottom_edge;
        break;
    default:
        break;
    }

    // If the gesture indicator bar type is valid, update the indicator bar
    if (gesture_indicator_bar_type < GESTURE_INDICATOR_BAR_TYPE_MAX) {
        if (gesture->checkIndicatorBarVisible(gesture_indicator_bar_type)) {
            ESP_UTILS_CHECK_FALSE_RETURN(
                gesture->setIndicatorBarLengthByOffset(gesture_indicator_bar_type, gesture_indicator_offset),
                false, "Gesture set bottom indicator bar length by offset failed"
            );
        } else {
            if (gesture->checkIndicatorBarScaleBackAnimRunning(gesture_indicator_bar_type)) {
                ESP_UTILS_CHECK_FALSE_RETURN(
                    gesture->controlIndicatorBarScaleBackAnim(gesture_indicator_bar_type, false), false,
                    "Gesture control indicator bar scale back anim failed"
                );
            }
            ESP_UTILS_CHECK_FALSE_RETURN(
                gesture->setIndicatorBarVisible(gesture_indicator_bar_type, true), false,
                "Gesture set indicator bar visible failed"
            );
        }
    }

    // If the gesture mask is enabled, show the mask object
    if (is_gesture_mask_enabled && !gesture->checkMaskVisible()) {
        ESP_UTILS_CHECK_FALSE_RETURN(
            gesture->setMaskObjectVisible(true), false, "Gesture show mask object failed"
        );
    }

end:
    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

bool Manager::processMaskIndicatorBarGestureReleaseEvent(lv_event_t *event)
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();

    ESP_UTILS_LOGD("Param: event(%p)", event);
    ESP_UTILS_CHECK_NULL_RETURN(event, true, "Invalid event");

    Gesture *gesture = nullptr;
    GestureInfo *gesture_info = nullptr;
    GestureIndicatorBarType gesture_indicator_bar_type = GESTURE_INDICATOR_BAR_TYPE_MAX;

    ESP_UTILS_CHECK_NULL_RETURN(event, true, "Invalid event");
    gesture = getGesture();
    ESP_UTILS_CHECK_NULL_RETURN(gesture, true, "Invalid gesture");
    gesture_info = (GestureInfo *)lv_event_get_param(event);
    ESP_UTILS_CHECK_NULL_RETURN(gesture_info, true, "Invalid gesture info");

    // Update the mask object and indicator bar of the gesture
    ESP_UTILS_CHECK_FALSE_RETURN(
        gesture->setMaskObjectVisible(false), false, "Gesture hide mask object failed"
    );
    switch (gesture_info->start_area) {
    case GESTURE_AREA_LEFT_EDGE:
        gesture_indicator_bar_type = GESTURE_INDICATOR_BAR_TYPE_LEFT;
        break;
    case GESTURE_AREA_RIGHT_EDGE:
        gesture_indicator_bar_type = GESTURE_INDICATOR_BAR_TYPE_RIGHT;
        break;
    case GESTURE_AREA_BOTTOM_EDGE:
        gesture_indicator_bar_type = GESTURE_INDICATOR_BAR_TYPE_BOTTOM;
        break;
    default:
        break;
    }
    if (gesture_indicator_bar_type < GESTURE_INDICATOR_BAR_TYPE_MAX &&
            (gesture->checkIndicatorBarVisible(gesture_indicator_bar_type))) {
        ESP_UTILS_CHECK_FALSE_RETURN(
            gesture->controlIndicatorBarScaleBackAnim(gesture_indicator_bar_type, true), false,
            "Gesture control indicator bar scale back anim failed"
        );
    }

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

} // namespace esp_brookesia::speaker
