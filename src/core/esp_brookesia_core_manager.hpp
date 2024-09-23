/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <map>
#include <unordered_map>
#include "esp_brookesia_core_app.hpp"
#include "esp_brookesia_core_home.hpp"
#include "esp_brookesia_core_type.h"

class ESP_Brookesia_Core;

class ESP_Brookesia_CoreManager {
public:
    friend class ESP_Brookesia_Core;

    ESP_Brookesia_CoreManager(ESP_Brookesia_Core &core, const ESP_Brookesia_CoreManagerData_t &data);
    ~ESP_Brookesia_CoreManager();

    int installApp(ESP_Brookesia_CoreApp &app);
    int installApp(ESP_Brookesia_CoreApp *app);
    int uninstallApp(ESP_Brookesia_CoreApp &app);
    int uninstallApp(ESP_Brookesia_CoreApp *app);
    bool uninstallApp(int id);

    // *INDENT-OFF*
    int getAppFreeId(void) const             { return _app_free_id++; }
    uint8_t getRunningAppCount(void) const   { return _id_running_app_map.size(); }
    int getRunningAppIndexByApp(ESP_Brookesia_CoreApp *app);
    int getRunningAppIndexById(int id);
    ESP_Brookesia_CoreApp *getInstalledApp(int id);
    ESP_Brookesia_CoreApp *getRunningAppByIdenx(uint8_t index);
    ESP_Brookesia_CoreApp *getRunningAppById(int id);
    ESP_Brookesia_CoreApp *getActiveApp(void) const { return _active_app; }
    const lv_img_dsc_t *getAppSnapshot(int id);
    // *INDENT-OFF*

protected:
    virtual bool processAppRunExtra(ESP_Brookesia_CoreApp *app)    { return true; }
    virtual bool processAppResumeExtra(ESP_Brookesia_CoreApp *app) { return true; }
    virtual bool processAppPauseExtra(ESP_Brookesia_CoreApp *app)  { return true; }
    virtual bool processAppCloseExtra(ESP_Brookesia_CoreApp *app)  { return true; }
    virtual bool processNavigationEvent(ESP_Brookesia_CoreNavigateType_t type) { return true; };

    bool processAppRun(ESP_Brookesia_CoreApp *app);
    bool processAppResume(ESP_Brookesia_CoreApp *app);
    bool processAppPause(ESP_Brookesia_CoreApp *app);
    bool processAppClose(ESP_Brookesia_CoreApp *app);
    bool saveAppSnapshot(ESP_Brookesia_CoreApp *app);
    bool releaseAppSnapshot(ESP_Brookesia_CoreApp *app);
    void resetActiveApp(void);

    ESP_Brookesia_Core &_core;
    const ESP_Brookesia_CoreManagerData_t &_core_data;

private:
    bool beginCore(void);
    bool delCore(void);
    bool startApp(int id);

    static void onAppEventCallback(lv_event_t *event);
    static void onNavigationEventCallback(lv_event_t *event);

    typedef struct {
        uint8_t *image_buffer;
        lv_img_dsc_t image_resource;
    } ESP_Brookesia_AppSnapshot_t;

    // App
    mutable uint32_t _app_free_id;
    ESP_Brookesia_CoreApp *_active_app;
    std::unordered_map <int, ESP_Brookesia_CoreApp *> _id_installed_app_map;
    std::unordered_map <int, ESP_Brookesia_CoreApp *> _id_running_app_map;
    std::unordered_map <int, std::shared_ptr<ESP_Brookesia_AppSnapshot_t>> _id_app_snapshot_map;
    // Navigation
    ESP_Brookesia_CoreNavigateType_t _navigate_type;
};
