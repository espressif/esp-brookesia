/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <cstring>
#include <cmath>
#include "esp_brookesia_systems_internal.h"
#if !ESP_BROOKESIA_BASE_MANAGER_ENABLE_DEBUG_LOG
#   define ESP_BROOKESIA_UTILS_DISABLE_DEBUG_LOG
#endif
#include "private/esp_brookesia_base_utils.hpp"
#include "lvgl/esp_brookesia_lv.hpp"
#include "esp_brookesia_base_manager.hpp"
#include "esp_brookesia_base_context.hpp"

using namespace std;
using namespace esp_brookesia::gui;

namespace esp_brookesia::systems::base {

Manager::Manager(Context &core, const Data &data):
    _system_context(core),
    _core_data(data)
{
}

Manager::~Manager()
{
    ESP_UTILS_LOGD("Destroy(@0x%p)", this);
    if (!del()) {
        ESP_UTILS_LOGE("Delete failed");
    }
}

int Manager::installApp(App *app)
{
    bool app_installed = false;
    bool display_process_app_installed = false;
    lv_area_t app_visual_area = {};
    Display &display = _system_context.getDisplay();

    ESP_UTILS_CHECK_NULL_RETURN(app, -1, "Invalid app");

    ESP_UTILS_LOGD("Install App(%p)", app);

    // Check if the app is already installed
    for (auto it = _id_installed_app_map.begin(); it != _id_installed_app_map.end(); it++) {
        ESP_UTILS_CHECK_FALSE_RETURN(it->second != app, -1, "Already installed");
    }

    // Initialize app
    ESP_UTILS_CHECK_FALSE_GOTO(app_installed = app->processInstall(&_system_context, _app_free_id), err, "App install failed");
    // Insert app to installed_app_map
    ESP_UTILS_CHECK_FALSE_GOTO(_id_installed_app_map.insert(pair <int, App *>(app->_id, app)).second, err,
                               "Insert app failed");

    ESP_UTILS_CHECK_FALSE_GOTO(display.getAppVisualArea(app, app_visual_area), err, "Display get app visual area failed");
    ESP_UTILS_CHECK_FALSE_GOTO(app->setVisualArea(app_visual_area), err, "App set visual area failed");
    ESP_UTILS_CHECK_FALSE_GOTO(app->calibrateVisualArea(), err, "App calibrate visual area failed");

    // Process display
    ESP_UTILS_CHECK_FALSE_GOTO(display_process_app_installed = display.processAppInstall(app), err,
                               "Display process app install failed");

    // Update free app id
    _app_free_id++;

    return app->getId();

err:
    if (display_process_app_installed && !display.processAppUninstall(app)) {
        ESP_UTILS_LOGE("Display process app uninstall failed");
    }
    if (app_installed && !app->processUninstall()) {
        ESP_UTILS_LOGE("App uninstall failed");
    }
    _id_installed_app_map.erase(app->_id);

    return -1;
}

int Manager::installApp(App &app)
{
    return installApp(&app);
}

int Manager::uninstallApp(App *app)
{
    bool ret = true;
    int app_id = -1;
    Display &display = _system_context.getDisplay();

    ESP_UTILS_CHECK_NULL_RETURN(app, false, "Invalid app");
    app_id = app->_id;

    ESP_UTILS_LOGD("Uninstall App(%d)", app_id);

    // Check if the app is already installed
    auto it = _id_installed_app_map.begin();
    for (; it != _id_installed_app_map.end(); it++) {
        if (it->second == app) {
            break;
        }
    }
    ESP_UTILS_CHECK_FALSE_RETURN(it->second == app, false, "App(%d) is not installed", app_id);

    // Process display
    ESP_UTILS_CHECK_FALSE_RETURN(display.processAppUninstall(app), false, "Display process app uninstall failed");

    // Deinit app
    ret = app->processUninstall();
    if (!ret) {
        ESP_UTILS_LOGE("App uninstall failed");
    }

    // Remove app from installed_app_map
    ESP_UTILS_CHECK_FALSE_RETURN(_id_installed_app_map.erase(app_id) > 0, false, "Remove app failed");

    return ret;
}

int Manager::uninstallApp(App &app)
{
    return uninstallApp(&app);
}

bool Manager::uninstallApp(int id)
{
    App *app = nullptr;

    ESP_UTILS_LOGD("Uninstall App(%d)", id);

    app = getInstalledApp(id);
    ESP_UTILS_CHECK_NULL_RETURN(app, false, "Get installed app failed");

    ESP_UTILS_CHECK_FALSE_RETURN(uninstallApp(app), false, "Uninstall app failed");

    return true;
}

bool Manager::initAppFromRegistry(std::vector<RegistryAppInfo> &app_infos)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    app_infos.clear();

