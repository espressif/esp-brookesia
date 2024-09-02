/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <cmath>
#include "esp_ui_core_manager.hpp"
#include "esp_ui_core.hpp"
#include "esp_ui_conf_internal.h"
#ifdef ESP_UI_MEMORY_INCLUDE
#include ESP_UI_MEMORY_INCLUDE
#endif

#if !ESP_UI_LOG_ENABLE_DEBUG_CORE_MANAGER
#undef ESP_UI_LOGD
#define ESP_UI_LOGD(...)
#endif

using namespace std;

ESP_UI_CoreManager::ESP_UI_CoreManager(ESP_UI_Core &core, const ESP_UI_CoreManagerData_t &data):
    _core(core),
    _core_data(data),
    _app_free_id(0),
    _active_app(nullptr),
    _navigate_type(ESP_UI_CORE_NAVIGATE_TYPE_MAX)
{
}

ESP_UI_CoreManager::~ESP_UI_CoreManager()
{
    ESP_UI_LOGD("Destroy(@0x%p)", this);
    if (!delCore()) {
        ESP_UI_LOGE("Delete failed");
    }
}

int ESP_UI_CoreManager::installApp(ESP_UI_CoreApp *app)
{
    bool app_installed = false;
    bool home_process_app_installed = false;
    lv_area_t app_visual_area = {};
    ESP_UI_CoreHome &home = _core._core_home;

    ESP_UI_CHECK_NULL_RETURN(app, -1, "Invalid app");

    ESP_UI_LOGD("Install App(%p)", app);

    // Check if the app is already installed
    for (auto it = _id_installed_app_map.begin(); it != _id_installed_app_map.end(); it++) {
        ESP_UI_CHECK_FALSE_RETURN(it->second != app, -1, "Already installed");
    }

    // Initialize app
    ESP_UI_CHECK_FALSE_GOTO(app_installed = app->processInstall(&_core, _app_free_id), err, "App install failed");
    ESP_UI_CHECK_FALSE_GOTO(home.getAppVisualArea(app, app_visual_area), err, "Home get app visual area failed");
    ESP_UI_CHECK_FALSE_GOTO(app->setVisualArea(app_visual_area), err, "App set visual area failed");
    ESP_UI_CHECK_FALSE_GOTO(app->calibrateVisualArea(), err, "App calibrate visual area failed");

    // Process home
    ESP_UI_CHECK_FALSE_GOTO(home_process_app_installed = home.processAppInstall(app), err,
                            "Home process app install failed");

    // Insert app to installed_app_map
    ESP_UI_CHECK_FALSE_GOTO(_id_installed_app_map.insert(pair <int, ESP_UI_CoreApp *>(app->_id, app)).second, err,
                            "Insert app failed");

    // Update free app id
    _app_free_id++;

    return app->getId();

err:
    if (home_process_app_installed && !home.processAppUninstall(app)) {
        ESP_UI_LOGE("Home process app uninstall failed");
    }
    if (app_installed && !app->processUninstall()) {
        ESP_UI_LOGE("App uninstall failed");
    }

    return -1;
}

int ESP_UI_CoreManager::installApp(ESP_UI_CoreApp &app)
{
    return installApp(&app);
}

int ESP_UI_CoreManager::uninstallApp(ESP_UI_CoreApp *app)
{
    bool ret = true;
    int app_id = -1;
    ESP_UI_CoreHome &home = _core._core_home;

    ESP_UI_CHECK_NULL_RETURN(app, false, "Invalid app");
    app_id = app->_id;

    ESP_UI_LOGD("Uninstall App(%d)", app_id);

    // Check if the app is already installed
    auto it = _id_installed_app_map.begin();
    for (; it != _id_installed_app_map.end(); it++) {
        if (it->second == app) {
            break;
        }
    }
    ESP_UI_CHECK_FALSE_RETURN(it->second == app, false, "App(%d) is not installed", app_id);

    // Process home
    ESP_UI_CHECK_FALSE_RETURN(home.processAppUninstall(app), false, "Home process app uninstall failed");

    // Deinit app
    ret = app->processUninstall();
    if (!ret) {
        ESP_UI_LOGE("App uninstall failed");
    }

    // Remove app from installed_app_map
    ESP_UI_CHECK_FALSE_RETURN(_id_installed_app_map.erase(app_id) > 0, false, "Remove app failed");

    return ret;
}

int ESP_UI_CoreManager::uninstallApp(ESP_UI_CoreApp &app)
{
    return uninstallApp(&app);
}

