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
#include "esp_ui_core_utils.h"
#include "esp_ui_core.hpp"
#include "esp_ui_core_app.hpp"

#if !ESP_UI_LOG_ENABLE_DEBUG_CORE_APP
#undef ESP_UI_LOGD
#define ESP_UI_LOGD(...)
#endif

using namespace std;

ESP_UI_CoreApp::ESP_UI_CoreApp(const ESP_UI_CoreAppData_t &data):
    _core(nullptr),
    _core_init_data(data),
    _status(ESP_UI_CORE_APP_STATUS_UNINSTALLED),
    _id(-1),
    _flags{},
    _display_style{},
    _app_style{},
    _resource_timer_count(0),
    _resource_anim_count(0),
    _resource_head_screen_index(0),
    _resource_screen_count(0),
    _active_screen(nullptr),
    _resource_head_timer(nullptr),
    _resource_head_anim(nullptr)
{
}

ESP_UI_CoreApp::ESP_UI_CoreApp(const char *name, const void *launcher_icon, bool use_default_screen):
    _core(nullptr),
    _core_init_data(ESP_UI_CORE_APP_DATA_DEFAULT(name, launcher_icon, use_default_screen)),
    _status(ESP_UI_CORE_APP_STATUS_UNINSTALLED),
    _id(-1),
    _flags{},
    _display_style{},
    _app_style{},
    _resource_timer_count(0),
    _resource_anim_count(0),
    _resource_head_screen_index(0),
    _resource_screen_count(0),
    _active_screen(nullptr),
    _resource_head_timer(nullptr),
    _resource_head_anim(nullptr)
{
}

bool ESP_UI_CoreApp::cleanResource(void)
{
    ESP_UI_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    if (!_core_active_data.flags.enable_recycle_resource) {
        ESP_UI_LOGD("App(%s: %d) clean resource is disabled", getName(), _id);
        return true;
    }
    ESP_UI_LOGD("App(%s: %d) clean resource", getName(), _id);

    std::list <lv_obj_t *> resource_screens = _resource_screens;
    lv_timer_t *timer_node = lv_timer_get_next(NULL);
    lv_anim_t *anim_node = (lv_anim_t *)_lv_ll_get_head(&LV_GC_ROOT(_lv_anim_ll));


    // Screen
    for (auto screen : resource_screens) {
        if (lv_obj_is_valid(screen)) {
            lv_obj_del(screen);
            _resource_screens.erase(find(_resource_screens.begin(), _resource_screens.end(), screen));
        }
    }
    ESP_UI_LOGD("Clean screen(%d), miss(%d): ", _resource_screen_count - (int)_resource_screens.size(),
                (int)_resource_screens.size());

    // Timer
    while ((timer_node != NULL) && (_resource_timers.size() > 0)) {
        auto timer_it = find(_resource_timers.begin(), _resource_timers.end(), timer_node);
        if (timer_it != _resource_timers.end()) {
            lv_timer_del(timer_node);
            _resource_timers.erase(timer_it);
            timer_node = lv_timer_get_next(NULL);
        } else {
            timer_node = lv_timer_get_next(timer_node);
        }
    }
    ESP_UI_LOGD("Clean timer(%d), miss(%d): ", _resource_timer_count - (int)_resource_timers.size(),
                (int)_resource_timers.size());

    // Animation
    while ((anim_node != NULL) && (_resource_anims.size() > 0)) {
        auto anim_it = find(_resource_anims.begin(), _resource_anims.end(), anim_node);
        if (anim_it != _resource_anims.end()) {
            lv_anim_del(anim_node->var, anim_node->exec_cb);
            _resource_anims.erase(anim_it);
            anim_node = (lv_anim_t *)_lv_ll_get_head(&LV_GC_ROOT(_lv_anim_ll));
        } else {
            anim_node = (lv_anim_t *)_lv_ll_get_next(&LV_GC_ROOT(_lv_anim_ll), anim_node);
        }
    }
    ESP_UI_LOGD("Clean anim(%d), miss(%d): ", _resource_anim_count - (int)_resource_anims.size(),
                (int)_resource_anims.size());

    return true;
}

