/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <algorithm>
#if __has_include("src/misc/lv_ll.h")
#include "src/misc/lv_ll.h"
#include "src/misc/lv_gc.h"
#else
#include "misc/lv_ll.h"
#include "misc/lv_gc.h"
#endif
#include "esp_brookesia_core_utils.h"
#include "esp_brookesia_core.hpp"
#include "esp_brookesia_core_app.hpp"

#if !ESP_BROOKESIA_LOG_ENABLE_DEBUG_CORE_APP
#undef ESP_BROOKESIA_LOGD
#define ESP_BROOKESIA_LOGD(...)
#endif

#define RESOURCE_LOOP_COUNT_MAX     (1000)

using namespace std;

ESP_Brookesia_CoreApp::ESP_Brookesia_CoreApp(const ESP_Brookesia_CoreAppData_t &data):
    _core(nullptr),
    _core_init_data(data),
    _status(ESP_BROOKESIA_CORE_APP_STATUS_UNINSTALLED),
    _id(-1),
    _flags{},
    _display_style{},
    _app_style{},
    _resource_timer_count(0),
    _resource_anim_count(0),
    _resource_head_screen_index(0),
    _resource_screen_count(0),
    _last_screen(nullptr),
    _active_screen(nullptr),
    _resource_head_timer(nullptr),
    _resource_head_anim(nullptr)
{
}

ESP_Brookesia_CoreApp::ESP_Brookesia_CoreApp(const char *name, const void *launcher_icon, bool use_default_screen):
    _core(nullptr),
    _core_init_data(ESP_BROOKESIA_CORE_APP_DATA_DEFAULT(name, launcher_icon, use_default_screen)),
    _status(ESP_BROOKESIA_CORE_APP_STATUS_UNINSTALLED),
    _id(-1),
    _flags{},
    _display_style{},
    _app_style{},
    _resource_timer_count(0),
    _resource_anim_count(0),
    _resource_head_screen_index(0),
    _resource_screen_count(0),
    _last_screen(nullptr),
    _active_screen(nullptr),
    _resource_head_timer(nullptr),
    _resource_head_anim(nullptr)
{
}

bool ESP_Brookesia_CoreApp::notifyCoreClosed(void) const
{
    lv_obj_t *event_obj = nullptr;
    lv_event_code_t event_code = _LV_EVENT_LAST;
    lv_res_t res = LV_RES_OK;
    ESP_Brookesia_CoreAppEventData_t event_data = {
        .id = _id,
        .type = ESP_BROOKESIA_CORE_APP_EVENT_TYPE_STOP,
        .data = nullptr
    };

    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_BROOKESIA_LOGD("App(%s: %d) notify core closed", getName(), _id);

    if (_flags.is_closing) {
        return true;
    }

    event_obj = _core->getEventObject();
    event_code = _core->getAppEventCode();
    ESP_BROOKESIA_CHECK_FALSE_RETURN(event_obj != nullptr, false, "Event object is invalid");
    ESP_BROOKESIA_CHECK_FALSE_RETURN(esp_brookesia_core_utils_check_event_code_valid(event_code), false, "Event code is invalid");

    res = lv_event_send(event_obj, event_code, &event_data);
    ESP_BROOKESIA_CHECK_FALSE_RETURN(res == LV_RES_OK, false, "Send app closed event failed");

    return true;
}

void ESP_Brookesia_CoreApp::setLauncherIconImage(const ESP_Brookesia_StyleImage_t &icon_image)
{
    _core_active_data.launcher_icon = icon_image;
}

bool ESP_Brookesia_CoreApp::startRecordResource(void)
{
    lv_disp_t *disp = nullptr;
    lv_area_t &visual_area = _app_style.calibrate_visual_area;

    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_BROOKESIA_LOGD("App(%s: %d) start record resource", getName(), _id);

    disp = _core->getDisplayDevice();
    ESP_BROOKESIA_CHECK_NULL_RETURN(disp, false, "Invalid display");

    if (_flags.is_resource_recording) {
        ESP_BROOKESIA_LOGD("Recording resource is already started, don't start again");
        return true;
    }

    if (_core_active_data.flags.enable_resize_visual_area) {
        ESP_BROOKESIA_LOGD("Resieze screen to visual area[(%d,%d)-(%d,%d)]", visual_area.x1, visual_area.y1, visual_area.x2,
                           visual_area.y2);
        _display_style.w = disp->driver->hor_res;
        _display_style.h = disp->driver->ver_res;
        disp->driver->hor_res = visual_area.x2 - visual_area.x1 + 1;
        disp->driver->ver_res = visual_area.y2 - visual_area.y1 + 1;
    }
    _resource_head_screen_index = disp->screen_cnt - 1;
    _resource_head_timer = lv_timer_get_next(nullptr);
    _resource_head_anim = (lv_anim_t *)_lv_ll_get_head(&LV_GC_ROOT(_lv_anim_ll));
    _flags.is_resource_recording = true;

    return true;
}

