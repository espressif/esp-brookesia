/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <cmath>
#include "esp_heap_caps.h"
#include "esp_brookesia_systems_internal.h"
#if !ESP_BROOKESIA_SPEAKER_MANAGER_ENABLE_DEBUG_LOG
#   define ESP_BROOKESIA_UTILS_DISABLE_DEBUG_LOG
#endif
#include "private/esp_brookesia_speaker_utils.hpp"
#include "widgets/gesture/esp_brookesia_gesture.hpp"
#include "lvgl/esp_brookesia_lv_helper.hpp"
#include "storage_nvs/esp_brookesia_service_storage_nvs.hpp"
#include "esp_brookesia_speaker_manager.hpp"
#include "esp_brookesia_speaker.hpp"
#include "boost/thread.hpp"

using namespace std;
using namespace esp_brookesia::gui;
using namespace esp_brookesia::ai_framework;
using namespace esp_brookesia::services;

namespace esp_brookesia::systems::speaker {

constexpr int QUICK_SETTINGS_UPDATE_CLOCK_INTERVAL_MS  = 1000;
constexpr int QUICK_SETTINGS_UPDATE_MEMORY_INTERVAL_MS = 5000;

Manager::Manager(base::Context &core_in, Display &display_in, const Data &data_in):
    base::Manager(core_in, core_in.getData().manager),
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

bool Manager::calibrateData(const gui::StyleSize screen_size, Display &display, Data &data)
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
            manager->processGestureScreenChange(Screen::MAIN, nullptr),
            "Process gesture failed"
        );
    }, LV_EVENT_SCREEN_LOADED, this);

    display.getDummyDrawMask()->addEventCallback([](lv_event_t *event) {
        ESP_UTILS_LOG_TRACE_GUARD();
        ESP_UTILS_LOGD("Param: event(0x%p)", event);

        auto manager = static_cast<Manager *>(lv_event_get_user_data(event));
        ESP_UTILS_CHECK_NULL_EXIT(manager, "Invalid manager");

        ESP_UTILS_CHECK_FALSE_EXIT(
            manager->processDisplayScreenChange(Screen::MAIN, nullptr),
            "Process screen change failed"
        );
    }, LV_EVENT_LONG_PRESSED, this);
    _draw_dummy_timer = std::make_unique<LvTimer>([this](void *) {
        ESP_UTILS_CHECK_FALSE_EXIT(
            this->processDisplayScreenChange(Screen::DRAW_DUMMY, nullptr),
            "Process screen change failed"
        );
    }, data.ai_buddy_resume_time_ms, this);

    // Quick settings
    // Process quick settings event signal
    display.getQuickSettings().connectEventSignal([this](QuickSettings::EventData event_data) {
        ESP_UTILS_LOG_TRACE_GUARD();

        ESP_UTILS_CHECK_FALSE_EXIT(
            processQuickSettingsEventSignal(event_data), "Process quick settings event signal failed"
        );
    });
    // Process quick settings storage service event signal
    StorageNVS::requestInstance().connectEventSignal([this](const StorageNVS::Event & event) {
        if ((event.operation != StorageNVS::Operation::UpdateNVS) || (event.sender == &display.getQuickSettings())) {
            ESP_UTILS_LOGD("Ignore event: operation(%d), sender(%p)", static_cast<int>(event.operation), event.sender);
            return;
        }

        ESP_UTILS_CHECK_FALSE_EXIT(
            processQuickSettingsStorageServiceEventSignal(event.key),
            "Process quick settings storage service event signal failed"
        );
    });
    // Init quick settings info
    StorageNVS::Value value;
    if (StorageNVS::requestInstance().getLocalParam(SETTINGS_WLAN_SWITCH, value)) {
        auto wifi_switch = display.getQuickSettings().getWifiButton();
        ESP_UTILS_CHECK_NULL_RETURN(wifi_switch, false, "Invalid wifi switch");

        ESP_UTILS_CHECK_FALSE_RETURN(std::holds_alternative<int>(value), false, "Invalid value");

        auto is_checked = std::get<int>(value);
        if (is_checked) {
            lv_obj_add_state(wifi_switch->getNativeHandle(), LV_STATE_CHECKED);
        } else {
            lv_obj_remove_state(wifi_switch->getNativeHandle(), LV_STATE_CHECKED);
        }
    } else {
        ESP_UTILS_LOGW("No wifi switch is set");
    }
    if (StorageNVS::requestInstance().getLocalParam(SETTINGS_VOLUME, value)) {
        ESP_UTILS_CHECK_FALSE_RETURN(std::holds_alternative<int>(value), false, "Invalid value");

        auto percent = std::get<int>(value);
        ESP_UTILS_CHECK_FALSE_RETURN(display.getQuickSettings().setVolume(percent), false, "Set volume failed");
    } else {
        ESP_UTILS_LOGW("No volume is set");
    }
    if (StorageNVS::requestInstance().getLocalParam(SETTINGS_BRIGHTNESS, value)) {
        ESP_UTILS_CHECK_FALSE_RETURN(std::holds_alternative<int>(value), false, "Invalid value");

        auto percent = std::get<int>(value);
        ESP_UTILS_CHECK_FALSE_RETURN(display.getQuickSettings().setBrightness(percent), false, "Set brightness failed");
    } else {
        ESP_UTILS_LOGW("No brightness is set");
    }
    // Create timers to update quick settings info
    // Update clock
    _quick_settings_update_clock_timer = std::make_unique<LvTimer>([this](void *) {
        auto &quick_settings = display.getQuickSettings();
        if (!quick_settings.isVisible()) {
            return;
        }

        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);
        ESP_UTILS_CHECK_FALSE_EXIT(
            quick_settings.setClockTime(timeinfo.tm_hour, timeinfo.tm_min),
            "Refresh status bar failed"
        );
    }, QUICK_SETTINGS_UPDATE_CLOCK_INTERVAL_MS, this);
    ESP_UTILS_CHECK_NULL_RETURN(_quick_settings_update_clock_timer, false, "Create quick settings update clock timer failed");
    // Update memory
    _quick_settings_update_memory_timer = std::make_unique<LvTimer>([this](void *) {
        auto &quick_settings = display.getQuickSettings();
        if (!quick_settings.isVisible()) {
            return;
        }

        auto sram_total_size = static_cast<int>(heap_caps_get_total_size(MALLOC_CAP_INTERNAL));
        auto sram_free_size = static_cast<int>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
        auto sram_used_size = sram_total_size - sram_free_size;
        auto sram_used_percent = sram_used_size * 100 / sram_total_size;
        ESP_UTILS_LOGI("Memory SRAM: %d%%(used: %d/%d KB)", sram_used_percent, sram_used_size / 1024, sram_total_size / 1024);
        ESP_UTILS_CHECK_FALSE_EXIT(quick_settings.setMemorySRAM(sram_used_percent), "Set memory sram failed");

        auto psram_total_size = static_cast<int>(heap_caps_get_total_size(MALLOC_CAP_SPIRAM));
        auto psram_free_size = static_cast<int>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
        auto psram_used_size = psram_total_size - psram_free_size;
        auto psram_used_percent = psram_used_size * 100 / psram_total_size;
        ESP_UTILS_LOGI("Memory PSRAM: %d%%(used: %d/%d KB)", psram_used_percent, psram_used_size / 1024, psram_total_size / 1024);
        ESP_UTILS_CHECK_FALSE_EXIT(quick_settings.setMemoryPSRAM(psram_used_percent), "Set memory psram failed");
    }, QUICK_SETTINGS_UPDATE_MEMORY_INTERVAL_MS, this);
    ESP_UTILS_CHECK_NULL_RETURN(_quick_settings_update_memory_timer, false, "Create quick settings update memory timer failed");

    // Gesture
    if (data.flags.enable_gesture) {
        // Get the touch device
        lv_indev_t *touch = _system_context.getTouchDevice();
        if (touch == nullptr) {
            ESP_UTILS_LOGW("No touch device is set, try to use default touch device");

            touch = getLvInputDev(_system_context.getDisplayDevice(), LV_INDEV_TYPE_POINTER);
            ESP_UTILS_CHECK_NULL_RETURN(touch, false, "No touch device is initialized");
            ESP_UTILS_LOGW("Using default touch device(@0x%p)", touch);

            ESP_UTILS_CHECK_FALSE_RETURN(_system_context.setTouchDevice(touch), false, "Core set touch device failed");
        }

        // Create and begin gesture
        _gesture = std::make_unique<Gesture>(_system_context, data.gesture);
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
            // ESP_UTILS_LOG_TRACE_GUARD();

            // ESP_UTILS_LOGD("Param: event(%p)", event);
            ESP_UTILS_CHECK_NULL_EXIT(event, "Invalid event");

            auto manager = static_cast<Manager *>(lv_event_get_user_data(event));
            ESP_UTILS_CHECK_NULL_EXIT(manager, "Invalid manager");
            ESP_UTILS_CHECK_FALSE_EXIT(
                manager->processNavigationGesturePressingEvent(event), "Process navigation gesture pressing event failed"
            );
        }, _gesture->getPressingEventCode(), this);
        lv_obj_add_event_cb(_gesture->getEventObj(), [](lv_event_t *event) {
            // ESP_UTILS_LOG_TRACE_GUARD();

            // ESP_UTILS_LOGD("Param: event(%p)", event);
            ESP_UTILS_CHECK_NULL_EXIT(event, "Invalid event");

            auto manager = static_cast<Manager *>(lv_event_get_user_data(event));
            ESP_UTILS_CHECK_NULL_EXIT(manager, "Invalid manager");
            ESP_UTILS_CHECK_FALSE_EXIT(
                manager->processNavigationGestureReleaseEvent(event), "Process navigation gesture release event failed"
            );
        }, _gesture->getReleaseEventCode(), this);
        // Mask object and indicator bar
        lv_obj_add_event_cb(_gesture->getEventObj(), [](lv_event_t *event) {
            // ESP_UTILS_LOG_TRACE_GUARD();

            // ESP_UTILS_LOGD("Param: event(%p)", event);
            ESP_UTILS_CHECK_NULL_EXIT(event, "Invalid event");

            auto manager = static_cast<Manager *>(lv_event_get_user_data(event));
            ESP_UTILS_CHECK_NULL_EXIT(manager, "Invalid manager");
            ESP_UTILS_CHECK_FALSE_EXIT(
                manager->processMaskIndicatorBarGesturePressingEvent(event), "Process mask indicator bar gesture pressing event failed"
            );
        }, _gesture->getPressingEventCode(), this);
        lv_obj_add_event_cb(_gesture->getEventObj(), [](lv_event_t *event) {
            // ESP_UTILS_LOG_TRACE_GUARD();

            // ESP_UTILS_LOGD("Param: event(%p)", event);
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
            // ESP_UTILS_LOG_TRACE_GUARD();

            // ESP_UTILS_LOGD("Param: event(%p)", event);
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

        // Quick Settings
        lv_obj_add_event_cb(_gesture->getEventObj(), [](lv_event_t *event) {
            // ESP_UTILS_LOG_TRACE_GUARD();

            // ESP_UTILS_LOGD("Param: event(%p)", event);
            ESP_UTILS_CHECK_NULL_EXIT(event, "Invalid event");

            auto manager = static_cast<Manager *>(lv_event_get_user_data(event));
            ESP_UTILS_CHECK_NULL_EXIT(manager, "Invalid manager");
            ESP_UTILS_CHECK_FALSE_EXIT(
                manager->processQuickSettingsGesturePressEvent(event),
                "Process quick settings gesture press event failed"
            );
        }, _gesture->getPressEventCode(), this);
        lv_obj_add_event_cb(_gesture->getEventObj(), [](lv_event_t *event) {
            // ESP_UTILS_LOG_TRACE_GUARD();

            // ESP_UTILS_LOGD("Param: event(%p)", event);
            ESP_UTILS_CHECK_NULL_EXIT(event, "Invalid event");

            auto manager = static_cast<Manager *>(lv_event_get_user_data(event));
            ESP_UTILS_CHECK_NULL_EXIT(manager, "Invalid manager");
            ESP_UTILS_CHECK_FALSE_EXIT(
                manager->processQuickSettingsGesturePressingEvent(event),
                "Process quick settings gesture pressing event failed"
            );
        }, _gesture->getPressingEventCode(), this);
        lv_obj_add_event_cb(_gesture->getEventObj(), [](lv_event_t *event) {
            // ESP_UTILS_LOG_TRACE_GUARD();

            // ESP_UTILS_LOGD("Param: event(%p)", event);
            ESP_UTILS_CHECK_NULL_EXIT(event, "Invalid event");

            auto manager = static_cast<Manager *>(lv_event_get_user_data(event));
            ESP_UTILS_CHECK_NULL_EXIT(manager, "Invalid manager");
            ESP_UTILS_CHECK_FALSE_EXIT(
                manager->processQuickSettingsGestureReleaseEvent(event),
                "Process quick settings gesture release event failed"
            );
        }, _gesture->getReleaseEventCode(), this);
    }

    _flags.is_initialized = true;

    // Then load the ai_buddy screen
    ESP_UTILS_CHECK_FALSE_RETURN(
        processDisplayScreenChange(Screen::DRAW_DUMMY, nullptr), false,
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

    _gesture.reset();
    _draw_dummy_timer.reset();
    _quick_settings_update_clock_timer.reset();
    _quick_settings_update_memory_timer.reset();
    _flags.is_initialized = false;

end:
    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

bool Manager::processAppRunExtra(base::App *app)
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();
    ESP_UTILS_LOGD("Param: app(%p)", app);

    App *speaker_app = static_cast<App *>(app);

    ESP_UTILS_CHECK_NULL_RETURN(speaker_app, false, "Invalid speaker app");

    ESP_UTILS_CHECK_FALSE_RETURN(processDisplayScreenChange(Screen::APP, speaker_app), false,
                                 "Process screen change failed");

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

bool Manager::processAppResumeExtra(base::App *app)
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();
    ESP_UTILS_LOGD("Param: app(%p)", app);

    App *speaker_app = static_cast<App *>(app);

    ESP_UTILS_CHECK_NULL_RETURN(speaker_app, false, "Invalid speaker app");

    ESP_UTILS_CHECK_FALSE_RETURN(processDisplayScreenChange(Screen::APP, speaker_app), false,
                                 "Process screen change failed");

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

bool Manager::processAppCloseExtra(base::App *app)
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();
    ESP_UTILS_LOGD("Param: app(%p)", app);

    App *speaker_app = static_cast<App *>(app);

    ESP_UTILS_CHECK_NULL_RETURN(speaker_app, false, "Invalid speaker app");

    if (getActiveApp() == app) {
        // Switch to the main screen to release the app resources
        ESP_UTILS_CHECK_FALSE_RETURN(processDisplayScreenChange(Screen::MAIN, nullptr), false,
                                     "Process screen change failed");
    }

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

bool Manager::processDisplayScreenChange(Screen screen, void *param)
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();
    ESP_UTILS_LOGD("Param: screen(%d), param(%p)", screen, param);

    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_UTILS_CHECK_FALSE_RETURN(screen < Screen::MAX, false, "Invalid screen");

    if (_display_active_screen == screen) {
        ESP_UTILS_LOGW("Already on the screen");
        goto end;
    }

    ESP_UTILS_CHECK_FALSE_RETURN(processGestureScreenChange(screen, param), false, "Process gesture failed");

    if ((screen != Screen::DRAW_DUMMY) &&
            (_display_active_screen == Screen::DRAW_DUMMY)) {
        _ai_buddy->pause();
        ESP_UTILS_CHECK_FALSE_RETURN(display.processDummyDraw(false), false, "Display load ai_buddy failed");
    }

    switch (screen) {
    case Screen::MAIN:
        ESP_UTILS_CHECK_FALSE_RETURN(display.processMainScreenLoad(), false, "Display load main screen failed");
        if (_draw_dummy_timer != nullptr) {
            ESP_UTILS_CHECK_FALSE_RETURN(_draw_dummy_timer->restart(), false, "Restart ai_buddy resume timer failed");
        }
        break;
    case Screen::APP:
        if (_draw_dummy_timer != nullptr) {
            ESP_UTILS_CHECK_FALSE_RETURN(_draw_dummy_timer->pause(), false, "Pause ai_buddy resume timer failed");
        }
        if (display.getQuickSettings().isVisible()) {
            ESP_UTILS_CHECK_FALSE_RETURN(
                processQuickSettingsMoveTop(), false, "Process quick settings move top failed"
            );
        }
        break;
    case Screen::DRAW_DUMMY:
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
    // ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();

    // ESP_UTILS_LOGD("Param: event(%p)", event);
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
    if (getDisplayActiveScreen() == Screen::MAIN) {
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

bool Manager::processGestureScreenChange(Screen screen, void *param)
{
    const App::Config *app_data = nullptr;

    ESP_UTILS_LOGD("Process gesture when screen change");
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_UTILS_CHECK_FALSE_RETURN(screen < Screen::MAX, false, "Invalid screen");

    switch (screen) {
    case Screen::MAIN:
        _flags.enable_gesture_navigation = data.flags.enable_app_launcher_gesture_navigation;
        _flags.enable_gesture_navigation_back = 0;
        _flags.enable_gesture_navigation_home = _flags.enable_gesture_navigation;
        _flags.enable_gesture_navigation_recents_app = _flags.enable_gesture_navigation;
        _flags.enable_gesture_show_mask_left_right_edge = false;
        _flags.enable_gesture_show_mask_bottom_edge = false;
        _flags.enable_gesture_show_left_right_indicator_bar = false;
        _flags.enable_gesture_show_bottom_indicator_bar = _flags.enable_gesture_navigation;
        break;
    case Screen::DRAW_DUMMY:
        _flags.enable_gesture_navigation = false;
        _flags.enable_gesture_navigation_back = false;
        _flags.enable_gesture_navigation_home = false;
        _flags.enable_gesture_navigation_recents_app = false;
        _flags.enable_gesture_show_mask_left_right_edge = false;
        _flags.enable_gesture_show_mask_bottom_edge = false;
        _flags.enable_gesture_show_left_right_indicator_bar = false;
        _flags.enable_gesture_show_bottom_indicator_bar = false;
        break;
    case Screen::APP:
        ESP_UTILS_CHECK_NULL_RETURN(param, false, "Invalid param");
        app_data = &((App *)param)->getActiveConfig();
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

bool Manager::processNavigationEvent(base::Manager::NavigateType type)
{
    bool ret = true;
    App *active_app = static_cast<App *>(getActiveApp());

    ESP_UTILS_LOGD("Process navigation event type(%d)", type);

    // Disable the gesture function of widgets
    _flags.is_app_launcher_gesture_disabled = true;

    switch (type) {
    case base::Manager::NavigateType::BACK:
        if (active_app == nullptr) {
            goto end;
        }
        // Call app back function
        ESP_UTILS_CHECK_FALSE_GOTO(ret = (active_app->back()), end, "App(%d) back failed", active_app->getId());
        break;
    case base::Manager::NavigateType::HOME:
    case base::Manager::NavigateType::RECENTS_SCREEN:
        if (active_app == nullptr) {
            processDisplayScreenChange(Screen::DRAW_DUMMY, nullptr);
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

bool Manager::processQuickSettingsEventSignal(QuickSettings::EventData event_data)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    auto &type = event_data.type;
    auto &storage_service = StorageNVS::requestInstance();
    bool is_long_pressed = false;
    switch (type) {
    case QuickSettings::EventType::WifiButtonClicked: {
        auto wifi_button = display.getQuickSettings().getWifiButton();
        ESP_UTILS_CHECK_NULL_RETURN(wifi_button, false, "Invalid wifi button");

        StorageNVS::Value value = static_cast<int>(wifi_button->hasState(LV_STATE_CHECKED));
        ESP_UTILS_LOGI("Wifi button clicked, value: %d", std::get<int>(value));
        ESP_UTILS_CHECK_FALSE_RETURN(
            storage_service.setLocalParam(SETTINGS_WLAN_SWITCH, value, &display.getQuickSettings()), false,
            "Set wifi state failed"
        );
        break;
    }
    case QuickSettings::EventType::VolumeButtonClicked: {
        ESP_UTILS_LOGI("Volume button clicked");
        auto level = display.getQuickSettings().getVolumeLevel();
        level = static_cast<QuickSettings::VolumeLevel>(static_cast<int>(level) + 1);
        if (level >= QuickSettings::VolumeLevel::MAX) {
            level = QuickSettings::VolumeLevel::MUTE;
        }
        ESP_UTILS_CHECK_FALSE_RETURN(display.getQuickSettings().setVolume(level), false, "Set volume failed");

        int percent = display.getQuickSettings().getVolumePercent();
        ESP_UTILS_CHECK_FALSE_RETURN(
            storage_service.setLocalParam(SETTINGS_VOLUME, percent, &display.getQuickSettings()), false,
            "Set volume failed"
        );
        break;
    }
    case QuickSettings::EventType::BrightnessButtonClicked: {
        ESP_UTILS_LOGI("Brightness button clicked");
        auto level = display.getQuickSettings().getBrightnessLevel();
        level = static_cast<QuickSettings::BrightnessLevel>(static_cast<int>(level) + 1);
        if (level >= QuickSettings::BrightnessLevel::MAX) {
            level = QuickSettings::BrightnessLevel::LEVEL_1;
        }
        ESP_UTILS_CHECK_FALSE_RETURN(display.getQuickSettings().setBrightness(level), false, "Set brightness failed");

        int percent = display.getQuickSettings().getBrightnessPercent();
        ESP_UTILS_CHECK_FALSE_RETURN(
            storage_service.setLocalParam(SETTINGS_BRIGHTNESS, percent, &display.getQuickSettings()), false,
            "Set brightness failed"
        );
        break;
    }
    case QuickSettings::EventType::WifiButtonLongPressed:
    case QuickSettings::EventType::VolumeButtonLongPressed:
    case QuickSettings::EventType::BrightnessButtonLongPressed: {
        is_long_pressed = true;
        break;
    }
    default:
        break;
    }

    if (is_long_pressed) {
        ESP_UTILS_CHECK_FALSE_RETURN(processQuickSettingsScrollTop(), false, "Process quick settings scroll top failed");
    }

    return true;
}

bool Manager::processQuickSettingsStorageServiceEventSignal(std::string key)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_LOGD("Param: key(%s)", key.c_str());

    StorageNVS::Value value;
    ESP_UTILS_CHECK_FALSE_RETURN(
        StorageNVS::requestInstance().getLocalParam(key, value), false, "Get local param failed"
    );

    LvLockGuard gui_guard;
    if (key == SETTINGS_WLAN_SWITCH) {
        auto wifi_button = display.getQuickSettings().getWifiButton();
        ESP_UTILS_CHECK_NULL_RETURN(wifi_button, false, "Invalid wifi button");

        ESP_UTILS_CHECK_FALSE_RETURN(std::holds_alternative<int>(value), false, "Invalid value");

        auto is_checked = std::get<int>(value);
        if (is_checked) {
            lv_obj_add_state(wifi_button->getNativeHandle(), LV_STATE_CHECKED);
        } else {
            lv_obj_remove_state(wifi_button->getNativeHandle(), LV_STATE_CHECKED);
        }
    } else if (key == SETTINGS_VOLUME) {
        ESP_UTILS_CHECK_FALSE_RETURN(std::holds_alternative<int>(value), false, "Invalid value");

        auto percent = std::get<int>(value);
        ESP_UTILS_CHECK_FALSE_RETURN(display.getQuickSettings().setVolume(percent), false, "Set volume failed");
    } else if (key == SETTINGS_BRIGHTNESS) {
        ESP_UTILS_CHECK_FALSE_RETURN(std::holds_alternative<int>(value), false, "Invalid value");

        auto percent = std::get<int>(value);
        ESP_UTILS_CHECK_FALSE_RETURN(display.getQuickSettings().setBrightness(percent), false, "Set brightness failed");
    }

    return true;
}

bool Manager::processQuickSettingsGesturePressEvent(lv_event_t *event)
{
    // ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();

    // ESP_UTILS_LOGD("Param: event(%p)", event);
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
    // ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();

    // ESP_UTILS_LOGD("Param: event(%p)", event);
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
    // ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();

    // ESP_UTILS_LOGD("Param: event(%p)", event);
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
            if ((_draw_dummy_timer != nullptr) && (_display_active_screen == Screen::MAIN)) {
                _draw_dummy_timer->pause();
            }
        } else {
            ESP_UTILS_CHECK_FALSE_RETURN(
                processQuickSettingsScrollTop(), false, "Process quick settings scroll top failed"
            );
            if ((_draw_dummy_timer != nullptr) && (_display_active_screen == Screen::MAIN)) {
                _draw_dummy_timer->restart();
            }
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

bool Manager::processQuickSettingsMoveTop()
{
    ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();

    auto &quick_settings = display.getQuickSettings();
    // Move the quick settings to the top edge and hide it
    ESP_UTILS_CHECK_FALSE_RETURN(
        quick_settings.moveY_To(
            _gesture->data.threshold.horizontal_edge - _system_context.getData().screen_size.height
        ), false, "Move quick settings failed"
    );
    ESP_UTILS_CHECK_FALSE_RETURN(quick_settings.setVisible(false), false, "Set quick settings visible failed");

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

bool Manager::processQuickSettingsScrollTop()
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
            _gesture->data.threshold.horizontal_edge - _system_context.getData().screen_size.height, false
        ), false, "Move quick settings failed"
    );

    ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS();
    return true;
}

bool Manager::processQuickSettingsScrollBottom()
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
    // ESP_UTILS_LOG_TRACE_GUARD();

    // ESP_UTILS_LOGD("Param: event(%p)", event);
    ESP_UTILS_CHECK_NULL_RETURN(event, true, "Invalid event");

    GestureInfo *gesture_info = nullptr;
    base::Manager::NavigateType navigation_type = base::Manager::NavigateType::MAX;

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
        navigation_type = base::Manager::NavigateType::BACK;
    } else if ((gesture_info->start_area & GESTURE_AREA_BOTTOM_EDGE) && (!gesture_info->flags.short_duration) &&
               (gesture_info->direction & GESTURE_DIR_UP) && _flags.enable_gesture_navigation_recents_app) {
        // Check if there is a "recents_screen" gesture
        navigation_type = base::Manager::NavigateType::RECENTS_SCREEN;
    }

    // Only process the navigation event if the navigation type is valid
    if (navigation_type != base::Manager::NavigateType::MAX) {
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
    // ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();

    GestureInfo *gesture_info = nullptr;
    base::Manager::NavigateType navigation_type = base::Manager::NavigateType::MAX;

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
        navigation_type = base::Manager::NavigateType::HOME;
    }

    // Only process the navigation event if the navigation type is valid
    if (navigation_type != base::Manager::NavigateType::MAX) {
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
    // ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();

    // ESP_UTILS_LOGD("Param: event(%p)", event);
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
                false, "Gesture set indicator bar length by offset failed"
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
    // ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS();

    // ESP_UTILS_LOGD("Param: event(%p)", event);
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

} // namespace esp_brookesia::systems::speaker
