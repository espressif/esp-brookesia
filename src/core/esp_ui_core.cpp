/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_ui_versions.h"
#include "esp_ui_core.hpp"

#if !ESP_UI_LOG_ENABLE_DEBUG_CORE_CORE
#undef ESP_UI_LOGD
#define ESP_UI_LOGD(...)
#endif

using namespace std;

ESP_UI_Core::ESP_UI_Core(const ESP_UI_CoreData_t &data, ESP_UI_CoreHome &home, ESP_UI_CoreManager &manager,
                         lv_disp_t *display):
    _core_data(data),
    _core_home(home),
    _core_manager(manager),
    _display(display),
    _touch(nullptr),
    _free_event_code(_LV_EVENT_LAST),
    _event_obj(nullptr),
    _data_update_event_code(_LV_EVENT_LAST),
    _navigate_event_code(_LV_EVENT_LAST),
    _app_event_code(_LV_EVENT_LAST)
{
}

ESP_UI_Core::~ESP_UI_Core()
{
    ESP_UI_LOGD("Destroy(@0x%p)", this);
    if (!delCore()) {
        ESP_UI_LOGE("Delete failed");
    }
}

bool ESP_UI_Core::setTouchDevice(lv_indev_t *touch) const
{
    ESP_UI_CHECK_FALSE_RETURN((touch != nullptr) && (lv_indev_get_type(touch) == LV_INDEV_TYPE_POINTER), false,
                              "Invalid touch device");

    ESP_UI_LOGD("Set touch device(@%p)", touch);
    _touch = touch;

    return true;
}

bool ESP_UI_Core::registerDateUpdateEventCallback(lv_event_cb_t callback, void *user_data) const
{
    ESP_UI_CHECK_NULL_RETURN(callback, false, "Invalid callback function");
    ESP_UI_CHECK_FALSE_RETURN(checkCoreInitialized(), false, "Core is not initialized");

    ESP_UI_CHECK_NULL_RETURN(lv_obj_add_event_cb(_event_obj.get(), callback, _data_update_event_code, user_data), false,
                             "Add data update event callback failed");

    return true;
};

bool ESP_UI_Core::unregisterDateUpdateEventCallback(lv_event_cb_t callback, void *user_data) const
{
    ESP_UI_CHECK_FALSE_RETURN(checkCoreInitialized(), false, "Core is not initialized");

    ESP_UI_CHECK_FALSE_RETURN(lv_obj_remove_event_cb_with_user_data(_event_obj.get(), callback, user_data), false,
                              "Remove data update event callback failed");

    return true;
};

bool ESP_UI_Core::sendDataUpdateEvent(void *param) const
{
    ESP_UI_CHECK_FALSE_RETURN(checkCoreInitialized(), false, "Core is not initialized");

    ESP_UI_CHECK_FALSE_RETURN(lv_event_send(_event_obj.get(), _data_update_event_code, param) == LV_RES_OK, false,
                              "Send data update event failed");

    return true;
}

bool ESP_UI_Core::registerNavigateEventCallback(lv_event_cb_t callback, void *user_data) const
{
    ESP_UI_CHECK_NULL_RETURN(callback, false, "Invalid callback function");
    ESP_UI_CHECK_FALSE_RETURN(checkCoreInitialized(), false, "Core is not initialized");

    ESP_UI_CHECK_NULL_RETURN(lv_obj_add_event_cb(_event_obj.get(), callback, _navigate_event_code, user_data), false,
                             "Add navigate event callback failed");

    return true;
}

bool ESP_UI_Core::unregisterNavigateEventCallback(lv_event_cb_t callback, void *user_data) const
{
    ESP_UI_CHECK_FALSE_RETURN(checkCoreInitialized(), false, "Core is not initialized");

    ESP_UI_CHECK_FALSE_RETURN(lv_obj_remove_event_cb_with_user_data(_event_obj.get(), callback, user_data), false,
                              "Remove navigate event callback failed");

    return true;
}