bool ESP_Brookesia_CoreApp::endRecordResource(void)
{
    bool ret = true;
    uint32_t resource_loop_count = 0;
    lv_disp_t *disp = nullptr;
    lv_obj_t *screen = nullptr;
    lv_timer_t *timer_node = nullptr;
    lv_anim_t *anim_node = nullptr;
    const lv_area_t &visual_area = _app_style.calibrate_visual_area;

    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_BROOKESIA_LOGD("App(%s: %d) end record resource", getName(), _id);

    if (!_flags.is_resource_recording) {
        ESP_BROOKESIA_LOGD("Recording resource is not started, please start first");
        return true;
    }

    disp = _core->getDisplayDevice();
    ESP_BROOKESIA_CHECK_NULL_RETURN(disp, false, "Invalid display");

    // Screen
    resource_loop_count = 0;
    for (int i = _resource_head_screen_index + 1; (i < (int)disp->screen_cnt) &&
            (resource_loop_count++ <  RESOURCE_LOOP_COUNT_MAX); i++) {
        screen = (lv_obj_t *)disp->screens[i];
        // Record or update the record information of the screen
        _resource_screens_class_parent_map[screen] = {screen->class_p, (lv_obj_t *)screen->parent};
        if (find(_resource_screens.begin(), _resource_screens.end(), screen) == _resource_screens.end()) {
            // Only record the newest timer
            _resource_screens.push_back(screen);
            _resource_screen_count++;
            // Move screens to visual area when loaded only if needed
            if (_core_active_data.flags.enable_resize_visual_area) {
                lv_obj_set_pos(screen, visual_area.x1, visual_area.y1);
                lv_obj_add_event_cb(screen, onResizeScreenLoadedEventCallback, LV_EVENT_SCREEN_LOAD_START, this);
                // Avoid resetting the position of the previous screen when using animations with `lv_scr_load_anim()`
                lv_obj_add_event_cb(screen, onResizeScreenLoadedEventCallback, LV_EVENT_SCREEN_UNLOAD_START, this);
            }
        } else {
            ESP_BROOKESIA_LOGD("Screen(@0x%p) is already recorded", screen);
        }
    }
    if ((_resource_head_screen_index >= (int)disp->screen_cnt) || (resource_loop_count >= RESOURCE_LOOP_COUNT_MAX)) {
        _resource_screens.clear();
        _resource_screens_class_parent_map.clear();
        _resource_screen_count = 0;
        ret = false;
        ESP_BROOKESIA_LOGE("record screen fail");
    } else {
        ESP_BROOKESIA_LOGD("record screen(%d): ", _resource_screen_count);
    }

    // Timer
    resource_loop_count = 0;
    timer_node = lv_timer_get_next(nullptr);
    while ((timer_node != nullptr) && (timer_node != _resource_head_timer) &&
            (resource_loop_count++ < RESOURCE_LOOP_COUNT_MAX)) {
        // Record or update the record information of the timer
        _resource_timers_cb_usr_map[timer_node] = {(lv_timer_cb_t)timer_node->timer_cb, timer_node->user_data};
        if (find(_resource_timers.begin(), _resource_timers.end(), timer_node) == _resource_timers.end()) {
            // Only record the newest timer
            _resource_timers.push_back(timer_node);
            _resource_timer_count++;
        } else {
            ESP_BROOKESIA_LOGD("Timer(@0x%p) is already recorded", timer_node);
        }
        timer_node = lv_timer_get_next(timer_node);
    }
    if (((timer_node == nullptr) && (_resource_head_timer != nullptr)) ||
            (resource_loop_count >= RESOURCE_LOOP_COUNT_MAX)) {
        _resource_timers.clear();
        _resource_timers_cb_usr_map.clear();
        _resource_timer_count = 0;
        ret = false;
        ESP_BROOKESIA_LOGE("record timer fail");
    } else {
        ESP_BROOKESIA_LOGD("record timer(%d): ", _resource_timer_count);
    }

    // Animation
    anim_node = (lv_anim_t *)_lv_ll_get_head(&LV_GC_ROOT(_lv_anim_ll));
    while ((anim_node != nullptr) && (anim_node != _resource_head_anim)) {
        // Record or update the record information of the animation
        _resource_anims_var_exec_map[anim_node] = {anim_node->var, anim_node->exec_cb};
        if (find(_resource_anims.begin(), _resource_anims.end(), anim_node) == _resource_anims.end()) {
            // Only record the newest timer
            _resource_anims.push_back(anim_node);
            _resource_anim_count++;
        } else {
            ESP_BROOKESIA_LOGD("Animation(@0x%p) is already recorded", anim_node);
        }
        anim_node = (lv_anim_t *)_lv_ll_get_next(&LV_GC_ROOT(_lv_anim_ll), anim_node);
    }
    if ((anim_node == nullptr) && (_resource_head_anim != nullptr)) {
        _resource_anims.clear();
        _resource_anims_var_exec_map.clear();
        _resource_anim_count = 0;
        ESP_BROOKESIA_LOGE("record animation fail");
    } else {
        ESP_BROOKESIA_LOGD("record animation(%d): ", _resource_anim_count);
    }

    if (_core_active_data.flags.enable_resize_visual_area) {
        ESP_BROOKESIA_LOGD("Resize screen back to display size(%d x %d)", _display_style.w, _display_style.h);
        disp->driver->hor_res = _display_style.w;
        disp->driver->ver_res = _display_style.h;
    }
    _flags.is_resource_recording = false;

    return ret;
}

