/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "lvgl.h"
#include "gui/esp_brookesia_gui_type.hpp"
#include "esp_brookesia_core_display.hpp"
#include "esp_brookesia_core_manager.hpp"
#include "esp_brookesia_core_event.hpp"

// *INDENT-OFF*

typedef struct {
    const char *name;
    ESP_Brookesia_StyleSize_t screen_size;
    union {
        ESP_Brookesia_CoreHomeData_t home;
        ESP_Brookesia_CoreDisplayData display;
    };
    ESP_Brookesia_CoreManagerData_t manager;
} ESP_Brookesia_CoreData_t;

typedef enum {
    ESP_BROOKESIA_CORE_APP_EVENT_TYPE_START = 0,
    ESP_BROOKESIA_CORE_APP_EVENT_TYPE_STOP,
    ESP_BROOKESIA_CORE_APP_EVENT_TYPE_OPERATION,
    ESP_BROOKESIA_CORE_APP_EVENT_TYPE_MAX,
} ESP_Brookesia_CoreAppEventType_t;

typedef struct {
    int id;
    ESP_Brookesia_CoreAppEventType_t type;
    void *data;
} ESP_Brookesia_CoreAppEventData_t;

class ESP_Brookesia_Core {
public:
    friend class ESP_Brookesia_CoreManager;

    ESP_Brookesia_Core(
        const ESP_Brookesia_CoreData_t &data, ESP_Brookesia_CoreDisplay &home, ESP_Brookesia_CoreManager &manager,
        lv_display_t *device
    );
    ~ESP_Brookesia_Core();

    /* Core */
    bool checkCoreInitialized(void) const               { return (_event_obj.get() != nullptr); }
    const ESP_Brookesia_CoreData_t &getCoreData(void) const    { return _core_data; }
    ESP_Brookesia_CoreDisplay &getCoreDisplay(void) const      { return _core_display; }
    ESP_Brookesia_CoreManager &getCoreManager(void) const      { return _core_manager; }
    ESP_Brookesia_CoreEvent *getCoreEvent(void)                { return &_core_event; }
    bool getDisplaySize(ESP_Brookesia_StyleSize_t &size);

    /* Device */
    bool setTouchDevice(lv_indev_t *touch) const;
    lv_display_t *getDisplayDevice(void) const { return _display_device; }
    lv_indev_t *getTouchDevice(void) const  { return _touch_device; }

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
    bool sendNavigateEvent(ESP_Brookesia_CoreNavigateType_t type) const;
    lv_event_code_t getNavigateEventCode(void) const    { return _navigate_event_code; }
    // App
    bool registerAppEventCallback(lv_event_cb_t callback, void *user_data) const;
    bool unregisterAppEventCallback(lv_event_cb_t callback, void *user_data) const;
    bool sendAppEvent(const ESP_Brookesia_CoreAppEventData_t *data) const;
    lv_event_code_t getAppEventCode(void) const         { return _app_event_code; }
    bool checkAppID_Valid(int id) const;

    /* LVGL */
    void registerLvLockCallback(ESP_Brookesia_GUI_LockCallback_t callback, int timeout);
    void registerLvUnlockCallback(ESP_Brookesia_GUI_UnlockCallback_t callback);
    bool lockLv(int timeout) const;
    bool lockLv(void) const;
    void unlockLv(void) const;


    /**
     * @brief Back compatibility
     */
    ESP_Brookesia_CoreHome &getCoreHome(void) const            { return _core_display; }

protected:
    bool beginCore(void);
    bool delCore(void);
    bool calibrateCoreData(ESP_Brookesia_CoreData_t &data);

    // Core
    const ESP_Brookesia_CoreData_t &_core_data;
    ESP_Brookesia_CoreDisplay      &_core_display;
    ESP_Brookesia_CoreManager      &_core_manager;
    ESP_Brookesia_CoreEvent        _core_event;
    // Device
    lv_display_t       *_display_device;
    mutable lv_indev_t *_touch_device;

private:
    static void onCoreDataUpdateEventCallback(lv_event_t *event);
    static void onCoreNavigateEventCallback(lv_event_t *event);

    // Event
    uint32_t _free_event_code;
    ESP_Brookesia_LvObj_t _event_obj;
    lv_event_code_t _data_update_event_code;
    lv_event_code_t _navigate_event_code;
    lv_event_code_t _app_event_code;

    // LVGL
    int _lv_lock_timeout;
    ESP_Brookesia_GUI_LockCallback_t _lv_lock_callback;
    ESP_Brookesia_GUI_UnlockCallback_t _lv_unlock_callback;
};

// *INDENT-ON*