bool ESP_UI_CoreApp::notifyCoreClosed(void) const
{
    lv_obj_t *event_obj = nullptr;
    lv_event_code_t event_code = _LV_EVENT_LAST;
    lv_res_t res = LV_RES_OK;
    ESP_UI_CoreAppEventData_t event_data = {
        .id = _id,
        .type = ESP_UI_CORE_APP_EVENT_TYPE_STOP,
        .data = nullptr
    };

    ESP_UI_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_UI_LOGD("App(%s: %d) notify core closed", getName(), _id);

    if (_flags.is_closing) {
        return true;
    }

    event_obj = _core->getEventObject();
    event_code = _core->getAppEventCode();
    ESP_UI_CHECK_FALSE_RETURN(event_obj != nullptr, false, "Event object is invalid");
    ESP_UI_CHECK_FALSE_RETURN(esp_ui_core_utils_check_event_code_valid(event_code), false, "Event code is invalid");

    res = lv_event_send(event_obj, event_code, &event_data);
    ESP_UI_CHECK_FALSE_RETURN(res == LV_RES_OK, false, "Send app closed event failed");

    return true;
}

bool ESP_UI_CoreApp::processInstall(ESP_UI_Core *core, int id)
{
    ESP_UI_CHECK_FALSE_RETURN(!checkInitialized(), false, "Already initialized");
    ESP_UI_CHECK_NULL_RETURN(_core_init_data.name, false, "App name is invalid");
    ESP_UI_CHECK_NULL_RETURN(core, false, "Core is invalid");

    ESP_UI_LOGD("App(%s: %d) install", _core_init_data.name, id);

    _core_active_data = _core_init_data;
    ESP_UI_CHECK_FALSE_RETURN(
        core->getCoreHome().calibrateCoreObjectSize(core->getCoreData().screen_size, _core_active_data.screen_size),
        false, "Calibrate screen size failed"
    );
    _core = core;
    _id = id;

    ESP_UI_CHECK_FALSE_RETURN(beginExtra(), false, "Begin extra failed");
    ESP_UI_CHECK_FALSE_RETURN(init(), false, "Init failed");

    _status = ESP_UI_CORE_APP_STATUS_CLOSED;

    return true;
}

void ESP_UI_CoreApp::setLauncherIconImage(const ESP_UI_StyleImage_t &icon_image)
{
    _core_active_data.launcher_icon = icon_image;
}

bool ESP_UI_CoreApp::processUninstall(void)
{
    ESP_UI_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_UI_LOGD("App(%s: %d) uninstall", getName(), _id);

    _core = nullptr;
    _core_active_data = {};
    _status = ESP_UI_CORE_APP_STATUS_UNINSTALLED;
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
    _temp_screen = nullptr;
    _resource_head_timer = nullptr;
    _resource_head_anim = nullptr;
    _resource_screens.clear();
    _resource_timers.clear();
    _resource_anims.clear();

    ESP_UI_CHECK_FALSE_RETURN(delExtra(), false, "Begin extra failed");
    ESP_UI_CHECK_FALSE_RETURN(deinit(), false, "Deinit failed");

    return true;
}