bool ESP_UI_Core::sendNavigateEvent(ESP_UI_CoreNavigateType_t type) const
{
    ESP_UI_CHECK_FALSE_RETURN(checkCoreInitialized(), false, "Core is not initialized");

    ESP_UI_CHECK_FALSE_RETURN(lv_event_send(_event_obj.get(), _navigate_event_code, (void *)type) == LV_RES_OK, false,
                              "Send navigate event failed");

    return true;
}

bool ESP_UI_Core::registerAppEventCallback(lv_event_cb_t callback, void *user_data) const
{
    ESP_UI_CHECK_NULL_RETURN(callback, false, "Invalid callback function");
    ESP_UI_CHECK_FALSE_RETURN(checkCoreInitialized(), false, "Core is not initialized");

    ESP_UI_CHECK_NULL_RETURN(lv_obj_add_event_cb(_event_obj.get(), callback, _app_event_code, user_data), false,
                             "Add app start event callback failed");

    return true;
}

bool ESP_UI_Core::unregisterAppEventCallback(lv_event_cb_t callback, void *user_data) const
{
    ESP_UI_CHECK_FALSE_RETURN(checkCoreInitialized(), false, "Core is not initialized");

    ESP_UI_CHECK_FALSE_RETURN(lv_obj_remove_event_cb_with_user_data(_event_obj.get(), callback, user_data), false,
                              "Remove app start event callback failed");

    return true;
}

bool ESP_UI_Core::sendAppEvent(const ESP_UI_CoreAppEventData_t *data) const
{
    ESP_UI_CHECK_FALSE_RETURN(checkCoreInitialized(), false, "Core is not initialized");

    ESP_UI_CHECK_FALSE_RETURN(lv_event_send(_event_obj.get(), _app_event_code, (void *)data) == LV_RES_OK, false,
                              "Send app start event failed");

    return true;
}

bool ESP_UI_Core::beginCore(void)
{
    ESP_UI_LvObj_t event_obj = nullptr;
    lv_event_code_t data_update_event_code = _LV_EVENT_LAST;
    lv_event_code_t navigate_event_code = _LV_EVENT_LAST;
    lv_event_code_t app_event_code = _LV_EVENT_LAST;

    ESP_UI_LOGI("Library version: %d.%d.%d", ESP_UI_VER_MAJOR, ESP_UI_VER_MINOR, ESP_UI_VER_PATCH);
    ESP_UI_LOGD("Begin core(@0x%p)", this);
    ESP_UI_CHECK_FALSE_RETURN(!checkCoreInitialized(), false, "Core is already initialized");

    // Initialize events
    event_obj = ESP_UI_LV_OBJ(obj, nullptr);
    ESP_UI_CHECK_NULL_RETURN(event_obj, false, "Failed to create event object");

    data_update_event_code = getFreeEventCode();
    ESP_UI_CHECK_FALSE_RETURN(esp_ui_core_utils_check_event_code_valid(data_update_event_code), false,
                              "Create data update event code failed");
    ESP_UI_CHECK_NULL_RETURN(
        lv_obj_add_event_cb(event_obj.get(), onCoreDataUpdateEventCallback, data_update_event_code, this), false,
        "Register navigate event callback failed"
    );

    navigate_event_code = getFreeEventCode();
    ESP_UI_CHECK_FALSE_RETURN(esp_ui_core_utils_check_event_code_valid(navigate_event_code), false,
                              "Create navigate event code failed");
    ESP_UI_CHECK_NULL_RETURN(
        lv_obj_add_event_cb(event_obj.get(), onCoreNavigateEventCallback, navigate_event_code, this), false,
        "Register navigate event callback failed"
    );

    app_event_code = getFreeEventCode();
    ESP_UI_CHECK_FALSE_RETURN(esp_ui_core_utils_check_event_code_valid(app_event_code), false,
                              "Create app event code failed");

    // Save data
    _event_obj = event_obj;
    _data_update_event_code = data_update_event_code;
    _navigate_event_code = navigate_event_code;
    _app_event_code = app_event_code;

    // Initialize cores
    ESP_UI_CHECK_FALSE_GOTO(_core_home.beginCore(), err, "Begin core home failed");
    ESP_UI_CHECK_FALSE_GOTO(_core_manager.beginCore(), err, "Begin core manager failed");

    return true;

err:
    ESP_UI_CHECK_FALSE_RETURN(delCore(), false, "Delete core failed");

    return false;
}

