/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <limits>
#include <memory>
#include "esp_brookesia_systems_internal.h"
#if !ESP_BROOKESIA_PHONE_STATUS_BAR_ENABLE_DEBUG_LOG
#   define ESP_BROOKESIA_UTILS_DISABLE_DEBUG_LOG
#endif
#include "phone/private/esp_brookesia_phone_utils.hpp"
#include "systems/base/esp_brookesia_base_context.hpp"
#include "lvgl/esp_brookesia_lv_helper.hpp"
#include "esp_brookesia_status_bar.hpp"

using namespace std;
using namespace esp_brookesia::gui;

namespace esp_brookesia::systems::phone {

StatusBar::StatusBar(base::Context &core, const StatusBar::Data &data, int battery_id, int wifi_id)
    : _system_context(core)
    , _data(data)
    , _battery_id(battery_id)
    , _wifi_id(wifi_id)
{
}

StatusBar::~StatusBar()
{
    ESP_UTILS_LOGD("Destroy(@0x%p)", this);
    if (!del()) {
        ESP_UTILS_LOGE("Delete failed");
    }
}

bool StatusBar::begin(lv_obj_t *parent)
{
    ESP_UTILS_LOGD("Begin(@0x%p)", this);
    ESP_UTILS_CHECK_NULL_RETURN(parent, false, "Invalid parent");
    ESP_UTILS_CHECK_FALSE_RETURN(!checkMainInitialized(), false, "Already initialized");

    ESP_UTILS_CHECK_FALSE_GOTO(beginMain(parent), err, "Begin main failed");
    ESP_UTILS_CHECK_FALSE_GOTO(beginWifi(), err, "Begin wifi failed");
    ESP_UTILS_CHECK_FALSE_GOTO(beginBattery(), err, "Begin battery failed");
    ESP_UTILS_CHECK_FALSE_GOTO(beginClock(), err, "Begin clock failed");

    ESP_UTILS_CHECK_FALSE_RETURN(_system_context.registerDateUpdateEventCallback(onDataUpdateEventCallback, this), false,
                                 "Register data update event callback failed");

    return true;

err:
    ESP_UTILS_CHECK_FALSE_RETURN(del(), false, "Delete failed");

    return false;
}

bool StatusBar::del(void)
{
    bool ret = true;

    ESP_UTILS_LOGD("Delete(0x%p)", this);

    if (!checkMainInitialized()) {
        return true;
    }

    if (_system_context.checkCoreInitialized() && !_system_context.unregisterDateUpdateEventCallback(onDataUpdateEventCallback, this)) {
        ESP_UTILS_LOGE("Unregister data update event callback failed");
        ret = false;
    }

    if (!delMain()) {
        ESP_UTILS_LOGE("Delete main failed");
        ret = false;
    }
    if (!delBattery()) {
        ESP_UTILS_LOGE("Delete battery failed");
        ret = false;
    }
    if (!delClock()) {
        ESP_UTILS_LOGE("Delete clock failed");
        ret = false;
    }

    _id_icon_map.clear();

    return ret;
}

bool StatusBar::setVisualMode(StatusBar::VisualMode mode) const
{
    ESP_UTILS_LOGD("Set Visual Mode(%d)", mode);
    ESP_UTILS_CHECK_FALSE_RETURN(checkMainInitialized(), false, "Not initialized");

    switch (mode) {
    case StatusBar::VisualMode::HIDE:
        lv_obj_add_flag(_main_obj.get(), LV_OBJ_FLAG_HIDDEN);
        break;
    case StatusBar::VisualMode::SHOW_FIXED:
        lv_obj_clear_flag(_main_obj.get(), LV_OBJ_FLAG_HIDDEN);
        break;
    default:
        break;
    }

    return true;
}

bool StatusBar::addIcon(const StatusBarIcon::Data &data, uint8_t area_index, int id)
{
    ESP_UTILS_LOGD("Add icon(%d) in area(%d)", id, area_index);
    ESP_UTILS_CHECK_FALSE_RETURN(checkMainInitialized(), false, "Not initialized");

    shared_ptr<StatusBarIcon> icon = make_shared<StatusBarIcon>(data);
    ESP_UTILS_CHECK_NULL_RETURN(icon, false, "Alloc icon failed");

    ESP_UTILS_CHECK_FALSE_RETURN(icon->begin(_system_context, _area_objs[area_index].get()), false, "Init icon failed");

    auto ret = _id_icon_map.insert(pair <int, shared_ptr<StatusBarIcon>> (id, icon));
    ESP_UTILS_CHECK_FALSE_RETURN(ret.second, false, "Insert icon failed");

    return true;
}

bool StatusBar::removeIcon(int id)
{
    ESP_UTILS_LOGD("Remove icon(%d)", id);
    ESP_UTILS_CHECK_FALSE_RETURN(checkMainInitialized(), false, "Not initialized");

    auto ret = _id_icon_map.find(id);
    ESP_UTILS_CHECK_FALSE_RETURN(ret != _id_icon_map.end(), false, "Icon id not found");

    int num = _id_icon_map.erase(id);
    ESP_UTILS_CHECK_FALSE_RETURN(num > 0, false, "Erase icon failed");

    return true;
}

bool StatusBar::setIconState(int id, int state) const
{
    shared_ptr<StatusBarIcon> icon = nullptr;

    ESP_UTILS_LOGD("Set icon(%d) state(%d)", id, state);

    auto ret = _id_icon_map.find(id);
    ESP_UTILS_CHECK_FALSE_RETURN(ret != _id_icon_map.end(), false, "Icon not found");

    icon = ret->second;
    ESP_UTILS_CHECK_NULL_RETURN(icon, false, "Found invalid icon");

    ESP_UTILS_CHECK_FALSE_RETURN(icon->setCurrentState(state), false, "Set icon state failed");

    return true;
}

bool StatusBar::checkVisible(void) const
{
    ESP_UTILS_CHECK_FALSE_RETURN(checkMainInitialized(), false, "Not initialized");

    return !lv_obj_has_flag(_main_obj.get(), LV_OBJ_FLAG_HIDDEN);
}

bool StatusBar::calibrateIconData(const StatusBar::Data &bar_data, const base::Display &display,
                                  StatusBarIcon::Data &icon_data)
{
    const StatusBar::AreaData &area = bar_data.area.data[bar_data.battery.area_index];

    ESP_UTILS_LOGD("Calibrate data");

    // Size
    ESP_UTILS_CHECK_FALSE_RETURN(display.calibrateCoreObjectSize(area.size, icon_data.size), false, "Calibrate size failed");
    // Image
    ESP_UTILS_CHECK_VALUE_RETURN(icon_data.icon.image_num, 1, StatusBarIcon::IMAGE_NUM_MAX,
                                 false, "Icon image num is invalid");
    for (int i = 0; i < icon_data.icon.image_num; i++) {
        ESP_UTILS_CHECK_FALSE_RETURN(display.calibrateCoreIconImage(icon_data.icon.images[i]), false,
                                     "Calibrate icon image failed");
    }

    return true;
}

bool StatusBar::calibrateData(const gui::StyleSize &screen_size, const base::Display &display,
                              StatusBar::Data &data)
{
    const gui::StyleSize *parent_size = &screen_size;

    ESP_UTILS_LOGD("Calibrate data");

    // Calibrate the min and max size
    if (data.flags.enable_main_size_min) {
        ESP_UTILS_CHECK_FALSE_RETURN(display.calibrateCoreObjectSize(screen_size, data.main.size_min), false,
                                     "Calibrate data main size min failed");
    }
    if (data.flags.enable_main_size_max) {
        ESP_UTILS_CHECK_FALSE_RETURN(display.calibrateCoreObjectSize(screen_size, data.main.size_max), false,
                                     "Calibrate data main size max failed");
    }

    /* Main */
    ESP_UTILS_CHECK_FALSE_RETURN(display.calibrateCoreObjectSize(*parent_size, data.main.size), false,
                                 "Calibrate main size failed");
    // Adjust the size according to the min and max size
    if (data.flags.enable_main_size_min) {
        data.main.size.width = max(data.main.size.width, data.main.size_min.width);
        data.main.size.height = max(data.main.size.height, data.main.size_min.height);
    }
    if (data.flags.enable_main_size_max) {
        data.main.size.width = min(data.main.size.width, data.main.size_max.width);
        data.main.size.height = min(data.main.size.height, data.main.size_max.height);
    }
    // Text
    parent_size = &data.main.size;
    ESP_UTILS_CHECK_FALSE_RETURN(display.calibrateCoreFont(parent_size, data.main.text_font), false,
                                 "Calibrate main text font failed");

    /* Area */
    parent_size = &data.main.size;
    ESP_UTILS_CHECK_VALUE_RETURN(data.area.num, 1, StatusBarIcon::IMAGE_NUM_MAX, false,
                                 "Area data num is invalid");
    for (int i = 0; i < data.area.num; i++) {
        ESP_UTILS_CHECK_FALSE_RETURN(display.calibrateCoreObjectSize(*parent_size, data.area.data[i].size), false,
                                     "Calibrate area(%d) size failed", i);
        ESP_UTILS_CHECK_VALUE_RETURN(
            static_cast<int>(data.area.data[i].layout_column_align), static_cast<int>(AreaAlign::UNKNOWN) + 1,
            static_cast<int>(AreaAlign::MAX) - 1, false, "Area(%d) layout align is invalid", i
        );
        ESP_UTILS_CHECK_VALUE_RETURN(data.area.data[i].layout_column_start_offset, 0, data.area.data[i].size.width,
                                     false, "Area(%d) layout start offset is invalid", i);
        ESP_UTILS_CHECK_VALUE_RETURN(data.area.data[i].layout_column_pad, 0, data.area.data[i].size.width,
                                     false, "Area(%d) layout pad is invalid", i);
    }

    /* Icon */
    // Common size
    parent_size = &data.main.size;
    ESP_UTILS_CHECK_FALSE_RETURN(display.calibrateCoreObjectSize(*parent_size, data.icon_common_size), false,
                                 "Calibrate icon common size failed");
    // Battery
    if (data.flags.enable_battery_icon) {
        ESP_UTILS_LOGD("Calibrate battery icon data");
        if (data.flags.enable_battery_icon_common_size) {
            data.battery.icon_data.size = data.icon_common_size;
        }
        ESP_UTILS_CHECK_FALSE_RETURN(calibrateIconData(data, display, data.battery.icon_data), false,
                                     "Calibrate battery icon data failed");
    }
    // Wifi
    if (data.flags.enable_wifi_icon) {
        ESP_UTILS_LOGD("Calibrate wifi icon data");
        if (data.flags.enable_wifi_icon_common_size) {
            data.wifi.icon_data.size = data.icon_common_size;
        }
        ESP_UTILS_CHECK_FALSE_RETURN(calibrateIconData(data, display, data.wifi.icon_data), false,
                                     "Calibrate wifi icon data failed");
    }

    return true;
}

bool StatusBar::beginMain(lv_obj_t *parent)
{
    lv_align_t area_align = LV_ALIGN_DEFAULT;
    ESP_Brookesia_LvObj_t main_obj = nullptr;
    ESP_Brookesia_LvObj_t area_obj = nullptr;
    vector<ESP_Brookesia_LvObj_t> area_objs;

    ESP_UTILS_LOGD("Begin main(@0x%p)", this);
    ESP_UTILS_CHECK_FALSE_RETURN(!checkMainInitialized(), false, "Already initialized");

    /* Create objects */
    // Main
    main_obj = ESP_BROOKESIA_LV_OBJ(obj, parent);
    ESP_UTILS_CHECK_NULL_RETURN(main_obj, false, "Create main object failed");
    // Area
    for (int i = 0; i < _data.area.num; i++) {
        area_obj = ESP_BROOKESIA_LV_OBJ(obj, main_obj.get());
        ESP_UTILS_CHECK_NULL_RETURN(area_obj, false, "Create area object failed");
        area_objs.push_back(area_obj);
    }

    /* Setup objects style */
    // Main
    lv_obj_add_style(main_obj.get(), _system_context.getDisplay().getCoreContainerStyle(), 0);
    lv_obj_set_align(main_obj.get(), LV_ALIGN_TOP_MID);
    lv_obj_set_style_bg_opa(main_obj.get(), LV_OPA_COVER, 0);
    lv_obj_clear_flag(main_obj.get(), LV_OBJ_FLAG_SCROLLABLE);
    // Area
    for (size_t i = 0; i < area_objs.size(); i++) {
        lv_obj_add_style(area_objs[i].get(), _system_context.getDisplay().getCoreContainerStyle(), 0);
        switch (_data.area.data[i].layout_column_align) {
        case StatusBar::AreaAlign::START:
            area_align = LV_ALIGN_LEFT_MID;
            break;
        case StatusBar::AreaAlign::CENTER:
            area_align = LV_ALIGN_CENTER;
            break;
        case StatusBar::AreaAlign::END:
            area_align = LV_ALIGN_RIGHT_MID;
            break;
        default:
            break;
        }
        lv_obj_align(area_objs[i].get(), area_align, 0, 0);
        lv_obj_set_flex_flow(area_objs[i].get(), LV_FLEX_FLOW_ROW);
        lv_obj_clear_flag(area_objs[i].get(), LV_OBJ_FLAG_SCROLLABLE);
    }

    /* Save objects */
    _main_obj = main_obj;
    _area_objs = area_objs;

    /* Update */
    ESP_UTILS_CHECK_FALSE_GOTO(updateMainByNewData(), err, "Update main failed");

    return true;

err:
    ESP_UTILS_CHECK_FALSE_RETURN(delMain(), false, "Delete main failed");

    return false;
}

bool StatusBar::updateMainByNewData(void)
{
    ESP_UTILS_LOGD("Update main(0x%p)", this);
    ESP_UTILS_CHECK_FALSE_RETURN(checkMainInitialized(), false, "Not initialized");

    lv_obj_set_size(_main_obj.get(), _data.main.size.width, _data.main.size.height);
    lv_obj_set_style_text_font(_main_obj.get(), (lv_font_t *)_data.main.text_font.font_resource, 0);
    lv_obj_set_style_text_color(_main_obj.get(), lv_color_hex(_data.main.text_color.color), 0);
    lv_obj_set_style_text_opa(_main_obj.get(), _data.main.text_color.opacity, 0);
    lv_obj_set_style_bg_color(_main_obj.get(), lv_color_hex(_data.main.background_color.color), 0);
    lv_obj_set_style_bg_opa(_main_obj.get(), _data.main.background_color.opacity, 0);

    lv_flex_align_t main_align = LV_FLEX_ALIGN_START;
    for (size_t i = 0; i < _area_objs.size(); i++) {
        lv_obj_set_size(_area_objs[i].get(), _data.area.data[i].size.width, _data.area.data[i].size.height);
        lv_obj_set_style_pad_column(_area_objs[i].get(), _data.area.data[i].layout_column_pad, 0);
        switch (_data.area.data[i].layout_column_align) {
        case StatusBar::AreaAlign::START:
            main_align = LV_FLEX_ALIGN_START;
            lv_obj_set_style_pad_left(_area_objs[i].get(), _data.area.data[i].layout_column_start_offset, 0);
            break;
        case StatusBar::AreaAlign::END:
            main_align = LV_FLEX_ALIGN_END;
            lv_obj_set_style_pad_right(_area_objs[i].get(), _data.area.data[i].layout_column_start_offset, 0);
            break;
        case StatusBar::AreaAlign::CENTER:
            main_align = LV_FLEX_ALIGN_CENTER;
            break;
        default:
            ESP_UTILS_CHECK_FALSE_RETURN(false, false, "Invalid layout align");
            break;
        }
        lv_obj_set_flex_align(_area_objs[i].get(), main_align, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    }

    return true;
}

bool StatusBar::delMain(void)
{
    ESP_UTILS_LOGD("Delete main(0x%p)", this);

    if (!checkMainInitialized()) {
        return true;
    }

    _main_obj.reset();
    _area_objs.clear();

    return true;
}

bool StatusBar::beginBattery(void)
{
    ESP_Brookesia_LvObj_t battery_label = nullptr;

    ESP_UTILS_LOGD("Begin battery(0x%p)", this);
    ESP_UTILS_CHECK_FALSE_RETURN(!checkBatteryInitialized(), false, "Already initialized");

    if (_data.flags.enable_battery_label) {
        battery_label = ESP_BROOKESIA_LV_OBJ(label, _area_objs[_data.battery.area_index].get());
        ESP_UTILS_CHECK_NULL_RETURN(battery_label, false, "Create battery label failed");

        lv_obj_add_style(battery_label.get(), _system_context.getDisplay().getCoreContainerStyle(), 0);
        _battery_label = battery_label;
    }
    if (_data.flags.enable_battery_icon) {
        ESP_UTILS_CHECK_FALSE_RETURN(addIcon(_data.battery.icon_data, _data.battery.area_index, _battery_id), false,
                                     "Add battery icon failed");
    }

    ESP_UTILS_CHECK_FALSE_GOTO(setBatteryPercent(false, 100), err, "Set battery percent failed");

    _is_battery_initialed = true;

    ESP_UTILS_CHECK_FALSE_GOTO(updateBatteryByNewData(), err, "Update battery object style failed");

    return true;

err:
    ESP_UTILS_CHECK_FALSE_RETURN(delBattery(), false, "Delete battery failed");

    return false;
}

bool StatusBar::updateBatteryByNewData(void)
{
    ESP_UTILS_LOGD("Update battery(0x%p)", this);
    ESP_UTILS_CHECK_FALSE_RETURN(checkBatteryInitialized(), false, "Not initialized");

    if (_data.flags.enable_battery_label) {
        if (_is_battery_lable_out_of_area) {
            _is_battery_lable_out_of_area = false;
            lv_obj_clear_flag(_battery_label.get(), LV_OBJ_FLAG_HIDDEN);
        }
        if (esp_brookesia_core_utils_check_obj_out_of_parent(_battery_label.get())) {
            _is_battery_lable_out_of_area = true;
            lv_obj_add_flag(_battery_label.get(), LV_OBJ_FLAG_HIDDEN);
            ESP_UTILS_LOGE("Battery label out of area, hide it");
        } else {
            lv_obj_set_style_text_color(_battery_label.get(), lv_color_hex(_data.main.text_color.color), 0);
            lv_obj_set_style_text_opa(_battery_label.get(), _data.main.text_color.opacity, 0);
        }
    }

    return true;
}

bool StatusBar::delBattery(void)
{
    ESP_UTILS_LOGD("Delete battery(0x%p)", this);

    if (!checkBatteryInitialized()) {
        return true;
    }

    if (checkMainInitialized() && _id_icon_map.find(_battery_id) != _id_icon_map.end()) {
        ESP_UTILS_CHECK_FALSE_RETURN(removeIcon(_battery_id), false, "Remove battery icon failed");
    }
    _battery_label.reset();
    _is_battery_initialed = false;

    return true;
}

bool StatusBar::setBatteryPercent(bool charge_flag, int percent) const
{
    ESP_UTILS_LOGD("Set battery percent(0x%p: %d%%)", this, percent);

    percent = max(min(percent, 100), 1);
    if (_data.flags.enable_battery_label && (_battery_label != nullptr)) {
        lv_label_set_text_fmt(_battery_label.get(), "%d%%", percent);
    }

    if (_data.flags.enable_battery_icon) {
        if (charge_flag) {
            _battery_state = 4;
        } else {
            _battery_state = (int)((percent - 1) / 25);
        }
        ESP_UTILS_CHECK_FALSE_RETURN(setIconState(_battery_id, _battery_state), false, "Set battery icon state failed");
    }

    return true;
}

bool StatusBar::showBatteryPercent(void) const
{
    ESP_UTILS_LOGD("Show battery percent(0x%p)", this);
    ESP_UTILS_CHECK_NULL_RETURN(_battery_label, false, "No battery label");

    lv_obj_clear_flag(_battery_label.get(), LV_OBJ_FLAG_HIDDEN);

    return true;
}

bool StatusBar::hideBatteryPercent(void) const
{
    ESP_UTILS_LOGD("Hide battery percent(0x%p)", this);
    ESP_UTILS_CHECK_NULL_RETURN(_battery_label, false, "No battery label");

    lv_obj_add_flag(_battery_label.get(), LV_OBJ_FLAG_HIDDEN);

    return true;
}

bool StatusBar::showBatteryIcon(void) const
{
    ESP_UTILS_LOGD("Show battery icon(0x%p)", this);
    ESP_UTILS_CHECK_FALSE_RETURN(setIconState(_battery_id, _battery_state), false, "Set battery icon state failed");

    return true;
}

bool StatusBar::hideBatteryIcon(void) const
{
    ESP_UTILS_LOGD("Hide battery icon(0x%p)", this);
    ESP_UTILS_CHECK_FALSE_RETURN(setIconState(_battery_id, -1), false, "Set battery icon state failed");

    return true;
}

bool StatusBar::beginWifi(void)
{
    ESP_UTILS_LOGD("Begin wifi(0x%p)", this);

    ESP_UTILS_CHECK_FALSE_RETURN(addIcon(_data.wifi.icon_data, _data.wifi.area_index, _wifi_id), false,
                                 "Add wifi icon failed");
    ESP_UTILS_CHECK_FALSE_GOTO(setWifiIconState(0), err, "Set wifi state failed");

    return true;

err:
    ESP_UTILS_CHECK_FALSE_RETURN(removeIcon(_wifi_id), false, "Delete wifi failed");

    return false;
}

bool StatusBar::setWifiIconState(int state) const
{
    ESP_UTILS_LOGD("Set wifi icon state(0x%p: %d)", this, state);
    ESP_UTILS_CHECK_FALSE_RETURN(setIconState(_wifi_id, state), false, "Set wifi icon state failed");

    return true;
}

bool StatusBar::setWifiIconState(WifiState state) const
{
    ESP_UTILS_LOGD("Set wifi icon state(0x%p: %d)", this, static_cast<int>(state));
    ESP_UTILS_CHECK_FALSE_RETURN(
        setIconState(_wifi_id, static_cast<int>(state)), false, "Set wifi icon state failed"
    );

    return true;
}

bool StatusBar::beginClock(void)
{
    ESP_Brookesia_LvObj_t clock_obj = nullptr;
    ESP_Brookesia_LvObj_t clock_hour_label = nullptr;
    ESP_Brookesia_LvObj_t clock_dot_label = nullptr;
    ESP_Brookesia_LvObj_t clock_min_label = nullptr;
    ESP_Brookesia_LvObj_t clock_period_label = nullptr;

    ESP_UTILS_LOGD("Begin clock(0x%p)", this);
    ESP_UTILS_CHECK_FALSE_RETURN(!checkClockInitialized(), false, "Already initialized");

    /* Create objects */
    clock_obj = ESP_BROOKESIA_LV_OBJ(obj, _area_objs[_data.clock.area_index].get());
    ESP_UTILS_CHECK_NULL_RETURN(clock_obj, false, "Alloc clock object failed");

    clock_hour_label = ESP_BROOKESIA_LV_OBJ(label, clock_obj.get());
    ESP_UTILS_CHECK_NULL_RETURN(clock_hour_label, false, "Alloc clock hour label failed");
    lv_obj_add_style(clock_hour_label.get(), _system_context.getDisplay().getCoreContainerStyle(), 0);

    clock_dot_label = ESP_BROOKESIA_LV_OBJ(label, clock_obj.get());
    ESP_UTILS_CHECK_NULL_RETURN(clock_dot_label, false, "Alloc clock dot label failed");
    lv_obj_add_style(clock_dot_label.get(), _system_context.getDisplay().getCoreContainerStyle(), 0);
    lv_label_set_text(clock_dot_label.get(), ":");

    clock_min_label = ESP_BROOKESIA_LV_OBJ(label, clock_obj.get());
    ESP_UTILS_CHECK_NULL_RETURN(clock_min_label, false, "Alloc clock min label failed");
    lv_obj_add_style(clock_min_label.get(), _system_context.getDisplay().getCoreContainerStyle(), 0);

    clock_period_label = ESP_BROOKESIA_LV_OBJ(label, clock_obj.get());
    ESP_UTILS_CHECK_NULL_RETURN(clock_period_label, false, "Alloc clock period label failed");
    lv_obj_add_style(clock_period_label.get(), _system_context.getDisplay().getCoreContainerStyle(), 0);

    // Setup objects style
    lv_obj_add_style(clock_obj.get(), _system_context.getDisplay().getCoreContainerStyle(), 0);
    lv_obj_set_size(clock_obj.get(), LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(clock_obj.get(), LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(clock_obj.get(), LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(clock_obj.get(), 0, 0);
    lv_obj_clear_flag(clock_obj.get(), LV_OBJ_FLAG_SCROLLABLE);

    _clock_obj = clock_obj;
    _clock_hour_label = clock_hour_label;
    _clock_dot_label = clock_dot_label;
    _clock_min_label = clock_min_label;
    _clock_period_label = clock_period_label;

    ESP_UTILS_CHECK_FALSE_GOTO(updateClockByNewData(), err, "Update clock style failed");
    ESP_UTILS_CHECK_FALSE_GOTO(setClockFormat(_clock_format), err, "Set clock format failed");
    ESP_UTILS_CHECK_FALSE_GOTO(setClock(_clock_hour, _clock_min, false), err, "Set clock failed");

    return true;

err:
    ESP_UTILS_CHECK_FALSE_RETURN(delClock(), false, "Delete clock failed");

    return false;
}

bool StatusBar::updateClockByNewData(void)
{
    ESP_UTILS_LOGD("Update clock(0x%p)", this);
    ESP_UTILS_CHECK_FALSE_RETURN(checkClockInitialized(), false, "Not initialized");

    if (_is_clock_out_of_area) {
        _is_clock_out_of_area = false;
        lv_obj_clear_flag(_clock_obj.get(), LV_OBJ_FLAG_HIDDEN);
    }
    if (esp_brookesia_core_utils_check_obj_out_of_parent(_clock_obj.get())) {
        _is_clock_out_of_area = true;
        lv_obj_add_flag(_clock_obj.get(), LV_OBJ_FLAG_HIDDEN);
        ESP_UTILS_LOGE("Clock out of area, hide it");
    } else {
        lv_obj_set_style_text_color(_clock_hour_label.get(), lv_color_hex(_data.main.text_color.color), 0);
        lv_obj_set_style_text_opa(_clock_hour_label.get(), _data.main.text_color.opacity, 0);
        lv_obj_set_style_text_color(_clock_min_label.get(), lv_color_hex(_data.main.text_color.color), 0);
        lv_obj_set_style_text_opa(_clock_min_label.get(), _data.main.text_color.opacity, 0);
        lv_obj_set_style_text_color(_clock_dot_label.get(), lv_color_hex(_data.main.text_color.color), 0);
        lv_obj_set_style_text_opa(_clock_dot_label.get(), _data.main.text_color.opacity, 0);
        lv_obj_set_style_text_color(_clock_period_label.get(), lv_color_hex(_data.main.text_color.color), 0);
        lv_obj_set_style_text_opa(_clock_period_label.get(), _data.main.text_color.opacity, 0);
    }

    return true;
}

bool StatusBar::delClock(void)
{
    ESP_UTILS_LOGD("Delete clock(0x%p)", this);

    if (!checkClockInitialized()) {
        return true;
    }

    _clock_obj.reset();
    _clock_hour_label.reset();
    _clock_dot_label.reset();
    _clock_min_label.reset();
    _clock_period_label.reset();

    return true;
}

bool StatusBar::setClockFormat(ClockFormat format) const
{
    ESP_UTILS_LOGD("Set clock format(%d)", static_cast<int>(format));
    ESP_UTILS_CHECK_NULL_RETURN(_clock_period_label, false, "Invalid clock period label");

    switch (format) {
    case ClockFormat::FORMAT_12H:
        lv_obj_clear_flag(_clock_period_label.get(), LV_OBJ_FLAG_HIDDEN);
        break;
    case ClockFormat::FORMAT_24H:
        lv_obj_add_flag(_clock_period_label.get(), LV_OBJ_FLAG_HIDDEN);
        break;
    default:
        ESP_UTILS_CHECK_FALSE_RETURN(false, false, "Invalid clock format");
        break;
    }

    _clock_format = format;

    return true;
}

bool StatusBar::setClock(int hour, int minute, bool is_pm) const
{
    ESP_UTILS_LOGD("Set clock(%02d:%02d %s)", hour, minute, is_pm ? "PM" : "AM");
    ESP_UTILS_CHECK_NULL_RETURN(_clock_obj, false, "Invalid clock");

    hour = max(min(hour, 23), 0);
    minute = max(min(minute, 59), 0);

    if (_clock_hour != hour) {
        _clock_hour = hour;
        if (_clock_format == ClockFormat::FORMAT_12H) {
            hour = hour % 12;
            if (hour == 0) {
                hour = 12;
            }
        }
        lv_label_set_text_fmt(_clock_hour_label.get(), "%02d", hour);
    }
    if (_clock_min != minute) {
        _clock_min = minute;
        lv_label_set_text_fmt(_clock_min_label.get(), "%02d", minute);
    }
    if (_clock_format == ClockFormat::FORMAT_12H) {
        lv_label_set_text(_clock_period_label.get(), is_pm ? " PM " : " AM ");
    }

    return true;
}

bool StatusBar::setClock(int hour, int minute) const
{
    ESP_UTILS_LOGD("Set clock(%02d:%02d)", hour, minute);

    hour = max(min(hour, 23), 0);
    minute = max(min(minute, 59), 0);
    bool is_pm = hour >= 12;

    ESP_UTILS_CHECK_FALSE_RETURN(setClock(hour, minute, is_pm), false, "Set clock failed");

    return true;
}

void StatusBar::onDataUpdateEventCallback(lv_event_t *event)
{
    StatusBar *status_bar = nullptr;

    ESP_UTILS_CHECK_NULL_EXIT(event, "Invalid event object");

    ESP_UTILS_LOGD("Data update event callback");
    status_bar = (StatusBar *)lv_event_get_user_data(event);
    ESP_UTILS_CHECK_NULL_EXIT(status_bar, "Invalid status bar object");

    // Main
    ESP_UTILS_CHECK_FALSE_EXIT(status_bar->updateMainByNewData(), "Update main object style failed");
    for (auto &icon : status_bar->_id_icon_map) {
        if (!icon.second->updateByNewData()) {
            ESP_UTILS_LOGE("Update icon(%d) style failed", icon.first);
        }
    }
    // Battery
    if (status_bar->checkBatteryInitialized() && !status_bar->updateBatteryByNewData()) {
        ESP_UTILS_LOGE("Update battery object style failed");
    }
    // Clock
    if (status_bar->checkClockInitialized() && !status_bar->updateClockByNewData()) {
        ESP_UTILS_LOGE("Update clock object style failed");
    }
}

} // namespace esp_brookesia::systems::phone
