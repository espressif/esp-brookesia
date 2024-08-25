/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <limits>
#include <memory>
#include "core/esp_ui_core.hpp"
#include "esp_ui_app_launcher.hpp"

#if !ESP_UI_LOG_ENABLE_DEBUG_WIDGETS_APP_LAUNCHER
#undef ESP_UI_LOGD
#define ESP_UI_LOGD(...)
#endif

#define ESP_UI_APP_LAUNCHER_SPOT_INACTIVE_STATE     LV_STATE_DEFAULT
#define ESP_UI_APP_LAUNCHER_SPOT_ACTIVE_STATE       LV_STATE_USER_1

using namespace std;

ESP_UI_AppLauncher::ESP_UI_AppLauncher(ESP_UI_Core &core, const ESP_UI_AppLauncherData_t &data):
    _core(core),
    _data(data),
    _table_current_page_index(-1),
    _table_page_icon_count_max(0),
    _table_page_pad_row(0),
    _table_page_pad_column(0),
    _main_obj(nullptr),
    _table_obj(nullptr),
    _indicator_obj(nullptr)
{
}

ESP_UI_AppLauncher::~ESP_UI_AppLauncher()
{
    ESP_UI_LOGD("Destroy(0x%p)", this);
    if (!del()) {
        ESP_UI_LOGE("Delete failed");
    }
}