bool ESP_Brookesia_CoreApp::cleanRecordResource(void)
{
    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_BROOKESIA_LOGD("App(%s: %d) clean resource", getName(), _id);

    bool ret = true;
    bool do_clean = false;
    int resource_loop_count = 0;
    int resource_clean_count = 0;
    lv_disp_t *disp = nullptr;
    lv_obj_t *screen_node = nullptr;
    lv_timer_t *timer_node = nullptr;
    lv_anim_t *anim_node = nullptr;

    disp = _core->getDisplayDevice();
    ESP_BROOKESIA_CHECK_NULL_RETURN(disp, false, "Invalid display");

    // Screen
    resource_loop_count = 0;
    resource_clean_count = 0;
    for (int i = 0; (i < (int)disp->screen_cnt) && (resource_loop_count++ <  RESOURCE_LOOP_COUNT_MAX);) {
        do_clean = false;
        screen_node = (lv_obj_t *)disp->screens[i];
        auto screen_it = find(_resource_screens.begin(), _resource_screens.end(), screen_node);
        if (screen_it != _resource_screens.end()) {
            auto screen_map_it = _resource_screens_class_parent_map.find(screen_node);
            if (screen_map_it == _resource_screens_class_parent_map.end()) {
                ESP_BROOKESIA_LOGE("Screen class parent map not found");
            } else {
                if ((screen_node->class_p == screen_map_it->second.first) &&
                        (screen_node->parent == screen_map_it->second.second)) {
                    lv_obj_del(screen_node);
                    do_clean = true;
                    resource_clean_count++;
                } else {
                    ESP_BROOKESIA_LOGD("Screen(@0x%p) information is not matched, skip", screen_node);
                }
                _resource_screens.erase(screen_it);
                _resource_screens_class_parent_map.erase(screen_map_it);
            }
        }
        i = do_clean ? 0 : i + 1;
    }
    if (resource_loop_count >= RESOURCE_LOOP_COUNT_MAX) {
        ret = false;
        ESP_BROOKESIA_LOGE("Clean screen loop count exceed max");
    } else {
        ESP_BROOKESIA_LOGD("Clean screen(%d), miss(%d): ", resource_clean_count, (int)(_resource_screen_count - resource_clean_count));
    }

    // Timer
    resource_loop_count = 0;
    resource_clean_count = 0;
    timer_node = lv_timer_get_next(nullptr);
    while ((timer_node != nullptr) && (_resource_timers.size() > 0) &&
            (resource_loop_count++ < RESOURCE_LOOP_COUNT_MAX)) {
        do_clean = false;
        auto timer_it = find(_resource_timers.begin(), _resource_timers.end(), timer_node);
        if (timer_it != _resource_timers.end()) {
            auto timer_map_it = _resource_timers_cb_usr_map.find(timer_node);
            if (timer_map_it == _resource_timers_cb_usr_map.end()) {
                ESP_BROOKESIA_LOGE("Timer cb usr map not found");
            } else  {
                if ((timer_map_it->second.first == timer_node->timer_cb) &&
                        (timer_map_it->second.second == timer_node->user_data)) {
                    lv_timer_del(timer_node);
                    do_clean = true;
                    resource_clean_count++;
                } else {
                    ESP_BROOKESIA_LOGD("Timer(@0x%p) information is not matched, skip", timer_node);
                }
                _resource_timers.erase(timer_it);
                _resource_timers_cb_usr_map.erase(timer_map_it);
            }
        }
        timer_node = do_clean ? lv_timer_get_next(nullptr) : lv_timer_get_next(timer_node);
    }
    if (resource_loop_count >= RESOURCE_LOOP_COUNT_MAX) {
        ret = false;
        ESP_BROOKESIA_LOGE("Clean timer loop count exceed max");
    } else {
        ESP_BROOKESIA_LOGD("Clean timer(%d), miss(%d): ", resource_clean_count, _resource_timer_count - resource_clean_count);
    }

    // Animation
    resource_loop_count = 0;
    resource_clean_count = 0;
    anim_node = (lv_anim_t *)_lv_ll_get_head(&LV_GC_ROOT(_lv_anim_ll));
    while ((anim_node != nullptr) && (_resource_anims.size() > 0) &&
            (resource_loop_count++ < RESOURCE_LOOP_COUNT_MAX)) {
        do_clean = false;
        auto anim_it = find(_resource_anims.begin(), _resource_anims.end(), anim_node);
        if (anim_it != _resource_anims.end()) {
            auto anim_map_it = _resource_anims_var_exec_map.find(anim_node);
            if (anim_map_it == _resource_anims_var_exec_map.end()) {
                ESP_BROOKESIA_LOGE("Animation var exec map not found");
            } else  {
                if ((anim_map_it->second.first == anim_node->var) &&
                        (anim_map_it->second.second == anim_node->exec_cb)) {
                    if (lv_anim_del(anim_node->var, anim_node->exec_cb)) {
                        do_clean = true;
                        resource_clean_count++;
                    } else {
                        ESP_BROOKESIA_LOGE("Delete animation failed");
                    }
                } else {
                    ESP_BROOKESIA_LOGD("Anim(@0x%p) information is not matched, skip", anim_node);
                }
                _resource_anims.erase(anim_it);
                _resource_anims_var_exec_map.erase(anim_map_it);
            }
        }
        anim_node = do_clean ? (lv_anim_t *)_lv_ll_get_head(&LV_GC_ROOT(_lv_anim_ll)) :
                    (lv_anim_t *)_lv_ll_get_next(&LV_GC_ROOT(_lv_anim_ll), anim_node);
    }
    if (resource_loop_count >= RESOURCE_LOOP_COUNT_MAX) {
        ret = false;
        ESP_BROOKESIA_LOGE("Clean timer loop count exceed max");
    } else {
        ESP_BROOKESIA_LOGD("Clean anim(%d), miss(%d): ", resource_clean_count, _resource_anim_count - resource_clean_count);
    }

    ESP_BROOKESIA_CHECK_FALSE_RETURN(resetRecordResource(), false, "Reset record resource failed");

    return ret;
}