    App::Registry::forEach([&](const auto & plugin) {
        ESP_UTILS_LOGI("Found app: %s", plugin.name.c_str());

        auto app = App::Registry::get(plugin.name);
        if (app == nullptr) {
            ESP_UTILS_LOGE("\t - Get instance failed");
            return;
        }
        ESP_UTILS_LOGI("\t - Get instance(%p) success", app.get());

        app_infos.emplace_back(plugin.name, app);
    });

    return true;
}

bool Manager::installAppFromRegistry(std::vector<RegistryAppInfo> &app_infos, std::vector<std::string> *ordered_app_names)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    // Reorder app_infos according to the order in ordered_app_names
    if (ordered_app_names != nullptr && !ordered_app_names->empty()) {
        std::vector<RegistryAppInfo> reordered_app_infos;

        // First add the apps in the order specified in ordered_app_names
        for (const std::string &ordered_name : *ordered_app_names) {
            auto it = std::find_if(app_infos.begin(), app_infos.end(), [&ordered_name](const RegistryAppInfo & info) {
                return std::get<0>(info) == ordered_name;
            });

            if (it != app_infos.end()) {
                reordered_app_infos.push_back(*it);
                app_infos.erase(it);
            }
        }

        // Then add the remaining apps
        for (const auto &info : app_infos) {
            reordered_app_infos.push_back(info);
        }

        // Replace the original app_infos
        app_infos = std::move(reordered_app_infos);
    }

    // Install apps
    for (auto &[name, app] : app_infos) {
        ESP_UTILS_LOGI("Install app: %s", name.c_str());

        auto app_id = installApp(app.get());
        if (!checkAppID_Valid(app_id)) {
            ESP_UTILS_LOGE("\t - Install failed");
        }
        ESP_UTILS_LOGI("\t - Install success (id: %d)", app_id);

        if (ordered_app_names != nullptr) {
            ordered_app_names->emplace_back(name);
        }
    }

    return true;
}

bool Manager::startApp(int id)
{
    App *app = NULL;
    App *app_old = NULL;

    // Check if the app is already running
    auto find_ret = _id_running_app_map.find(id);
    if (find_ret != _id_running_app_map.end()) {
        app = find_ret->second;
        ESP_UTILS_LOGD("App(%d) is already running, just resume it", app->_id);
        // If so, resume app
        ESP_UTILS_CHECK_FALSE_RETURN(processAppResume(app), false, "Resume app failed");

        return true;
    }

    // If not, then find the target app from installed app map
    find_ret = _id_installed_app_map.find(id);
    ESP_UTILS_CHECK_FALSE_RETURN(find_ret != _id_installed_app_map.end(), false, "Can't find app in installed app map");
    app = find_ret->second;

    // Check if the running app num is at the limit
    if ((_core_data.app.max_running_num != 0) && (int)_id_running_app_map.size() >= _core_data.app.max_running_num) {
        for (auto it = _id_running_app_map.begin(); it != _id_running_app_map.end(); it++) {
            app_old = it->second;
        }
        ESP_UTILS_CHECK_NULL_RETURN(app_old, false, "Get old app failed");

        ESP_UTILS_LOGW("Running app num(%d) is already at the limit, will close the oldest app(%d)",
                       (int)_id_running_app_map.size(), app_old->_id);

        ESP_UTILS_CHECK_FALSE_RETURN(processAppClose(app_old), false, "Close app failed");
    }

    // Start app
    ESP_UTILS_CHECK_FALSE_RETURN(processAppRun(app), false, "Start app failed");

    // Add app to running_app_map
    ESP_UTILS_CHECK_FALSE_GOTO(_id_running_app_map.insert(pair <int, App *>(id, app)).second, err,
                               "Insert app to running map failed");

    return true;

err:
    ESP_UTILS_CHECK_FALSE_RETURN(processAppClose(app), false, "Close app failed");

    return false;
}

