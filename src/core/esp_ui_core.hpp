/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "lvgl.h"
#include "esp_ui_core_utils.h"
#include "esp_ui_core_home.hpp"
#include "esp_ui_core_manager.hpp"

// *INDENT-OFF*
class ESP_UI_Core {
public:
    friend class ESP_UI_CoreManager;

    ESP_UI_Core(const ESP_UI_CoreData_t &data, ESP_UI_CoreHome &home, ESP_UI_CoreManager &manager, lv_disp_t *display);
    ~ESP_UI_Core();

    /* Core */
    bool checkCoreInitialized(void) const               { return (_event_obj.get() != nullptr); }
    const ESP_UI_CoreData_t &getCoreData(void) const    { return _core_data; }
    ESP_UI_CoreHome &getCoreHome(void) const            { return _core_home; }
    ESP_UI_CoreManager &getCoreManager(void) const      { return _core_manager; }

    /* Device */
    bool setTouchDevice(lv_indev_t *touch) const;
    lv_disp_t *getDisplayDevice(void) const { return _display; }
    lv_indev_t *getTouchDevice(void) const  { return _touch; }

    /* Event */
    lv_obj_t *getEventObject(void) const    { return _event_obj.get(); }
    lv_event_code_t getFreeEventCode(void)  { return (lv_event_code_t)++_free_event_code; }
    // Data Update
    bool registerDateUpdateEventCallback(lv_event_cb_t callback, void *user_data) const;
    bool unregisterDateUpdateEventCallback(lv_event_cb_t callback, void *user_data) const;
    bool sendDataUpdateEvent(void *param = nullptr) const;
    lv_event_code_t getDataUpdateEventCode(void) const  { return _data_update_event_code; }
    // Navigate
    bool registerNavigateEventCallback(lv_event_cb_t callback, void *user_data) const;
    bool unregisterNavigateEventCallback(lv_event_cb_t callback, void *user_data) const;
    bool sendNavigateEvent(ESP_UI_CoreNavigateType_t type) const;
    lv_event_code_t getNavigateEventCode(void) const    { return _navigate_event_code; }
    // App
    bool registerAppEventCallback(lv_event_cb_t callback, void *user_data) const;
    bool unregisterAppEventCallback(lv_event_cb_t callback, void *user_data) const;
    bool sendAppEvent(const ESP_UI_CoreAppEventData_t *data) const;
    lv_event_code_t getAppEventCode(void) const         { return _app_event_code; }

protected:
    bool beginCore(void);
    bool delCore(void);
    bool calibrateCoreData(ESP_UI_CoreData_t &data);

    // Core
    const ESP_UI_CoreData_t &_core_data;
    ESP_UI_CoreHome         &_core_home;
    ESP_UI_CoreManager      &_core_manager;
    // Device
    lv_disp_t          *_display;
    mutable lv_indev_t *_touch;

private:
    static void onCoreDataUpdateEventCallback(lv_event_t *event);
    static void onCoreNavigateEventCallback(lv_event_t *event);

    // Event
    uint32_t _free_event_code;
    ESP_UI_LvObj_t _event_obj;
    lv_event_code_t _data_update_event_code;
    lv_event_code_t _navigate_event_code;
    lv_event_code_t _app_event_code;
};
// *INDENT-OFF*
