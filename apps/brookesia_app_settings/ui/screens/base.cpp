/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "private/esp_brookesia_app_settings_utils.hpp"
#include "gui/lvgl/esp_brookesia_lv_helper.hpp"
#include "esp_brookesia.hpp"
#include <src/core/lv_obj.h>
#include <src/core/lv_obj_pos.h>
#include <src/lv_api_map_v8.h>
#include "base.hpp"

using namespace std;
using namespace esp_brookesia::systems::speaker;

namespace esp_brookesia::apps {

SettingsUI_ScreenBase::SettingsUI_ScreenBase(
    App &ui_app,
    const SettingsUI_ScreenBaseData &base_data,
    SettingsUI_ScreenBaseType type
):
    app(ui_app),
    data(base_data),
    _screen_object(nullptr),
    _navigation_click_event_id(systems::base::Event::ID::CUSTOM),
    _type(type),
    _objects{}
{
}

SettingsUI_ScreenBase::~SettingsUI_ScreenBase()
{
    ESP_UTILS_LOGD("Destroy(0x%p)", this);

    ESP_UTILS_CHECK_FALSE_EXIT(del(), "Delete failed");
}

bool SettingsUI_ScreenBase::begin(std::string header_title_name, std::string navigation_title_name)
{
    ESP_UTILS_LOGD("Begin(0x%p)", this);
    ESP_UTILS_CHECK_FALSE_RETURN(!header_title_name.empty(), false, "Invalid header title name");
    ESP_UTILS_CHECK_FALSE_RETURN(!checkInitialized(), false, "Already initialized");

    ESP_Brookesia_LvObj_t header_object = nullptr;
    ESP_Brookesia_LvObj_t header_title_label = nullptr;
    ESP_Brookesia_LvObj_t header_navigation_main_object = nullptr;
    ESP_Brookesia_LvObj_t header_navigation_icon_object = nullptr;
    ESP_Brookesia_LvObj_t header_navigation_icon_image = nullptr;
    ESP_Brookesia_LvObj_t header_navigation_title_label = nullptr;
    ESP_Brookesia_LvObj_t content_object = nullptr;

    _screen_object = lv_obj_create(nullptr);
    ESP_UTILS_CHECK_NULL_RETURN(_screen_object, false, "Create screen object failed");
    header_object = ESP_BROOKESIA_LV_OBJ(obj, _screen_object);
    ESP_UTILS_CHECK_NULL_RETURN(header_object, false, "Create header object failed");
    if (data.flags.enable_header_title) {
        header_title_label = ESP_BROOKESIA_LV_OBJ(label, header_object.get());
        ESP_UTILS_CHECK_NULL_RETURN(header_title_label, false, "Create header title label failed");
    }
    if (_type == SettingsUI_ScreenBaseType::CHILD) {
        header_navigation_main_object = ESP_BROOKESIA_LV_OBJ(obj, header_object.get());
        ESP_UTILS_CHECK_NULL_RETURN(header_navigation_main_object, false, "Create header navigation main object failed");
        header_navigation_icon_object = ESP_BROOKESIA_LV_OBJ(obj, header_navigation_main_object.get());
        ESP_UTILS_CHECK_NULL_RETURN(header_navigation_icon_object, false, "Create header navigation icon object failed");
        header_navigation_icon_image = ESP_BROOKESIA_LV_OBJ(img, header_navigation_icon_object.get());
        ESP_UTILS_CHECK_NULL_RETURN(header_navigation_icon_image, false, "Create header navigation icon image failed");
        header_navigation_title_label = ESP_BROOKESIA_LV_OBJ(label, header_navigation_main_object.get());
        ESP_UTILS_CHECK_NULL_RETURN(header_navigation_title_label, false, "Create header navigation title label failed");
    }
    content_object = ESP_BROOKESIA_LV_OBJ(obj, _screen_object);
    ESP_UTILS_CHECK_NULL_RETURN(content_object, false, "Create content object failed");

    systems::base::Display &core_home = app.getSystemContext()->getDisplay();
    // Screen
    // lv_obj_add_style(_screen_object, core_home.getCoreContainerStyle(), 0);
    lv_obj_set_size(_screen_object, data.screen.size.width, data.screen.size.height);
    // lv_obj_set_style_radius(_screen_object, data.screen.size.radius, 0);
    // lv_obj_set_style_clip_corner(_screen_object, true, 0);
    // lv_obj_remove_flag(_screen_object, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    // Header
    lv_obj_add_style(header_object.get(), core_home.getCoreContainerStyle(), 0);
    // Header: Title
    if (data.flags.enable_header_title) {
        lv_obj_add_style(header_title_label.get(), core_home.getCoreContainerStyle(), 0);
        lv_obj_center(header_title_label.get());
        lv_label_set_text_fmt(header_title_label.get(), "%s", header_title_name.c_str());
    }
    // Header: Navigation Icon
    if (_type == SettingsUI_ScreenBaseType::CHILD) {
        // Header: Navigation Main
        lv_obj_add_style(header_navigation_main_object.get(), core_home.getCoreContainerStyle(), 0);
        lv_obj_set_flex_align(header_navigation_main_object.get(), LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_flex_flow(header_navigation_main_object.get(), LV_FLEX_FLOW_ROW);
        lv_obj_add_flag(header_navigation_main_object.get(), LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_flag(header_navigation_main_object.get(), LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_PRESS_LOCK);
        lv_obj_add_event_cb(header_navigation_main_object.get(), onNavigationTouchEventCallback,
                            LV_EVENT_PRESSED, this);
        lv_obj_add_event_cb(header_navigation_main_object.get(), onNavigationTouchEventCallback,
                            LV_EVENT_PRESS_LOST, this);
        lv_obj_add_event_cb(header_navigation_main_object.get(), onNavigationTouchEventCallback,
                            LV_EVENT_RELEASED, this);
        lv_obj_add_event_cb(header_navigation_main_object.get(), onNavigationTouchEventCallback,
                            LV_EVENT_CLICKED, this);
        // Header: Navigation Icon Object
        lv_obj_add_style(header_navigation_icon_object.get(), core_home.getCoreContainerStyle(), 0);
        lv_obj_remove_flag(header_navigation_icon_object.get(), LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
        // Header: Navigation Icon Image
        lv_obj_center(header_navigation_icon_image.get());
        lv_obj_add_style(header_navigation_icon_image.get(), core_home.getCoreContainerStyle(), 0);
        lv_obj_remove_flag(header_navigation_icon_image.get(), LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
        lv_image_set_inner_align(header_navigation_icon_image.get(), LV_IMAGE_ALIGN_CENTER);
        // Header: Navigation Label
        lv_obj_add_style(header_navigation_title_label.get(), core_home.getCoreContainerStyle(), 0);
        lv_label_set_text_fmt(header_navigation_title_label.get(), "%s", navigation_title_name.c_str());
        lv_obj_remove_flag(header_navigation_title_label.get(), LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    }
    // Content
    lv_obj_set_style_clip_corner(content_object.get(), true, 0);
    lv_obj_add_style(content_object.get(), core_home.getCoreContainerStyle(), 0);
    lv_obj_add_flag(content_object.get(), LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_align(content_object.get(), LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_flex_flow(content_object.get(), LV_FLEX_FLOW_COLUMN);

    _objects[(int)SettingsUI_ScreenBaseObject::HEADER_OBJECT] = header_object;
    if (data.flags.enable_header_title) {
        _objects[(int)SettingsUI_ScreenBaseObject::HEADER_TITLE_LABEL] = header_title_label;
    }
    _objects[(int)SettingsUI_ScreenBaseObject::NAVIGATION_MAIN_OBJECT] = header_navigation_main_object;
    _objects[(int)SettingsUI_ScreenBaseObject::NAVIGATION_ICON_OBJECT] = header_navigation_icon_object;
    _objects[(int)SettingsUI_ScreenBaseObject::NAVIGATION_ICON_IMAGE] = header_navigation_icon_image;
    _objects[(int)SettingsUI_ScreenBaseObject::NAVIGATION_TITLE_LABEL] = header_navigation_title_label;
    _objects[(int)SettingsUI_ScreenBaseObject::CONTENT_OBJECT] = content_object;

    ESP_UTILS_CHECK_FALSE_GOTO(processDataUpdate(), err, "Process data update failed");

    if (_type == SettingsUI_ScreenBaseType::CHILD) {
        _navigation_click_event_id = app.getSystemContext()->getEvent().getFreeEventID();
    }

    return true;

err:
    ESP_UTILS_CHECK_FALSE_RETURN(del(), false, "Delete failed");

    return false;
}
bool SettingsUI_ScreenBase::del()
{
    ESP_UTILS_LOGD("Delete(0x%p)", this);

    if (!checkInitialized()) {
        ESP_UTILS_LOGD("Not initialized");
        return true;
    }

    _screen_object = nullptr;
    for (int i = 0; i < (int)SettingsUI_ScreenBaseObject::MAX; i++) {
        _objects[i].reset();
    }
    _cell_containers_map.clear();

    if (_type == SettingsUI_ScreenBaseType::CHILD) {
        app.getSystemContext()->getEvent().unregisterEvent(_navigation_click_event_id);
    }

    return true;
}

bool SettingsUI_ScreenBase::processDataUpdate()
{
    ESP_UTILS_LOGD("Process data update");
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    // Screen
    lv_obj_set_style_bg_color(_screen_object, lv_color_hex(data.screen.background_color.color), 0);
    lv_obj_set_style_bg_opa(_screen_object, data.screen.background_color.opacity, 0);

    // Header
    const SettingsUI_ScreenBaseHeaderData &header_data = data.header;
    lv_obj_t *header_main_object = getObject(SettingsUI_ScreenBaseObject::HEADER_OBJECT);
    lv_obj_set_size(header_main_object, header_data.size.width, header_data.size.height);
    lv_obj_align(header_main_object, LV_ALIGN_TOP_MID, 0, header_data.align_top_offset);
    lv_obj_set_style_bg_color(header_main_object, lv_color_hex(header_data.background_color.color), 0);
    lv_obj_set_style_bg_opa(header_main_object, header_data.background_color.opacity, 0);

    // Header: Title
    if (data.flags.enable_header_title) {
        lv_obj_t *header_title_label = getObject(SettingsUI_ScreenBaseObject::HEADER_TITLE_LABEL);
        lv_obj_set_style_text_font(header_title_label, (const lv_font_t *)header_data.title_text_font.font_resource, 0);
        lv_obj_set_style_text_color(header_title_label, lv_color_hex(header_data.title_text_color.color), 0);
        lv_obj_set_style_text_opa(header_title_label, header_data.title_text_color.opacity, 0);
    }

    // Content
    const SettingsUI_ScreenBaseContentData &content_data = data.content;
    lv_obj_t *content_main_object = getObject(SettingsUI_ScreenBaseObject::CONTENT_OBJECT);
    lv_obj_set_size(content_main_object, content_data.size.width, content_data.size.height);
    lv_obj_align(content_main_object, LV_ALIGN_BOTTOM_MID, 0, -content_data.align_bottom_offset);
    lv_obj_set_style_pad_row(content_main_object, content_data.row_pad, 0);
    lv_obj_set_style_bg_color(content_main_object, lv_color_hex(content_data.background_color.color), 0);
    lv_obj_set_style_bg_opa(content_main_object, content_data.background_color.opacity, 0);
    lv_obj_set_style_pad_top(content_main_object, content_data.top_pad, 0);
    lv_obj_set_style_pad_bottom(content_main_object, content_data.bottom_pad, 0);
    lv_obj_set_style_pad_left(content_main_object, content_data.left_pad, 0);
    lv_obj_set_style_pad_right(content_main_object, content_data.right_pad, 0);

    if (_type == SettingsUI_ScreenBaseType::CHILD) {
        // Header: Navigation Main
        const SettingsUI_ScreenBaseHeaderNavigation &header_navigation_data = data.header_navigation;
        lv_obj_t *header_navigation_main_object = getObject(SettingsUI_ScreenBaseObject::NAVIGATION_MAIN_OBJECT);
        lv_obj_align(
            header_navigation_main_object, gui::toLvAlign(header_navigation_data.align.type),
            header_navigation_data.align.offset_x, header_navigation_data.align.offset_y
        );
        lv_obj_set_style_pad_column(header_navigation_main_object, header_navigation_data.main_column_pad, 0);

        // Header: Navigation Icon Object
        lv_obj_t *header_navigation_icon_object = getObject(SettingsUI_ScreenBaseObject::NAVIGATION_ICON_OBJECT);
        lv_obj_set_size(header_navigation_icon_object, header_navigation_data.icon_size.width,
                        header_navigation_data.icon_size.height);

        // Header: Navigation Icon Image
        lv_obj_t *header_navigation_icon_image = getObject(SettingsUI_ScreenBaseObject::NAVIGATION_ICON_IMAGE);
        lv_img_set_src(header_navigation_icon_image, header_navigation_data.icon_image.resource);
        lv_obj_set_style_img_recolor(header_navigation_icon_image,
                                     lv_color_hex(header_navigation_data.icon_image.recolor.color), 0);
        lv_obj_set_style_img_recolor_opa(header_navigation_icon_image,
                                         header_navigation_data.icon_image.recolor.opacity, 0);
        float min_factor = min(
                               (float)(header_navigation_data.icon_size.width) /
                               ((lv_img_dsc_t *)header_navigation_data.icon_image.resource)->header.w,
                               (float)(header_navigation_data.icon_size.height) /
                               ((lv_img_dsc_t *)header_navigation_data.icon_image.resource)->header.h
                           );

        lv_image_set_scale(header_navigation_icon_image, min_factor * LV_SCALE_NONE);
        lv_obj_set_size(header_navigation_icon_image, header_navigation_data.icon_size.width,
                        header_navigation_data.icon_size.height);
        lv_obj_refr_size(header_navigation_icon_image);

        // Header: Navigation Title
        lv_obj_t *header_navigation_title_label = getObject(SettingsUI_ScreenBaseObject::NAVIGATION_TITLE_LABEL);
        lv_obj_set_style_text_font(header_navigation_title_label,
                                   (const lv_font_t *)header_navigation_data.title_text_font.font_resource, 0);
        lv_obj_set_style_text_color(header_navigation_title_label,
                                    lv_color_hex(header_navigation_data.title_text_color.color), 0);
        lv_obj_set_style_text_opa(header_navigation_title_label, header_navigation_data.title_text_color.opacity, 0);
    }

    return true;
}

SettingsUI_WidgetCellContainer *SettingsUI_ScreenBase::addCellContainer(int key)
{
    ESP_UTILS_LOGD("Add cell container(%d)", key);
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), nullptr, "Not initialized");

    unique_ptr<SettingsUI_WidgetCellContainer> cell_container = nullptr;
    cell_container = make_unique<SettingsUI_WidgetCellContainer>(app, data.cell_container);
    ESP_UTILS_CHECK_NULL_RETURN(cell_container, nullptr, "Create cell container failed");

    ESP_UTILS_CHECK_FALSE_RETURN(
        cell_container->begin(getObject(SettingsUI_ScreenBaseObject::CONTENT_OBJECT)), nullptr,
        "Cell container begin failed"
    );

    if (getCellContainer(key) != nullptr) {
        ESP_UTILS_LOGW("Cell container already exists, replacing it");
    }
    _cell_containers_map[key] = move(cell_container);

    return _cell_containers_map[key].get();
}

bool SettingsUI_ScreenBase::delCellContainer(int key)
{
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    auto it = _cell_containers_map.find(key);
    if (it == _cell_containers_map.end()) {
        ESP_UTILS_LOGW("Cell container not found");
        return true;
    }

    _cell_containers_map.erase(it);

    return true;
}

SettingsUI_WidgetCell *SettingsUI_ScreenBase::getCell(int container_key, int cell_key) const
{
    auto container = getCellContainer(container_key);
    if (container == nullptr) {
        return nullptr;
    }

    return container->getCellByKey(cell_key);
}

lv_obj_t *SettingsUI_ScreenBase::getElementObject(int container_key, int cell_key, SettingsUI_WidgetCellElement element) const
{
    auto container = getCellContainer(container_key);
    if (container == nullptr) {
        return nullptr;
    }

    auto cell = container->getCellByKey(cell_key);
    if (cell == nullptr) {
        return nullptr;
    }

    return cell->getElementObject(element);
}

bool SettingsUI_ScreenBase::calibrateCommonHeader(const gui::StyleSize &parent_size, const systems::base::Display &display,
        SettingsUI_ScreenBaseHeaderData &data)
{
    const gui::StyleSize *compare_size = nullptr;

    ESP_UTILS_LOGD("Calibrate common header");

    compare_size = &parent_size;
    ESP_UTILS_CHECK_FALSE_RETURN(display.calibrateCoreObjectSize(*compare_size, data.size), false, "Invalid");
    ESP_UTILS_CHECK_VALUE_RETURN(data.align_top_offset, 0, compare_size->height - data.size.height, false,
                                 "Invalid align_top_offset");

    compare_size = &data.size;
    ESP_UTILS_CHECK_FALSE_RETURN(display.calibrateCoreFont(compare_size, data.title_text_font), false,
                                 "Invalid title_text_font");

    return true;
}

bool SettingsUI_ScreenBase::calibrateCommonContent(const gui::StyleSize &parent_size, const systems::base::Display &display,
        SettingsUI_ScreenBaseContentData &data)
{
    uint16_t parent_w = 0;
    uint16_t parent_h = 0;
    const gui::StyleSize *compare_size = nullptr;

    ESP_UTILS_LOGD("Calibrate common content");

    compare_size = &parent_size;
    parent_h = compare_size->height;
    ESP_UTILS_CHECK_FALSE_RETURN(display.calibrateCoreObjectSize(*compare_size, data.size), false, "Invalid size");
    ESP_UTILS_CHECK_VALUE_RETURN(data.align_bottom_offset, 0, parent_h - data.size.height, false,
                                 "Invalid align_bottom_offset");

    compare_size = &data.size;
    parent_w = compare_size->width;
    parent_h = compare_size->height;
    ESP_UTILS_CHECK_VALUE_RETURN(data.row_pad, 0, parent_h, false, "Invalid row_pad");
    ESP_UTILS_CHECK_VALUE_RETURN(data.top_pad, 0, parent_h, false, "Invalid top_pad");
    ESP_UTILS_CHECK_VALUE_RETURN(data.bottom_pad, 0, parent_h, false, "Invalid bottom_pad");
    ESP_UTILS_CHECK_VALUE_RETURN(data.left_pad, 0, parent_w, false, "Invalid left_pad");
    ESP_UTILS_CHECK_VALUE_RETURN(data.right_pad, 0, parent_w, false, "Invalid right_pad");

    return true;
}

bool SettingsUI_ScreenBase::calibrateHeaderNavigation(const gui::StyleSize &parent_size, const systems::base::Display &display,
        SettingsUI_ScreenBaseHeaderNavigation &data)
{
    uint16_t parent_w = 0;
    const gui::StyleSize *compare_size = nullptr;

    ESP_UTILS_LOGD("Calibrate header navigation");

    compare_size = &parent_size;
    parent_w = compare_size->width;
    ESP_UTILS_CHECK_VALUE_RETURN(data.main_column_pad, 0, parent_w, false, "Invalid main_column_pad");
    ESP_UTILS_CHECK_FALSE_RETURN(display.calibrateCoreObjectSize(*compare_size, data.icon_size), false, "Invalid icon_size");
    ESP_UTILS_CHECK_FALSE_RETURN(display.calibrateCoreIconImage(data.icon_image), false, "Invalid icon_image");
    ESP_UTILS_CHECK_FALSE_RETURN(display.calibrateCoreFont(compare_size, data.title_text_font), false,
                                 "Invalid title_text_font");

    return true;
}

bool SettingsUI_ScreenBase::calibrateData(const gui::StyleSize &parent_size, const systems::base::Display &display,
        SettingsUI_ScreenBaseData &data)
{
    gui::StyleSize compare_size = {};

    ESP_UTILS_LOGD("Calibrate data");

    ESP_UTILS_CHECK_FALSE_RETURN(
        display.calibrateCoreObjectSize(parent_size, data.screen.size), false, "Invalid screen size"
    );

    SettingsUI_ScreenBaseHeaderData &header_data = data.header;
    SettingsUI_ScreenBaseContentData &content_data = data.content;
    SettingsUI_ScreenBaseHeaderNavigation &navigation_data = data.header_navigation;
    // Header
    compare_size = data.screen.size;
    ESP_UTILS_CHECK_FALSE_RETURN(calibrateCommonHeader(compare_size, display, header_data), false,
                                 "Invalid child header");
    // Header Navigation
    ESP_UTILS_CHECK_FALSE_RETURN(calibrateHeaderNavigation(compare_size, display, navigation_data), false,
                                 "Invalid child header navigation");
    // Content
    compare_size = data.screen.size;
    compare_size.height -= (header_data.align_top_offset + header_data.size.height);
    if (data.flags.enable_content_size_flex) {
        ESP_UTILS_CHECK_VALUE_RETURN(content_data.align_bottom_offset, 0, compare_size.height - 1, false,
                                     "Invalid child content align_bottom_offset");
        content_data.size.width_percent = 100;
        content_data.size.height = compare_size.height - content_data.align_bottom_offset;
        content_data.size.flags.enable_width_percent = true;
        content_data.size.flags.enable_height_percent = false;
        content_data.size.flags.enable_square = false;
    }
    ESP_UTILS_CHECK_FALSE_RETURN(calibrateCommonContent(compare_size, display, content_data), false,
                                 "Invalid child content");
    // Cell Container
    ESP_UTILS_CHECK_FALSE_RETURN(
        SettingsUI_WidgetCellContainer::calibrateData(parent_size, display, data.cell_container),
        false, "Invalid child cell container data"
    );

    return true;
}

void SettingsUI_ScreenBase::onNavigationTouchEventCallback(lv_event_t *event)
{
    lv_event_code_t event_code = _LV_EVENT_LAST;
    SettingsUI_ScreenBase *screen = nullptr;

    ESP_UTILS_LOGD("Navigation touch event callback");
    ESP_UTILS_CHECK_NULL_EXIT(event, "Invalid event object");

    event_code = lv_event_get_code(event);
    screen = (SettingsUI_ScreenBase *)lv_event_get_user_data(event);
    ESP_UTILS_CHECK_FALSE_EXIT(event_code < _LV_EVENT_LAST, "Invalid event code");
    ESP_UTILS_CHECK_NULL_EXIT(screen, "Invalid screen");

    switch (event_code) {
    case LV_EVENT_CLICKED:
        ESP_UTILS_LOGD("Clicked");
        if (screen->_flags.is_navigation_pressed_losted) {
            break;
        }
        ESP_UTILS_CHECK_FALSE_EXIT(
            screen->app.getSystemContext()->getEvent().sendEvent(screen->getEventObject(), screen->getNavigaitionClickEventID(),
                    (void *)screen), "Send navigation click event failed"
        );
        break;
    case LV_EVENT_PRESSED:
        ESP_UTILS_LOGD("Pressed");
        screen->_flags.is_navigation_pressed_losted = false;
        break;
    case LV_EVENT_PRESS_LOST:
        ESP_UTILS_LOGD("Press lost");
        screen->_flags.is_navigation_pressed_losted = true;
        [[fallthrough]];
    case LV_EVENT_RELEASED:
        ESP_UTILS_LOGD("Release");
        break;
    default:
        ESP_UTILS_CHECK_FALSE_EXIT(false, "Invalid event code(%d)", event_code);
        break;
    }
}

} // namespace esp_brookesia::apps
