/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_brookesia_versions.h"
#include "esp_brookesia_core.hpp"

#if !ESP_BROOKESIA_LOG_ENABLE_DEBUG_CORE_CORE
#undef ESP_BROOKESIA_LOGD
#define ESP_BROOKESIA_LOGD(...)
#endif

using namespace std;

ESP_Brookesia_Core::ESP_Brookesia_Core(const ESP_Brookesia_CoreData_t &data, ESP_Brookesia_CoreHome &home, ESP_Brookesia_CoreManager &manager,
                                       lv_disp_t *display):
    _core_data(data),
    _core_home(home),
    _core_manager(manager),
    _core_event(),
    _display(display),
    _touch(nullptr),
    _free_event_code(_LV_EVENT_LAST),
    _event_obj(nullptr),
    _data_update_event_code(_LV_EVENT_LAST),
    _navigate_event_code(_LV_EVENT_LAST),
    _app_event_code(_LV_EVENT_LAST),
    _lv_lock_callback(nullptr),
    _lv_unlock_callback(nullptr)
{
}

ESP_Brookesia_Core::~ESP_Brookesia_Core()
{
    ESP_BROOKESIA_LOGD("Destroy(@0x%p)", this);
    if (!delCore()) {
        ESP_BROOKESIA_LOGE("Delete failed");
    }
}

bool ESP_Brookesia_Core::getDisplaySize(ESP_Brookesia_StyleSize_t &size)
{
    // Check if the display is set. If not, use the default display
    if (_display == nullptr) {
        ESP_BROOKESIA_LOGW("Display is not set, use default display");
        _display = lv_disp_get_default();
        ESP_BROOKESIA_CHECK_NULL_RETURN(_display, false, "Display device is not initialized");
    }

    size = {
        .width = (uint16_t)lv_disp_get_hor_res(_display),
        .height = (uint16_t)lv_disp_get_ver_res(_display),
    };

    return true;
}

bool ESP_Brookesia_Core::setTouchDevice(lv_indev_t *touch) const
{
    ESP_BROOKESIA_CHECK_FALSE_RETURN((touch != nullptr) && (lv_indev_get_type(touch) == LV_INDEV_TYPE_POINTER), false,
                                     "Invalid touch device");

    ESP_BROOKESIA_LOGD("Set touch device(@%p)", touch);
    _touch = touch;

    return true;
}

bool ESP_Brookesia_Core::registerDateUpdateEventCallback(lv_event_cb_t callback, void *user_data) const
{
    ESP_BROOKESIA_CHECK_NULL_RETURN(callback, false, "Invalid callback function");
    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkCoreInitialized(), false, "Core is not initialized");

    ESP_BROOKESIA_CHECK_NULL_RETURN(lv_obj_add_event_cb(_event_obj.get(), callback, _data_update_event_code, user_data), false,
                                    "Add data update event callback failed");

    return true;
};

bool ESP_Brookesia_Core::unregisterDateUpdateEventCallback(lv_event_cb_t callback, void *user_data) const
{
    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkCoreInitialized(), false, "Core is not initialized");

    ESP_BROOKESIA_CHECK_FALSE_RETURN(lv_obj_remove_event_cb_with_user_data(_event_obj.get(), callback, user_data), false,
                                     "Remove data update event callback failed");

    return true;
};

bool ESP_Brookesia_Core::sendDataUpdateEvent(void *param) const
{
    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkCoreInitialized(), false, "Core is not initialized");

    ESP_BROOKESIA_CHECK_FALSE_RETURN(lv_event_send(_event_obj.get(), _data_update_event_code, param) == LV_RES_OK, false,
                                     "Send data update event failed");

    return true;
}

bool ESP_Brookesia_Core::registerNavigateEventCallback(lv_event_cb_t callback, void *user_data) const
{
    ESP_BROOKESIA_CHECK_NULL_RETURN(callback, false, "Invalid callback function");
    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkCoreInitialized(), false, "Core is not initialized");

    ESP_BROOKESIA_CHECK_NULL_RETURN(lv_obj_add_event_cb(_event_obj.get(), callback, _navigate_event_code, user_data), false,
                                    "Add navigate event callback failed");

    return true;
}

bool ESP_Brookesia_Core::unregisterNavigateEventCallback(lv_event_cb_t callback, void *user_data) const
{
    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkCoreInitialized(), false, "Core is not initialized");

    ESP_BROOKESIA_CHECK_FALSE_RETURN(lv_obj_remove_event_cb_with_user_data(_event_obj.get(), callback, user_data), false,
                                     "Remove navigate event callback failed");

    return true;
}