bool ESP_UI_CoreManager::uninstallApp(int id)
{
    ESP_UI_CoreApp *app = nullptr;

    ESP_UI_LOGD("Uninstall App(%d)", id);

    app = getInstalledApp(id);
    ESP_UI_CHECK_NULL_RETURN(app, false, "Get installed app failed");

    ESP_UI_CHECK_FALSE_RETURN(uninstallApp(app), false, "Uninstall app failed");

    return true;
}

bool ESP_UI_CoreManager::startApp(int id)
{
    ESP_UI_CoreApp *app = NULL;
    ESP_UI_CoreApp *app_old = NULL;

    // Check if the app is already running
    auto find_ret = _id_running_app_map.find(id);
    if (find_ret != _id_running_app_map.end()) {
        app = find_ret->second;
        ESP_UI_LOGD("App(%d) is already running, just resume it", app->_id);
        // If so, resume app
        ESP_UI_CHECK_FALSE_RETURN(processAppResume(app), false, "Resume app failed");

        return true;
    }

    // If not, then find the target app from installed app map
    find_ret = _id_installed_app_map.find(id);
    ESP_UI_CHECK_FALSE_RETURN(find_ret != _id_installed_app_map.end(), false, "Can't find app in installed app map");
    app = find_ret->second;

    // Check if the running app num is at the limit
    if ((_core_data.app.max_running_num != 0) && (int)_id_running_app_map.size() >= _core_data.app.max_running_num) {
        for (auto it = _id_running_app_map.begin(); it != _id_running_app_map.end(); it++) {
            app_old = it->second;
        }
        ESP_UI_CHECK_NULL_RETURN(app_old, false, "Get old app failed");

        ESP_UI_LOGW("Running app num(%d) is already at the limit, will close the oldest app(%d)",
                    (int)_id_running_app_map.size(), app_old->_id);

        ESP_UI_CHECK_FALSE_RETURN(processAppClose(app_old), false, "Close app failed");
    }

    // Start app
    ESP_UI_CHECK_FALSE_RETURN(processAppRun(app), false, "Start app failed");

    // Add app to running_app_map
    ESP_UI_CHECK_FALSE_GOTO(_id_running_app_map.insert(pair <int, ESP_UI_CoreApp *>(id, app)).second, err,
                            "Insert app to running map failed");

    return true;

err:
    ESP_UI_CHECK_FALSE_RETURN(processAppClose(app), false, "Close app failed");

    return false;
}

bool ESP_UI_CoreManager::processAppRun(ESP_UI_CoreApp *app)
{
    bool is_home_run = false;
    bool is_app_run = false;
    ESP_UI_CoreHome &home = _core._core_home;

    ESP_UI_CHECK_NULL_RETURN(app, false, "Invalid app");
    ESP_UI_LOGD("Process app(%d) run", app->_id);

    // Process home, and get the visual area of the app
    ESP_UI_CHECK_FALSE_RETURN(is_home_run = home.processAppRun(app), false, "Process home before app run failed");

    // Process app
    ESP_UI_CHECK_FALSE_GOTO(is_app_run = app->processRun(), err, "Process app run failed");

    // Process extra
    ESP_UI_CHECK_FALSE_GOTO(processAppRunExtra(app), err, "Process app run extra failed");

    // Update active app
    _active_app = app;

    return true;

err:
    if (is_home_run && !home.processAppClose(app)) {
        ESP_UI_LOGE("Home process close failed");
    }
    if (is_app_run && !app->processClose(true)) {
        ESP_UI_LOGE("App process close failed");
    }
    ESP_UI_CHECK_FALSE_RETURN(home.processMainScreenLoad(), false, "Home load main screen failed");

    return false;
}

bool ESP_UI_CoreManager::processAppResume(ESP_UI_CoreApp *app)
{
    ESP_UI_CoreHome &home = _core._core_home;

    ESP_UI_CHECK_NULL_RETURN(app, false, "Invalid app");
    ESP_UI_LOGD("Process app(%d) resume", app->_id);

    // Check if the screen is showing app and the app is not the active one
    if ((_active_app != nullptr) && (_active_app != app)) {
        // if so, pause the active app
        ESP_UI_CHECK_FALSE_RETURN(processAppPause(_active_app), false, "App process pause failed");
    }

    // Process home
    ESP_UI_CHECK_FALSE_RETURN(home.processAppResume(app), false, "Home process resume failed");

    // Process app, only load active screen if the app is not shown
    ESP_UI_CHECK_FALSE_RETURN(app->processResume(), false, "App process resume failed");

    // Process extra
    ESP_UI_CHECK_FALSE_RETURN(processAppResumeExtra(app), false, "Process app resume extra failed");

    // Update active app
    _active_app = app;

    return true;
}

