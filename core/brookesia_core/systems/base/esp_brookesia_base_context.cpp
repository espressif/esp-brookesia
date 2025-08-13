/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <cstring>
#include "esp_brookesia_systems_internal.h"
#if !ESP_BROOKESIA_BASE_CORE_ENABLE_DEBUG_LOG
#   define ESP_BROOKESIA_UTILS_DISABLE_DEBUG_LOG
#endif
#include "private/esp_brookesia_base_utils.hpp"
#include "gui/lvgl/esp_brookesia_lv_lock.hpp"
#include "squareline/ui_comp/ui_comp.h"
#include "esp_brookesia_base_context.hpp"

using namespace std;
using namespace esp_brookesia::gui;

namespace esp_brookesia::systems::base {

Context::Context(
    const Data &data, Display &display, Manager &manager,
    lv_display_t *device
):
    _data(data),
    _display(display),
    _manager(manager),
    _event(),
    _display_device(device),
    _touch_device(nullptr),
    _free_event_code(_LV_EVENT_LAST),
    _event_obj(nullptr),
    _data_update_event_code(_LV_EVENT_LAST),
    _navigate_event_code(_LV_EVENT_LAST),
    _app_event_code(_LV_EVENT_LAST)
{
}

Context::~Context()
{
    ESP_UTILS_LOGD("Destroy(@0x%p)", this);
    if (!del()) {
        ESP_UTILS_LOGE("Delete failed");
    }
}

bool Context::getDisplaySize(gui::StyleSize &size)
{
    // Check if the display is set. If not, use the default display
    if (_display_device == nullptr) {
        ESP_UTILS_LOGW("Display is not set, use default display");
        _display_device = lv_disp_get_default();
        ESP_UTILS_CHECK_NULL_RETURN(_display_device, false, "Display device is not initialized");
    }

    size = {
        .width = (int)lv_disp_get_hor_res(_display_device),
        .height = (int)lv_disp_get_ver_res(_display_device),
    };

    return true;
}

bool Context::setTouchDevice(lv_indev_t *touch)
{
    ESP_UTILS_CHECK_FALSE_RETURN((touch != nullptr) && (lv_indev_get_type(touch) == LV_INDEV_TYPE_POINTER), false,
                                 "Invalid touch device");

    ESP_UTILS_LOGD("Set touch device(@%p)", touch);
    _touch_device = touch;

    return true;
}

bool Context::registerDateUpdateEventCallback(lv_event_cb_t callback, void *user_data)
{
    ESP_UTILS_CHECK_NULL_RETURN(callback, false, "Invalid callback function");
    ESP_UTILS_CHECK_FALSE_RETURN(checkCoreInitialized(), false, "Context is not initialized");

    ESP_UTILS_CHECK_NULL_RETURN(lv_obj_add_event_cb(_event_obj.get(), callback, _data_update_event_code, user_data), false,
                                "Add data update event callback failed");

    return true;
};

bool Context::unregisterDateUpdateEventCallback(lv_event_cb_t callback, void *user_data)
{
    ESP_UTILS_CHECK_FALSE_RETURN(checkCoreInitialized(), false, "Context is not initialized");

    ESP_UTILS_CHECK_FALSE_RETURN(lv_obj_remove_event_cb_with_user_data(_event_obj.get(), callback, user_data), false,
                                 "Remove data update event callback failed");

    return true;
};

bool Context::sendDataUpdateEvent(void *param)
{
    ESP_UTILS_CHECK_FALSE_RETURN(checkCoreInitialized(), false, "Context is not initialized");

    ESP_UTILS_CHECK_FALSE_RETURN(lv_obj_send_event(_event_obj.get(), _data_update_event_code, param) == LV_RES_OK, false,
                                 "Send data update event failed");

    return true;
}

bool Context::registerNavigateEventCallback(lv_event_cb_t callback, void *user_data)
{
    ESP_UTILS_CHECK_NULL_RETURN(callback, false, "Invalid callback function");
    ESP_UTILS_CHECK_FALSE_RETURN(checkCoreInitialized(), false, "Context is not initialized");

    ESP_UTILS_CHECK_NULL_RETURN(lv_obj_add_event_cb(_event_obj.get(), callback, _navigate_event_code, user_data), false,
                                "Add navigate event callback failed");

    return true;
}

bool Context::unregisterNavigateEventCallback(lv_event_cb_t callback, void *user_data)
{
    ESP_UTILS_CHECK_FALSE_RETURN(checkCoreInitialized(), false, "Context is not initialized");

    ESP_UTILS_CHECK_FALSE_RETURN(lv_obj_remove_event_cb_with_user_data(_event_obj.get(), callback, user_data), false,
                                 "Remove navigate event callback failed");

    return true;
}

bool Context::sendNavigateEvent(Manager::NavigateType type)
{
    ESP_UTILS_CHECK_FALSE_RETURN(checkCoreInitialized(), false, "Context is not initialized");

    ESP_UTILS_CHECK_FALSE_RETURN(lv_obj_send_event(_event_obj.get(), _navigate_event_code, (void *)type) == LV_RES_OK, false,
                                 "Send navigate event failed");

    return true;
}

bool Context::registerAppEventCallback(lv_event_cb_t callback, void *user_data)
{
    ESP_UTILS_CHECK_NULL_RETURN(callback, false, "Invalid callback function");
    ESP_UTILS_CHECK_FALSE_RETURN(checkCoreInitialized(), false, "Context is not initialized");

    ESP_UTILS_CHECK_NULL_RETURN(lv_obj_add_event_cb(_event_obj.get(), callback, _app_event_code, user_data), false,
                                "Add app start event callback failed");

    return true;
}

bool Context::unregisterAppEventCallback(lv_event_cb_t callback, void *user_data)
{
    ESP_UTILS_CHECK_FALSE_RETURN(checkCoreInitialized(), false, "Context is not initialized");

    ESP_UTILS_CHECK_FALSE_RETURN(lv_obj_remove_event_cb_with_user_data(_event_obj.get(), callback, user_data), false,
                                 "Remove app start event callback failed");

    return true;
}

bool Context::sendAppEvent(const Context::AppEventData *data)
{
    ESP_UTILS_CHECK_FALSE_RETURN(checkCoreInitialized(), false, "Context is not initialized");

    ESP_UTILS_CHECK_FALSE_RETURN(lv_obj_send_event(_event_obj.get(), _app_event_code, (void *)data) == LV_RES_OK, false,
                                 "Send app start event failed");

    return true;
}

bool Context::lockLv(int timeout)
{
    ESP_UTILS_CHECK_FALSE_RETURN(LvLock::getInstance().lock(timeout), false, "Lock failed");

    return true;
}

bool Context::unlockLv()
{
    ESP_UTILS_CHECK_FALSE_RETURN(LvLock::getInstance().unlock(), false, "Unlock failed");

    return true;
}

bool Context::begin(void)
{
    gui::LvObjSharedPtr event_obj = nullptr;
    lv_event_code_t data_update_event_code = _LV_EVENT_LAST;
    lv_event_code_t navigate_event_code = _LV_EVENT_LAST;
    lv_event_code_t app_event_code = _LV_EVENT_LAST;

    ESP_UTILS_LOGI("Library version: %d.%d.%d", BROOKESIA_CORE_VER_MAJOR, BROOKESIA_CORE_VER_MINOR, BROOKESIA_CORE_VER_PATCH);
    ESP_UTILS_LOGD("Begin core(@0x%p)", this);
    ESP_UTILS_CHECK_FALSE_RETURN(!checkCoreInitialized(), false, "Context is already initialized");

    // Initialize events
    event_obj = ESP_BROOKESIA_LV_OBJ(obj, nullptr);
    ESP_UTILS_CHECK_NULL_RETURN(event_obj, false, "Failed to create event object");

    data_update_event_code = getFreeEventCode();
    ESP_UTILS_CHECK_FALSE_RETURN(esp_brookesia_core_utils_check_event_code_valid(data_update_event_code), false,
                                 "Create data update event code failed");
    ESP_UTILS_CHECK_NULL_RETURN(
        lv_obj_add_event_cb(event_obj.get(), onCoreDataUpdateEventCallback, data_update_event_code, this), false,
        "Register navigate event callback failed"
    );

    navigate_event_code = getFreeEventCode();
    ESP_UTILS_CHECK_FALSE_RETURN(esp_brookesia_core_utils_check_event_code_valid(navigate_event_code), false,
                                 "Create navigate event code failed");
    ESP_UTILS_CHECK_NULL_RETURN(
        lv_obj_add_event_cb(event_obj.get(), onCoreNavigateEventCallback, navigate_event_code, this), false,
        "Register navigate event callback failed"
    );

    app_event_code = getFreeEventCode();
    ESP_UTILS_CHECK_FALSE_RETURN(esp_brookesia_core_utils_check_event_code_valid(app_event_code), false,
                                 "Create app event code failed");

    // Save data
    _event_obj = event_obj;
    _data_update_event_code = data_update_event_code;
    _navigate_event_code = navigate_event_code;
    _app_event_code = app_event_code;

    // Initialize cores
    ESP_UTILS_CHECK_FALSE_GOTO(_display.begin(), err, "Begin core display failed");
    ESP_UTILS_CHECK_FALSE_GOTO(_manager.begin(), err, "Begin core manager failed");

    // Initialize others
#if ESP_BROOKESIA_SQUARELINE_ENABLE_UI_COMP
    esp_brookesia_squareline_ui_comp_init();
#endif

    return true;

err:
    ESP_UTILS_CHECK_FALSE_RETURN(del(), false, "Delete core failed");

    return false;
}

bool Context::del(void)
{
    bool ret = true;

    ESP_UTILS_LOGD("Delete(@0x%p)", this);

    if (!checkCoreInitialized()) {
        return true;
    }

    if (!_manager.del()) {
        ESP_UTILS_LOGE("Delete core manager failed");
        ret = false;
    }
    if (!_display.del()) {
        ESP_UTILS_LOGE("Delete core display failed");
        ret = false;
    }

    _display_device = nullptr;
    _touch_device = nullptr;
    _free_event_code = _LV_EVENT_LAST;
    _event_obj.reset();
    _data_update_event_code = _LV_EVENT_LAST;
    _navigate_event_code = _LV_EVENT_LAST;
    _app_event_code = _LV_EVENT_LAST;

    return ret;
}

bool Context::calibrateCoreData(Data &data)
{
    ESP_UTILS_CHECK_NULL_RETURN(_display_device, false, "Display device is not initialized");

    gui::StyleSize display_size = {
        (int)lv_disp_get_hor_res(_display_device),
        (int)lv_disp_get_ver_res(_display_device)
    };

    /* Basic */
    ESP_UTILS_CHECK_NULL_RETURN(data.name, false, "Context name is invalid");
    ESP_UTILS_CHECK_FALSE_RETURN(_display.calibrateCoreObjectSize(display_size, data.screen_size), false,
                                 "Invalid Context screen_size");

    // Display
    ESP_UTILS_CHECK_FALSE_RETURN(_display.calibrateCoreData(data.display), false, "Invalid Context display data");

    return true;
}

void Context::onCoreDataUpdateEventCallback(lv_event_t *event)
{
    Context *core = nullptr;

    ESP_UTILS_LOGD("Context date update event callback");
    ESP_UTILS_CHECK_NULL_EXIT(event, "Invalid event object");

    core = (Context *)lv_event_get_user_data(event);
    ESP_UTILS_CHECK_NULL_EXIT(core, "Invalid core object");

    ESP_UTILS_CHECK_FALSE_EXIT(core->_display.updateByNewData(), "Context display update failed");
}

void Context::onCoreNavigateEventCallback(lv_event_t *event)
{
    Context *core = nullptr;
    Manager::NavigateType type = Manager::NavigateType::MAX;
    void *param = nullptr;

    ESP_UTILS_LOGD("Navigate event callback");
    ESP_UTILS_CHECK_NULL_EXIT(event, "Invalid event object");

    core = (Context *)lv_event_get_user_data(event);
    ESP_UTILS_CHECK_NULL_EXIT(core, "Invalid core object");

    param = lv_event_get_param(event);
    memcpy(&type, &param, sizeof(Manager::NavigateType));

    switch (type) {
    case Manager::NavigateType::RECENTS_SCREEN:
        ESP_UTILS_LOGD("Navigate to recents_screen");
        break;
    case Manager::NavigateType::HOME:
        ESP_UTILS_LOGD("Navigate to home");
        break;
    case Manager::NavigateType::BACK:
        ESP_UTILS_LOGD("Navigate to back");
        break;
    default:
        ESP_UTILS_LOGW("Unknown navigate type: %d", static_cast<int>(type));
        break;
    }
}

} // namespace esp_brookesia::systems::base