bool ESP_UI_AppLauncher::begin(lv_obj_t *parent)
{
    ESP_UI_LvObj_t main_obj = nullptr;
    ESP_UI_LvObj_t table_obj = nullptr;
    ESP_UI_LvObj_t page_obj = nullptr;
    ESP_UI_LvObj_t indicator_obj = nullptr;
    ESP_UI_LvObj_t spot_obj = nullptr;
    vector <ESP_UI_AppLauncherMixObject_t> mix_objs;

    ESP_UI_LOGD("Begin(0x%p)", this);
    ESP_UI_CHECK_NULL_RETURN(parent, false, "Invalid parent");
    ESP_UI_CHECK_FALSE_RETURN(!checkInitialized(), false, "Initialized");

    /* Create objects */
    // Main
    main_obj = ESP_UI_LV_OBJ(obj, parent);
    ESP_UI_CHECK_NULL_RETURN(main_obj, false, "Create main_obj failed");
    // Table
    table_obj = ESP_UI_LV_OBJ(obj, main_obj.get());
    ESP_UI_CHECK_NULL_RETURN(table_obj, false, "Create table_obj failed");
    // Spot
    indicator_obj = ESP_UI_LV_OBJ(obj, main_obj.get());
    ESP_UI_CHECK_NULL_RETURN(indicator_obj, false, "Create indicator_obj failed");
    // Mix objects
    for (int i = 0; i < _data.table.default_num; i++) {
        ESP_UI_CHECK_FALSE_RETURN(createMixObject(table_obj, indicator_obj, mix_objs), false,
                                  "Create mix object failed");
    }

    /* Setup objects style */
    // Main
    lv_obj_add_style(main_obj.get(), _core.getCoreHome().getCoreContainerStyle(), 0);
    // Table
    lv_obj_add_style(table_obj.get(), _core.getCoreHome().getCoreContainerStyle(), 0);
    lv_obj_align(table_obj.get(), LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_flex_flow(table_obj.get(), LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(table_obj.get(), LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(table_obj.get(), LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_snap_x(table_obj.get(), LV_SCROLL_SNAP_CENTER);
    lv_obj_clear_flag(table_obj.get(), LV_OBJ_FLAG_SCROLLABLE);
    // Indicator
    lv_obj_add_style(indicator_obj.get(), _core.getCoreHome().getCoreContainerStyle(), 0);
    lv_obj_set_flex_flow(indicator_obj.get(), LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(indicator_obj.get(), LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    // Event
    ESP_UI_CHECK_FALSE_RETURN(_core.registerDateUpdateEventCallback(onDataUpdateEventCallback, this), false,
                              "Register data update event callback failed");

    /* Save objects */
    _main_obj = main_obj;
    _table_obj = table_obj;
    _indicator_obj = indicator_obj;
    _mix_objs = mix_objs;

    /* Update */
    ESP_UI_CHECK_FALSE_GOTO(updateByNewData(), err, "Update failed");

    /* Other operations */
    ESP_UI_CHECK_FALSE_RETURN(scrollToPage(0), false, "Change to default screen failed");
    ESP_UI_CHECK_FALSE_RETURN(updateActiveSpot(), false, "Update active spot failed");

    return true;

err:
    ESP_UI_CHECK_FALSE_RETURN(del(), false, "Delete failed");

    return false;
}

bool ESP_UI_AppLauncher::del(void)
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
    _table_obj.reset();
    _indicator_obj.reset();
    _mix_objs.clear();
    _id_mix_icon_map.clear();

    return ret;
}

bool ESP_UI_AppLauncher::addIcon(uint8_t page_index, const ESP_UI_AppLauncherIconInfo_t &info)
{
    int table_last_page_index = _table_current_page_index;
    ESP_UI_AppLauncherMixIcon_t mix_icon = {};

    ESP_UI_LOGD("Add icon(%d) to table(%d)", info.id, page_index);
    ESP_UI_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_UI_CHECK_NULL_RETURN(info.name, false, "Invalid icon name");
    ESP_UI_CHECK_FALSE_RETURN(page_index < _mix_objs.size(), false, "Table index out of range");

    mix_icon.target_page_index = page_index;
    if (_mix_objs[page_index].page_icon_count >= _table_page_icon_count_max) {
        for (size_t i = 0; i < _mix_objs.size(); i++) {
            if (!checkTableFull(i)) {
                page_index = i;
                break;
            }
        }
        if (_mix_objs[page_index].page_icon_count >= _table_page_icon_count_max) {
            ESP_UI_LOGW("All table pages are full, create a new page");
            page_index = _mix_objs.size();
            ESP_UI_CHECK_FALSE_RETURN(createMixObject(_table_obj, _indicator_obj, _mix_objs), false,
                                      "Create mix object failed");
            ESP_UI_CHECK_FALSE_RETURN(updateMixByNewData(page_index, _mix_objs), false,
                                      "Update mix object style failed");
            _table_current_page_index = page_index;
            ESP_UI_CHECK_FALSE_RETURN(scrollToPage(table_last_page_index), false, "Scroll to page failed");
        }
    }
    mix_icon.current_page_index = page_index;

    mix_icon.icon = make_shared<ESP_UI_AppLauncherIcon>(_core, info, _data.icon);
    ESP_UI_CHECK_NULL_RETURN(mix_icon.icon, false, "Create icon failed");

    ESP_UI_CHECK_FALSE_RETURN(mix_icon.icon->begin(_mix_objs[page_index].page_obj.get()), false,
                              "Begin icon failed");

    auto res = _id_mix_icon_map.insert(pair<int, ESP_UI_AppLauncherMixIcon_t>(info.id, mix_icon));
    ESP_UI_CHECK_FALSE_RETURN(res.second, false, "Insert icon failed");

    _mix_objs[page_index].page_icon_count++;

    return true;
}

bool ESP_UI_AppLauncher::removeIcon(int id)
{
    uint8_t current_page_index = 0;

    ESP_UI_LOGD("Remove icon(%d)", id);
    ESP_UI_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    auto res = _id_mix_icon_map.find(id);
    ESP_UI_CHECK_FALSE_RETURN(res != _id_mix_icon_map.end(), false, "Icon not found");
    ESP_UI_CHECK_NULL_RETURN(res->second.icon, false, "Invalid icon");
    current_page_index = res->second.current_page_index;
    ESP_UI_CHECK_VALUE_RETURN(current_page_index, 0, (int)_mix_objs.size() - 1, false, "Table index out of range");

    _mix_objs[current_page_index].page_icon_count--;
    _id_mix_icon_map.erase(id);

    if ((_mix_objs[current_page_index].page_icon_count == 0) && (_mix_objs.size() > _data.table.default_num)) {
        ESP_UI_CHECK_FALSE_RETURN(destoryMixObject(current_page_index, _mix_objs), false, "Destroy mix object failed");
    }

    return true;
}

bool ESP_UI_AppLauncher::changeIconTable(int id, uint8_t new_table_index)
{
    ESP_UI_LOGD("Change icon(%d) table to %d", id, new_table_index);
    ESP_UI_CHECK_VALUE_RETURN(new_table_index, 0, (int)_mix_objs.size(), false, "Table index out of range");
    ESP_UI_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    auto res = _id_mix_icon_map.find(id);
    ESP_UI_CHECK_FALSE_RETURN(res != _id_mix_icon_map.end(), false, "Icon not found");
    ESP_UI_CHECK_NULL_RETURN(res->second.icon, false, "Invalid icon");

    ESP_UI_CHECK_FALSE_RETURN(res->second.icon->del(), false, "Delete icon failed");
    ESP_UI_CHECK_FALSE_RETURN(res->second.icon->begin(_mix_objs[new_table_index].page_obj.get()), false,
                              "Begin icon failed");

    if (res->second.current_page_index < (int)_mix_objs.size()) {
        _mix_objs[res->second.current_page_index].page_icon_count--;
    }
    _mix_objs[new_table_index].page_icon_count++;
    res->second.current_page_index = new_table_index;

    return true;
}

bool ESP_UI_AppLauncher::scrollToPage(uint8_t index)
{
    ESP_UI_LOGD("Scroll to page(%d)", index);
    ESP_UI_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_UI_CHECK_VALUE_RETURN(index, 0, (int)_mix_objs.size() - 1, false, "Table index out of range");

    if (_table_current_page_index == index) {
        return true;
    }

    lv_obj_add_flag(_table_obj.get(), LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_scroll_to_view_recursive(_mix_objs[index].page_obj.get(), _data.flags.enable_table_scroll_anim ?
                                    LV_ANIM_ON : LV_ANIM_OFF);
    lv_obj_clear_flag(_table_obj.get(), LV_OBJ_FLAG_SCROLLABLE);

    _table_current_page_index = index;

    ESP_UI_CHECK_FALSE_RETURN(updateActiveSpot(), false, "Update active spot failed");

    return true;
}

bool ESP_UI_AppLauncher::scrollToRightPage(void)
{
    ESP_UI_LOGD("Current page is %d, scroll to right page", _table_current_page_index);
    ESP_UI_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    if (_table_current_page_index >= (int)_mix_objs.size() - 1) {
        ESP_UI_LOGD("The current page is the last page");
        return true;
    }

    return scrollToPage(_table_current_page_index + 1);
}

bool ESP_UI_AppLauncher::scrollToLeftPage(void)
{
    ESP_UI_LOGD("Current page is %d, scroll to left page", _table_current_page_index);
    ESP_UI_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    if (_table_current_page_index <= 0) {
        ESP_UI_LOGD("The current page is the first page");
        return true;
    }

    return scrollToPage(_table_current_page_index - 1);
}

bool ESP_UI_AppLauncher::checkTableFull(uint8_t page_index) const
{
    ESP_UI_CHECK_VALUE_RETURN(page_index, 0, (int)_mix_objs.size() - 1, false, "Table index out of range");
    ESP_UI_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    return (_mix_objs[page_index].page_icon_count >= _table_page_icon_count_max);
}

bool ESP_UI_AppLauncher::checkVisible(void) const
{
    ESP_UI_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    return lv_obj_is_visible(_main_obj.get());
}

bool ESP_UI_AppLauncher::checkPointInsideMain(lv_point_t &point) const
{
    lv_area_t area = {};

    ESP_UI_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    lv_obj_refr_pos(_main_obj.get());
    lv_obj_get_coords(_main_obj.get(), &area);

    return _lv_area_is_point_on(&area, &point, lv_obj_get_style_radius(_main_obj.get(), 0));
}

bool ESP_UI_AppLauncher::calibrateData(const ESP_UI_StyleSize_t &screen_size, const ESP_UI_CoreHome &home,
                                       ESP_UI_AppLauncherData_t &data)
{
    uint16_t parent_w = 0;
    uint16_t parent_h = 0;
    const ESP_UI_StyleSize_t *parent_size = nullptr;

    ESP_UI_LOGD("Calibrate data");

    /* Main */
    parent_size = &screen_size;
    parent_w = parent_size->width;
    parent_h = parent_size->height;
    ESP_UI_CHECK_FALSE_RETURN(home.calibrateCoreObjectSize(*parent_size, data.main.size), false,
                              "Invalid main size");
    ESP_UI_CHECK_VALUE_RETURN(data.main.y_start, 0, parent_h - 1, false, "Invalid main y start");
    ESP_UI_CHECK_VALUE_RETURN(data.main.y_start + data.main.size.height, 1, parent_h, false,
                              "Main height is out of range");

    /* Table */
    parent_size = &data.main.size;
    parent_w = parent_size->width;
    parent_h = parent_size->height;
    ESP_UI_CHECK_FALSE_RETURN(data.table.default_num > 0, false, "Invalid table default number");
    ESP_UI_CHECK_FALSE_RETURN(home.calibrateCoreObjectSize(*parent_size, data.table.size), false,
                              "Invalid table size");

    /* Spot */
    parent_size = &data.main.size;
    parent_w = parent_size->width;
    parent_h = parent_size->height;
    // Main
    ESP_UI_CHECK_FALSE_RETURN(home.calibrateCoreObjectSize(*parent_size, data.indicator.main_size), false,
                              "Invalid spot main size");
    ESP_UI_CHECK_VALUE_RETURN(data.indicator.main_layout_column_pad, 1, parent_w, false,
                              "Invalid spot main layout column pad");
    ESP_UI_CHECK_VALUE_RETURN(data.indicator.main_layout_bottom_offset, 0, parent_h, false,
                              "Invalid spot main layout bottom offset");
    // Icon
    parent_size = &data.indicator.main_size;
    parent_w = parent_size->width;
    parent_h = parent_size->height;
    ESP_UI_CHECK_FALSE_RETURN(home.calibrateCoreObjectSize(*parent_size, data.indicator.spot_inactive_size), false,
                              "Invalid spot icon inactive size");
    ESP_UI_CHECK_FALSE_RETURN(home.calibrateCoreObjectSize(*parent_size, data.indicator.spot_active_size), false,
                              "Invalid spot icon active size");

    /* Launcher Icon */
    // Main
    parent_size = &data.table.size;
    parent_w = parent_size->width;
    parent_h = parent_size->height;
    ESP_UI_CHECK_FALSE_RETURN(home.calibrateCoreObjectSize(*parent_size, data.icon.main.size), false,
                              "Invalid launcher icon main size");
    ESP_UI_CHECK_VALUE_RETURN(data.icon.main.layout_row_pad, 1, data.icon.main.size.height, false,
                              "Invalid launcher icon main layout row pad");
    // Image
    parent_size = &data.icon.main.size;
    parent_w = parent_size->width;
    parent_h = parent_size->height;
    ESP_UI_CHECK_FALSE_RETURN(home.calibrateCoreObjectSize(*parent_size, data.icon.image.default_size), false,
                              "Invalid launcher icon image default size");
    ESP_UI_CHECK_FALSE_RETURN(home.calibrateCoreObjectSize(*parent_size, data.icon.image.press_size), false,
                              "Invalid launcher icon image press size");
    // Label
    ESP_UI_CHECK_FALSE_RETURN(home.calibrateCoreFont(nullptr, data.icon.label.text_font), false,
                              "Invalid label text font");

    return true;
}

bool ESP_UI_AppLauncher::createMixObject(ESP_UI_LvObj_t &table_obj, ESP_UI_LvObj_t &indicator_obj,
        vector<ESP_UI_AppLauncherMixObject_t> &mix_objs)
{
    ESP_UI_LvObj_t page_main_obj = nullptr;
    ESP_UI_LvObj_t page_obj = nullptr;
    ESP_UI_LvObj_t spot_obj = nullptr;

    ESP_UI_LOGD("Create mix object");
    ESP_UI_CHECK_NULL_RETURN(table_obj.get(), false, "Invalid table object");
    ESP_UI_CHECK_NULL_RETURN(indicator_obj.get(), false, "Invalid indicator object");

    page_main_obj = ESP_UI_LV_OBJ(obj, table_obj.get());
    ESP_UI_CHECK_NULL_RETURN(page_main_obj, false, "Create page_main_obj failed");
    page_obj = ESP_UI_LV_OBJ(obj, page_main_obj.get());
    ESP_UI_CHECK_NULL_RETURN(page_obj, false, "Create page_obj failed");
    spot_obj = ESP_UI_LV_OBJ(obj, indicator_obj.get());
    ESP_UI_CHECK_NULL_RETURN(spot_obj, false, "Create spot_obj failed");

    lv_obj_add_style(page_main_obj.get(), _core.getCoreHome().getCoreContainerStyle(), 0);

    lv_obj_center(page_obj.get());
    lv_obj_add_style(page_obj.get(), _core.getCoreHome().getCoreContainerStyle(), 0);
    lv_obj_set_flex_flow(page_obj.get(), LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(page_obj.get(), LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_clear_flag(page_obj.get(), LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_add_style(spot_obj.get(), _core.getCoreHome().getCoreContainerStyle(), 0);
    lv_obj_set_style_radius(spot_obj.get(), LV_RADIUS_CIRCLE, 0);

    mix_objs.push_back({0, page_main_obj, page_obj, spot_obj});

    return true;
}

bool ESP_UI_AppLauncher::updateMixByNewData(uint8_t index, vector<ESP_UI_AppLauncherMixObject_t> &mix_objs)
{
    ESP_UI_LOGD("Update mix object(%d) style", index);
    ESP_UI_CHECK_VALUE_RETURN(index, 0, (int)mix_objs.size() - 1, false, "Table page index out of range");

    ESP_UI_LvObj_t &page_main_obj = mix_objs[index].page_main_obj;
    ESP_UI_LvObj_t &page_obj = mix_objs[index].page_obj;
    ESP_UI_LvObj_t &spot_obj = mix_objs[index].spot_obj;

    // Table
    lv_obj_set_size(page_main_obj.get(), _data.table.size.width, _data.table.size.height);
    // lv_obj_set_size(page_obj.get(), _table_page_width, _table_page_height);
    lv_obj_set_style_pad_row(page_obj.get(), _table_page_pad_row, 0);
    lv_obj_set_style_pad_ver(page_obj.get(), _table_page_pad_row, 0);
    lv_obj_set_style_pad_column(page_obj.get(), _table_page_pad_column, 0);
    lv_obj_set_style_pad_hor(page_obj.get(), _table_page_pad_column, 0);
    lv_obj_set_size(page_obj.get(), _data.table.size.width, _data.table.size.height);
    // Indicator
    lv_obj_set_size(spot_obj.get(), _data.indicator.spot_inactive_size.width, _data.indicator.spot_inactive_size.height);
    lv_obj_set_style_bg_color(spot_obj.get(), lv_color_hex(_data.indicator.spot_active_background_color.color),
                              ESP_UI_APP_LAUNCHER_SPOT_ACTIVE_STATE);
    lv_obj_set_style_bg_opa(spot_obj.get(), _data.indicator.spot_active_background_color.opacity,
                            ESP_UI_APP_LAUNCHER_SPOT_ACTIVE_STATE);
    lv_obj_set_style_bg_color(spot_obj.get(), lv_color_hex(_data.indicator.spot_inactive_background_color.color),
                              ESP_UI_APP_LAUNCHER_SPOT_INACTIVE_STATE);
    lv_obj_set_style_bg_opa(spot_obj.get(), _data.indicator.spot_inactive_background_color.opacity,
                            ESP_UI_APP_LAUNCHER_SPOT_INACTIVE_STATE);

    return true;
}

bool ESP_UI_AppLauncher::destoryMixObject(uint8_t index, vector <ESP_UI_AppLauncherMixObject_t> &mix_objs)
{
    ESP_UI_LOGD("Destroy mix object(%d)", index);
    ESP_UI_CHECK_VALUE_RETURN(index, 0, (int)mix_objs.size() - 1, false, "Table page index out of range");
    ESP_UI_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    mix_objs.erase(mix_objs.begin() + index);

    return true;
}

bool ESP_UI_AppLauncher::updateActiveSpot(void)
{
    ESP_UI_LOGD("Update active spot");
    ESP_UI_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    for (size_t i = 0; i < _mix_objs.size(); i++) {
        if ((int)i == _table_current_page_index) {
            lv_obj_add_state(_mix_objs[i].spot_obj.get(), ESP_UI_APP_LAUNCHER_SPOT_ACTIVE_STATE);
            lv_obj_set_size(_mix_objs[i].spot_obj.get(), _data.indicator.spot_active_size.width,
                            _data.indicator.spot_active_size.height);
        } else {
            lv_obj_clear_state(_mix_objs[i].spot_obj.get(), ESP_UI_APP_LAUNCHER_SPOT_ACTIVE_STATE);
            lv_obj_set_size(_mix_objs[i].spot_obj.get(), _data.indicator.spot_inactive_size.width,
                            _data.indicator.spot_inactive_size.height);
        }
    }

    return true;
}

bool ESP_UI_AppLauncher::updateByNewData(void)
{
    uint8_t app_num_hor = 0;
    uint8_t app_num_ver = 0;
    uint8_t new_table_num = 0;
    uint8_t old_table_num = 0;
    uint8_t new_table_icon_count_max = 0;
    uint8_t old_table_icon_count_max = 0;
    ESP_UI_LvObj_t table_obj = nullptr;
    ESP_UI_LvObj_t page_obj = nullptr;
    ESP_UI_LvObj_t spot_obj = nullptr;

    ESP_UI_LOGD("Update(0x%p)", this);
    ESP_UI_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    // Calculate the max amount of app's icons in column and row.
    app_num_hor = _data.table.size.width / _data.icon.main.size.width;
    app_num_ver = _data.table.size.height / _data.icon.main.size.height;
    ESP_UI_CHECK_FALSE_RETURN(app_num_hor > 0 && app_num_ver > 0, false, "Invalid app number");
    new_table_icon_count_max = app_num_hor * app_num_ver;
    new_table_num = max((int)_data.table.default_num, (int)(_id_mix_icon_map.size() + new_table_icon_count_max - 1) /
                        new_table_icon_count_max);
    old_table_num = _mix_objs.size();
    old_table_icon_count_max = _table_page_icon_count_max;

    // Save the new table pad size and icon count max
    _table_page_pad_column = (_data.table.size.width - app_num_hor * _data.icon.main.size.width) / (app_num_hor + 1);
    _table_page_pad_row = (_data.table.size.height - app_num_ver * _data.icon.main.size.height) / (app_num_ver + 1);
    _table_page_icon_count_max = new_table_icon_count_max;

    // Check if the table number is changed
    if (old_table_num > new_table_num) {
        ESP_UI_LOGW("The table number is too large, change: %d->%d", old_table_num, new_table_num);
        for (auto &id_icon : _id_mix_icon_map) {
            // Check if the icons need to be moved to the new tables
            if (id_icon.second.current_page_index >= new_table_num) {
                // Process the icons which table index is out of range
                for (int i = 0; i < new_table_num; i++) {
                    if (!checkTableFull(i)) {
                        ESP_UI_LOGD("Change icon(%d) table: %d->%d", id_icon.first, id_icon.second.current_page_index, i);
                        ESP_UI_CHECK_FALSE_RETURN(changeIconTable(id_icon.first, i), false, "Change icon table failed");
                        id_icon.second.current_page_index = i;
                        break;
                    }
                }
                ESP_UI_CHECK_FALSE_RETURN(id_icon.second.current_page_index < new_table_num, false,
                                          "Change icon table failed");
                continue;
            }
        }
        // If the new table number is less than the old table number, remove the old tables
        for (int i = new_table_num; i < old_table_num; i++) {
            ESP_UI_CHECK_FALSE_RETURN(destoryMixObject(i, _mix_objs), false, "Destroy mix object(%d) failed", i);
        }
    } else if (old_table_num < new_table_num) {
        ESP_UI_LOGW("The table number is insufficient, change: %d->%d", old_table_num, new_table_num);
        // If the new table number is greater than the old table number, add the new tables
        for (int i = old_table_num; i < new_table_num; i++) {
            ESP_UI_CHECK_FALSE_RETURN(createMixObject(_table_obj, _indicator_obj, _mix_objs), false,
                                      "Create mix object failed");
        }
    }
    // Check if the icon table is full
    if (old_table_icon_count_max > new_table_icon_count_max) {
        for (auto it = _id_mix_icon_map.rbegin(); it != _id_mix_icon_map.rend(); it++) {
            // Check if the icons need to be moved to the new tables
            if (_mix_objs[it->second.current_page_index].page_icon_count > new_table_icon_count_max) {
                // Move the redundant icons to other tables
                for (int i = 0; i < new_table_num; i++) {
                    if (!checkTableFull(i)) {
                        ESP_UI_LOGD("Change icon(%d) table: %d->%d", it->first, it->second.current_page_index, i);
                        ESP_UI_CHECK_FALSE_RETURN(changeIconTable(it->first, i), false, "Change icon table failed");
                        it->second.current_page_index = i;
                        break;
                    }
                    ESP_UI_CHECK_FALSE_RETURN(i < new_table_num - 1, false, "All tables are full");
                }
            }
        }
    }

    /* Update object style */
    // Main
    lv_obj_set_size(_main_obj.get(), _data.main.size.width, _data.main.size.height);
    lv_obj_align(_main_obj.get(), LV_ALIGN_TOP_MID, 0, _data.main.y_start);
    // Table
    lv_obj_set_size(_table_obj.get(), _data.table.size.width, _data.table.size.height);
    // Indicator
    lv_obj_set_size(_indicator_obj.get(), _data.indicator.main_size.width, _data.indicator.main_size.height);
    lv_obj_set_style_pad_column(_indicator_obj.get(), _data.indicator.main_layout_column_pad, 0);
    lv_obj_align(_indicator_obj.get(), LV_ALIGN_BOTTOM_MID, 0, -_data.indicator.main_layout_bottom_offset);
    // Mix
    for (size_t i = 0; i < _mix_objs.size(); i++) {
        ESP_UI_CHECK_FALSE_RETURN(updateMixByNewData(i, _mix_objs), false, "Update mix object(%d) style failed",
                                  (int)i);
    }
    ESP_UI_CHECK_FALSE_RETURN(updateActiveSpot(), false, "Update active spot failed");
    // Icon
    for (auto &id_icon : _id_mix_icon_map) {
        // Process the icons which current table index is not equal to target table index
        if (id_icon.second.target_page_index != id_icon.second.current_page_index) {
            ESP_UI_LOGD("Try to change icon(%d) table: %d->%d", id_icon.first, id_icon.second.current_page_index,
                        id_icon.second.target_page_index);
            if (!checkTableFull(id_icon.second.target_page_index)) {
                ESP_UI_CHECK_FALSE_RETURN(changeIconTable(id_icon.first, id_icon.second.target_page_index), false,
                                          "Change icon table failed");
                id_icon.second.current_page_index = id_icon.second.target_page_index;
                ESP_UI_LOGD("Change success");
                goto next;
            } else {
                ESP_UI_LOGD("Change icon table failed, table is full");
            }
        }
next:
        ESP_UI_CHECK_FALSE_RETURN(id_icon.second.icon->updateByNewData(), false, "Update icon style failed");
    }

    return true;
}

void ESP_UI_AppLauncher::onDataUpdateEventCallback(lv_event_t *event)
{
    ESP_UI_AppLauncher *app_launcher = nullptr;

    ESP_UI_LOGD("Data update event callback");
    ESP_UI_CHECK_NULL_EXIT(event, "Invalid event object");

    app_launcher = (ESP_UI_AppLauncher *)lv_event_get_user_data(event);
    ESP_UI_CHECK_NULL_EXIT(app_launcher, "Invalid app launcher object");

    ESP_UI_CHECK_FALSE_EXIT(app_launcher->updateByNewData(), "Update object style failed");
}

void ESP_UI_AppLauncher::onScreenChangeEventCallback(lv_event_t *event)
{
    ESP_UI_AppLauncher *app_launcher = nullptr;

    ESP_UI_LOGD("Screen change event callback");
    ESP_UI_CHECK_NULL_EXIT(event, "Invalid event object");

    app_launcher = (ESP_UI_AppLauncher *)lv_event_get_user_data(event);
    ESP_UI_CHECK_NULL_EXIT(app_launcher, "Invalid app launcher object");

    app_launcher->_table_current_page_index = lv_tabview_get_tab_act(app_launcher->_table_obj.get());
    ESP_UI_CHECK_FALSE_EXIT(app_launcher->updateActiveSpot(), "Update active spot failed");
}