bool ESP_UI_CoreManager::processAppPause(ESP_UI_CoreApp *app)
{
    ESP_UI_CoreHome &home = _core._core_home;

    ESP_UI_CHECK_NULL_RETURN(app, false, "Invalid app");
    ESP_UI_LOGD("Process app(%d) pause", app->_id);

    // Process app
    ESP_UI_CHECK_FALSE_RETURN(app->processPause(), false, "App process pause failed");
    if (_core_data.flags.enable_app_save_snapshot) {
        if (!saveAppSnapshot(app)) {
            ESP_UI_LOGE("Save app snapshot failed");
        }
    }

    // Process home,
    ESP_UI_CHECK_FALSE_GOTO(home.processAppPause(app), err, "Home process load failed");

    // Process extra
    ESP_UI_CHECK_FALSE_GOTO(processAppPauseExtra(app), err, "Process app pause extra failed");

    return true;

err:
    ESP_UI_CHECK_FALSE_RETURN(processAppClose(app), false, "Close app failed");

    return false;
}

bool ESP_UI_CoreManager::processAppClose(ESP_UI_CoreApp *app)
{
    ESP_UI_CoreHome &home = _core._core_home;

    ESP_UI_CHECK_NULL_RETURN(app, false, "Invalid app");
    ESP_UI_LOGD("Process app(%d) close", app->_id);

    // Process app, enable auto clean when the app is showing
    ESP_UI_CHECK_FALSE_RETURN(app->processClose(_active_app == app), false, "App process close failed");
    if (_core_data.flags.enable_app_save_snapshot) {
        if (!releaseAppSnapshot(app)) {
            ESP_UI_LOGE("Release app snapshot failed");
        }
    }

    // Process home, load main screen if the app is showing
    ESP_UI_CHECK_FALSE_RETURN(home.processAppClose(app), false, "Home process close failed");

    // Process extra
    ESP_UI_CHECK_FALSE_RETURN(processAppCloseExtra(app), false, "Process app pause extra failed");

    // Remove app from running map and update active app
    ESP_UI_CHECK_FALSE_RETURN(_id_running_app_map.erase(app->_id) > 0, false, "Remove app from running map failed");
    if (_active_app == app) {
        _active_app = nullptr;
    }

    return true;
}

bool ESP_UI_CoreManager::saveAppSnapshot(ESP_UI_CoreApp *app)
{
#if !LV_USE_SNAPSHOT
    ESP_UI_CHECK_FALSE_RETURN(false, false, "`LV_USE_SNAPSHOT` is not enabled");
#else
    bool resize_app_screen = false;
    uint8_t *snapshot_buffer = nullptr;
    uint32_t snapshot_buffer_size = 0;
    lv_res_t ret = LV_RES_INV;
    lv_area_t app_screen_area = {};
    shared_ptr<ESP_UI_AppSnapshot_t> snapshot = nullptr;

    ESP_UI_CHECK_NULL_RETURN(app, false, "Invalid app");
    ESP_UI_LOGD("Save app(%d) snapshot", app->_id);

    ESP_UI_CHECK_FALSE_RETURN(lv_obj_is_valid(app->_active_screen), false, "Invalid active screen");
    app_screen_area = app->_active_screen->coords;
    if ((lv_area_get_width(&app_screen_area) != _core.getCoreData().screen_size.width) ||
            (lv_area_get_height(&app_screen_area) != _core.getCoreData().screen_size.height)) {
        ESP_UI_LOGD("Active screen size is not match screen size, resize it");
        app->_active_screen->coords = (lv_area_t) {
            .x1 = 0,
            .y1 = 0,
            .x2 = (lv_coord_t)(_core.getCoreData().screen_size.width - 1),
            .y2 = (lv_coord_t)(_core.getCoreData().screen_size.height - 1),
        };
        resize_app_screen = true;
    }

    auto it = _id_app_snapshot_map.find(app->_id);
    snapshot = (it != _id_app_snapshot_map.end()) ? it->second : nullptr;

    // Malloc snapshot buffer if no buffer or buffer size changed
    snapshot_buffer_size = lv_snapshot_buf_size_needed(app->_active_screen, LV_IMG_CF_TRUE_COLOR);
    if (snapshot == nullptr) {
        snapshot = make_shared<ESP_UI_AppSnapshot_t>();
        ESP_UI_CHECK_NULL_GOTO(snapshot, err, "Make snapshot object failed");

        snapshot_buffer = (uint8_t *)ESP_UI_MEMORY_MALLOC(snapshot_buffer_size);
        ESP_UI_CHECK_NULL_GOTO(snapshot_buffer, err, "Alloc snapshot buffer(%d) fail", (int)snapshot_buffer_size);

        snapshot->image_buffer = snapshot_buffer;
    } else if (snapshot_buffer_size != snapshot->image_resource.data_size) {
        ESP_UI_MEMORY_FREE(snapshot->image_buffer);
        snapshot->image_buffer = nullptr;

        snapshot_buffer = (uint8_t *)ESP_UI_MEMORY_MALLOC(snapshot_buffer_size);
        ESP_UI_CHECK_NULL_GOTO(snapshot_buffer, err, "Realloc snapshot buffer(%d) fail", (int)snapshot_buffer_size);

        snapshot->image_buffer = snapshot_buffer;
    }

    // And take snapshot for recent screen
    ret = lv_snapshot_take_to_buf(app->_active_screen, LV_IMG_CF_TRUE_COLOR, &snapshot->image_resource,
                                  snapshot->image_buffer, snapshot_buffer_size);
    ESP_UI_CHECK_FALSE_GOTO(ret == LV_RES_OK, err, "Take snapshot fail");

    _id_app_snapshot_map[app->_id] = snapshot;
    if (resize_app_screen) {
        app->_active_screen->coords = app_screen_area;
    }

    return true;

err:
    ESP_UI_MEMORY_FREE(snapshot_buffer);
    if (resize_app_screen) {
        app->_active_screen->coords = app_screen_area;
    }

    return false;
#endif
}