bool ESP_UI_Core::delCore(void)
{
    bool ret = true;

    ESP_UI_LOGD("Delete(@0x%p)", this);

    if (!checkCoreInitialized()) {
        return true;
    }

    _display = nullptr;
    _touch = nullptr;
    _free_event_code = _LV_EVENT_LAST;
    _event_obj.reset();
    _data_update_event_code = _LV_EVENT_LAST;
    _navigate_event_code = _LV_EVENT_LAST;
    _app_event_code = _LV_EVENT_LAST;

    if (!_core_home.delCore()) {
        ESP_UI_LOGE("Delete core home failed");
        ret = false;
    }
    if (!_core_manager.delCore()) {
        ESP_UI_LOGE("Delete core manager failed");
        ret = false;
    }

    return ret;
}

bool ESP_UI_Core::calibrateCoreData(ESP_UI_CoreData_t &data)
{
    ESP_UI_CHECK_NULL_RETURN(_display, false, "Display device is not initialized");

    ESP_UI_StyleSize_t display_size = {
        (uint16_t)lv_disp_get_hor_res(_display),
        (uint16_t)lv_disp_get_ver_res(_display)
    };

    /* Basic */
    ESP_UI_CHECK_NULL_RETURN(data.name, false, "Core name is invalid");
    ESP_UI_CHECK_FALSE_RETURN(_core_home.calibrateCoreObjectSize(display_size, data.screen_size), false,
                              "Invalid Core screen_size");

    // Home
    ESP_UI_CHECK_FALSE_RETURN(_core_home.calibrateCoreData(data.home), false, "Invalid Core home data");

    return true;
}

void ESP_UI_Core::onCoreDataUpdateEventCallback(lv_event_t *event)
{
    ESP_UI_Core *core = nullptr;

    ESP_UI_LOGD("Core date update event callback");
    ESP_UI_CHECK_NULL_EXIT(event, "Invalid event object");

    core = (ESP_UI_Core *)lv_event_get_user_data(event);
    ESP_UI_CHECK_NULL_EXIT(core, "Invalid core object");

    ESP_UI_CHECK_FALSE_EXIT(core->_core_home.updateByNewData(), "Core home update failed");
}

void ESP_UI_Core::onCoreNavigateEventCallback(lv_event_t *event)
{
    ESP_UI_Core *core = nullptr;
    ESP_UI_CoreNavigateType_t type = ESP_UI_CORE_NAVIGATE_TYPE_MAX;
    void *param = nullptr;

    ESP_UI_LOGD("Navigate event callback");
    ESP_UI_CHECK_NULL_EXIT(event, "Invalid event object");

    core = (ESP_UI_Core *)lv_event_get_user_data(event);
    ESP_UI_CHECK_NULL_EXIT(core, "Invalid core object");

    param = lv_event_get_param(event);
    memcpy(&type, &param, sizeof(ESP_UI_CoreNavigateType_t));
    ESP_UI_CHECK_VALUE_EXIT(type, 0, ESP_UI_CORE_NAVIGATE_TYPE_MAX - 1, "Invalid navigate type");

    switch (type) {
    case ESP_UI_CORE_NAVIGATE_TYPE_RECENTS_SCREEN:
        ESP_UI_LOGD("Navigate to recents_screen");
        break;
    case ESP_UI_CORE_NAVIGATE_TYPE_HOME:
        ESP_UI_LOGD("Navigate to home");
        break;
    case ESP_UI_CORE_NAVIGATE_TYPE_BACK:
        ESP_UI_LOGD("Navigate to back");
        break;
    default:
        ESP_UI_LOGW("Unknown navigate type: %d", type);
        break;
    }
}