bool ESP_UI_CoreApp::processRun(lv_area_t area)
{
    ESP_UI_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_UI_LOGD("App(%s: %d) run", getName(), _id);

    ESP_UI_CHECK_FALSE_RETURN(setVisualArea(area), false, "Set app visual area failed");
    ESP_UI_CHECK_FALSE_RETURN(startResizeVisualArea(), false, "Start resize visual area failed");
    ESP_UI_CHECK_FALSE_RETURN(initActiveScreen(), false, "Init active screen failed");
    ESP_UI_CHECK_FALSE_GOTO(startRecordResource(), err, "Start record resource failed");
    ESP_UI_CHECK_FALSE_GOTO(saveDisplayTheme(), err, "Save display theme failed");
    ESP_UI_CHECK_FALSE_GOTO(run(), err, "Run app failed");
    ESP_UI_CHECK_FALSE_GOTO(endRecordResource(), err, "End record resource failed");
    ESP_UI_CHECK_FALSE_GOTO(saveRecentScreen(), err, "Save recent screen failed");
    ESP_UI_CHECK_FALSE_GOTO(endResizeVisualArea(), err, "End resize visual area failed");

    _status = ESP_UI_CORE_APP_STATUS_RUNNING;

    return true;

err:
    if (_core_active_data.flags.enable_default_screen && lv_obj_is_valid(_active_screen)) {
        lv_obj_del(_active_screen);
        _active_screen = nullptr;
    }

    return false;
}

bool ESP_UI_CoreApp::processResume(void)
{
    ESP_UI_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_UI_LOGD("App(%s: %d) resume", getName(), _id);

    ESP_UI_CHECK_FALSE_RETURN(loadRecentScreen(), false, "Load recent screen failed");
    ESP_UI_CHECK_FALSE_RETURN(loadAppTheme(), false, "Load app theme failed");
    ESP_UI_CHECK_FALSE_RETURN(resume(), false, "Resume app failed");

    _status = ESP_UI_CORE_APP_STATUS_RUNNING;

    return true;
}

bool ESP_UI_CoreApp::processPause(void)
{
    ESP_UI_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_UI_LOGD("App(%s: %d) pause", getName(), _id);

    ESP_UI_CHECK_FALSE_RETURN(pause(), false, "Pause app failed");
    ESP_UI_CHECK_FALSE_RETURN(saveAppTheme(), false, "Save app theme failed");
    ESP_UI_CHECK_FALSE_RETURN(saveRecentScreen(), false, "Save app recent screen failed");
    ESP_UI_CHECK_FALSE_RETURN(loadDisplayTheme(), false, "Load display theme failed");

    _status = ESP_UI_CORE_APP_STATUS_PAUSED;

    return true;
}

bool ESP_UI_CoreApp::processClose(bool is_app_active)
{
    ESP_UI_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_UI_LOGD("App(%s: %d) close", getName(), _id);

    // Prevent recursive close
    _flags.is_closing = true;

    if (!close()) {
        ESP_UI_LOGE("Close app failed");
    }
    // Check if the app is active, if not, clean the resource immediately.
    // Otherwise, clean the resource when the screen is unloaded
    if (is_app_active) {
        // Save the last screen
        ESP_UI_CHECK_FALSE_RETURN(saveRecentScreen(), false, "Save app recent screen failed");
        // This is to prevent the screen from being cleaned before the screen is unloaded
        ESP_UI_CHECK_FALSE_RETURN(enableAutoClean(), false, "Enable auto clean failed");
    } else {
        ESP_UI_CHECK_FALSE_RETURN(cleanResource(), false, "Clean resource failed");
        ESP_UI_CHECK_FALSE_RETURN(cleanActiveScreen(), false, "Clean active screen failed");
    }
    ESP_UI_CHECK_FALSE_RETURN(loadDisplayTheme(), false, "Load display theme failed");

    _flags.is_closing = false;
    _status = ESP_UI_CORE_APP_STATUS_CLOSED;

    return true;
}

bool ESP_UI_CoreApp::initActiveScreen(void)
{
    ESP_UI_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_UI_LOGD("App(%s: %d) init active screen", getName(), _id);

    // Create active screen if needed
    if (_core_active_data.flags.enable_default_screen) {
        ESP_UI_LOGD("Init and load default active screen");

        _active_screen = lv_obj_create(nullptr);
        ESP_UI_CHECK_NULL_RETURN(_active_screen, false, "Create default screen failed");

        // Move screens to visual area when loaded only if needed
        if (_core_active_data.flags.enable_resize_visual_area) {
            lv_obj_add_event_cb(_active_screen, onResizeScreenLoadedEventCallback, LV_EVENT_SCREEN_LOADED, this);
        }

        lv_scr_load(_active_screen);

        if (_flags.is_screen_small) {
            ESP_UI_CHECK_FALSE_RETURN(delTempScreen(), false, "Delete temp screen failed");
        }
    } else {
        ESP_UI_LOGD("Use custom screens");
    }

    return true;
}

