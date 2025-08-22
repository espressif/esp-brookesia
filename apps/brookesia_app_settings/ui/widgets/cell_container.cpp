/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "private/esp_brookesia_app_settings_utils.hpp"
#include <src/widgets/label/lv_label.h>
#include <src/widgets/textarea/lv_textarea.h>
#include "cell_container.hpp"
#ifdef ESP_UTILS_LOGD
#   undef ESP_UTILS_LOGD
#endif
#define ESP_UTILS_LOGD(...)

#define DOULE_LABEL_MAIN_HEIHGH_FACTOR  (1.5)

using namespace std;
using namespace esp_brookesia::systems::speaker;

namespace esp_brookesia::apps {

SettingsUI_WidgetCell::SettingsUI_WidgetCell(App &ui_app, const SettingsUI_WidgetCellData &cell_data,
        SettingsUI_WidgetCellElement elements):
    data(cell_data),
    _flags{},
    _core_app(ui_app),
    _click_event_code(systems::base::Event::ID::CUSTOM),
    _split_line_points{},
    _elements_conf{},
    _elements(elements)
{
}

SettingsUI_WidgetCell::~SettingsUI_WidgetCell()
{
    ESP_UTILS_LOGD("Destroy(0x%p)", this);

    ESP_UTILS_CHECK_FALSE_EXIT(del(), "Delete failed");
}

bool SettingsUI_WidgetCell::begin(lv_obj_t *parent)
{
    ESP_UTILS_LOGD("Begin(0x%p)", this);
    ESP_UTILS_CHECK_FALSE_RETURN((parent != nullptr) && lv_obj_is_valid(parent), false, "Invalid parent object");
    ESP_UTILS_CHECK_FALSE_RETURN(!checkInitialized(), false, "Already initialized");

    ESP_Brookesia_LvObj_t main_object = nullptr;
    ESP_Brookesia_LvObj_t left_area_object = nullptr;
    ESP_Brookesia_LvObj_t left_icon_object = nullptr;
    ESP_Brookesia_LvObj_t left_icon_image = nullptr;
    ESP_Brookesia_LvObj_t left_label_object = nullptr;
    ESP_Brookesia_LvObj_t left_main_label = nullptr;
    ESP_Brookesia_LvObj_t left_minor_label = nullptr;
    ESP_Brookesia_LvObj_t left_text_edit_object = nullptr;
    ESP_Brookesia_LvObj_t right_area_object = nullptr;
    ESP_Brookesia_LvObj_t right_switch = nullptr;
    ESP_Brookesia_LvObj_t right_icons_object = nullptr;
    ESP_Brookesia_LvObj_t right_label_object = nullptr;
    ESP_Brookesia_LvObj_t right_main_label = nullptr;
    ESP_Brookesia_LvObj_t right_minor_label = nullptr;
    ESP_Brookesia_LvObj_t split_line = nullptr;
    ESP_Brookesia_LvObj_t center_area_object = nullptr;
    ESP_Brookesia_LvObj_t center_slider_object = nullptr;

    // Main
    main_object = ESP_BROOKESIA_LV_OBJ(obj, parent);
    ESP_UTILS_CHECK_NULL_RETURN(main_object, false, "Create main object failed");
    // Left: Area
    if (_elements & SettingsUI_WidgetCellElement::_LEFT_AREA) {
        left_area_object = ESP_BROOKESIA_LV_OBJ(obj, main_object.get());
        ESP_UTILS_CHECK_NULL_RETURN(left_area_object, false, "Create left are object failed");
    }
    // Left: Icon
    if (_elements & SettingsUI_WidgetCellElement::LEFT_ICON) {
        left_icon_object = ESP_BROOKESIA_LV_OBJ(obj, left_area_object.get());
        ESP_UTILS_CHECK_NULL_RETURN(left_icon_object, false, "Create left icon object failed");
        left_icon_image = ESP_BROOKESIA_LV_OBJ(img, left_icon_object.get());
        ESP_UTILS_CHECK_NULL_RETURN(left_icon_image, false, "Create left icon image failed");
    }
    // Left: Label Object
    if (_elements & SettingsUI_WidgetCellElement::_LEFT_LABEL) {
        left_label_object = ESP_BROOKESIA_LV_OBJ(obj, left_area_object.get());
        ESP_UTILS_CHECK_NULL_RETURN(left_label_object, false, "Create left label object failed");
    }
    // Left: Main Label
    if (_elements & SettingsUI_WidgetCellElement::LEFT_MAIN_LABEL) {
        left_main_label = ESP_BROOKESIA_LV_OBJ(label, left_label_object.get());
        ESP_UTILS_CHECK_NULL_RETURN(left_main_label, false, "Create left main label failed");
    }
    // Left: Minor Label
    if (_elements & SettingsUI_WidgetCellElement::LEFT_MINOR_LABEL) {
        left_minor_label = ESP_BROOKESIA_LV_OBJ(label, left_label_object.get());
        ESP_UTILS_CHECK_NULL_RETURN(left_minor_label, false, "Create left minor label failed");
    }
    // Left: Text Edit
    if (_elements & SettingsUI_WidgetCellElement::LEFT_TEXT_EDIT) {
        left_text_edit_object = ESP_BROOKESIA_LV_OBJ(textarea, left_area_object.get());
        ESP_UTILS_CHECK_NULL_RETURN(left_text_edit_object, false, "Create left text edit failed");
    }
    // Center: Area
    if (_elements & SettingsUI_WidgetCellElement::_CENTER_AREA) {
        center_area_object = ESP_BROOKESIA_LV_OBJ(obj, main_object.get());
        ESP_UTILS_CHECK_NULL_RETURN(center_area_object, false, "Create center area object failed");
    }
    // Center: Slider
    if (_elements & SettingsUI_WidgetCellElement::CENTER_SLIDER) {
        center_slider_object = ESP_BROOKESIA_LV_OBJ(slider, center_area_object.get());
        ESP_UTILS_CHECK_NULL_RETURN(center_slider_object, false, "Create center slider failed");
    }
    // Right: Area
    if (_elements & SettingsUI_WidgetCellElement::_RIGHT_AREA) {
        right_area_object = ESP_BROOKESIA_LV_OBJ(obj, main_object.get());
        ESP_UTILS_CHECK_NULL_RETURN(right_area_object, false, "Create right area object failed");
    }
    // Right: Switch
    if (_elements & SettingsUI_WidgetCellElement::RIGHT_SWITCH) {
        right_switch = ESP_BROOKESIA_LV_OBJ(switch, right_area_object.get());
        ESP_UTILS_CHECK_NULL_RETURN(right_switch, false, "Create right switch failed");
    }
    // Right: Icons
    if (_elements & SettingsUI_WidgetCellElement::RIGHT_ICONS) {
        right_icons_object = ESP_BROOKESIA_LV_OBJ(obj, right_area_object.get());
        ESP_UTILS_CHECK_NULL_RETURN(right_icons_object, false, "Create right icons object failed");
    }
    // Right: Label Object
    if (_elements & SettingsUI_WidgetCellElement::_RIGHT_LABEL) {
        right_label_object = ESP_BROOKESIA_LV_OBJ(obj, right_area_object.get());
        ESP_UTILS_CHECK_NULL_RETURN(right_label_object, false, "Create right label object failed");
    }
    // Right: Main Label
    if (_elements & SettingsUI_WidgetCellElement::RIGHT_MAIN_LABEL) {
        right_main_label = ESP_BROOKESIA_LV_OBJ(label, right_label_object.get());
        ESP_UTILS_CHECK_NULL_RETURN(right_main_label, false, "Create right main label failed");
    }
    // Right: Minor Label
    if (_elements & SettingsUI_WidgetCellElement::RIGHT_MINOR_LABEL) {
        right_minor_label = ESP_BROOKESIA_LV_OBJ(label, right_label_object.get());
        ESP_UTILS_CHECK_NULL_RETURN(right_minor_label, false, "Create right minor label failed");
    }
    // Bottom Line
    split_line = ESP_BROOKESIA_LV_OBJ(line, main_object.get());
    ESP_UTILS_CHECK_NULL_RETURN(split_line, false, "Create split line failed");

    systems::base::Display &core_home = _core_app.getSystemContext()->getDisplay();
    // Main
    lv_obj_add_style(main_object.get(), core_home.getCoreContainerStyle(), 0);
    lv_obj_remove_flag(main_object.get(), LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_remove_flag(main_object.get(), LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(main_object.get(), onCellTouchEventCallback, LV_EVENT_PRESSED, this);
    lv_obj_add_event_cb(main_object.get(), onCellTouchEventCallback, LV_EVENT_PRESS_LOST, this);
    lv_obj_add_event_cb(main_object.get(), onCellTouchEventCallback, LV_EVENT_RELEASED, this);
    lv_obj_add_event_cb(main_object.get(), onCellTouchEventCallback, LV_EVENT_CLICKED, this);
    _elements_map[SettingsUI_WidgetCellElement::MAIN] = main_object;
    // Left: Area
    if (left_area_object != nullptr) {
        lv_obj_add_style(left_area_object.get(), core_home.getCoreContainerStyle(), 0);
        lv_obj_set_flex_flow(left_area_object.get(), LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(left_area_object.get(), LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_remove_flag(left_area_object.get(), LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
        _elements_map[SettingsUI_WidgetCellElement::_LEFT_AREA] = left_area_object;
    }
    // Left: Icon
    if (left_icon_object != nullptr) {
        lv_obj_add_style(left_icon_object.get(), core_home.getCoreContainerStyle(), 0);
        lv_obj_remove_flag(left_icon_object.get(), LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
        _left_icon_object = left_icon_object;
    }
    if (left_icon_image != nullptr) {
        lv_obj_add_style(left_icon_image.get(), core_home.getCoreContainerStyle(), 0);
        lv_obj_center(left_icon_image.get());
        lv_obj_remove_flag(left_icon_image.get(), LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
        _elements_map[SettingsUI_WidgetCellElement::LEFT_ICON] = left_icon_image;
    }
    // Left: Label Object
    if (left_label_object != nullptr) {
        lv_obj_add_style(left_label_object.get(), core_home.getCoreContainerStyle(), 0);
        lv_obj_set_flex_flow(left_label_object.get(), LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(left_label_object.get(), LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
        lv_obj_remove_flag(left_label_object.get(), LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
        _elements_map[SettingsUI_WidgetCellElement::_LEFT_LABEL] = left_label_object;
    }
    // Left: Main Label
    if (left_main_label != nullptr) {
        lv_obj_add_style(left_main_label.get(), core_home.getCoreContainerStyle(), 0);
        lv_obj_remove_flag(left_main_label.get(), LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
        _elements_map[SettingsUI_WidgetCellElement::LEFT_MAIN_LABEL] = left_main_label;
    }
    // Left: Minor Label
    if (left_minor_label != nullptr) {
        lv_obj_add_style(left_minor_label.get(), core_home.getCoreContainerStyle(), 0);
        lv_obj_remove_flag(left_minor_label.get(), LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
        _elements_map[SettingsUI_WidgetCellElement::LEFT_MINOR_LABEL] = left_minor_label;
    }
    // Left: Text Edit
    if (left_text_edit_object != nullptr) {
        lv_obj_add_style(left_text_edit_object.get(), core_home.getCoreContainerStyle(), 0);
        lv_obj_remove_flag(left_text_edit_object.get(), LV_OBJ_FLAG_SCROLLABLE);
        _elements_map[SettingsUI_WidgetCellElement::LEFT_TEXT_EDIT] = left_text_edit_object;
    }
    // Center: Area
    if (center_area_object != nullptr) {
        lv_obj_center(center_area_object.get());
        lv_obj_add_style(center_area_object.get(), core_home.getCoreContainerStyle(), 0);
        lv_obj_set_flex_flow(center_area_object.get(), LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(center_area_object.get(), LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_remove_flag(center_area_object.get(), LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
        _elements_map[SettingsUI_WidgetCellElement::_CENTER_AREA] = center_area_object;
    }
    if (center_slider_object != nullptr) {
        // lv_obj_add_style(center_slider_object.get(), core_home.getCoreContainerStyle(), 0);
        _elements_map[SettingsUI_WidgetCellElement::CENTER_SLIDER] = center_slider_object;
    }
    // Right: Area
    if (right_area_object != nullptr) {
        lv_obj_add_style(right_area_object.get(), core_home.getCoreContainerStyle(), 0);
        lv_obj_set_flex_flow(right_area_object.get(), LV_FLEX_FLOW_ROW_REVERSE);
        lv_obj_set_flex_align(right_area_object.get(), LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_remove_flag(right_area_object.get(), LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
        _elements_map[SettingsUI_WidgetCellElement::_RIGHT_AREA] = right_area_object;
    }
    // Right: Switch
    if (right_switch != nullptr) {
        // lv_obj_add_style(right_switch.get(), core_home.getCoreContainerStyle(), 0);
        _elements_map[SettingsUI_WidgetCellElement::RIGHT_SWITCH] = right_switch;
    }
    // Right: Icons
    if (right_icons_object != nullptr) {
        lv_obj_add_style(right_icons_object.get(), core_home.getCoreContainerStyle(), 0);
        lv_obj_set_flex_flow(right_icons_object.get(), LV_FLEX_FLOW_ROW_REVERSE);
        lv_obj_set_flex_align(right_icons_object.get(), LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_remove_flag(right_icons_object.get(), LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
        _elements_map[SettingsUI_WidgetCellElement::RIGHT_ICONS] = right_icons_object;
    }
    // Right: Label Object
    if (right_label_object != nullptr) {
        lv_obj_add_style(right_label_object.get(), core_home.getCoreContainerStyle(), 0);
        lv_obj_set_flex_flow(right_label_object.get(), LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(right_label_object.get(), LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_remove_flag(right_label_object.get(), LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
        _elements_map[SettingsUI_WidgetCellElement::_RIGHT_LABEL] = right_label_object;
    }
    // Right: Main Label
    if (right_main_label != nullptr) {
        lv_obj_add_style(right_main_label.get(), core_home.getCoreContainerStyle(), 0);
        lv_obj_remove_flag(right_main_label.get(), LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
        _elements_map[SettingsUI_WidgetCellElement::RIGHT_MAIN_LABEL] = right_main_label;
    }
    // Right: Minor Label
    if (right_minor_label != nullptr) {
        lv_obj_add_style(right_minor_label.get(), core_home.getCoreContainerStyle(), 0);
        lv_obj_remove_flag(right_minor_label.get(), LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
        _elements_map[SettingsUI_WidgetCellElement::RIGHT_MINOR_LABEL] = right_minor_label;
    }
    // Split Line
    lv_obj_align(split_line.get(), LV_ALIGN_BOTTOM_LEFT, 0, 0);
    _split_line = split_line;
    // Event
    _click_event_code = _core_app.getSystemContext()->getEvent().getFreeEventID();

    ESP_UTILS_CHECK_FALSE_GOTO(processDataUpdate(), err, "Process data update failed");

    return true;

err:
    ESP_UTILS_CHECK_FALSE_RETURN(del(), false, "Delete failed");

    return false;
}

bool SettingsUI_WidgetCell::del()
{
    ESP_UTILS_LOGD("Delete(0x%p)", this);
    if (!checkInitialized()) {
        return true;
    }

    _elements_map.clear();
    _left_icon_object.reset();
    _core_app.getSystemContext()->getEvent().unregisterEvent(_click_event_code);

    return true;
}

bool SettingsUI_WidgetCell::setSplitLineVisible(bool visible)
{
    ESP_UTILS_LOGD("Set split line visible(%d)", visible);
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    if (visible) {
        lv_obj_remove_flag(_split_line.get(), LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(_split_line.get(), LV_OBJ_FLAG_HIDDEN);
    }

    return true;
}

bool SettingsUI_WidgetCell::processDataUpdate()
{
    ESP_UTILS_LOGD("Process data update");
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    // Main
    lv_obj_t *main_object = getElementObject(SettingsUI_WidgetCellElement::MAIN);
    lv_obj_set_style_radius(main_object, data.main.radius, 0);
    lv_obj_set_size(main_object, data.main.size.width, data.main.size.height);
    lv_obj_set_style_bg_color(main_object, lv_color_hex(data.main.inactive_background_color.color), 0);
    lv_obj_set_style_bg_opa(main_object, data.main.inactive_background_color.opacity, 0);
    // Left: Area
    lv_obj_t *left_area_object = getElementObject(SettingsUI_WidgetCellElement::_LEFT_AREA);
    if (left_area_object != nullptr) {
        lv_obj_align(left_area_object, LV_ALIGN_LEFT_MID, data.area.left_align_x_offset, 0);
        lv_obj_set_style_pad_column(left_area_object, data.area.left_column_pad, 0);
    }
    // Left: Icon
    if (_left_icon_object != nullptr) {
        lv_obj_set_size(_left_icon_object.get(), data.icon.left_size.width, data.icon.left_size.height);
    }
    // Left: Label
    lv_obj_t *left_label_object = getElementObject(SettingsUI_WidgetCellElement::_LEFT_LABEL);
    if (left_label_object != nullptr) {
        lv_obj_set_style_pad_row(left_label_object, data.label.left_row_pad, 0);
    }
    // Left: Main Label
    lv_obj_t *left_main_label = getElementObject(SettingsUI_WidgetCellElement::LEFT_MAIN_LABEL);
    if (left_main_label != nullptr) {
        lv_obj_set_style_text_font(left_main_label, (const lv_font_t *)data.label.left_main_text_font.font_resource, 0);
        lv_obj_set_style_text_color(left_main_label, lv_color_hex(data.label.left_main_text_color.color), 0);
        lv_obj_set_style_text_opa(left_main_label, data.label.left_main_text_color.opacity, 0);
    }
    // Left: Minor Label
    lv_obj_t *left_minor_label = getElementObject(SettingsUI_WidgetCellElement::LEFT_MINOR_LABEL);
    if (left_minor_label != nullptr) {
        lv_obj_set_style_text_font(left_minor_label, (const lv_font_t *)data.label.left_minor_text_font.font_resource, 0);
        lv_obj_set_style_text_color(left_minor_label, lv_color_hex(data.label.left_minor_text_color.color), 0);
        lv_obj_set_style_text_opa(left_minor_label, data.label.left_minor_text_color.opacity, 0);
    }
    // Left: Text Edit
    lv_obj_t *left_text_edit = getElementObject(SettingsUI_WidgetCellElement::LEFT_TEXT_EDIT);
    if (left_text_edit != nullptr) {
        lv_obj_set_size(left_text_edit, data.text_edit.size.width, data.text_edit.size.height);
        lv_obj_set_style_text_font(left_text_edit, (const lv_font_t *)data.text_edit.text_font.font_resource, 0);
        lv_obj_set_style_text_color(left_text_edit, lv_color_hex(data.text_edit.text_color.color), 0);
        lv_obj_set_style_text_opa(left_text_edit, data.text_edit.text_color.opacity, 0);
        lv_obj_set_style_border_color(
            left_text_edit, lv_color_hex(data.text_edit.cursor_color.color), LV_PART_CURSOR | LV_STATE_FOCUSED
        );
        lv_obj_set_style_border_opa(
            left_text_edit, data.text_edit.cursor_color.opacity, LV_PART_CURSOR | LV_STATE_FOCUSED
        );
        lv_coord_t text_height = lv_font_get_line_height((const lv_font_t *)data.text_edit.text_font.font_resource);
        lv_coord_t padding_top = (data.text_edit.size.height - text_height) / 2;
        lv_obj_set_style_pad_top(left_text_edit, padding_top, 0);
        lv_obj_set_style_pad_bottom(left_text_edit, padding_top, 0);
    }
    // Center: Slider
    lv_obj_t *center_slider_object = getElementObject(SettingsUI_WidgetCellElement::CENTER_SLIDER);
    if (center_slider_object != nullptr) {
        lv_obj_set_style_width(center_slider_object, data.slider.main_size.width, LV_PART_MAIN);
        lv_obj_set_style_height(center_slider_object, data.slider.main_size.height, LV_PART_MAIN);
        lv_obj_set_style_bg_color(center_slider_object, lv_color_hex(data.slider.main_color.color), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(center_slider_object, data.slider.main_color.opacity, LV_PART_MAIN);
        lv_obj_set_style_radius(center_slider_object, 0, LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(center_slider_object, lv_color_hex(data.slider.indicator_color.color), LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(center_slider_object, data.slider.indicator_color.opacity, LV_PART_INDICATOR);
        lv_obj_set_style_pad_hor(center_slider_object, data.slider.knob_size.width, LV_PART_KNOB);
        lv_obj_set_style_pad_ver(center_slider_object, data.slider.knob_size.height, LV_PART_KNOB);
        lv_obj_set_style_bg_color(center_slider_object, lv_color_hex(data.slider.knob_color.color), LV_PART_KNOB);
        lv_obj_set_style_bg_opa(center_slider_object, data.slider.knob_color.opacity, LV_PART_KNOB);
    }
    // Right: Area
    lv_obj_t *right_area_object = getElementObject(SettingsUI_WidgetCellElement::_RIGHT_AREA);
    if (right_area_object != nullptr) {
        lv_obj_align(right_area_object, LV_ALIGN_RIGHT_MID, -data.area.right_align_x_offset, 0);
        lv_obj_set_style_pad_column(right_area_object, data.area.right_column_pad, 0);
    }
    // Right: Switch
    lv_obj_t *right_switch_object = getElementObject(SettingsUI_WidgetCellElement::RIGHT_SWITCH);
    if (right_switch_object != nullptr) {
        lv_obj_set_style_width(right_switch_object, data.sw.main_size.width, LV_PART_MAIN);
        lv_obj_set_style_height(right_switch_object, data.sw.main_size.height, LV_PART_MAIN);
        lv_obj_set_style_bg_color(right_switch_object, lv_color_hex(data.sw.inactive_indicator_color.color), LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(right_switch_object, data.sw.inactive_indicator_color.opacity, LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(right_switch_object, lv_color_hex(data.sw.active_indicator_color.color), LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_set_style_bg_opa(right_switch_object, data.sw.active_indicator_color.opacity, LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_set_style_width(right_switch_object, data.sw.knob_size.width, LV_PART_KNOB);
        lv_obj_set_style_height(right_switch_object, data.sw.knob_size.height, LV_PART_KNOB);
        lv_obj_set_style_bg_color(right_switch_object, lv_color_hex(data.sw.knob_color.color), LV_PART_KNOB);
        lv_obj_set_style_bg_opa(right_switch_object, data.sw.knob_color.opacity, LV_PART_KNOB);
    }
    // Right: Icon
    lv_obj_t *right_icons_object = getElementObject(SettingsUI_WidgetCellElement::RIGHT_ICONS);
    if (right_icons_object != nullptr) {
        lv_obj_set_size(right_icons_object, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    }
    // Right: Label
    lv_obj_t *right_label_object = getElementObject(SettingsUI_WidgetCellElement::_RIGHT_LABEL);
    if (right_label_object != nullptr) {
        lv_obj_set_style_pad_row(right_label_object, data.label.right_row_pad, 0);
    }
    // Right: Main Label
    lv_obj_t *right_main_label = getElementObject(SettingsUI_WidgetCellElement::RIGHT_MAIN_LABEL);
    if (right_main_label != nullptr) {
        lv_obj_set_style_text_font(right_main_label, (const lv_font_t *)data.label.right_main_text_font.font_resource, 0);
        lv_obj_set_style_text_color(right_main_label, lv_color_hex(data.label.right_main_text_color.color), 0);
        lv_obj_set_style_text_opa(right_main_label, data.label.right_main_text_color.opacity, 0);
    }
    // Right: Minor Label
    lv_obj_t *right_minor_label = getElementObject(SettingsUI_WidgetCellElement::RIGHT_MINOR_LABEL);
    if (right_minor_label != nullptr) {
        lv_obj_set_style_text_font(right_minor_label, (const lv_font_t *)data.label.right_minor_text_font.font_resource, 0);
        lv_obj_set_style_text_color(right_minor_label, lv_color_hex(data.label.right_minor_text_color.color), 0);
        lv_obj_set_style_text_opa(right_minor_label, data.label.right_minor_text_color.opacity, 0);
    }
    // Split Line
    lv_obj_update_layout(main_object);
    lv_obj_refr_pos(main_object);
    _split_line_points[0].x = data.area.left_align_x_offset;
    _split_line_points[1].x = data.main.size.width - data.area.right_align_x_offset;
    if (_left_icon_object != nullptr) {
        _split_line_points[0].x += data.icon.left_size.width + data.area.left_column_pad;
    }
    lv_line_set_points(_split_line.get(), _split_line_points.data(), _split_line_points.size());
    lv_obj_set_style_line_width(_split_line.get(), data.split_line.width, 0);
    lv_obj_set_style_line_color(_split_line.get(), lv_color_hex(data.split_line.color.color), 0);
    lv_obj_set_style_line_opa(_split_line.get(), data.split_line.color.opacity, 0);

    ESP_UTILS_CHECK_FALSE_RETURN(updateConf(_elements_conf), false, "Update conf failed");

    return true;
}

bool SettingsUI_WidgetCell::updateConf(const SettingsUI_WidgetCellConf &conf)
{
    ESP_UTILS_LOGD("Update conf");
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    if (conf.flags.enable_left_icon) {
        ESP_UTILS_CHECK_FALSE_RETURN(
            updateLeftIcon(conf.left_icon_size, conf.left_icon_image), false, "Update left icon failed"
        );
    }
    if (conf.flags.enable_left_main_label) {
        ESP_UTILS_CHECK_FALSE_RETURN(updateLeftMainLabel(conf.left_main_label_text), false,
                                     "Update left main label text failed");
    }
    if (conf.flags.enable_left_minor_label) {
        ESP_UTILS_CHECK_FALSE_RETURN(updateLeftMinorLabel(conf.left_minor_label_text), false,
                                     "Update left minor label text failed");
    }
    if (conf.flags.enable_left_text_edit_placeholder) {
        ESP_UTILS_CHECK_FALSE_RETURN(updateLeftTextEditPlaceholder(conf.left_text_edit_placeholder_text), false,
                                     "Update left text edit placeholder text failed");
    }
    if (conf.flags.enable_right_main_label) {
        ESP_UTILS_CHECK_FALSE_RETURN(updateRightMainLabel(conf.right_main_label_text), false,
                                     "Update right main label text failed");
    }
    if (conf.flags.enable_right_minor_label) {
        ESP_UTILS_CHECK_FALSE_RETURN(updateRightMinorLabel(conf.right_minor_label_text), false,
                                     "Update right minor label text failed");
    }
    if (conf.flags.enable_right_icons) {
        ESP_UTILS_CHECK_FALSE_RETURN(
            updateRightIcons(conf.right_icon_size, conf.right_icon_images), false, "Update right icon images failed"
        );
    }
    lv_obj_t *main_object = getElementObject(SettingsUI_WidgetCellElement::MAIN);
    ESP_UTILS_CHECK_NULL_RETURN(main_object, false, "Invalid main object");
    if (conf.flags.enable_clickable) {
        lv_obj_add_flag(main_object, LV_OBJ_FLAG_CLICKABLE);
    } else {
        lv_obj_remove_flag(main_object, LV_OBJ_FLAG_CLICKABLE);
    }

    if ((conf.flags.enable_left_main_label + conf.flags.enable_left_minor_label > 1) ||
            (conf.flags.enable_right_main_label + conf.flags.enable_right_minor_label > 1)) {
        lv_obj_set_height(main_object, data.main.size.height * DOULE_LABEL_MAIN_HEIHGH_FACTOR);
    }

    _elements_conf = conf;

    return true;
}

bool SettingsUI_WidgetCell::updateLeftIcon(const gui::StyleSize &size, const gui::StyleImage &image)
{
    ESP_UTILS_LOGD("Update left icon");
    ESP_UTILS_CHECK_FALSE_RETURN(_elements & SettingsUI_WidgetCellElement::LEFT_ICON, false, "Left icon not enabled");

    gui::StyleSize calibrate_size = size;
    _core_app.getSystemContext()->getDisplay().calibrateCoreObjectSize(data.main.size, calibrate_size, true);
    bool size_changed = ((calibrate_size.width != 0) && (calibrate_size.height != 0));

    lv_obj_t *left_icon_image = getElementObject(SettingsUI_WidgetCellElement::LEFT_ICON);
    ESP_UTILS_CHECK_NULL_RETURN(left_icon_image, false, "Invalid left icon image");

    ESP_UTILS_CHECK_FALSE_RETURN(
        updateIconImage(left_icon_image, image, size_changed ? calibrate_size : data.icon.left_size),
        false, "Update left icon image failed"
    );

    if (size_changed) {
        lv_obj_t *left_icon_object = lv_obj_get_parent(left_icon_image);
        ESP_UTILS_CHECK_NULL_RETURN(left_icon_object, false, "Invalid left icon object");
        lv_obj_set_size(left_icon_object, calibrate_size.width, calibrate_size.height);
    }

    return true;
}

bool SettingsUI_WidgetCell::updateLeftMainLabel(std::string text)
{
    ESP_UTILS_LOGD("Update left main label");
    ESP_UTILS_CHECK_FALSE_RETURN(_elements & SettingsUI_WidgetCellElement::LEFT_MAIN_LABEL, false,
                                 "Left main label not enabled");

    lv_obj_t *left_main_label = getElementObject(SettingsUI_WidgetCellElement::LEFT_MAIN_LABEL);
    ESP_UTILS_CHECK_NULL_RETURN(left_main_label, false, "Invalid left main label");

    if (text.empty()) {
        lv_obj_add_flag(left_main_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(left_main_label, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(left_main_label, text.c_str());
    }

    return true;
}

bool SettingsUI_WidgetCell::updateLeftMinorLabel(std::string text)
{
    ESP_UTILS_LOGD("Update left minor label");
    ESP_UTILS_CHECK_FALSE_RETURN(_elements & SettingsUI_WidgetCellElement::LEFT_MINOR_LABEL, false,
                                 "Left minor label not enabled");

    lv_obj_t *left_minor_label = getElementObject(SettingsUI_WidgetCellElement::LEFT_MINOR_LABEL);
    ESP_UTILS_CHECK_NULL_RETURN(left_minor_label, false, "Invalid left minor label");

    if (text.empty()) {
        lv_obj_add_flag(left_minor_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(left_minor_label, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(left_minor_label, text.c_str());
    }

    return true;
}

bool SettingsUI_WidgetCell::updateLeftTextEditPlaceholder(std::string text)
{
    ESP_UTILS_LOGD("Update left text edit placeholder");
    ESP_UTILS_CHECK_FALSE_RETURN(_elements & SettingsUI_WidgetCellElement::LEFT_TEXT_EDIT, false, "Left text edit not enabled");

    lv_obj_t *left_text_edit = getElementObject(SettingsUI_WidgetCellElement::LEFT_TEXT_EDIT);
    ESP_UTILS_CHECK_NULL_RETURN(left_text_edit, false, "Invalid left text edit");

    lv_textarea_set_placeholder_text(left_text_edit, text.c_str());

    return true;
}

bool SettingsUI_WidgetCell::updateRightMainLabel(std::string text)
{
    ESP_UTILS_LOGD("Update right main label");
    ESP_UTILS_CHECK_FALSE_RETURN(_elements & SettingsUI_WidgetCellElement::RIGHT_MAIN_LABEL, false,
                                 "right main label not enabled");

    lv_obj_t *right_main_label = getElementObject(SettingsUI_WidgetCellElement::RIGHT_MAIN_LABEL);
    ESP_UTILS_CHECK_NULL_RETURN(right_main_label, false, "Invalid right main label");

    if (text.empty()) {
        lv_obj_add_flag(right_main_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(right_main_label, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(right_main_label, text.c_str());
    }

    return true;
}

bool SettingsUI_WidgetCell::updateRightMinorLabel(std::string text)
{
    ESP_UTILS_LOGD("Update right minor label");
    ESP_UTILS_CHECK_FALSE_RETURN(_elements & SettingsUI_WidgetCellElement::RIGHT_MINOR_LABEL, false,
                                 "right minor label not enabled");

    lv_obj_t *right_minor_label = getElementObject(SettingsUI_WidgetCellElement::RIGHT_MINOR_LABEL);
    ESP_UTILS_CHECK_NULL_RETURN(right_minor_label, false, "Invalid right minor label");

    if (text.empty()) {
        lv_obj_add_flag(right_minor_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(right_minor_label, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(right_minor_label, text.c_str());
    }

    return true;
}

bool SettingsUI_WidgetCell::updateRightIcons(
    const gui::StyleSize &size, const std::vector<gui::StyleImage> &right_icons
)
{
    ESP_UTILS_LOGD("Update right icons");
    ESP_UTILS_CHECK_FALSE_RETURN(_elements & SettingsUI_WidgetCellElement::RIGHT_ICONS, false, "Right icons not enabled");

    gui::StyleSize calibrate_size = size;
    _core_app.getSystemContext()->getDisplay().calibrateCoreObjectSize(data.main.size, calibrate_size, true);
    bool size_changed = ((calibrate_size.width != 0) && (calibrate_size.height != 0));

    lv_obj_t *right_icons_object = getElementObject(SettingsUI_WidgetCellElement::RIGHT_ICONS);
    ESP_UTILS_CHECK_NULL_RETURN(right_icons_object, false, "Invalid right icons object");

    lv_style_t *container_style = _core_app.getSystemContext()->getDisplay().getCoreContainerStyle();
    int update_count = right_icons.size();
    int current_count = _right_icon_object_images.size();
    for (int i = 0; (i < update_count) || (i < current_count); i++) {
        if (i >= current_count) {
            // Create new object if not enough
            ESP_Brookesia_LvObj_t right_icon_object = ESP_BROOKESIA_LV_OBJ(obj, right_icons_object);
            ESP_UTILS_CHECK_NULL_RETURN(right_icon_object, false, "Create right icon object failed");

            ESP_Brookesia_LvObj_t right_icon_image = ESP_BROOKESIA_LV_OBJ(img, right_icon_object.get());
            ESP_UTILS_CHECK_NULL_RETURN(right_icon_image, false, "Create right icon image failed");

            lv_obj_add_style(right_icon_object.get(), container_style, 0);
            lv_obj_remove_flag(right_icon_object.get(), LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_style(right_icon_image.get(), container_style, 0);
            lv_obj_center(right_icon_image.get());
            lv_obj_remove_flag(right_icon_image.get(), LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

            _right_icon_object_images.emplace_back(std::pair<ESP_Brookesia_LvObj_t, ESP_Brookesia_LvObj_t>(
                    std::move(right_icon_object), std::move(right_icon_image)
                                                   ));
            // Update current count after add new object
            current_count = _right_icon_object_images.size();
        } else {
            // Get existing object
            auto &[_, right_icon_image] = _right_icon_object_images[i];
            if (i >= update_count) {
                // Hide object if not enough
                lv_obj_add_flag(right_icon_image.get(), LV_OBJ_FLAG_HIDDEN);
            } else {
                // Show object if enough
                lv_obj_remove_flag(right_icon_image.get(), LV_OBJ_FLAG_HIDDEN);
            }
        }

        auto &[right_icon_object, right_icon_image] = _right_icon_object_images[i];
        lv_obj_set_size(right_icon_object.get(), data.icon.right_size.width, data.icon.right_size.height);
        if (i < update_count) {
            ESP_UTILS_CHECK_FALSE_RETURN(
                updateIconImage(
                    right_icon_image.get(), right_icons[i], size_changed ? calibrate_size : data.icon.right_size
                ), false, "Update right icon(%d) image failed", (int)i
            );
            if (size_changed) {
                lv_obj_set_size(right_icon_object.get(), calibrate_size.width, calibrate_size.height);
            }
        }
    }

    return true;
}

bool SettingsUI_WidgetCell::updateClickable(bool clickable)
{
    ESP_UTILS_LOGD("Update clickable(%d)", clickable);
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    lv_obj_t *main_object = getElementObject(SettingsUI_WidgetCellElement::MAIN);
    ESP_UTILS_CHECK_NULL_RETURN(main_object, false, "Invalid main object");

    if (clickable) {
        lv_obj_add_flag(main_object, LV_OBJ_FLAG_CLICKABLE);
    } else {
        lv_obj_remove_flag(main_object, LV_OBJ_FLAG_CLICKABLE);
    }
    _flags.is_cell_click_disable = !clickable;

    return true;
}

bool SettingsUI_WidgetCell::calibrateData(const gui::StyleSize &parent_size, const systems::base::Display &display,
        SettingsUI_WidgetCellData &data)
{
    uint16_t compare_w = 0;
    uint16_t compare_h = 0;
    const gui::StyleSize *compare_size = nullptr;

    ESP_UTILS_LOGD("Calibrate data");

    // Main
    compare_size = &parent_size;
    ESP_UTILS_CHECK_FALSE_RETURN(display.calibrateCoreObjectSize(*compare_size, data.main.size, true, false), false,
                                 "Calibrate main size failed");

    // Area
    compare_size = &data.main.size;
    compare_w = compare_size->width;
    ESP_UTILS_CHECK_VALUE_RETURN(data.area.left_column_pad, 0, compare_w, false, "Invalid left area column pad");
    ESP_UTILS_CHECK_VALUE_RETURN(data.area.right_column_pad, 0, compare_w, false, "Invalid right area column pad");

    // Icon
    compare_size = &data.main.size;
    ESP_UTILS_CHECK_FALSE_RETURN(display.calibrateCoreObjectSize(*compare_size, data.icon.left_size), false,
                                 "Calibrate icon left size failed");
    ESP_UTILS_CHECK_FALSE_RETURN(display.calibrateCoreObjectSize(*compare_size, data.icon.right_size), false,
                                 "Calibrate icon right size failed");

    // Switch
    compare_size = &data.main.size;
    ESP_UTILS_CHECK_FALSE_RETURN(display.calibrateCoreObjectSize(*compare_size, data.sw.main_size), false,
                                 "Calibrate switch main size failed");
    compare_size = &data.sw.main_size;
    ESP_UTILS_CHECK_FALSE_RETURN(display.calibrateCoreObjectSize(*compare_size, data.sw.knob_size), false,
                                 "Calibrate switch knob size failed");

    // Label Object
    compare_size = &data.main.size;
    compare_h = compare_size->height;
    ESP_UTILS_CHECK_VALUE_RETURN(data.label.left_row_pad, 0, compare_h, false, "Invalid label left row pad");
    ESP_UTILS_CHECK_FALSE_RETURN(display.calibrateCoreFont(compare_size, data.label.left_main_text_font), false,
                                 "Calibrate label left main text font failed");
    ESP_UTILS_CHECK_FALSE_RETURN(display.calibrateCoreFont(compare_size, data.label.left_minor_text_font), false,
                                 "Calibrate label left minor text font failed");
    ESP_UTILS_CHECK_VALUE_RETURN(data.label.right_row_pad, 0, compare_h, false, "Invalid label right row pad");
    ESP_UTILS_CHECK_FALSE_RETURN(display.calibrateCoreFont(compare_size, data.label.right_main_text_font), false,
                                 "Calibrate label right main text font failed");
    ESP_UTILS_CHECK_FALSE_RETURN(display.calibrateCoreFont(compare_size, data.label.right_minor_text_font), false,
                                 "Calibrate label right minor text font failed");

    // Text Edit
    compare_size = &data.main.size;
    ESP_UTILS_CHECK_FALSE_RETURN(display.calibrateCoreObjectSize(*compare_size, data.text_edit.size), false,
                                 "Calibrate left text edit size failed");
    compare_size = &data.text_edit.size;
    ESP_UTILS_CHECK_FALSE_RETURN(display.calibrateCoreFont(compare_size, data.text_edit.text_font), false,
                                 "Calibrate left text edit text font failed");

    // Slider
    compare_size = &data.main.size;
    ESP_UTILS_CHECK_FALSE_RETURN(display.calibrateCoreObjectSize(*compare_size, data.slider.main_size), false,
                                 "Calibrate center slider main size failed");
    compare_size = &data.slider.main_size;
    ESP_UTILS_CHECK_FALSE_RETURN(
        display.calibrateCoreObjectSize(*compare_size, data.slider.knob_size, false, false), false,
        "Calibrate center slider knob size failed"
    );

    return true;
}

bool SettingsUI_WidgetCell::updateIconImage(lv_obj_t *icon, const gui::StyleImage &image,
        const gui::StyleSize &size)
{
    ESP_UTILS_CHECK_FALSE_RETURN(_core_app.getSystemContext()->getDisplay().calibrateCoreIconImage(image), false,
                                 "Invalid image");
    ESP_UTILS_CHECK_NULL_RETURN(icon, false, "Invalid icon_object");

    lv_img_set_src(icon, image.resource);
    lv_obj_set_style_img_recolor(icon, lv_color_hex(image.recolor.color), 0);
    lv_obj_set_style_img_recolor_opa(icon, image.recolor.opacity, 0);
    float min_factor = min(
                           (float)(size.width) / ((lv_img_dsc_t *)image.resource)->header.w,
                           (float)(size.height) / ((lv_img_dsc_t *)image.resource)->header.h
                       );
    lv_image_set_scale(icon, LV_SCALE_NONE * min_factor);
    lv_obj_set_size(icon, size.width, size.height);
    lv_obj_refr_size(icon);

    return true;
}

void SettingsUI_WidgetCell::onCellTouchEventCallback(lv_event_t *event)
{
    lv_obj_t *target_object = nullptr;
    lv_event_code_t event_code = _LV_EVENT_LAST;
    SettingsUI_WidgetCell *cell = nullptr;

    ESP_UTILS_LOGD("Cell touch event callback");
    ESP_UTILS_CHECK_NULL_EXIT(event, "Invalid event object");

    target_object = (lv_obj_t *)lv_event_get_target(event);
    event_code = (lv_event_code_t)lv_event_get_code(event);
    cell = (SettingsUI_WidgetCell *)lv_event_get_user_data(event);
    ESP_UTILS_CHECK_FALSE_EXIT(event_code < _LV_EVENT_LAST, "Invalid event code");
    ESP_UTILS_CHECK_NULL_EXIT(cell, "Invalid cell");

    switch (event_code) {
    case LV_EVENT_CLICKED:
        ESP_UTILS_LOGD("Clicked");
        if (cell->_flags.is_cell_pressed_losted || cell->_flags.is_cell_click_disable) {
            break;
        }
        ESP_UTILS_CHECK_FALSE_EXIT(
            cell->_core_app.getSystemContext()->getEvent().sendEvent(
                cell->getEventObject(), cell->getClickEventID(), (void *)cell
            ), "Send event failed"
        );
        break;
    case LV_EVENT_PRESSED:
        ESP_UTILS_LOGD("Pressed");
        cell->_flags.is_cell_pressed_losted = false;
        lv_obj_set_style_bg_color(target_object, lv_color_hex(cell->data.main.active_background_color.color), 0);
        lv_obj_set_style_bg_opa(target_object, cell->data.main.active_background_color.opacity, 0);
        break;
    case LV_EVENT_PRESS_LOST:
        ESP_UTILS_LOGD("Press lost");
        cell->_flags.is_cell_pressed_losted = true;
        [[fallthrough]];
    case LV_EVENT_RELEASED:
        ESP_UTILS_LOGD("Release");
        lv_obj_set_style_bg_color(target_object, lv_color_hex(cell->data.main.inactive_background_color.color), 0);
        lv_obj_set_style_bg_opa(target_object, cell->data.main.inactive_background_color.opacity, 0);
        break;
    default:
        ESP_UTILS_CHECK_FALSE_EXIT(false, "Invalid event code(%d)", event_code);
        break;
    }
}

SettingsUI_WidgetCellContainer::SettingsUI_WidgetCellContainer(App &ui_app,
        const SettingsUI_WidgetCellContainerData &container_data):
    data(container_data),
    _core_app(ui_app),
    _conf{},
    _last_cell(nullptr)
{
}

SettingsUI_WidgetCellContainer::~SettingsUI_WidgetCellContainer()
{
    ESP_UTILS_LOGD("Destroy(0x%p)", this);

    ESP_UTILS_CHECK_FALSE_EXIT(del(), "Delete failed");
}

bool SettingsUI_WidgetCellContainer::begin(lv_obj_t *parent)
{
    ESP_UTILS_LOGD("Begin(0x%p)", this);
    ESP_UTILS_CHECK_FALSE_RETURN((parent != nullptr) && lv_obj_is_valid(parent), false, "Invalid parent object");
    ESP_UTILS_CHECK_FALSE_RETURN(!checkInitialized(), false, "Already initialized");

    ESP_Brookesia_LvObj_t main_object = nullptr;
    ESP_Brookesia_LvObj_t container_object = nullptr;
    ESP_Brookesia_LvObj_t title_label = nullptr;

    main_object = ESP_BROOKESIA_LV_OBJ(obj, parent);
    ESP_UTILS_CHECK_NULL_RETURN(main_object, false, "Create container object failed");
    title_label = ESP_BROOKESIA_LV_OBJ(label, main_object.get());
    ESP_UTILS_CHECK_NULL_RETURN(title_label, false, "Create title label failed");
    container_object = ESP_BROOKESIA_LV_OBJ(obj, main_object.get());
    ESP_UTILS_CHECK_NULL_RETURN(container_object, false, "Create main object failed");

    systems::base::Display &core_home = _core_app.getSystemContext()->getDisplay();
    // Main
    lv_obj_add_style(main_object.get(), core_home.getCoreContainerStyle(), 0);
    lv_obj_set_flex_flow(main_object.get(), LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(main_object.get(), LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(main_object.get(), LV_OBJ_FLAG_SCROLLABLE);
    // Container
    lv_obj_add_style(container_object.get(), core_home.getCoreContainerStyle(), 0);
    lv_obj_set_flex_flow(container_object.get(), LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(container_object.get(), LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(container_object.get(), LV_OBJ_FLAG_SCROLLABLE);
    // Title
    lv_obj_add_style(title_label.get(), core_home.getCoreContainerStyle(), 0);

    _main_object = main_object;
    _container_object = container_object;
    _title_label = title_label;

    ESP_UTILS_CHECK_FALSE_GOTO(processDataUpdate(), err, "Process data update failed");

    return true;

err:
    ESP_UTILS_CHECK_FALSE_RETURN(del(), false, "Delete failed");

    return false;
}

bool SettingsUI_WidgetCellContainer::del()
{
    ESP_UTILS_LOGD("Delete(0x%p)", this);
    if (!checkInitialized()) {
        return true;
    }

    _cells.clear();
    _main_object.reset();
    _container_object.reset();
    _title_label.reset();
    _last_cell = nullptr;

    return true;
}

bool SettingsUI_WidgetCellContainer::processDataUpdate()
{
    ESP_UTILS_LOGD("Process data update");
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    // Main
    lv_obj_set_style_pad_row(_main_object.get(), data.main.row_pad, 0);
    // Container
    lv_obj_set_width(_container_object.get(), data.container.size.width);
    lv_obj_set_style_radius(_container_object.get(), data.container.radius, 0);
    lv_obj_set_style_bg_color(_container_object.get(), lv_color_hex(data.container.background_color.color), 0);
    lv_obj_set_style_bg_opa(_container_object.get(), data.container.background_color.opacity, 0);
    lv_obj_set_style_pad_top(_container_object.get(), data.container.top_pad, 0);
    lv_obj_set_style_pad_bottom(_container_object.get(), data.container.bottom_pad, 0);
    lv_obj_set_style_pad_left(_container_object.get(), data.container.left_pad, 0);
    lv_obj_set_style_pad_right(_container_object.get(), data.container.right_pad, 0);
    // Title
    lv_obj_set_style_text_font(_title_label.get(), (const lv_font_t *)data.title.text_font.font_resource, 0);
    lv_obj_set_style_text_color(_title_label.get(), lv_color_hex(data.title.text_color.color), 0);
    lv_obj_set_style_text_opa(_title_label.get(), data.title.text_color.opacity, 0);

    ESP_UTILS_CHECK_FALSE_RETURN(updateConf(_conf), false, "Update conf failed");
    for (auto &cell : _cells) {
        ESP_UTILS_CHECK_FALSE_RETURN(cell.second->processDataUpdate(), false, "Cell process data update failed");
    }

    return true;
}

SettingsUI_WidgetCell *SettingsUI_WidgetCellContainer::addCell(int key, SettingsUI_WidgetCellElement elements)
{
    ESP_UTILS_LOGD("Add cell(%d,%d)", key, (int)elements);
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), nullptr, "Not initialized");

    unique_ptr<SettingsUI_WidgetCell> cell = make_unique<SettingsUI_WidgetCell>(_core_app, data.cell, elements);
    ESP_UTILS_CHECK_NULL_RETURN(cell, nullptr, "Create cell object failed");

    ESP_UTILS_CHECK_FALSE_RETURN(cell->begin(_container_object.get()), nullptr, "Cell begin failed");
    ESP_UTILS_CHECK_FALSE_RETURN(cell->setSplitLineVisible(false), nullptr, "Cell set split line visible failed");

    if (getCellByKey(key) != nullptr) {
        ESP_UTILS_LOGD("Cell already exists, replacing it");
    }
    _cells.emplace_back(make_pair(key, std::move(cell)));

    if (_last_cell != nullptr) {
        ESP_UTILS_CHECK_FALSE_RETURN(_last_cell->setSplitLineVisible(true), nullptr,
                                     "Last cell set split line visible failed");
    }
    _last_cell = _cells.back().second.get();

    return _last_cell;
}

bool SettingsUI_WidgetCellContainer::cleanCells()
{
    ESP_UTILS_LOGD("Clean cells");
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    _cells.clear();
    _last_cell = nullptr;

    return true;
}

bool SettingsUI_WidgetCellContainer::delCellByKey(int key)
{
    ESP_UTILS_LOGD("Delete cell(%d)", key);
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    int index = -1;
    for (auto &cell : _cells) {
        index++;
        if (cell.first == key) {
            break;
        }
    }
    ESP_UTILS_CHECK_FALSE_RETURN(index >= 0, false, "Cell not found");

    ESP_UTILS_CHECK_FALSE_RETURN(delCellByIndex(static_cast<size_t>(index)), false, "Delete cell failed");

    if (_cells.empty()) {
        _last_cell = nullptr;
    } else {
        _last_cell = _cells.back().second.get();
    }

    return true;
}

bool SettingsUI_WidgetCellContainer::delCellByIndex(size_t index)
{
    ESP_UTILS_LOGD("Delete cell(%d)", (int)index);
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_UTILS_CHECK_FALSE_RETURN(index < _cells.size(), false, "Index out of range");

    _cells.erase(next(_cells.begin(), index));

    if (_cells.empty()) {
        _last_cell = nullptr;
    } else {
        _last_cell = _cells.back().second.get();
    }

    return true;
}

bool SettingsUI_WidgetCellContainer::updateConf(const SettingsUI_WidgetCellContainerConf &conf)
{
    ESP_UTILS_LOGD("Update conf");
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    if (conf.flags.enable_title) {
        ESP_UTILS_CHECK_FALSE_RETURN(!conf.title_text.empty(), false, "Empty title text");
        lv_label_set_text(_title_label.get(), conf.title_text.c_str());
        lv_obj_remove_flag(_title_label.get(), LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(_title_label.get(), LV_OBJ_FLAG_HIDDEN);
    }

    return true;
}

SettingsUI_WidgetCell *SettingsUI_WidgetCellContainer::getCellByKey(int key) const
{
    for (auto &cell : _cells) {
        if (cell.first == key) {
            return cell.second.get();
        }
    }

    return nullptr;
}

SettingsUI_WidgetCell *SettingsUI_WidgetCellContainer::getCellByIndex(size_t index) const
{
    ESP_UTILS_CHECK_FALSE_RETURN(index < _cells.size(), nullptr, "Index out of range");

    if (index < _cells.size()) {
        auto it = _cells.begin();
        std::advance(it, index);

        return it->second.get();
    }

    return nullptr;
}

int SettingsUI_WidgetCellContainer::getCellIndex(SettingsUI_WidgetCell *cell) const
{
    ESP_UTILS_CHECK_NULL_RETURN(cell, -1, "Invalid cell");

    int index = 0;
    for (auto &cell_pair : _cells) {
        if (cell_pair.second.get() == cell) {
            return index;
        }
        index++;
    }

    return index;
}

bool SettingsUI_WidgetCellContainer::calibrateData(const gui::StyleSize &parent_size, const systems::base::Display &display,
        SettingsUI_WidgetCellContainerData &data)
{
    uint16_t compare_w = 0;
    uint16_t compare_h = 0;
    const gui::StyleSize *compare_size = nullptr;

    ESP_UTILS_LOGD("Calibrate data");

    // Main
    compare_size = &parent_size;
    compare_h = parent_size.height;
    ESP_UTILS_CHECK_VALUE_RETURN(data.main.row_pad, 0, compare_h, false, "Invalid main row pad");

    // Container
    compare_size = &parent_size;
    ESP_UTILS_CHECK_FALSE_RETURN(display.calibrateCoreObjectSize(*compare_size, data.container.size, true, false), false,
                                 "Invalid main size");
    compare_size = &data.container.size;
    compare_w = compare_size->width;
    compare_h = parent_size.height;
    ESP_UTILS_CHECK_VALUE_RETURN(data.container.top_pad, 0, compare_h, false, "Invalid main top pad");
    ESP_UTILS_CHECK_VALUE_RETURN(data.container.bottom_pad, 0, compare_h, false, "Invalid main bottom pad");
    ESP_UTILS_CHECK_VALUE_RETURN(data.container.left_pad, 0, compare_w, false, "Invalid main left pad");
    ESP_UTILS_CHECK_VALUE_RETURN(data.container.right_pad, 0, compare_w, false, "Invalid main right pad");

    // Title
    compare_size = &data.container.size;
    ESP_UTILS_CHECK_FALSE_RETURN(display.calibrateCoreFont(compare_size, data.title.text_font), false,
                                 "Invalid title text font");

    // Cell
    compare_size = &data.container.size;
    ESP_UTILS_CHECK_FALSE_RETURN(SettingsUI_WidgetCell::calibrateData(*compare_size, display, data.cell), false,
                                 "Calibrate cell data failed");

    return true;
}

} // namespace esp_brookesia::apps