bool ESP_UI_CoreManager::releaseAppSnapshot(ESP_UI_CoreApp *app)
{
    ESP_UI_CHECK_NULL_RETURN(app, false, "Invalid app");
    ESP_UI_LOGD("Release app(%d) snapshot", app->_id);

    auto it = _id_app_snapshot_map.find(app->_id);
    if (it == _id_app_snapshot_map.end()) {
        return true;
    }

    ESP_UI_CHECK_NULL_RETURN(it->second, false, "Invalid snapshot object");
    ESP_UI_MEMORY_FREE(it->second->image_buffer);
    ESP_UI_CHECK_FALSE_RETURN(_id_app_snapshot_map.erase(app->_id) > 0, false, "Free snapshot failed");

    return true;
}

void ESP_UI_CoreManager::resetActiveApp(void)
{
    ESP_UI_LOGD("Reset active app");
    _active_app = nullptr;
}

int ESP_UI_CoreManager::getRunningAppIndexByApp(ESP_UI_CoreApp *app)
{
    ESP_UI_CHECK_NULL_RETURN(app, -1, "Invalid app");

    for (auto it = _id_running_app_map.begin(); it != _id_running_app_map.end(); ++it) {
        if (it->second == app) {
            return distance(it, _id_running_app_map.end()) - 1;
        }
    }

    ESP_UI_LOGE("App not found in running app list");
    return -1;
}

int ESP_UI_CoreManager::getRunningAppIndexById(int id)
{
    auto it = _id_running_app_map.find(id);
    if (it != _id_running_app_map.end()) {
        return distance(it, _id_running_app_map.end()) - 1;
    }

    ESP_UI_LOGE("App not found in running app list");
    return -1;
}

ESP_UI_CoreApp *ESP_UI_CoreManager::getInstalledApp(int id)
{
    auto it = _id_installed_app_map.find(id);
    if (it != _id_installed_app_map.end()) {
        return it->second;
    }

    ESP_UI_LOGE("App not found in installed app list");
    return nullptr;
}

ESP_UI_CoreApp *ESP_UI_CoreManager::getRunningAppByIdenx(uint8_t index)
{
    if (index >= _id_running_app_map.size()) {
        return nullptr;
    }

    auto it = _id_running_app_map.begin();
    advance(it, _id_running_app_map.size() - index - 1);

    return it->second;
}

ESP_UI_CoreApp *ESP_UI_CoreManager::getRunningAppById(int id)
{
    auto it = _id_running_app_map.find(id);
    if (it != _id_running_app_map.end()) {
        return it->second;
    }

    ESP_UI_LOGE("App not found in running app list");
    return nullptr;
}

