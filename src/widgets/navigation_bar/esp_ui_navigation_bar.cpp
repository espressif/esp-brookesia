/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <algorithm>
#include "esp_ui_navigation_bar.hpp"

using namespace std;

#if !ESP_UI_LOG_ENABLE_DEBUG_WIDGETS_NAVIGATION
#undef ESP_UI_LOGD
#define ESP_UI_LOGD(...)
#endif

ESP_UI_NavigationBar::ESP_UI_NavigationBar(const ESP_UI_Core &core, const ESP_UI_NavigationBarData_t &data):
    _core(core),
    _data(data),
    _is_icon_pressed_losted(true),
    _main_obj(nullptr)
{
}

ESP_UI_NavigationBar::~ESP_UI_NavigationBar()
{
    ESP_UI_LOGD("Destroy(0x%p)", this);
    if (!del()) {
        ESP_UI_LOGE("Delete failed");
    }
}

bool ESP_UI_NavigationBar::begin(lv_obj_t *parent)
{
    ESP_UI_LvObj_t main_obj = nullptr;
    ESP_UI_LvObj_t button_obj = nullptr;
    ESP_UI_LvObj_t icon_main_obj = nullptr;
    ESP_UI_LvObj_t icon_image_obj = nullptr;
    vector<ESP_UI_LvObj_t> button_objs;
    vector<ESP_UI_LvObj_t> icon_main_objs;
    vector<ESP_UI_LvObj_t> icon_image_objs;

    ESP_UI_LOGD("Begin(0x%p)", this);
    ESP_UI_CHECK_NULL_RETURN(parent, false, "Invalid parent");
    ESP_UI_CHECK_FALSE_RETURN(!checkInitialized(), false, "Already initialized");

    /* Create objects */
    // Main
    main_obj = ESP_UI_LV_OBJ(obj, parent);
    ESP_UI_CHECK_NULL_RETURN(main_obj, false, "Create main object failed");
    // Button
    for (int i = 0; i < ESP_UI_NAVIGATION_BAR_DATA_BUTTON_NUM; i++) {
        button_obj = ESP_UI_LV_OBJ(obj, main_obj.get());
        ESP_UI_CHECK_NULL_RETURN(button_obj, false, "Create button failed");
        button_objs.push_back(button_obj);

        icon_main_obj = ESP_UI_LV_OBJ(obj, button_obj.get());
        ESP_UI_CHECK_NULL_RETURN(icon_main_obj, false, "Create icon main failed");
        icon_main_objs.push_back(icon_main_obj);

        icon_image_obj = ESP_UI_LV_OBJ(img, icon_main_obj.get());
        ESP_UI_CHECK_NULL_RETURN(icon_image_obj, false, "Create icon image failed");
        icon_image_objs.push_back(icon_image_obj);
    }
    ESP_UI_CHECK_FALSE_RETURN(_core.registerDateUpdateEventCallback(onDataUpdateEventCallback, this), false,
                              "Register data update event callback failed");

    /* Setup objects style */
    // Main
    lv_obj_add_style(main_obj.get(), _core.getCoreHome().getCoreContainerStyle(), 0);
    lv_obj_set_align(main_obj.get(), LV_ALIGN_BOTTOM_MID);
    lv_obj_set_flex_flow(main_obj.get(), LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(main_obj.get(), LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(main_obj.get(), LV_OBJ_FLAG_SCROLLABLE);
    // Button
    for (int i = 0; i < ESP_UI_NAVIGATION_BAR_DATA_BUTTON_NUM; i++) {
        lv_obj_add_style(button_objs[i].get(), _core.getCoreHome().getCoreContainerStyle(), 0);
        lv_obj_set_style_bg_opa(button_objs[i].get(), LV_OPA_TRANSP, 0);
        lv_obj_add_flag(button_objs[i].get(), LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(button_objs[i].get(), LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_PRESS_LOCK);
        lv_obj_add_event_cb(button_objs[i].get(), onIconTouchEventCallback, LV_EVENT_PRESSED, this);
        lv_obj_add_event_cb(button_objs[i].get(), onIconTouchEventCallback, LV_EVENT_PRESS_LOST, this);
        lv_obj_add_event_cb(button_objs[i].get(), onIconTouchEventCallback, LV_EVENT_RELEASED, this);
        lv_obj_add_event_cb(button_objs[i].get(), onIconTouchEventCallback, LV_EVENT_CLICKED, this);
        // Icon object
        lv_obj_add_style(icon_main_objs[i].get(), _core.getCoreHome().getCoreContainerStyle(), 0);
        lv_obj_align(icon_main_objs[i].get(), LV_ALIGN_CENTER, 0, 0);
        lv_obj_clear_flag(icon_main_objs[i].get(), LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
        // Icon image
        lv_obj_add_style(icon_image_objs[i].get(), _core.getCoreHome().getCoreContainerStyle(), 0);
        lv_obj_align(icon_image_objs[i].get(), LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_size(icon_image_objs[i].get(), LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_img_set_size_mode(icon_image_objs[i].get(), LV_IMG_SIZE_MODE_REAL);
        lv_obj_clear_flag(icon_image_objs[i].get(), LV_OBJ_FLAG_CLICKABLE);
    }

    /* Save objects */
    _main_obj = main_obj;
    _button_objs = button_objs;
    _icon_main_objs = icon_main_objs;
    _icon_image_objs = icon_image_objs;

    /* Update */
    ESP_UI_CHECK_FALSE_GOTO(updateByNewData(), err, "Update by new data failed");

    return true;

err:
    ESP_UI_CHECK_FALSE_RETURN(del(), false, "Delete failed");

    return false;
}

bool ESP_UI_NavigationBar::del(void)
{
    bool ret = true;

    ESP_UI_LOGD("Delete(0x%p)", this);

    if (!checkInitialized()) {
        return true;
    }

    if (_core.checkCoreInitialized() && !_core.unregisterDateUpdateEventCallback(onDataUpdateEventCallback, this)) {
        ESP_UI_LOGE("Unregister data update event callback failed");
        ret = false;
    }

    _main_obj.reset();
    _button_objs.clear();
    _icon_main_objs.clear();
    _icon_image_objs.clear();

    return ret;
}

bool ESP_UI_NavigationBar::setVisualMode(ESP_UI_NavigationBarVisualMode_t mode) const
{
    ESP_UI_LOGD("Set Visual Mode(%d)", mode);
    ESP_UI_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    switch (mode) {
    case ESP_UI_NAVIGATION_BAR_VISUAL_MODE_HIDE:
        lv_obj_add_flag(_main_obj.get(), LV_OBJ_FLAG_HIDDEN);
        break;
    case ESP_UI_NAVIGATION_BAR_VISUAL_MODE_SHOW_FIXED:
        lv_obj_clear_flag(_main_obj.get(), LV_OBJ_FLAG_HIDDEN);
        break;
    default:
        break;
    }

    return true;
}

bool ESP_UI_NavigationBar::checkVisible(void) const
{
    ESP_UI_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    return !lv_obj_has_flag(_main_obj.get(), LV_OBJ_FLAG_HIDDEN);
}

bool ESP_UI_NavigationBar::calibrateData(const ESP_UI_StyleSize_t &screen_size, const ESP_UI_CoreHome &home,
        ESP_UI_NavigationBarData_t &data)
{
    ESP_UI_CoreNavigateType_t navigate_type = ESP_UI_CORE_NAVIGATE_TYPE_MAX;
    const ESP_UI_StyleSize_t *parent_size = nullptr;

    ESP_UI_LOGD("Calibrate data");

    // Calibrate the min and max size
    if (data.flags.enable_main_size_min) {
        ESP_UI_CHECK_FALSE_RETURN(home.calibrateCoreObjectSize(screen_size, data.main.size_min), false,
                                  "Calibrate data main size min failed");
    }
    if (data.flags.enable_main_size_max) {
        ESP_UI_CHECK_FALSE_RETURN(home.calibrateCoreObjectSize(screen_size, data.main.size_max), false,
                                  "Calibrate data main size max failed");
    }

    /* Main */
    parent_size = &screen_size;
    ESP_UI_CHECK_FALSE_RETURN(home.calibrateCoreObjectSize(*parent_size, data.main.size), false,
                              "Invalid main size");
    // Adjust the size according to the min and max size
    if (data.flags.enable_main_size_min) {
        data.main.size.width = max(data.main.size.width, data.main.size_min.width);
        data.main.size.height = max(data.main.size.height, data.main.size_min.height);
    }
    if (data.flags.enable_main_size_max) {
        data.main.size.width = min(data.main.size.width, data.main.size_max.width);
        data.main.size.height = min(data.main.size.height, data.main.size_max.height);
    }
    // Button
    parent_size = &data.main.size;
    ESP_UI_CHECK_FALSE_RETURN(home.calibrateCoreObjectSize(*parent_size, data.button.icon_size), false,
                              "Invalid button icon size");
    for (int i = 0; i < ESP_UI_NAVIGATION_BAR_DATA_BUTTON_NUM; i++) {
        navigate_type = data.button.navigate_types[i];
        ESP_UI_CHECK_VALUE_RETURN(navigate_type, 0, ESP_UI_CORE_NAVIGATE_TYPE_MAX - 1, false,
                                  "Invalid button navigate type");
        ESP_UI_CHECK_NULL_RETURN(data.button.icon_images[i].resource, false, "Invalid button icon image resources");
    }

    return true;
}

bool ESP_UI_NavigationBar::updateByNewData(void)
{
    float h_factor = 0;
    float w_factor = 0;
    lv_img_dsc_t *icon_image_resource = nullptr;

    ESP_UI_LOGD("Update(0x%p)", this);
    ESP_UI_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    // Main
    lv_obj_set_size(_main_obj.get(), _data.main.size.width, _data.main.size.height);
    lv_obj_set_style_bg_color(_main_obj.get(), lv_color_hex(_data.main.background_color.color), 0);
    lv_obj_set_style_bg_opa(_main_obj.get(), _data.main.background_color.opacity, 0);

    for (int i = 0; i < ESP_UI_NAVIGATION_BAR_DATA_BUTTON_NUM; i++) {
        // Button
        lv_obj_set_size(_button_objs[i].get(), _data.main.size.width / ESP_UI_NAVIGATION_BAR_DATA_BUTTON_NUM,
                        _data.main.size.height);
        lv_obj_set_style_bg_color(_button_objs[i].get(), lv_color_hex(_data.button.active_background_color.color),
                                  LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(_button_objs[i].get(), _data.button.active_background_color.opacity, LV_STATE_PRESSED);
        // Icon main
        lv_obj_set_size(_icon_main_objs[i].get(), _data.button.icon_size.width, _data.button.icon_size.height);
        // Icon image
        icon_image_resource = (lv_img_dsc_t *)_data.button.icon_images[i].resource;
        lv_img_set_src(_icon_image_objs[i].get(), icon_image_resource);
        lv_obj_set_style_img_recolor(_icon_image_objs[i].get(),
                                     lv_color_hex(_data.button.icon_images[i].recolor.color), 0);
        lv_obj_set_style_img_recolor_opa(_icon_image_objs[i].get(),
                                         _data.button.icon_images[i].recolor.opacity, 0);
        // Calculate the multiple of the size between the target and the image.
        h_factor = (float)(_data.button.icon_size.height) / icon_image_resource->header.h;
        w_factor = (float)(_data.button.icon_size.width) / icon_image_resource->header.w;
        // Scale the image to a suitable size.
        // So you don’t have to consider the size of the source image.
        if (h_factor < w_factor) {
            lv_img_set_zoom(_icon_image_objs[i].get(), (int)(h_factor * LV_IMG_ZOOM_NONE));
        } else {
            lv_img_set_zoom(_icon_image_objs[i].get(), (int)(w_factor * LV_IMG_ZOOM_NONE));
        }
        lv_obj_refr_size(_icon_image_objs[i].get());
    }

    return true;
}

void ESP_UI_NavigationBar::onDataUpdateEventCallback(lv_event_t *event)
{
    ESP_UI_NavigationBar *navigation_bar = nullptr;

    ESP_UI_LOGD("Data update event callback");
    ESP_UI_CHECK_NULL_EXIT(event, "Invalid event object");

    navigation_bar = (ESP_UI_NavigationBar *)lv_event_get_user_data(event);
    ESP_UI_CHECK_NULL_EXIT(navigation_bar, "Invalid navigation bar object");

    ESP_UI_CHECK_FALSE_EXIT(navigation_bar->updateByNewData(), "Update failed");
}

void ESP_UI_NavigationBar::onIconTouchEventCallback(lv_event_t *event)
{
    lv_obj_t *button_obj = nullptr;
    lv_event_code_t event_code = _LV_EVENT_LAST;
    ESP_UI_NavigationBar *navigation_bar = nullptr;
    ESP_UI_CoreNavigateType_t navigate_type = ESP_UI_CORE_NAVIGATE_TYPE_MAX;

    ESP_UI_LOGD("Icon touch event callback");
    ESP_UI_CHECK_NULL_EXIT(event, "Invalid event object");

    event_code = lv_event_get_code(event);
    button_obj = lv_event_get_current_target(event);
    navigation_bar = (ESP_UI_NavigationBar *)lv_event_get_user_data(event);
    ESP_UI_CHECK_FALSE_EXIT(event_code < _LV_EVENT_LAST, "Invalid event code");
    ESP_UI_CHECK_NULL_EXIT(button_obj, "Invalid button object");
    ESP_UI_CHECK_NULL_EXIT(navigation_bar, "Invalid navigation bar");

    switch (event_code) {
    case LV_EVENT_CLICKED:
        ESP_UI_LOGD("Clicked");
        if (navigation_bar->_is_icon_pressed_losted) {
            break;
        }
        for (size_t i = 0; i < navigation_bar->_button_objs.size(); i++) {
            if (button_obj == navigation_bar->_button_objs[i].get()) {
                navigate_type = navigation_bar->_data.button.navigate_types[i];
                break;
            }
        }
        ESP_UI_CHECK_VALUE_EXIT(navigate_type, 0, ESP_UI_CORE_NAVIGATE_TYPE_MAX - 1, "Invalid navigate type");
        ESP_UI_CHECK_FALSE_EXIT(navigation_bar->_core.sendNavigateEvent(navigate_type), "Send navigate event failed");
        break;
    case LV_EVENT_PRESSED:
        ESP_UI_LOGD("Pressed");
        navigation_bar->_is_icon_pressed_losted = false;
        lv_obj_set_style_bg_opa(button_obj, navigation_bar->_data.button.active_background_color.opacity, 0);
        break;
    case LV_EVENT_PRESS_LOST:
        ESP_UI_LOGD("Press lost");
        navigation_bar->_is_icon_pressed_losted = true;
        [[fallthrough]];
    case LV_EVENT_RELEASED:
        ESP_UI_LOGD("Release");
        lv_obj_set_style_bg_opa(button_obj, LV_OPA_TRANSP, 0);
        break;
    default:
        ESP_UI_CHECK_FALSE_EXIT(false, "Invalid event code(%d)", event_code);
        break;
    }
}
