/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <tuple>
#include <map>
#include <unordered_map>
#include "lvgl/esp_brookesia_lv_helper.hpp"
#include "esp_brookesia_base_app.hpp"
#include "esp_brookesia_base_display.hpp"

namespace esp_brookesia::systems::base {

class Context;

class Manager {
public:
    friend class Context;

    enum class NavigateType {
        BACK,
        HOME,
        RECENTS_SCREEN,
        MAX,
    };

    struct Data {
        struct {
            int max_running_num;
        } app;
        struct {
            uint8_t enable_app_save_snapshot: 1;
        } flags;
    };

    using RegistryAppInfo = std::tuple<std::string, std::shared_ptr<App>>;

    Manager(Context &core, const Data &data);
    ~Manager();

    int installApp(App &app);
    int installApp(App *app);
    int uninstallApp(App &app);
    int uninstallApp(App *app);
    bool uninstallApp(int id);

    bool initAppFromRegistry(std::vector<RegistryAppInfo> &app_infos);
    bool installAppFromRegistry(std::vector<RegistryAppInfo> &app_infos, std::vector<std::string> *ordered_app_names = nullptr);

    bool checkAppID_Valid(int id)
    {
        return (id >= App::APP_ID_MIN) && (getInstalledApp(id) != nullptr);
    }
    int getAppFreeId(void)
    {
        return _app_free_id++;
    }
    uint8_t getRunningAppCount(void)
    {
        return _id_running_app_map.size();
    }
    int getRunningAppIndexByApp(App *app);
    int getRunningAppIndexById(int id);
    App *getInstalledApp(int id);
    App *getRunningAppByIdenx(uint8_t index);
    App *getRunningAppById(int id);
    App *getActiveApp(void)
    {
        return _active_app;
    }
    const lv_draw_buf_t *getAppSnapshot(int id);

protected:
    virtual bool processAppRunExtra(App *app)
    {
        return true;
    }
    virtual bool processAppResumeExtra(App *app)
    {
        return true;
    }
    virtual bool processAppPauseExtra(App *app)
    {
        return true;
    }
    virtual bool processAppCloseExtra(App *app)
    {
        return true;
    }
    virtual bool processNavigationEvent(NavigateType type)
    {
        return true;
    }

    bool processAppRun(App *app);
    bool processAppResume(App *app);
    bool processAppPause(App *app);
    bool processAppClose(App *app);
    bool saveAppSnapshot(App *app);
    bool releaseAppSnapshot(App *app);
    void resetActiveApp(void);

    Context &_system_context;
    const Data &_core_data;

private:
    bool begin(void);
    bool del(void);
    bool startApp(int id);

    static void onAppEventCallback(lv_event_t *event);
    static void onNavigationEventCallback(lv_event_t *event);

    uint32_t _app_free_id{App::APP_ID_MIN};
    App *_active_app{nullptr};
    std::unordered_map <int, App *> _id_installed_app_map;
    std::unordered_map <int, App *> _id_running_app_map;
    std::unordered_map <int, lv_draw_buf_t *> _id_app_snapshot_map;
    // Navigation
    NavigateType _navigate_type{NavigateType::MAX};
};

} // namespace esp_brookesia::systems::base

/**
 * Backward compatibility
 */
using ESP_Brookesia_CoreNavigateType_t [[deprecated("Use `esp_brookesia::systems::base::Manager::NavigateType` instead")]] =
    esp_brookesia::systems::base::Manager::NavigateType;
#define ESP_BROOKESIA_CORE_NAVIGATE_TYPE_BACK           ESP_Brookesia_CoreNavigateType_t::BACK
#define ESP_BROOKESIA_CORE_NAVIGATE_TYPE_HOME           ESP_Brookesia_CoreNavigateType_t::HOME
#define ESP_BROOKESIA_CORE_NAVIGATE_TYPE_RECENTS_SCREEN ESP_Brookesia_CoreNavigateType_t::RECENTS_SCREEN
#define ESP_BROOKESIA_CORE_NAVIGATE_TYPE_MAX            ESP_Brookesia_CoreNavigateType_t::MAX
using ESP_Brookesia_CoreManagerData_t [[deprecated("Use `esp_brookesia::systems::base::Manager::Data` instead")]] =
    esp_brookesia::systems::base::Manager::Data;
using ESP_Brookesia_CoreManager [[deprecated("Use `esp_brookesia::systems::base::Manager` instead")]] =
    esp_brookesia::systems::base::Manager;
