/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_brookesia_app_launcher_icon.hpp"

#if !ESP_BROOKESIA_LOG_ENABLE_DEBUG_WIDGETS_APP_LAUNCHER
#undef ESP_BROOKESIA_LOGD
#define ESP_BROOKESIA_LOGD(...)
#endif

using namespace std;

ESP_Brookesia_AppLauncherIcon::ESP_Brookesia_AppLauncherIcon(ESP_Brookesia_Core &core, const ESP_Brookesia_AppLauncherIconInfo_t &info,
        const ESP_Brookesia_AppLauncherIconData_t &data):
    _core(core),
    _info(info),
    _data(data),
    _flags{},
    _image_default_zoom(LV_IMG_ZOOM_NONE),
    _image_press_zoom(LV_IMG_ZOOM_NONE),
    _main_obj(nullptr),
    _icon_main_obj(nullptr),
    _icon_image_obj(nullptr),
    _name_label(nullptr)
{
}

ESP_Brookesia_AppLauncherIcon::~ESP_Brookesia_AppLauncherIcon()
{
    ESP_BROOKESIA_LOGD("Destroy(@0x%p)", this);
    if (!del()) {
        ESP_BROOKESIA_LOGE("Delete failed");
    }
}

bool ESP_Brookesia_AppLauncherIcon::begin(lv_obj_t *parent)
{
    ESP_Brookesia_LvObj_t main_obj = nullptr;
    ESP_Brookesia_LvObj_t icon_main_obj = nullptr;
    ESP_Brookesia_LvObj_t icon_image_obj = nullptr;
    ESP_Brookesia_LvObj_t name_label = nullptr;

    ESP_BROOKESIA_LOGD("Begin(%d: @0x%p)", _info.id, this);
    ESP_BROOKESIA_CHECK_NULL_RETURN(parent, false, "Invalid parent object");
    ESP_BROOKESIA_CHECK_NULL_RETURN(_info.name, false, "Invalid name");
    ESP_BROOKESIA_CHECK_NULL_RETURN(_info.image.resource, false, "Invalid image resource");
    ESP_BROOKESIA_CHECK_FALSE_RETURN(!checkInitialized(), false, "Initialized");

    /* Create objects */
    // Main
    main_obj = ESP_BROOKESIA_LV_OBJ(obj, parent);
    ESP_BROOKESIA_CHECK_NULL_RETURN(main_obj, false, "Create main_obj failed");
    // Icon
    icon_main_obj = ESP_BROOKESIA_LV_OBJ(obj, main_obj.get());
    ESP_BROOKESIA_CHECK_NULL_RETURN(icon_main_obj.get(), false, "Create icon_main_obj failed");
    // Image
    icon_image_obj = ESP_BROOKESIA_LV_OBJ(img, icon_main_obj.get());
    ESP_BROOKESIA_CHECK_NULL_RETURN(icon_image_obj, false, "Create icon_image_obj failed");
    // Name
    name_label = ESP_BROOKESIA_LV_OBJ(label, main_obj.get());
    ESP_BROOKESIA_CHECK_NULL_RETURN(name_label, false, "Create name_label failed");

    /* Setup objects style */
    // Main
    lv_obj_add_style(main_obj.get(), _core.getCoreHome().getCoreContainerStyle(), 0);
    lv_obj_set_flex_flow(main_obj.get(), LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(main_obj.get(), LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(main_obj.get(), LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(main_obj.get(), LV_OBJ_FLAG_EVENT_BUBBLE);
    // Icon
    lv_obj_add_style(icon_main_obj.get(), _core.getCoreHome().getCoreContainerStyle(), 0);
    lv_obj_clear_flag(icon_main_obj.get(), LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(icon_main_obj.get(), LV_OBJ_FLAG_EVENT_BUBBLE);
    // Image
    lv_obj_add_style(icon_image_obj.get(), _core.getCoreHome().getCoreContainerStyle(), 0);
    lv_obj_center(icon_image_obj.get());
    lv_img_set_src(icon_image_obj.get(), _info.image.resource);
    lv_obj_set_style_img_recolor(icon_image_obj.get(), lv_color_hex(_info.image.recolor.color), 0);
    lv_obj_set_style_img_recolor_opa(icon_image_obj.get(), _info.image.recolor.opacity, 0);
    lv_obj_set_size(icon_image_obj.get(), LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_img_set_size_mode(icon_image_obj.get(), LV_IMG_SIZE_MODE_REAL);
    lv_obj_add_flag(icon_image_obj.get(), LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_clear_flag(icon_image_obj.get(), LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_add_event_cb(icon_image_obj.get(), onIconTouchEventCallback, LV_EVENT_PRESSED, this);
    lv_obj_add_event_cb(icon_image_obj.get(), onIconTouchEventCallback, LV_EVENT_PRESS_LOST, this);
    lv_obj_add_event_cb(icon_image_obj.get(), onIconTouchEventCallback, LV_EVENT_RELEASED, this);
    lv_obj_add_event_cb(icon_image_obj.get(), onIconTouchEventCallback, LV_EVENT_CLICKED, this);
    // Name
    lv_obj_add_style(name_label.get(), _core.getCoreHome().getCoreContainerStyle(), 0);
    lv_label_set_text_static(name_label.get(), _info.name);

    /* Save objects */
    _main_obj = main_obj;
    _icon_main_obj = icon_main_obj;
    _icon_image_obj = icon_image_obj;
    _name_label = name_label;

    /* Update style */
    ESP_BROOKESIA_CHECK_FALSE_GOTO(updateByNewData(), err, "Update object style failed");

    return true;

err:
    ESP_BROOKESIA_CHECK_FALSE_RETURN(del(), false, "Delete icon failed");

    return false;
}

bool ESP_Brookesia_AppLauncherIcon::del(void)
{
    ESP_BROOKESIA_LOGD("Delete(%d: @0x%p)", _info.id, this);

    if (!checkInitialized()) {
        return true;
    }

    _main_obj.reset();
    _icon_main_obj.reset();
    _icon_image_obj.reset();
    _name_label.reset();

    return true;
}

bool ESP_Brookesia_AppLauncherIcon::toggleClickable(bool clickable)
{
    ESP_BROOKESIA_LOGD("Toggle clickable(%d: @0x%p)", _info.id, this);
    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkInitialized(), false, "Icon is not initialized");

    if (clickable) {
        lv_obj_add_flag(_icon_image_obj.get(), LV_OBJ_FLAG_CLICKABLE);
    } else {
        lv_obj_clear_flag(_icon_image_obj.get(), LV_OBJ_FLAG_CLICKABLE);
    }
    _flags.is_click_disable = !clickable;

    return true;
}

bool ESP_Brookesia_AppLauncherIcon::updateByNewData(void)
{
    float h_factor = 0;
    float w_factor = 0;

    ESP_BROOKESIA_LOGD("Update(%d: @0x%p)", _info.id, this);
    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkInitialized(), false, "Icon is not initialized");

    // Main
    lv_obj_set_size(_main_obj.get(), _data.main.size.width, _data.main.size.height);
    lv_obj_set_style_pad_row(_main_obj.get(), _data.main.layout_row_pad, 0);
    // Icon
    lv_obj_set_size(_icon_main_obj.get(), _data.image.default_size.width, _data.image.default_size.height);
    // Label
    lv_obj_set_style_text_font(_name_label.get(), (lv_font_t *)_data.label.text_font.font_resource, 0);
    lv_obj_set_style_text_color(_name_label.get(), lv_color_hex(_data.label.text_color.color), 0);
    lv_obj_set_style_text_opa(_name_label.get(), _data.label.text_color.opacity, 0);
    // Image
    // Calculate the multiple of the size between the target and the image.
    h_factor = (float)(_data.image.default_size.width) / ((lv_img_dsc_t *)_info.image.resource)->header.h;
    w_factor = (float)(_data.image.default_size.height) / ((lv_img_dsc_t *)_info.image.resource)->header.w;
    // Scale the image to a suitable size.
    // So you don’t have to consider the size of the source image.
    if (h_factor < w_factor) {
        _image_default_zoom = (int)(h_factor * LV_IMG_ZOOM_NONE);
        lv_img_set_zoom(_icon_image_obj.get(), _image_default_zoom);
    } else {
        _image_default_zoom = (int)(w_factor * LV_IMG_ZOOM_NONE);
        lv_img_set_zoom(_icon_image_obj.get(), _image_default_zoom);
    }
    lv_obj_refr_size(_icon_image_obj.get());
    // Calculate the multiple of the size between the target and the image.
    h_factor = (float)(_data.image.press_size.width) / ((lv_img_dsc_t *)_info.image.resource)->header.h;
    w_factor = (float)(_data.image.press_size.height) / ((lv_img_dsc_t *)_info.image.resource)->header.w;
    // Scale the image to a suitable size.
    // So you don’t have to consider the size of the source image.
    if (h_factor < w_factor) {
        _image_press_zoom = (int)(h_factor * LV_IMG_ZOOM_NONE);
    } else {
        _image_press_zoom = (int)(w_factor * LV_IMG_ZOOM_NONE);
    }

    return true;
}

void ESP_Brookesia_AppLauncherIcon::onIconTouchEventCallback(lv_event_t *event)
{
    ESP_Brookesia_AppLauncherIcon *icon = nullptr;
    lv_event_code_t event_code = _LV_EVENT_LAST;
    lv_obj_t *icon_image_obj = nullptr;
    ESP_Brookesia_CoreAppEventData_t app_event_data = {
        .type = ESP_BROOKESIA_CORE_APP_EVENT_TYPE_START,
    };

    ESP_BROOKESIA_LOGD("Icon touch event callback");
    ESP_BROOKESIA_CHECK_NULL_EXIT(event, "Invalid event object");

    icon = (ESP_Brookesia_AppLauncherIcon *)lv_event_get_user_data(event);
    event_code = lv_event_get_code(event);
    icon_image_obj = (lv_obj_t *)lv_event_get_current_target(event);
    ESP_BROOKESIA_CHECK_NULL_EXIT(icon, "Invalid icon");
    ESP_BROOKESIA_CHECK_NULL_EXIT(icon_image_obj, "Invalid icon image");

    switch (event_code) {
    case LV_EVENT_CLICKED:
        ESP_BROOKESIA_LOGD("Clicked");
        if (icon->_flags.is_pressed_losted || icon->_flags.is_click_disable) {
            break;
        }
        app_event_data.id = icon->_info.id;
        ESP_BROOKESIA_CHECK_FALSE_EXIT(icon->_core.sendAppEvent(&app_event_data), "Send app event failed");
        break;
    case LV_EVENT_PRESSED:
        ESP_BROOKESIA_LOGD("Pressed");
        if (icon->_flags.is_click_disable) {
            break;
        }
        // Zoom out icon
        lv_img_set_zoom(icon_image_obj, icon->_image_press_zoom);
        lv_obj_refr_size(icon_image_obj);
        icon->_flags.is_pressed_losted = false;
        break;
    case LV_EVENT_PRESS_LOST:
        ESP_BROOKESIA_LOGD("Press lost");
        icon->_flags.is_pressed_losted = true;
        [[fallthrough]];
    case LV_EVENT_RELEASED:
        ESP_BROOKESIA_LOGD("Released");
        // Zoom in icon
        lv_img_set_zoom(icon_image_obj, icon->_image_default_zoom);
        lv_obj_refr_size(icon_image_obj);
        break;
    default:
        ESP_BROOKESIA_CHECK_FALSE_EXIT(false, "Invalid event code(%d)", event_code);
        break;
    }
}
