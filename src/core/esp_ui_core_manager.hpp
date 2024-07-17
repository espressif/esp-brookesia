/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <map>
#include <unordered_map>
#include "esp_ui_core_app.hpp"
#include "esp_ui_core_home.hpp"
#include "esp_ui_core_type.h"

class ESP_UI_Core;

class ESP_UI_CoreManager {
public:
    friend class ESP_UI_Core;

    ESP_UI_CoreManager(ESP_UI_Core &core, const ESP_UI_CoreManagerData_t &data);
    ~ESP_UI_CoreManager();

    int installApp(ESP_UI_CoreApp &app);
    int installApp(ESP_UI_CoreApp *app);
    int uninstallApp(ESP_UI_CoreApp &app);
    int uninstallApp(ESP_UI_CoreApp *app);
    bool uninstallApp(int id);

    // *INDENT-OFF*
    int getAppFreeId(void) const             { return _app_free_id++; }
    uint8_t getRunningAppCount(void) const   { return _id_running_app_map.size(); }
    int getRunningAppIndexByApp(ESP_UI_CoreApp *app);
    int getRunningAppIndexById(int id);
    ESP_UI_CoreApp *getInstalledApp(int id);
    ESP_UI_CoreApp *getRunningAppByIdenx(uint8_t index);
    ESP_UI_CoreApp *getRunningAppById(int id);
    ESP_UI_CoreApp *getActiveApp(void) const { return _active_app; }
    const lv_img_dsc_t *getAppSnapshot(int id);
    // *INDENT-OFF*

protected:
    virtual bool processAppRunExtra(ESP_UI_CoreApp *app)    { return true; }
    virtual bool processAppResumeExtra(ESP_UI_CoreApp *app) { return true; }
    virtual bool processAppPauseExtra(ESP_UI_CoreApp *app)  { return true; }
    virtual bool processAppCloseExtra(ESP_UI_CoreApp *app)  { return true; }
    virtual bool processNavigationEvent(ESP_UI_CoreNavigateType_t type) { return true; };

    bool processAppRun(ESP_UI_CoreApp *app);
    bool processAppResume(ESP_UI_CoreApp *app);
    bool processAppPause(ESP_UI_CoreApp *app);
    bool processAppClose(ESP_UI_CoreApp *app);
    bool saveAppSnapshot(ESP_UI_CoreApp *app);
    bool releaseAppSnapshot(ESP_UI_CoreApp *app);
    void resetActiveApp(void);

    ESP_UI_Core &_core;
    const ESP_UI_CoreManagerData_t &_core_data;

private:
    bool beginCore(void);
    bool delCore(void);
    bool startApp(int id);

    static void onAppEventCallback(lv_event_t *event);
    static void onNavigationEventCallback(lv_event_t *event);

    typedef struct {
        uint8_t *image_buffer;
        lv_img_dsc_t image_resource;
    } ESP_UI_AppSnapshot_t;

    // App
    mutable uint32_t _app_free_id;
    ESP_UI_CoreApp *_active_app;
    std::unordered_map <int, ESP_UI_CoreApp *> _id_installed_app_map;
    std::unordered_map <int, ESP_UI_CoreApp *> _id_running_app_map;
    std::unordered_map <int, std::shared_ptr<ESP_UI_AppSnapshot_t>> _id_app_snapshot_map;
    // Navigation
    ESP_UI_CoreNavigateType_t _navigate_type;
};