bool ESP_Brookesia_CoreApp::processInstall(ESP_Brookesia_Core *core, int id)
{
    ESP_BROOKESIA_CHECK_FALSE_RETURN(!checkInitialized(), false, "Already initialized");
    ESP_BROOKESIA_CHECK_NULL_RETURN(_core_init_data.name, false, "App name is invalid");
    ESP_BROOKESIA_CHECK_NULL_RETURN(core, false, "Core is invalid");

    ESP_BROOKESIA_LOGD("App(%s: %d) install", _core_init_data.name, id);

    _core_active_data = _core_init_data;
    ESP_BROOKESIA_CHECK_FALSE_RETURN(
        core->getCoreHome().calibrateCoreObjectSize(core->getCoreData().screen_size, _core_active_data.screen_size),
        false, "Calibrate screen size failed"
    );
    _core = core;
    _id = id;

    ESP_BROOKESIA_CHECK_FALSE_GOTO(beginExtra(), err, "Begin extra failed");
    ESP_BROOKESIA_CHECK_FALSE_GOTO(init(), err, "Init failed");

    _status = ESP_BROOKESIA_CORE_APP_STATUS_CLOSED;

    return true;

err:
    ESP_BROOKESIA_CHECK_FALSE_RETURN(processUninstall(), false, "Uninstall failed");

    return false;
}

bool ESP_Brookesia_CoreApp::processUninstall(void)
{
    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_BROOKESIA_LOGD("App(%s: %d) uninstall", getName(), _id);

    _core = nullptr;
    _core_active_data = {};
    _status = ESP_BROOKESIA_CORE_APP_STATUS_UNINSTALLED;
    _id = -1;
    _flags = {};
    _display_style = {};
    _app_style = {};
    _resource_timer_count = 0;
    _resource_anim_count = 0;
    _resource_head_screen_index = 0;
    _resource_screen_count = 0;
    if (_core_active_data.flags.enable_default_screen && lv_obj_is_valid(_active_screen)) {
        lv_obj_del(_active_screen);
    }
    _active_screen = nullptr;
    // TODO
    // _temp_screen = nullptr;
    _resource_head_timer = nullptr;
    _resource_head_anim = nullptr;
    _resource_screens.clear();
    _resource_timers.clear();
    _resource_anims.clear();

    ESP_BROOKESIA_CHECK_FALSE_RETURN(delExtra(), false, "Begin extra failed");
    ESP_BROOKESIA_CHECK_FALSE_RETURN(deinit(), false, "Deinit failed");

    return true;
}