bool Manager::processAppRun(App *app)
{
    bool is_display_run = false;
    bool is_app_run = false;
    Display &display = _system_context.getDisplay();

    ESP_UTILS_CHECK_NULL_RETURN(app, false, "Invalid app");
    ESP_UTILS_LOGD("Process app(%d) run", app->_id);

    // Process display, and get the visual area of the app
    ESP_UTILS_CHECK_FALSE_RETURN(is_display_run = display.processAppRun(app), false, "Process display before app run failed");

    // Process app
    ESP_UTILS_CHECK_FALSE_GOTO(is_app_run = app->processRun(), err, "Process app run failed");

    // Process extra
    ESP_UTILS_CHECK_FALSE_GOTO(processAppRunExtra(app), err, "Process app run extra failed");

    // Update active app
    _active_app = app;

    return true;

err:
    if (is_display_run && !display.processAppClose(app)) {
        ESP_UTILS_LOGE("Display process close failed");
    }
    if (is_app_run && !app->processClose(true)) {
        ESP_UTILS_LOGE("App process close failed");
    }
    ESP_UTILS_CHECK_FALSE_RETURN(display.processMainScreenLoad(), false, "Display load main screen failed");

    return false;
}

bool Manager::processAppResume(App *app)
{
    Display &display = _system_context.getDisplay();

    ESP_UTILS_CHECK_NULL_RETURN(app, false, "Invalid app");
    ESP_UTILS_LOGD("Process app(%d) resume", app->_id);

    // Check if the screen is showing app and the app is not the active one
    if ((_active_app != nullptr) && (_active_app != app)) {
        // if so, pause the active app
        ESP_UTILS_CHECK_FALSE_RETURN(processAppPause(_active_app), false, "App process pause failed");
    }

    // Process display
    ESP_UTILS_CHECK_FALSE_RETURN(display.processAppResume(app), false, "Display process resume failed");

    // Process app, only load active screen if the app is not shown
    ESP_UTILS_CHECK_FALSE_RETURN(app->processResume(), false, "App process resume failed");

    // Process extra
    ESP_UTILS_CHECK_FALSE_RETURN(processAppResumeExtra(app), false, "Process app resume extra failed");

    // Update active app
    _active_app = app;

    return true;
}

bool Manager::processAppPause(App *app)
{
    Display &display = _system_context.getDisplay();

    ESP_UTILS_CHECK_NULL_RETURN(app, false, "Invalid app");
    ESP_UTILS_LOGD("Process app(%d) pause", app->_id);

    // Process app
    ESP_UTILS_CHECK_FALSE_RETURN(app->processPause(), false, "App process pause failed");
    if (_core_data.flags.enable_app_save_snapshot) {
        if (!saveAppSnapshot(app)) {
            ESP_UTILS_LOGE("Save app snapshot failed");
        }
    }

    // Process display,
    ESP_UTILS_CHECK_FALSE_GOTO(display.processAppPause(app), err, "Display process load failed");

    // Process extra
    ESP_UTILS_CHECK_FALSE_GOTO(processAppPauseExtra(app), err, "Process app pause extra failed");

    return true;

err:
    ESP_UTILS_CHECK_FALSE_RETURN(processAppClose(app), false, "Close app failed");

    return false;
}

bool Manager::processAppClose(App *app)
{
    Display &display = _system_context.getDisplay();

    ESP_UTILS_CHECK_NULL_RETURN(app, false, "Invalid app");
    ESP_UTILS_LOGD("Process app(%d) close", app->_id);

    // Process app, enable auto clean when the app is showing
    ESP_UTILS_CHECK_FALSE_RETURN(app->processClose(_active_app == app), false, "App process close failed");
    if (_core_data.flags.enable_app_save_snapshot) {
        if (!releaseAppSnapshot(app)) {
            ESP_UTILS_LOGE("Release app snapshot failed");
        }
    }

    // Process display, load main screen if the app is showing
    ESP_UTILS_CHECK_FALSE_RETURN(display.processAppClose(app), false, "Display process close failed");

    // Process extra
    ESP_UTILS_CHECK_FALSE_RETURN(processAppCloseExtra(app), false, "Process app pause extra failed");

    // Remove app from running map and update active app
    ESP_UTILS_CHECK_FALSE_RETURN(_id_running_app_map.erase(app->_id) > 0, false, "Remove app from running map failed");
    if (_active_app == app) {
        _active_app = nullptr;
    }

    return true;
}

