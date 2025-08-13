/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <memory>
#include "style/esp_brookesia_gui_style.hpp"
#include "esp_brookesia_base_display.hpp"
#include "esp_brookesia_base_manager.hpp"
#include "esp_brookesia_base_event.hpp"

namespace esp_brookesia::systems::base {

class Context {
public:
    struct Data {
        const char *name;
        gui::StyleSize screen_size;
        Display::Data display;
        Manager::Data manager;
    };

    enum class AppEventType : uint8_t {
        START,
        STOP,
        OPERATION,
        MAX,
    };

    struct AppEventData {
        int id;
        AppEventType type;
        void *data;
    };

    Context(const Context &) = delete;
    Context(Context &&) = delete;
    Context &operator=(const Context &) = delete;
    Context &operator=(Context &&) = delete;

    Context(const Data &data, Display &display, Manager &manager, lv_display_t *device);
    ~Context();

    /* Context */
    bool checkCoreInitialized(void) const
    {
        return (_event_obj.get() != nullptr);
    }
    const Data &getData(void) const
    {
        return _data;
    }
    Display &getDisplay(void)
    {
        return _display;
    }
    Manager &getManager(void)
    {
        return _manager;
    }
    Event &getEvent(void)
    {
        return _event;
    }
    bool getDisplaySize(gui::StyleSize &size);

    /* Device */
    bool setTouchDevice(lv_indev_t *touch);
    lv_display_t *getDisplayDevice(void)
    {
        return _display_device;
    }
    lv_indev_t *getTouchDevice(void)
    {
        return _touch_device;
    }

    /* Event */
    lv_obj_t *getEventObject(void) const
    {
        return _event_obj.get();
    }
    lv_event_code_t getFreeEventCode(void)
    {
        return (lv_event_code_t)++_free_event_code;
    }
    // Data Update
    bool registerDateUpdateEventCallback(lv_event_cb_t callback, void *user_data);
    bool unregisterDateUpdateEventCallback(lv_event_cb_t callback, void *user_data);
    bool sendDataUpdateEvent(void *param = nullptr);
    lv_event_code_t getDataUpdateEventCode(void) const
    {
        return _data_update_event_code;
    }
    // Navigate
    bool registerNavigateEventCallback(lv_event_cb_t callback, void *user_data);
    bool unregisterNavigateEventCallback(lv_event_cb_t callback, void *user_data);
    bool sendNavigateEvent(Manager::NavigateType type);
    lv_event_code_t getNavigateEventCode(void) const
    {
        return _navigate_event_code;
    }
    // App
    bool registerAppEventCallback(lv_event_cb_t callback, void *user_data);
    bool unregisterAppEventCallback(lv_event_cb_t callback, void *user_data);
    bool sendAppEvent(const AppEventData *data);
    lv_event_code_t getAppEventCode(void) const
    {
        return _app_event_code;
    }
    bool checkAppID_Valid(int id)
    {
        return _manager.checkAppID_Valid(id);
    }

    /* LVGL */
    bool lockLv(int timeout = -1);
    bool unlockLv();

    /* App */
    bool initAppFromRegistry(std::vector<Manager::RegistryAppInfo> &app_infos)
    {
        return _manager.initAppFromRegistry(app_infos);
    }
    bool installAppFromRegistry(
        std::vector<Manager::RegistryAppInfo> &app_infos,
        std::vector<std::string> *ordered_app_names = nullptr
    )
    {
        return _manager.installAppFromRegistry(app_infos, ordered_app_names);
    }

    [[deprecated("Use `getData()` instead")]]
    const Data &getCoreData(void) const
    {
        return getData();
    }
    [[deprecated("Use `getDisplay()` instead")]]
    Display &getCoreDisplay(void)
    {
        return getDisplay();
    }
    [[deprecated("Use `getManager()` instead")]]
    Manager &getCoreManager(void)
    {
        return getManager();
    }
    [[deprecated("Use `getEvent()` instead")]]
    Event *getCoreEvent(void)
    {
        return &getEvent();
    }

protected:
    bool begin(void);
    bool del(void);
    bool calibrateCoreData(Data &data);

    // Context
    const Data &_data;
    Display &_display;
    Manager &_manager;
    Event   _event;
    // Device
    lv_display_t *_display_device = nullptr;
    lv_indev_t   *_touch_device = nullptr;

private:
    static void onCoreDataUpdateEventCallback(lv_event_t *event);
    static void onCoreNavigateEventCallback(lv_event_t *event);

    // Event
    uint32_t _free_event_code;
    esp_brookesia::gui::LvObjSharedPtr _event_obj;
    lv_event_code_t _data_update_event_code;
    lv_event_code_t _navigate_event_code;
    lv_event_code_t _app_event_code;
};

} // namespace esp_brookesia::systems::base

using ESP_Brookesia_CoreData_t [[deprecated("Use `esp_brookesia::systems::base::Context::Data` instead")]] =
    esp_brookesia::systems::base::Context::Data;
using ESP_Brookesia_CoreAppEventType_t [[deprecated("Use `esp_brookesia::systems::base::AppEventType` instead")]] =
    esp_brookesia::systems::base::Context::AppEventType;
#define ESP_BROOKESIA_CORE_APP_EVENT_TYPE_START     ESP_Brookesia_CoreAppEventType_t::START
#define ESP_BROOKESIA_CORE_APP_EVENT_TYPE_STOP      ESP_Brookesia_CoreAppEventType_t::STOP
#define ESP_BROOKESIA_CORE_APP_EVENT_TYPE_OPERATION ESP_Brookesia_CoreAppEventType_t::OPERATION
#define ESP_BROOKESIA_CORE_APP_EVENT_TYPE_MAX       ESP_Brookesia_CoreAppEventType_t::MAX
using ESP_Brookesia_CoreAppEventData_t [[deprecated("Use `esp_brookesia::systems::base::Context::AppEventData` instead")]] =
    esp_brookesia::systems::base::Context::AppEventData;
using ESP_Brookesia_Core [[deprecated("Use `esp_brookesia::systems::base::Context` instead")]] =
    esp_brookesia::systems::base::Context;