bool ESP_Brookesia_CoreApp::processRun()
{
    bool ret = true;

    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_BROOKESIA_LOGD("App(%s: %d) run", getName(), _id);

    // TODO
    // if (_flags.is_screen_small) {
    //     // Create a temp screen to recolor the background
    //     ESP_BROOKESIA_CHECK_FALSE_RETURN(createAndloadTempScreen(), false, "Create temp screen failed");
    // }
    ESP_BROOKESIA_CHECK_FALSE_RETURN(saveRecentScreen(false), false, "Save recent screen before run failed");
    ESP_BROOKESIA_CHECK_FALSE_RETURN(resetRecordResource(), false, "Reset record resource failed");
    ESP_BROOKESIA_CHECK_FALSE_RETURN(startRecordResource(), false, "Start record resource failed");
    if (_core_active_data.flags.enable_default_screen) {
        ESP_BROOKESIA_CHECK_FALSE_RETURN(initDefaultScreen(), false, "Create active screen failed");
    }
    ESP_BROOKESIA_CHECK_FALSE_RETURN(saveDisplayTheme(), false, "Save display theme failed");
    ESP_BROOKESIA_LOGD("Do run");
    if (!run()) {
        ESP_BROOKESIA_LOGE("Run app failed");
        ret = false;
    }
    ESP_BROOKESIA_CHECK_FALSE_RETURN(endRecordResource(), false, "Start record resource failed");
    if (!saveRecentScreen(true)) {
        ESP_BROOKESIA_LOGE("Save recent screen after run failed");
        ret = false;
    }
    ESP_BROOKESIA_CHECK_FALSE_GOTO(ret, err, "App run failed");

    _status = ESP_BROOKESIA_CORE_APP_STATUS_RUNNING;

    return true;

err:
    ESP_BROOKESIA_CHECK_FALSE_RETURN(processClose(true), false, "Close app failed");

    return false;
}

bool ESP_Brookesia_CoreApp::processResume(void)
{
    bool ret = true;

    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_BROOKESIA_LOGD("App(%s: %d) resume", getName(), _id);

    ESP_BROOKESIA_CHECK_FALSE_RETURN(loadRecentScreen(), false, "Load recent screen failed");
    ESP_BROOKESIA_CHECK_FALSE_GOTO(loadAppTheme(), err, "Load app theme failed");
    ESP_BROOKESIA_CHECK_FALSE_GOTO(startRecordResource(), err, "Start record resource failed");
    ESP_BROOKESIA_LOGD("Do resume");
    if (!(ret = resume())) {
        ESP_BROOKESIA_LOGE("Resume app failed");
    }
    ESP_BROOKESIA_CHECK_FALSE_GOTO(endRecordResource(), err, "End record resource failed");

    _status = ESP_BROOKESIA_CORE_APP_STATUS_RUNNING;

    return ret;

err:
    ESP_BROOKESIA_CHECK_FALSE_RETURN(processClose(true), false, "Close app failed");

    return false;
}

bool ESP_Brookesia_CoreApp::processPause(void)
{
    bool ret = true;

    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_BROOKESIA_LOGD("App(%s: %d) pause", getName(), _id);

    ESP_BROOKESIA_LOGD("Do pause");
    if (!(ret = pause())) {
        ESP_BROOKESIA_LOGE("Pause failed");
    }
    ESP_BROOKESIA_CHECK_FALSE_GOTO(saveAppTheme(), err, "Save app theme failed");
    ESP_BROOKESIA_CHECK_FALSE_GOTO(saveRecentScreen(false), err, "Save recent screen failed");
    ESP_BROOKESIA_CHECK_FALSE_GOTO(loadDisplayTheme(), err, "Load display theme failed");

    _status = ESP_BROOKESIA_CORE_APP_STATUS_PAUSED;

    return ret;

err:
    ESP_BROOKESIA_CHECK_FALSE_RETURN(processClose(true), false, "Close app failed");

    return false;
}