bool ESP_UI_CoreApp::startResizeVisualArea(void)
{
    lv_disp_t *disp = nullptr;
    lv_area_t &visual_area = _app_style.visual_area;

    ESP_UI_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_UI_LOGD("App(%s: %d) start resize visual area", getName(), _id);

    disp = _core->getDisplayDevice();
    ESP_UI_CHECK_NULL_RETURN(disp, false, "Invalid display");

    if (_flags.is_screen_small) {
        // Create a temp screen to recolor the background
        ESP_UI_CHECK_FALSE_RETURN(createAndloadTempScreen(), false, "Create temp screen failed");
    }

    _display_style.w = disp->driver->hor_res;
    _display_style.h = disp->driver->ver_res;
    disp->driver->hor_res = visual_area.x2 - visual_area.x1 + 1;
    disp->driver->ver_res = visual_area.y2 - visual_area.y1 + 1;

    return true;
}

bool ESP_UI_CoreApp::endResizeVisualArea(void)
{
    lv_disp_t *disp = nullptr;
    lv_area_t &visual_area = _app_style.visual_area;

    ESP_UI_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_UI_LOGD("App(%s: %d) end resize visual area", getName(), _id);

    disp = _core->getDisplayDevice();
    ESP_UI_CHECK_NULL_RETURN(disp, false, "Invalid display");

    disp->driver->hor_res = _display_style.w;
    disp->driver->ver_res = _display_style.h;

    // Move the active screen to visual area again to make sure it's in the right position
    ESP_UI_CHECK_FALSE_RETURN(lv_obj_is_valid(_active_screen), false, "Invalid active screen");
    lv_obj_set_pos(_active_screen, visual_area.x1, visual_area.y1);
    lv_obj_invalidate(_active_screen);

    // Move screens to visual area when loaded only if needed
    if (!_core_active_data.flags.enable_default_screen) {
        ESP_UI_LOGD("Resize all custom screens");
        for (auto screen : _resource_screens) {
            if (!lv_obj_is_valid(screen)) {
                continue;
            }
            lv_obj_set_pos(screen, visual_area.x1, visual_area.y1);
            lv_obj_invalidate(_active_screen);
            lv_obj_add_event_cb(screen, onResizeScreenLoadedEventCallback, LV_EVENT_SCREEN_LOAD_START, this);
        }
    }

    return true;
}

bool ESP_UI_CoreApp::startRecordResource(void)
{
    lv_disp_t *disp = nullptr;

    ESP_UI_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    if (!_core_active_data.flags.enable_recycle_resource && !_core_active_data.flags.enable_resize_visual_area) {
        ESP_UI_LOGD("App(%s: %d) start record resource is disabled", getName(), _id);
        return true;
    }
    ESP_UI_LOGD("App(%s: %d) start record resource", getName(), _id);

    disp = _core->getDisplayDevice();
    ESP_UI_CHECK_NULL_RETURN(disp, false, "Invalid display");

    // Screen
    _resource_head_screen_index = disp->screen_cnt - 1;
    _resource_screen_count = 0;
    _resource_screens.clear();

    // Timer
    _resource_head_timer = lv_timer_get_next(nullptr);
    _resource_timer_count = 0;
    _resource_timers.clear();

    // Animation
    _resource_head_anim = (lv_anim_t *)_lv_ll_get_head(&LV_GC_ROOT(_lv_anim_ll));
    _resource_anim_count = 0;
    _resource_anims.clear();

    return true;
}