bool Manager::saveAppSnapshot(App *app)
{
#if !LV_USE_SNAPSHOT
    ESP_UTILS_CHECK_FALSE_RETURN(false, false, "`LV_USE_SNAPSHOT` is not enabled");
#else
    bool resize_app_screen = false;
    lv_res_t ret = LV_RES_INV;
    lv_area_t app_screen_area = {};
    lv_draw_buf_t *snapshot_buffer = nullptr;

    ESP_UTILS_CHECK_NULL_RETURN(app, false, "Invalid app");
    ESP_UTILS_LOGD("Save app(%d) snapshot", app->_id);

    ESP_UTILS_CHECK_FALSE_RETURN(app->_active_screen != nullptr, false, "Invalid active screen");
    app_screen_area = app->_active_screen->coords;
    if ((lv_area_get_width(&app_screen_area) != _system_context.getData().screen_size.width) ||
            (lv_area_get_height(&app_screen_area) != _system_context.getData().screen_size.height)) {
        ESP_UTILS_LOGD("Active screen size is not match screen size, resize it");
        app->_active_screen->coords = (lv_area_t) {
            .x1 = 0,
            .y1 = 0,
            .x2 = (lv_coord_t)(_system_context.getData().screen_size.width - 1),
            .y2 = (lv_coord_t)(_system_context.getData().screen_size.height - 1),
        };
        resize_app_screen = true;
    }

    auto it = _id_app_snapshot_map.find(app->_id);
    auto color_format = _system_context.getDisplayDevice()->color_format;
    snapshot_buffer = (it != _id_app_snapshot_map.end()) ? it->second : nullptr;

    if ((snapshot_buffer == nullptr) || (snapshot_buffer->header.w != lv_area_get_width(&app_screen_area)) ||
            (snapshot_buffer->header.h != lv_area_get_height(&app_screen_area))) {
        if (snapshot_buffer != nullptr) {
            lv_draw_buf_destroy(snapshot_buffer);
        }
        snapshot_buffer = lv_snapshot_create_draw_buf(app->_active_screen, color_format);
        ESP_UTILS_CHECK_NULL_GOTO(snapshot_buffer, err, "Create snapshot buffer failed");
    }

    // And take snapshot for recent screen
    ret = lv_snapshot_take_to_draw_buf(app->_active_screen, color_format, snapshot_buffer);
    ESP_UTILS_CHECK_FALSE_GOTO(ret == LV_RESULT_OK, err, "Take snapshot fail");

    _id_app_snapshot_map[app->_id] = snapshot_buffer;
    if (resize_app_screen) {
        app->_active_screen->coords = app_screen_area;
    }

    return true;

err:
    if (snapshot_buffer != nullptr) {
        lv_draw_buf_destroy(snapshot_buffer);
    }
    if (resize_app_screen) {
        app->_active_screen->coords = app_screen_area;
    }

    return false;
#endif
}

bool Manager::releaseAppSnapshot(App *app)
{
    ESP_UTILS_CHECK_NULL_RETURN(app, false, "Invalid app");
    ESP_UTILS_LOGD("Release app(%d) snapshot", app->_id);

    auto it = _id_app_snapshot_map.find(app->_id);
    if (it == _id_app_snapshot_map.end()) {
        return true;
    }

    auto snapshot_buffer = it->second;
    if (snapshot_buffer != nullptr) {
        lv_draw_buf_destroy(snapshot_buffer);
    }
    ESP_UTILS_CHECK_FALSE_RETURN(_id_app_snapshot_map.erase(app->_id) > 0, false, "Free snapshot failed");

    return true;
}

void Manager::resetActiveApp(void)
{
    ESP_UTILS_LOGD("Reset active app");
    _active_app = nullptr;
}

int Manager::getRunningAppIndexByApp(App *app)
{
    ESP_UTILS_CHECK_NULL_RETURN(app, -1, "Invalid app");

    for (auto it = _id_running_app_map.begin(); it != _id_running_app_map.end(); ++it) {
        if (it->second == app) {
            return distance(it, _id_running_app_map.end()) - 1;
        }
    }

    return -1;
}

int Manager::getRunningAppIndexById(int id)
{
    auto it = _id_running_app_map.find(id);
    if (it != _id_running_app_map.end()) {
        return distance(it, _id_running_app_map.end()) - 1;
    }

    return -1;
}

App *Manager::getInstalledApp(int id)
{
    auto it = _id_installed_app_map.find(id);
    if (it != _id_installed_app_map.end()) {
        return it->second;
    }

    return nullptr;
}