bool ESP_Brookesia_CoreApp::processClose(bool is_app_active)
{
    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_BROOKESIA_LOGD("App(%s: %d) close", getName(), _id);

    // Prevent recursive close
    _flags.is_closing = true;

    ESP_BROOKESIA_LOGD("Do close");
    ESP_BROOKESIA_CHECK_FALSE_GOTO(close(), err, "Close failed");
    // Check if the app is active, if not, clean the resource immediately.
    // Otherwise, clean the resource when the screen is unloaded
    if (is_app_active) {
        // Save the last screen
        ESP_BROOKESIA_CHECK_FALSE_GOTO(saveRecentScreen(false), err, "Save recent screen failed");
        // This is to prevent the screen from being cleaned before the screen is unloaded
        ESP_BROOKESIA_CHECK_FALSE_GOTO(enableAutoClean(), err, "Enable auto clean failed");
    } else {
        ESP_BROOKESIA_LOGD("Do clean resource");
        if (!cleanResource()) {
            ESP_BROOKESIA_LOGE("Clean resource failed");
        }
        if (_core_active_data.flags.enable_recycle_resource) {
            ESP_BROOKESIA_CHECK_FALSE_GOTO(cleanRecordResource(), err, "Clean record resource failed");
        } else if (_core_active_data.flags.enable_default_screen) {
            ESP_BROOKESIA_CHECK_FALSE_GOTO(cleanDefaultScreen(), err, "Clean active screen failed");
        }
    }
    ESP_BROOKESIA_CHECK_FALSE_GOTO(loadDisplayTheme(), err, "Load display theme failed");

    _flags.is_closing = false;
    _status = ESP_BROOKESIA_CORE_APP_STATUS_CLOSED;

    return true;

err:
    _flags.is_closing = false;

    return false;
}

bool ESP_Brookesia_CoreApp::setVisualArea(const lv_area_t &area)
{
    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_BROOKESIA_LOGD("App(%s: %d) set origin visual area[(%d,%d)-(%d,%d)]", getName(),
                       _id, area.x1, area.y1, area.x2, area.y2);

    _app_style.origin_visual_area = area;

    return true;
}

bool ESP_Brookesia_CoreApp::calibrateVisualArea(void)
{
    uint16_t visual_area_x = 0;
    uint16_t visual_area_y = 0;
    uint16_t visual_area_w = 0;
    uint16_t visual_area_h = 0;
    lv_area_t visual_area = _app_style.origin_visual_area;
    const ESP_Brookesia_StyleSize_t &screen_size = _core->getCoreData().screen_size;
    const ESP_Brookesia_StyleSize_t &app_size = getCoreActiveData().screen_size;

    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_BROOKESIA_LOGD("App(%s: %d) calibrate visual area[origin: (%d,%d)-(%d,%d)]", getName(),
                       _id, visual_area.x1, visual_area.y1, visual_area.x2, visual_area.y2);

    visual_area_w = visual_area.x2 - visual_area.x1 + 1;
    visual_area_h = visual_area.y2 - visual_area.y1 + 1;
    visual_area_x = visual_area.x1;
    visual_area_y = visual_area.y1;
    if (visual_area_w > app_size.width) {
        visual_area_x = visual_area.x1 + (visual_area_w - app_size.width) / 2;
    }
    if (visual_area_h > app_size.height) {
        visual_area_y = visual_area.y1 + (visual_area_h - app_size.height) / 2;
    }
    visual_area_w = min(visual_area_w, app_size.width);
    visual_area_h = min(visual_area_h, app_size.height);
    visual_area.x1 = visual_area_x;
    visual_area.y1 = visual_area_y;
    visual_area.x2 = visual_area_x + visual_area_w - 1;
    visual_area.y2 = visual_area_y + visual_area_h - 1;

    _app_style.calibrate_visual_area = visual_area;
    _flags.is_screen_small = ((lv_area_get_height(&visual_area) < screen_size.height) ||
                              (lv_area_get_width(&visual_area) < screen_size.width));

    ESP_BROOKESIA_LOGD("Calibrate visual area(%d,%d-%d,%d)", visual_area.x1, visual_area.y1, visual_area.x2, visual_area.y2);

    return true;
}

bool ESP_Brookesia_CoreApp::initDefaultScreen(void)
{
    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_BROOKESIA_LOGD("App(%s: %d) init default screen", getName(), _id);

    _active_screen = lv_obj_create(nullptr);
    ESP_BROOKESIA_CHECK_NULL_RETURN(_active_screen, false, "Create default screen failed");

    lv_scr_load(_active_screen);

    // TODO
    // if (_flags.is_screen_small) {
    //     ESP_BROOKESIA_CHECK_FALSE_RETURN(delTempScreen(), false, "Delete temp screen failed");
    // }

    return true;
}