bool ESP_UI_CoreApp::endRecordResource(void)
{
    lv_disp_t *disp = nullptr;
    lv_timer_t *timer_node = nullptr;
    lv_anim_t *anim_node = nullptr;

    ESP_UI_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    if (!_core_active_data.flags.enable_recycle_resource && !_core_active_data.flags.enable_resize_visual_area) {
        ESP_UI_LOGD("App(%s: %d) end record resource is disabled", getName(), _id);
        return true;
    }
    ESP_UI_LOGD("App(%s: %d) end record resource", getName(), _id);

    disp = _core->getDisplayDevice();
    ESP_UI_CHECK_NULL_RETURN(disp, false, "Invalid display");

    // Screen
    for (int i = _resource_head_screen_index + 1; i < (int)disp->screen_cnt; i++) {
        _resource_screens.push_back(disp->screens[i]);
        _resource_screen_count++;
    }
    if (_resource_head_screen_index >= (int)disp->screen_cnt) {
        ESP_UI_LOGE("record screen fail");
    } else {
        ESP_UI_LOGD("record screen(%d): ", _resource_screen_count);
    }

    // Timer
    timer_node = lv_timer_get_next(nullptr);
    while ((timer_node != nullptr) && (timer_node != _resource_head_timer)) {
        _resource_timers.push_back(timer_node);
        _resource_timer_count++;
        timer_node = lv_timer_get_next(timer_node);
        if ((timer_node == nullptr) && (_resource_head_timer != nullptr)) {
            break;
        }
    }
    if ((timer_node == nullptr) && (_resource_head_timer != nullptr)) {
        _resource_timers.clear();
        _resource_timer_count = 0;
        ESP_UI_LOGE("record timer fail");
    } else {
        ESP_UI_LOGD("record timer(%d): ", _resource_timer_count);
    }

    // Animation
    anim_node = (lv_anim_t *)_lv_ll_get_head(&LV_GC_ROOT(_lv_anim_ll));
    while ((anim_node != nullptr) && (anim_node != _resource_head_anim)) {
        _resource_anims.push_back(anim_node);
        _resource_anim_count++;
        anim_node = (lv_anim_t *)_lv_ll_get_next(&LV_GC_ROOT(_lv_anim_ll), anim_node);
    }
    if ((anim_node == nullptr) && (_resource_head_anim != nullptr)) {
        _resource_anims.clear();
        _resource_anim_count = 0;
        ESP_UI_LOGE("record animation fail");
    } else {
        ESP_UI_LOGD("record animation(%d): ", _resource_anim_count);
    }

    return true;
}

bool ESP_UI_CoreApp::saveRecentScreen(void)
{
    ESP_UI_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_UI_LOGD("App(%s: %d) save recent screen", getName(), _id);

    _active_screen = lv_disp_get_scr_act(_core->getDisplayDevice());
    ESP_UI_CHECK_NULL_RETURN(_active_screen, false, "Invalid active screen");

    return true;
}

bool ESP_UI_CoreApp::loadRecentScreen(void)
{
    ESP_UI_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_UI_LOGD("App(%s: %d) load recent screen", getName(), _id);

    if (_flags.is_screen_small) {
        // Create a temp screen to recolor the background
        ESP_UI_CHECK_FALSE_RETURN(createAndloadTempScreen(), false, "Create temp screen failed");
    }

    ESP_UI_CHECK_FALSE_RETURN(lv_obj_is_valid(_active_screen), false, "Invalid active screen");
    lv_scr_load(_active_screen);

    if (_flags.is_screen_small) {
        ESP_UI_CHECK_FALSE_RETURN(delTempScreen(), false, "Delete temp screen failed");
    }

    return true;
}