App *Manager::getRunningAppByIdenx(uint8_t index)
{
    if (index >= _id_running_app_map.size()) {
        return nullptr;
    }

    auto it = _id_running_app_map.begin();
    advance(it, _id_running_app_map.size() - index - 1);

    return it->second;
}

App *Manager::getRunningAppById(int id)
{
    auto it = _id_running_app_map.find(id);
    if (it != _id_running_app_map.end()) {
        return it->second;
    }

    return nullptr;
}

const lv_draw_buf_t *Manager::getAppSnapshot(int id)
{
    auto it = _id_app_snapshot_map.find(id);
    ESP_UTILS_CHECK_FALSE_RETURN(it != _id_app_snapshot_map.end(), nullptr, "App snapshot not found");

    return it->second;
}

bool Manager::begin(void)
{
    ESP_UTILS_LOGD("Begin(@0x%p)", this);

    ESP_UTILS_CHECK_FALSE_RETURN(_system_context.registerAppEventCallback(onAppEventCallback, this), false,
                                 "Register app event failed");
    ESP_UTILS_CHECK_FALSE_GOTO(_system_context.registerNavigateEventCallback(onNavigationEventCallback, this), err,
                               "Register navigation event failed");

    return true;

err:
    ESP_UTILS_CHECK_FALSE_RETURN(del(), false, "Delete failed");

    return false;
}

bool Manager::del(void)
{
    bool ret = true;
    unordered_map <int, App *> id_installed_app_map = _id_installed_app_map;

    ESP_UTILS_LOGD("Delete(@0x%p)", this);

    if (_system_context.checkCoreInitialized()) {
        if (!_system_context.unregisterAppEventCallback(onAppEventCallback, this)) {
            ESP_UTILS_LOGE("Unregister app event failed");
            ret = false;
        }
    }

    _app_free_id = 0;
    _active_app = nullptr;
    for (auto app : id_installed_app_map) {
        if (!uninstallApp(app.second)) {
            ESP_UTILS_LOGE("Uninstall app(%d) failed", app.second->_id);
            ret = false;
        }
    }
    _id_installed_app_map.clear();
    _id_running_app_map.clear();
    _id_app_snapshot_map.clear();

    return ret;
}

void Manager::onAppEventCallback(lv_event_t *event)
{
    int id = -1;
    App *app = nullptr;
    Manager *manager = nullptr;
    Context::AppEventData *event_data = nullptr;

    ESP_UTILS_LOGD("App start event callback");
    ESP_UTILS_CHECK_NULL_EXIT(event, "Invalid event object");

    manager = (Manager *)lv_event_get_user_data(event);
    event_data = (Context::AppEventData *)lv_event_get_param(event);
    ESP_UTILS_CHECK_NULL_EXIT(manager, "Invalid manager object");
    ESP_UTILS_CHECK_FALSE_EXIT((event_data != nullptr) && (event_data->type < Context::AppEventType::MAX),
                               "Invalid event data");

    id = event_data->id;
    switch (event_data->type) {
    case Context::AppEventType::START:
        ESP_UTILS_LOGD("Start app(%d)", id);
        ESP_UTILS_CHECK_FALSE_EXIT(manager->startApp(id), "Run app failed");
        break;
    case Context::AppEventType::STOP:
        ESP_UTILS_LOGD("Stop app(%d)", id);
        app = manager->getRunningAppById(id);
        ESP_UTILS_CHECK_NULL_EXIT(app, "Invalid app");
        ESP_UTILS_CHECK_FALSE_EXIT(manager->processAppClose(app), "Close app failed");
        break;
    default:
        break;
    }
}

void Manager::onNavigationEventCallback(lv_event_t *event)
{
    void *param = nullptr;
    Manager *manager = nullptr;
    NavigateType navigation_type = NavigateType::MAX;

    ESP_UTILS_LOGD("Navigatiton bar event callback");
    ESP_UTILS_CHECK_NULL_EXIT(event, "Invalid event object");

    manager = (Manager *)lv_event_get_user_data(event);
    ESP_UTILS_CHECK_NULL_EXIT(manager, "Invalid manager");

    param = lv_event_get_param(event);
    memcpy(&navigation_type, &param, sizeof(NavigateType));

    ESP_UTILS_CHECK_FALSE_EXIT(manager->processNavigationEvent(navigation_type), "Process navigation bar event failed");
}

} // namespace esp_brookesia::systems::base