bool ESP_Brookesia_CoreApp::cleanDefaultScreen(void)
{
    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_BROOKESIA_LOGD("App(%s: %d) clean default active screen", getName(), _id);

    if (lv_obj_is_valid(_active_screen)) {
        lv_obj_del(_active_screen);
    } else {
        ESP_BROOKESIA_LOGW("Active screen is already cleaned");
    }
    _active_screen = nullptr;

    return true;
}

bool ESP_Brookesia_CoreApp::saveRecentScreen(bool check_valid)
{
    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_BROOKESIA_LOGD("App(%s: %d) save recent screen", getName(), _id);

    lv_obj_t *active_screen = lv_disp_get_scr_act(_core->getDisplayDevice());
    ESP_BROOKESIA_CHECK_FALSE_RETURN(active_screen != nullptr, false, "Invalid active screen");

    if (check_valid) {
        ESP_BROOKESIA_CHECK_FALSE_RETURN(active_screen != _last_screen, false, "No app screen");
    }
    _active_screen = active_screen;
    _last_screen = active_screen;

    return true;
}

bool ESP_Brookesia_CoreApp::loadRecentScreen(void)
{
    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_BROOKESIA_LOGD("App(%s: %d) load recent screen", getName(), _id);

    // TODO
    // if (_flags.is_screen_small) {
    //     // Create a temp screen to recolor the background
    //     ESP_BROOKESIA_CHECK_FALSE_RETURN(createAndloadTempScreen(), false, "Create temp screen failed");
    // }

    ESP_BROOKESIA_CHECK_FALSE_RETURN(lv_obj_is_valid(_active_screen), false, "Invalid active screen");
    lv_scr_load(_active_screen);

    // if (_flags.is_screen_small) {
    //     ESP_BROOKESIA_CHECK_FALSE_RETURN(delTempScreen(), false, "Delete temp screen failed");
    // }

    return true;
}

bool ESP_Brookesia_CoreApp::resetRecordResource(void)
{
    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_BROOKESIA_LOGD("App(%s: %d) reset record resource", getName(), _id);

    // Screen
    _resource_screen_count = 0;
    _resource_screens.clear();
    _resource_screens_class_parent_map.clear();

    // Timer
    _resource_timer_count = 0;
    _resource_timers.clear();
    _resource_timers_cb_usr_map.clear();

    // Animation
    _resource_anim_count = 0;
    _resource_anims.clear();
    _resource_anims_var_exec_map.clear();

    _flags.is_resource_recording = false;

    return true;
}

bool ESP_Brookesia_CoreApp::enableAutoClean(void)
{
    lv_obj_t *last_screen = _core->getDisplayDevice()->scr_to_load;

    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_BROOKESIA_LOGD("App(%s: %d) enable auto clean", getName(), _id);

    // Check if the last screen is valid, if not, use the active screen
    if (last_screen == nullptr) {
        last_screen = _active_screen;
    }
    ESP_BROOKESIA_LOGD("Clean resource when screen(0x%p) loaded", last_screen);

    ESP_BROOKESIA_CHECK_FALSE_RETURN(lv_obj_is_valid(last_screen), false, "Invalid last screen");
    lv_obj_add_event_cb(last_screen, onCleanResourceEventCallback, LV_EVENT_SCREEN_UNLOADED, this);

    return true;
}

bool ESP_Brookesia_CoreApp::saveDisplayTheme(void)
{
    lv_disp_t *display = nullptr;
    lv_theme_t *theme = nullptr;

    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_BROOKESIA_LOGD("App(%s: %d) save display theme", getName(), _id);

    display = _core->getDisplayDevice();
    ESP_BROOKESIA_CHECK_NULL_RETURN(display, false, "Invalid display");

    theme = lv_disp_get_theme(display);
    ESP_BROOKESIA_CHECK_NULL_RETURN(theme, false, "Invalid display theme");

    _display_style.theme = theme;

    return true;
}

bool ESP_Brookesia_CoreApp::loadDisplayTheme(void)
{
    lv_disp_t *display = nullptr;
    lv_theme_t *&theme = _display_style.theme;

    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_BROOKESIA_LOGD("App(%s: %d) load display theme", getName(), _id);

    display = _core->getDisplayDevice();
    ESP_BROOKESIA_CHECK_NULL_RETURN(display, false, "Invalid display");

    ESP_BROOKESIA_CHECK_NULL_RETURN(theme, false, "Invalid display theme");
    lv_disp_set_theme(display, theme);

    return true;
}