bool ESP_UI_CoreApp::setVisualArea(const lv_area_t &area)
{
    uint16_t visual_area_x = 0;
    uint16_t visual_area_y = 0;
    uint16_t visual_area_w = 0;
    uint16_t visual_area_h = 0;
    lv_area_t visual_area = area;
    const ESP_UI_StyleSize_t &screen_size = getCoreActiveData().screen_size;

    ESP_UI_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_UI_LOGD("App(%s: %d) set visual area(%d,%d-%d,%d)", getName(), _id, area.x1, area.y1, area.x2, area.y2);

    visual_area_w = visual_area.x2 - visual_area.x1 + 1;
    visual_area_h = visual_area.y2 - visual_area.y1 + 1;
    visual_area_x = visual_area.x1;
    visual_area_y = visual_area.y1;
    if (visual_area_w > screen_size.width) {
        visual_area_x = visual_area.x1 + (visual_area_w - screen_size.width) / 2;
    }
    if (visual_area_h > screen_size.height) {
        visual_area_y = visual_area.y1 + (visual_area_h - screen_size.height) / 2;
    }
    visual_area_w = min(visual_area_w, screen_size.width);
    visual_area_h = min(visual_area_h, screen_size.height);
    visual_area.x1 = visual_area_x;
    visual_area.y1 = visual_area_y;
    visual_area.x2 = visual_area_x + visual_area_w - 1;
    visual_area.y2 = visual_area_y + visual_area_h - 1;
    _app_style.visual_area = visual_area;

    _flags.is_screen_small = ((lv_area_get_height(&visual_area) < screen_size.height) ||
                              (lv_area_get_width(&visual_area) < screen_size.width));

    return true;
}

bool ESP_UI_CoreApp::cleanActiveScreen(void)
{
    // Clean active screen if used
    if (!_core_active_data.flags.enable_default_screen) {
        ESP_UI_LOGD("App(%s: %d) use custom screen and don't need to clean", getName(), _id);
        return true;
    }

    ESP_UI_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_UI_LOGD("App(%s: %d) clean default active screen", getName(), _id);

    if (lv_obj_is_valid(_active_screen)) {
        lv_obj_del(_active_screen);
    } else {
        ESP_UI_LOGW("Active screen is already cleaned");
    }
    _active_screen = nullptr;

    return true;
}

bool ESP_UI_CoreApp::enableAutoClean(void)
{
    lv_obj_t *last_screen = _core->getDisplayDevice()->scr_to_load;

    ESP_UI_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_UI_LOGD("App(%s: %d) enable auto clean", getName(), _id);

    // Check if the last screen is valid, if not, use the active screen
    if (last_screen == nullptr) {
        last_screen = _active_screen;
    }
    ESP_UI_LOGD("Clean resource when screen(0x%p) loaded", last_screen);

    ESP_UI_CHECK_FALSE_RETURN(lv_obj_is_valid(last_screen), false, "Invalid last screen");
    lv_obj_add_event_cb(last_screen, onCleanResourceEventCallback, LV_EVENT_SCREEN_UNLOADED, this);

    return true;
}

bool ESP_UI_CoreApp::saveDisplayTheme(void)
{
    lv_disp_t *display = nullptr;
    lv_theme_t *theme = nullptr;

    ESP_UI_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_UI_LOGD("App(%s: %d) save display theme", getName(), _id);

    display = _core->getDisplayDevice();
    ESP_UI_CHECK_NULL_RETURN(display, false, "Invalid display");

    theme = lv_disp_get_theme(display);
    ESP_UI_CHECK_NULL_RETURN(theme, false, "Invalid display theme");

    _display_style.theme = theme;

    return true;
}

bool ESP_UI_CoreApp::loadDisplayTheme(void)
{
    lv_disp_t *display = nullptr;
    lv_theme_t *&theme = _display_style.theme;

    ESP_UI_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_UI_LOGD("App(%s: %d) load display theme", getName(), _id);

    display = _core->getDisplayDevice();
    ESP_UI_CHECK_NULL_RETURN(display, false, "Invalid display");

    ESP_UI_CHECK_NULL_RETURN(theme, false, "Invalid display theme");
    lv_disp_set_theme(display, theme);

    return true;
}