bool ESP_Brookesia_Core::sendNavigateEvent(ESP_Brookesia_CoreNavigateType_t type) const
{
    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkCoreInitialized(), false, "Core is not initialized");

    ESP_BROOKESIA_CHECK_FALSE_RETURN(lv_event_send(_event_obj.get(), _navigate_event_code, (void *)type) == LV_RES_OK, false,
                                     "Send navigate event failed");

    return true;
}

bool ESP_Brookesia_Core::registerAppEventCallback(lv_event_cb_t callback, void *user_data) const
{
    ESP_BROOKESIA_CHECK_NULL_RETURN(callback, false, "Invalid callback function");
    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkCoreInitialized(), false, "Core is not initialized");

    ESP_BROOKESIA_CHECK_NULL_RETURN(lv_obj_add_event_cb(_event_obj.get(), callback, _app_event_code, user_data), false,
                                    "Add app start event callback failed");

    return true;
}

bool ESP_Brookesia_Core::unregisterAppEventCallback(lv_event_cb_t callback, void *user_data) const
{
    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkCoreInitialized(), false, "Core is not initialized");

    ESP_BROOKESIA_CHECK_FALSE_RETURN(lv_obj_remove_event_cb_with_user_data(_event_obj.get(), callback, user_data), false,
                                     "Remove app start event callback failed");

    return true;
}

bool ESP_Brookesia_Core::sendAppEvent(const ESP_Brookesia_CoreAppEventData_t *data) const
{
    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkCoreInitialized(), false, "Core is not initialized");

    ESP_BROOKESIA_CHECK_FALSE_RETURN(lv_event_send(_event_obj.get(), _app_event_code, (void *)data) == LV_RES_OK, false,
                                     "Send app start event failed");

    return true;
}

void ESP_Brookesia_Core::registerLvLockCallback(ESP_Brookesia_LvLockCallback_t callback, int timeout)
{
    _lv_lock_callback = callback;
    _lv_lock_timeout = timeout;
}

void ESP_Brookesia_Core::registerLvUnlockCallback(ESP_Brookesia_LvUnlockCallback_t callback)
{
    _lv_unlock_callback = callback;
}

bool ESP_Brookesia_Core::lockLv(void) const
{
    ESP_BROOKESIA_CHECK_NULL_RETURN(_lv_lock_callback, false, "Lock callback is not set");
    ESP_BROOKESIA_CHECK_FALSE_RETURN(_lv_lock_callback(_lv_lock_timeout), false, "Lock failed");

    return false;
}

bool ESP_Brookesia_Core::lockLv(int timeout) const
{
    ESP_BROOKESIA_CHECK_NULL_RETURN(_lv_lock_callback, false, "Lock callback is not set");
    ESP_BROOKESIA_CHECK_FALSE_RETURN(_lv_lock_callback(timeout), false, "Lock failed");

    return false;
}

void ESP_Brookesia_Core::unlockLv(void) const
{
    ESP_BROOKESIA_CHECK_NULL_EXIT(_lv_unlock_callback, "Unlock callback is not set");

    _lv_unlock_callback();
}

bool ESP_Brookesia_Core::beginCore(void)
{
    ESP_Brookesia_LvObj_t event_obj = nullptr;
    lv_event_code_t data_update_event_code = _LV_EVENT_LAST;
    lv_event_code_t navigate_event_code = _LV_EVENT_LAST;
    lv_event_code_t app_event_code = _LV_EVENT_LAST;

    ESP_BROOKESIA_LOGI("Library version: %d.%d.%d", ESP_BROOKESIA_VER_MAJOR, ESP_BROOKESIA_VER_MINOR, ESP_BROOKESIA_VER_PATCH);
    ESP_BROOKESIA_LOGD("Begin core(@0x%p)", this);
    ESP_BROOKESIA_CHECK_FALSE_RETURN(!checkCoreInitialized(), false, "Core is already initialized");

    // Initialize events
    event_obj = ESP_BROOKESIA_LV_OBJ(obj, nullptr);
    ESP_BROOKESIA_CHECK_NULL_RETURN(event_obj, false, "Failed to create event object");

    data_update_event_code = getFreeEventCode();
    ESP_BROOKESIA_CHECK_FALSE_RETURN(esp_brookesia_core_utils_check_event_code_valid(data_update_event_code), false,
                                     "Create data update event code failed");
    ESP_BROOKESIA_CHECK_NULL_RETURN(
        lv_obj_add_event_cb(event_obj.get(), onCoreDataUpdateEventCallback, data_update_event_code, this), false,
        "Register navigate event callback failed"
    );

    navigate_event_code = getFreeEventCode();
    ESP_BROOKESIA_CHECK_FALSE_RETURN(esp_brookesia_core_utils_check_event_code_valid(navigate_event_code), false,
                                     "Create navigate event code failed");
    ESP_BROOKESIA_CHECK_NULL_RETURN(
        lv_obj_add_event_cb(event_obj.get(), onCoreNavigateEventCallback, navigate_event_code, this), false,
        "Register navigate event callback failed"
    );

    app_event_code = getFreeEventCode();
    ESP_BROOKESIA_CHECK_FALSE_RETURN(esp_brookesia_core_utils_check_event_code_valid(app_event_code), false,
                                     "Create app event code failed");

    // Save data
    _event_obj = event_obj;
    _data_update_event_code = data_update_event_code;
    _navigate_event_code = navigate_event_code;
    _app_event_code = app_event_code;

    // Initialize cores
    ESP_BROOKESIA_CHECK_FALSE_GOTO(_core_home.beginCore(), err, "Begin core home failed");
    ESP_BROOKESIA_CHECK_FALSE_GOTO(_core_manager.beginCore(), err, "Begin core manager failed");

    // Initialize others
#if ESP_BROOKESIA_SQUARELINE_USE_INTERNAL_UI_COMP
    esp_brookesia_squareline_ui_comp_init();
#endif /* ESP_BROOKESIA_SQUARELINE_USE_INTERNAL_UI_COMP */

    return true;

err:
    ESP_BROOKESIA_CHECK_FALSE_RETURN(delCore(), false, "Delete core failed");

    return false;
}