const lv_img_dsc_t *ESP_UI_CoreManager::getAppSnapshot(int id)
{
    auto it = _id_app_snapshot_map.find(id);
    ESP_UI_CHECK_FALSE_RETURN(it != _id_app_snapshot_map.end(), nullptr, "App not found in running app list");

    return &it->second->image_resource;
}

bool ESP_UI_CoreManager::beginCore(void)
{
    ESP_UI_LOGD("Begin(@0x%p)", this);

    ESP_UI_CHECK_FALSE_RETURN(_core.registerAppEventCallback(onAppEventCallback, this), false,
                              "Register app event failed");
    ESP_UI_CHECK_FALSE_GOTO(_core.registerNavigateEventCallback(onNavigationEventCallback, this), err,
                            "Register navigation event failed");

    return true;

err:
    ESP_UI_CHECK_FALSE_RETURN(delCore(), false, "Delete failed");

    return false;
}

bool ESP_UI_CoreManager::delCore(void)
{
    bool ret = true;
    unordered_map <int, ESP_UI_CoreApp *> id_installed_app_map = _id_installed_app_map;

    ESP_UI_LOGD("Delete(@0x%p)", this);

    if (_core.checkCoreInitialized()) {
        if (!_core.unregisterAppEventCallback(onAppEventCallback, this)) {
            ESP_UI_LOGE("Unregister app event failed");
            ret = false;
        }
    }

    _app_free_id = 0;
    _active_app = nullptr;
    for (auto app : id_installed_app_map) {
        if (!uninstallApp(app.second)) {
            ESP_UI_LOGE("Uninstall app(%d) failed", app.second->_id);
            ret = false;
        }
    }
    _id_installed_app_map.clear();
    _id_running_app_map.clear();
    _id_app_snapshot_map.clear();

    return ret;
}

void ESP_UI_CoreManager::onAppEventCallback(lv_event_t *event)
{
    int id = -1;
    ESP_UI_CoreApp *app = nullptr;
    ESP_UI_CoreManager *manager = nullptr;
    ESP_UI_CoreAppEventData_t *event_data = nullptr;

    ESP_UI_LOGD("App start event callback");
    ESP_UI_CHECK_NULL_EXIT(event, "Invalid event object");

    manager = (ESP_UI_CoreManager *)lv_event_get_user_data(event);
    event_data = (ESP_UI_CoreAppEventData_t *)lv_event_get_param(event);
    ESP_UI_CHECK_NULL_EXIT(manager, "Invalid manager object");
    ESP_UI_CHECK_FALSE_EXIT((event_data != nullptr) && (event_data->type < ESP_UI_CORE_APP_EVENT_TYPE_MAX),
                            "Invalid event data");

    id = event_data->id;
    switch (event_data->type) {
    case ESP_UI_CORE_APP_EVENT_TYPE_START:
        ESP_UI_LOGD("Start app(%d)", id);
        ESP_UI_CHECK_FALSE_EXIT(manager->startApp(id), "Run app failed");
        break;
    case ESP_UI_CORE_APP_EVENT_TYPE_STOP:
        ESP_UI_LOGD("Stop app(%d)", id);
        app = manager->getRunningAppById(id);
        ESP_UI_CHECK_NULL_EXIT(app, "Invalid app");
        ESP_UI_CHECK_FALSE_EXIT(manager->processAppClose(app), "Close app failed");
        break;
    default:
        break;
    }
}

void ESP_UI_CoreManager::onNavigationEventCallback(lv_event_t *event)
{
    void *param = nullptr;
    ESP_UI_CoreManager *manager = nullptr;
    ESP_UI_CoreNavigateType_t navigation_type = ESP_UI_CORE_NAVIGATE_TYPE_MAX;

    ESP_UI_LOGD("Navigatiton bar event callback");
    ESP_UI_CHECK_NULL_EXIT(event, "Invalid event object");

    manager = (ESP_UI_CoreManager *)lv_event_get_user_data(event);
    ESP_UI_CHECK_NULL_EXIT(manager, "Invalid manager");

    param = lv_event_get_param(event);
    memcpy(&navigation_type, &param, sizeof(ESP_UI_CoreNavigateType_t));
    ESP_UI_CHECK_FALSE_EXIT(navigation_type < ESP_UI_CORE_NAVIGATE_TYPE_MAX, "Invalid navigate type");

    ESP_UI_CHECK_FALSE_EXIT(manager->processNavigationEvent(navigation_type), "Process navigation bar event failed");
}