bool ESP_UI_CoreApp::saveAppTheme(void)
{
    lv_disp_t *display = nullptr;
    lv_theme_t *theme = nullptr;

    ESP_UI_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_UI_LOGD("App(%s: %d) save app theme", getName(), _id);

    display = _core->getDisplayDevice();
    ESP_UI_CHECK_NULL_RETURN(display, false, "Invalid display");

    theme = lv_disp_get_theme(display);
    ESP_UI_CHECK_NULL_RETURN(theme, false, "Invalid app theme");

    _app_style.theme = theme;

    return true;
}

bool ESP_UI_CoreApp::loadAppTheme(void)
{
    lv_disp_t *display = nullptr;
    lv_theme_t *&theme = _display_style.theme;

    ESP_UI_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_UI_LOGD("App(%s: %d) load app theme", getName(), _id);

    display = _core->getDisplayDevice();
    ESP_UI_CHECK_NULL_RETURN(display, false, "Invalid display");

    ESP_UI_CHECK_NULL_RETURN(theme, false, "Invalid app theme");
    lv_disp_set_theme(display, theme);

    return true;
}

bool ESP_UI_CoreApp::createAndloadTempScreen(void)
{
    ESP_UI_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_UI_LOGD("App(%s: %d) create temp screen", getName(), _id);

    _temp_screen = lv_obj_create(nullptr);
    ESP_UI_CHECK_NULL_RETURN(_temp_screen, false, "Create temp screen failed");

    lv_obj_set_style_bg_color(_temp_screen, lv_color_hex(_core->getCoreData().home.background.color.color), 0);
    lv_obj_set_style_bg_opa(_temp_screen, _core->getCoreData().home.background.color.opacity, 0);
    lv_scr_load(_temp_screen);

    return true;
}

bool ESP_UI_CoreApp::delTempScreen(void)
{
    ESP_UI_CHECK_FALSE_RETURN((_temp_screen != nullptr) && lv_obj_is_valid(_temp_screen), false, "Invalid temp screen");
    ESP_UI_LOGD("App(%s: %d) delete temp screen", getName(), _id);

    lv_obj_del(_temp_screen);
    _temp_screen = nullptr;

    return true;
}

void ESP_UI_CoreApp::onCleanResourceEventCallback(lv_event_t *event)
{
    ESP_UI_CoreApp *app = nullptr;

    ESP_UI_LOGD("App clean resource event callback");
    ESP_UI_CHECK_NULL_EXIT(event, "Invalid event");

    app = (ESP_UI_CoreApp *)lv_event_get_user_data(event);
    ESP_UI_CHECK_NULL_EXIT(app, "Invalid app");

    ESP_UI_LOGD("Clean app(%s: %d) resources", app->getName(), app->_id);
    ESP_UI_CHECK_FALSE_EXIT(app->checkInitialized(), "Not initialized");

    if (!app->cleanResource()) {
        ESP_UI_LOGE("Clean resource failed");
    }
    if (!app->cleanActiveScreen()) {
        ESP_UI_LOGE("Clean active screen failed");
    }
}

void ESP_UI_CoreApp::onResizeScreenLoadedEventCallback(lv_event_t *event)
{
    ESP_UI_CoreApp *app = nullptr;
    lv_obj_t *screen = nullptr;
    lv_area_t area = { 0 };

    ESP_UI_LOGD("App resize screen loaded event callback");
    ESP_UI_CHECK_NULL_EXIT(event, "Invalid event");

    app = (ESP_UI_CoreApp *)lv_event_get_user_data(event);
    screen = (lv_obj_t *)lv_event_get_target(event);
    ESP_UI_CHECK_NULL_EXIT(app, "Invalid app");
    ESP_UI_CHECK_NULL_EXIT(screen, "Invalid screen");

    ESP_UI_CHECK_FALSE_EXIT(app->checkInitialized(), "Not initialized");
    ESP_UI_LOGD("Resize app(%s: %d) screen", app->getName(), app->_id);

    area = app->getVisualArea();
    lv_obj_set_pos(screen, area.x1, area.y1);
    lv_obj_invalidate(screen);
}