bool ESP_Brookesia_Core::delCore(void)
{
    bool ret = true;

    ESP_BROOKESIA_LOGD("Delete(@0x%p)", this);

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
        ESP_BROOKESIA_LOGE("Delete core home failed");
        ret = false;
    }
    if (!_core_manager.delCore()) {
        ESP_BROOKESIA_LOGE("Delete core manager failed");
        ret = false;
    }

    return ret;
}

bool ESP_Brookesia_Core::calibrateCoreData(ESP_Brookesia_CoreData_t &data)
{
    ESP_BROOKESIA_CHECK_NULL_RETURN(_display, false, "Display device is not initialized");

    ESP_Brookesia_StyleSize_t display_size = {
        (uint16_t)lv_disp_get_hor_res(_display),
        (uint16_t)lv_disp_get_ver_res(_display)
    };

    /* Basic */
    ESP_BROOKESIA_CHECK_NULL_RETURN(data.name, false, "Core name is invalid");
    ESP_BROOKESIA_CHECK_FALSE_RETURN(_core_home.calibrateCoreObjectSize(display_size, data.screen_size), false,
                                     "Invalid Core screen_size");

    // Home
    ESP_BROOKESIA_CHECK_FALSE_RETURN(_core_home.calibrateCoreData(data.home), false, "Invalid Core home data");

    return true;
}

void ESP_Brookesia_Core::onCoreDataUpdateEventCallback(lv_event_t *event)
{
    ESP_Brookesia_Core *core = nullptr;

    ESP_BROOKESIA_LOGD("Core date update event callback");
    ESP_BROOKESIA_CHECK_NULL_EXIT(event, "Invalid event object");

    core = (ESP_Brookesia_Core *)lv_event_get_user_data(event);
    ESP_BROOKESIA_CHECK_NULL_EXIT(core, "Invalid core object");

    ESP_BROOKESIA_CHECK_FALSE_EXIT(core->_core_home.updateByNewData(), "Core home update failed");
}

void ESP_Brookesia_Core::onCoreNavigateEventCallback(lv_event_t *event)
{
    ESP_Brookesia_Core *core = nullptr;
    ESP_Brookesia_CoreNavigateType_t type = ESP_BROOKESIA_CORE_NAVIGATE_TYPE_MAX;
    void *param = nullptr;

    ESP_BROOKESIA_LOGD("Navigate event callback");
    ESP_BROOKESIA_CHECK_NULL_EXIT(event, "Invalid event object");

    core = (ESP_Brookesia_Core *)lv_event_get_user_data(event);
    ESP_BROOKESIA_CHECK_NULL_EXIT(core, "Invalid core object");

    param = lv_event_get_param(event);
    memcpy(&type, &param, sizeof(ESP_Brookesia_CoreNavigateType_t));
    ESP_BROOKESIA_CHECK_VALUE_EXIT(type, 0, ESP_BROOKESIA_CORE_NAVIGATE_TYPE_MAX - 1, "Invalid navigate type");

    switch (type) {
    case ESP_BROOKESIA_CORE_NAVIGATE_TYPE_RECENTS_SCREEN:
        ESP_BROOKESIA_LOGD("Navigate to recents_screen");
        break;
    case ESP_BROOKESIA_CORE_NAVIGATE_TYPE_HOME:
        ESP_BROOKESIA_LOGD("Navigate to home");
        break;
    case ESP_BROOKESIA_CORE_NAVIGATE_TYPE_BACK:
        ESP_BROOKESIA_LOGD("Navigate to back");
        break;
    default:
        ESP_BROOKESIA_LOGW("Unknown navigate type: %d", type);
        break;
    }
}