bool ESP_Brookesia_CoreApp::saveAppTheme(void)
{
    lv_disp_t *display = nullptr;
    lv_theme_t *theme = nullptr;

    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_BROOKESIA_LOGD("App(%s: %d) save app theme", getName(), _id);

    display = _core->getDisplayDevice();
    ESP_BROOKESIA_CHECK_NULL_RETURN(display, false, "Invalid display");

    theme = lv_disp_get_theme(display);
    ESP_BROOKESIA_CHECK_NULL_RETURN(theme, false, "Invalid app theme");

    _app_style.theme = theme;

    return true;
}

bool ESP_Brookesia_CoreApp::loadAppTheme(void)
{
    lv_disp_t *display = nullptr;
    lv_theme_t *&theme = _display_style.theme;

    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_BROOKESIA_LOGD("App(%s: %d) load app theme", getName(), _id);

    display = _core->getDisplayDevice();
    ESP_BROOKESIA_CHECK_NULL_RETURN(display, false, "Invalid display");

    ESP_BROOKESIA_CHECK_NULL_RETURN(theme, false, "Invalid app theme");
    lv_disp_set_theme(display, theme);

    return true;
}

void ESP_Brookesia_CoreApp::onCleanResourceEventCallback(lv_event_t *event)
{
    ESP_Brookesia_CoreApp *app = nullptr;

    ESP_BROOKESIA_LOGD("App clean resource event callback");
    ESP_BROOKESIA_CHECK_NULL_EXIT(event, "Invalid event");

    app = (ESP_Brookesia_CoreApp *)lv_event_get_user_data(event);
    ESP_BROOKESIA_CHECK_NULL_EXIT(app, "Invalid app");

    ESP_BROOKESIA_LOGD("Clean app(%s: %d) resources", app->getName(), app->_id);
    ESP_BROOKESIA_CHECK_FALSE_EXIT(app->checkInitialized(), "Not initialized");

    if (!app->cleanResource()) {
        ESP_BROOKESIA_LOGE("Clean resource failed");
    }
    if (app->_core_active_data.flags.enable_recycle_resource) {
        if (!app->cleanRecordResource()) {
            ESP_BROOKESIA_LOGE("Clean record resource failed");
        }
    } else if (app->_core_active_data.flags.enable_default_screen && !app->cleanDefaultScreen()) {
        ESP_BROOKESIA_LOGE("Clean default screen failed");
    }
}

void ESP_Brookesia_CoreApp::onResizeScreenLoadedEventCallback(lv_event_t *event)
{
    ESP_Brookesia_CoreApp *app = nullptr;
    lv_obj_t *screen = nullptr;
    lv_area_t area = { 0 };

    ESP_BROOKESIA_LOGD("App resize screen loaded event callback");
    ESP_BROOKESIA_CHECK_NULL_EXIT(event, "Invalid event");

    app = (ESP_Brookesia_CoreApp *)lv_event_get_user_data(event);
    screen = (lv_obj_t *)lv_event_get_target(event);
    ESP_BROOKESIA_CHECK_NULL_EXIT(app, "Invalid app");
    ESP_BROOKESIA_CHECK_NULL_EXIT(screen, "Invalid screen");

    ESP_BROOKESIA_CHECK_FALSE_EXIT(app->checkInitialized(), "Not initialized");
    ESP_BROOKESIA_LOGD("Resize app(%s: %d) screen", app->getName(), app->_id);

    area = app->getVisualArea();
    lv_obj_set_pos(screen, area.x1, area.y1);
}

// TODO
// bool ESP_Brookesia_CoreApp::createAndloadTempScreen(void)
// {
//     ESP_BROOKESIA_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
//     ESP_BROOKESIA_LOGD("App(%s: %d) create temp screen", getName(), _id);

//     _temp_screen = lv_obj_create(nullptr);
//     ESP_BROOKESIA_CHECK_NULL_RETURN(_temp_screen, false, "Create temp screen failed");

//     lv_obj_set_style_bg_color(_temp_screen, lv_color_hex(_core->getCoreData().home.background.color.color), 0);
//     lv_obj_set_style_bg_opa(_temp_screen, _core->getCoreData().home.background.color.opacity, 0);
//     lv_scr_load(_temp_screen);
//     lv_obj_invalidate(_temp_screen);

//     return true;
// }

// bool ESP_Brookesia_CoreApp::delTempScreen(void)
// {
//     ESP_BROOKESIA_CHECK_FALSE_RETURN((_temp_screen != nullptr) && lv_obj_is_valid(_temp_screen), false, "Invalid temp screen");
//     ESP_BROOKESIA_LOGD("App(%s: %d) delete temp screen", getName(), _id);

//     lv_obj_del(_temp_screen);
//     _temp_screen = nullptr;

//     return true;
// }
