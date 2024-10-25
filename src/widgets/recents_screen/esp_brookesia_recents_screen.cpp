/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <cmath>
#include "esp_brookesia_conf_internal.h"
#include "esp_brookesia_recents_screen.hpp"

#if !ESP_BROOKESIA_LOG_ENABLE_DEBUG_WIDGETS_RECENTS_SCREEN
#undef ESP_BROOKESIA_LOGD
#define ESP_BROOKESIA_LOGD(...)
#endif

#define MEMORY_LABEL_TEXT_FORMAT        "%d + %d %s of %d + %d %s available"
#define MEMORY_LABEL_TEXT_UNIT          "KB"

using namespace std;

ESP_Brookesia_RecentsScreen::ESP_Brookesia_RecentsScreen(ESP_Brookesia_Core &core, const ESP_Brookesia_RecentsScreenData_t &data):
    _core(core),
    _data(data),
    _is_trash_pressed_losted(false),
    _trash_icon_default_zoom(LV_IMG_ZOOM_NONE),
    _trash_icon_press_zoom(LV_IMG_ZOOM_NONE),
    _main_obj(nullptr),
    _memory_obj(nullptr),
    _memory_label(nullptr),
    _snapshot_table(nullptr),
    _trash_obj(nullptr),
    _trash_icon(nullptr)
{
}

ESP_Brookesia_RecentsScreen::~ESP_Brookesia_RecentsScreen()
{
    ESP_BROOKESIA_LOGD("Destroy(0x%p)", this);
    if (!del()) {
        ESP_BROOKESIA_LOGE("Delete failed");
    }
}

bool ESP_Brookesia_RecentsScreen::begin(lv_obj_t *parent)
{
    ESP_Brookesia_LvObj_t main_obj = nullptr;
    ESP_Brookesia_LvObj_t memory_obj = nullptr;
    ESP_Brookesia_LvObj_t memory_label = nullptr;
    ESP_Brookesia_LvObj_t snapshot_table = nullptr;
    ESP_Brookesia_LvObj_t trash_obj = nullptr;
    ESP_Brookesia_LvObj_t trash_icon = nullptr;

    ESP_BROOKESIA_LOGD("Begin(0x%p)", this);
    ESP_BROOKESIA_CHECK_NULL_RETURN(parent, false, "Invalid parent object");
    ESP_BROOKESIA_CHECK_FALSE_RETURN(!checkInitialized(), false, "RecentsScreen is already initialized");

    /* Create objects */
    // Main
    main_obj = ESP_BROOKESIA_LV_OBJ(obj, parent);
    ESP_BROOKESIA_CHECK_NULL_RETURN(main_obj, false, "Create main object failed");
    // Memory
    if (_data.flags.enable_memory) {
        ESP_BROOKESIA_LOGD("Enable memory label");
        memory_obj = ESP_BROOKESIA_LV_OBJ(obj, main_obj.get());
        ESP_BROOKESIA_CHECK_NULL_RETURN(main_obj, false, "Create main object failed");
        memory_label = ESP_BROOKESIA_LV_OBJ(label, memory_obj.get());
        ESP_BROOKESIA_CHECK_NULL_RETURN(memory_label, false, "Create memory label failed");
    }
    // Snapshot snapshot_table
    snapshot_table = ESP_BROOKESIA_LV_OBJ(obj, main_obj.get());
    ESP_BROOKESIA_CHECK_NULL_RETURN(snapshot_table, false, "Create snapshot snapshot_table failed");
    // Trash
    trash_obj = ESP_BROOKESIA_LV_OBJ(obj, main_obj.get());
    ESP_BROOKESIA_CHECK_NULL_RETURN(trash_obj, false, "Create trash_icon object failed");
    trash_icon = ESP_BROOKESIA_LV_OBJ(img, trash_obj.get());
    ESP_BROOKESIA_CHECK_NULL_RETURN(trash_icon, false, "Create trash_icon icon failed");

    /* Setup objects style */
    // Main
    lv_obj_add_style(main_obj.get(), _core.getCoreHome().getCoreContainerStyle(), 0);
    lv_obj_set_flex_flow(main_obj.get(), LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(main_obj.get(), LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(main_obj.get(), LV_OBJ_FLAG_SCROLLABLE);
    // Memory
    if (_data.flags.enable_memory) {
        // Object
        lv_obj_add_style(memory_obj.get(), _core.getCoreHome().getCoreContainerStyle(), 0);
        lv_obj_clear_flag(memory_obj.get(), LV_OBJ_FLAG_SCROLLABLE);
        // Label
        lv_obj_add_style(memory_label.get(), _core.getCoreHome().getCoreContainerStyle(), 0);
        lv_obj_clear_flag(memory_label.get(), LV_OBJ_FLAG_SCROLLABLE);
    }
    // Snapshot snapshot_table
    lv_obj_add_style(snapshot_table.get(), _core.getCoreHome().getCoreContainerStyle(), 0);
    lv_obj_set_flex_flow(snapshot_table.get(), LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(snapshot_table.get(), LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(snapshot_table.get(), LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_snap_x(snapshot_table.get(), LV_SCROLL_SNAP_CENTER);
    lv_obj_clear_flag(snapshot_table.get(), LV_OBJ_FLAG_SCROLLABLE);
    // Trash
    lv_obj_add_style(trash_obj.get(), _core.getCoreHome().getCoreContainerStyle(), 0);
    lv_obj_clear_flag(trash_obj.get(), LV_OBJ_FLAG_SCROLLABLE);
    // Transh icon
    lv_obj_center(trash_icon.get());
    lv_obj_add_style(trash_icon.get(), _core.getCoreHome().getCoreContainerStyle(), 0);
    lv_obj_set_size(trash_icon.get(), LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_img_set_size_mode(trash_icon.get(), LV_IMG_SIZE_MODE_REAL);
    lv_obj_add_flag(trash_icon.get(), LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(trash_icon.get(), LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_add_event_cb(trash_icon.get(), onTrashTouchEventCallback, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(trash_icon.get(), onTrashTouchEventCallback, LV_EVENT_PRESSED, this);
    lv_obj_add_event_cb(trash_icon.get(), onTrashTouchEventCallback, LV_EVENT_PRESS_LOST, this);
    lv_obj_add_event_cb(trash_icon.get(), onTrashTouchEventCallback, LV_EVENT_RELEASED, this);
    // Event
    ESP_BROOKESIA_CHECK_FALSE_RETURN(_core.registerDateUpdateEventCallback(onDataUpdateEventCallback, this), false,
                                     "Register data update event callback failed");

    // Save objects
    _main_obj = main_obj;
    _memory_obj = memory_obj;
    _memory_label = memory_label;
    _snapshot_table = snapshot_table;
    _trash_obj = trash_obj;
    _trash_icon = trash_icon;
    _snapshot_deleted_event_code = _core.getFreeEventCode();

    // Update
    ESP_BROOKESIA_CHECK_FALSE_GOTO(updateByNewData(), err, "Update failed");
    lv_label_set_text_fmt(_memory_label.get(), MEMORY_LABEL_TEXT_FORMAT, 0, 0, _data.memory.label_unit_text,
                          0, 0, _data.memory.label_unit_text);

    return true;

err:
    ESP_BROOKESIA_CHECK_FALSE_RETURN(del(), false, "Delete failed");

    return false;
}

bool ESP_Brookesia_RecentsScreen::del(void)
{
    bool ret = true;

    ESP_BROOKESIA_LOGD("Delete(0x%p)", this);

    if (!checkInitialized()) {
        return true;
    }

    if (_core.checkCoreInitialized() && !_core.unregisterDateUpdateEventCallback(onDataUpdateEventCallback, this)) {
        ESP_BROOKESIA_LOGE("Unregister data update event callback failed");
        ret = false;
    }

    _main_obj.reset();
    _memory_obj.reset();
    _memory_label.reset();
    _snapshot_table.reset();
    _trash_obj.reset();
    _trash_icon.reset();
    _id_snapshot_map.clear();

    return ret;
}

bool ESP_Brookesia_RecentsScreen::setVisible(bool visible) const
{
    ESP_BROOKESIA_LOGD("Set visible(%d)", visible);
    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    if (visible) {
        lv_obj_clear_flag(_main_obj.get(), LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(_main_obj.get(), LV_OBJ_FLAG_HIDDEN);
    }

    return true;
}

bool ESP_Brookesia_RecentsScreen::addSnapshot(const ESP_Brookesia_RecentsScreenSnapshotConf_t &conf)
{
    shared_ptr<ESP_Brookesia_RecentsScreenSnapshot> snapshot = nullptr;

    ESP_BROOKESIA_LOGD("Add snapshot(%d)", conf.id);
    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    snapshot = make_shared<ESP_Brookesia_RecentsScreenSnapshot>(_core, conf, _data.snapshot_table.snapshot);
    ESP_BROOKESIA_CHECK_NULL_RETURN(snapshot, false, "Create snapshot failed");

    ESP_BROOKESIA_CHECK_FALSE_RETURN(snapshot->begin(_snapshot_table.get()), false, "Begin snapshot failed");

    if (checkSnapshotExist(conf.id)) {
        ESP_BROOKESIA_LOGW("Already exist, override it");
        _id_snapshot_map[conf.id] = snapshot;
    } else {
        auto ret = _id_snapshot_map.insert(std::pair<int, shared_ptr<ESP_Brookesia_RecentsScreenSnapshot>>(conf.id, snapshot));
        ESP_BROOKESIA_CHECK_FALSE_RETURN(ret.second, false, "Insert snapshot failed");
    }

    ESP_BROOKESIA_CHECK_FALSE_RETURN(scrollToSnapshotById(conf.id), false, "Scroll to snapshot failed");

    return true;
}

bool ESP_Brookesia_RecentsScreen::removeSnapshot(int id)
{
    ESP_BROOKESIA_LOGD("Remove snapshot(%d)", id);
    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkSnapshotExist(id), false, "Snapshot is not exist");

    int num = _id_snapshot_map.erase(id);
    ESP_BROOKESIA_CHECK_FALSE_RETURN(num > 0, false, "Remove snapshot failed");

    return true;
}

bool ESP_Brookesia_RecentsScreen::scrollToSnapshotById(int id)
{
    lv_obj_t *snapshot_main_obj = nullptr;

    ESP_BROOKESIA_LOGD("Scroll to snapshot id(%d)", id);
    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkSnapshotExist(id), false, "Snapshot is not exist");

    snapshot_main_obj = _id_snapshot_map.at(id)->getMainObj();
    ESP_BROOKESIA_CHECK_NULL_RETURN(snapshot_main_obj, false, "Invalid snapshot main object");

    lv_obj_add_flag(_snapshot_table.get(), LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_scroll_to_view(snapshot_main_obj, _data.flags.enable_table_scroll_anim ? LV_ANIM_ON : LV_ANIM_OFF);
    lv_obj_clear_flag(_snapshot_table.get(), LV_OBJ_FLAG_SCROLLABLE);

    return true;
}

bool ESP_Brookesia_RecentsScreen::scrollToSnapshotByIndex(uint8_t index)
{
    lv_obj_t *snapshot_main_obj = nullptr;

    ESP_BROOKESIA_LOGD("Scroll to snapshot index(%d)", index);
    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_BROOKESIA_CHECK_FALSE_RETURN(index < _id_snapshot_map.size(), false, "Invalid snapshot index");

    auto it = _id_snapshot_map.begin();
    advance(it, _id_snapshot_map.size() - index - 1);
    snapshot_main_obj = it->second->getMainObj();
    ESP_BROOKESIA_CHECK_NULL_RETURN(snapshot_main_obj, false, "Invalid snapshot main object");

    lv_obj_add_flag(_snapshot_table.get(), LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_scroll_to_view(snapshot_main_obj, _data.flags.enable_table_scroll_anim ? LV_ANIM_ON : LV_ANIM_OFF);
    lv_obj_clear_flag(_snapshot_table.get(), LV_OBJ_FLAG_SCROLLABLE);

    return true;
}

bool ESP_Brookesia_RecentsScreen::moveSnapshotY(int id, int y)
{
    lv_obj_t *drag_obj = NULL;

    ESP_BROOKESIA_LOGD("Move snapshot(%d) to y(%d)", id, y);
    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkSnapshotExist(id), false, "Snapshot is not exist");

    drag_obj = _id_snapshot_map.at(id)->getDragObj();
    ESP_BROOKESIA_CHECK_NULL_RETURN(drag_obj, false, "Invalid snapshot drag object");

    lv_obj_set_y(drag_obj, y);

    return true;
}

bool ESP_Brookesia_RecentsScreen::updateSnapshotImage(int id)
{
    ESP_BROOKESIA_LOGD("Update snapshot(%d) image", id);
    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkSnapshotExist(id), false, "Snapshot is not exist");

    ESP_BROOKESIA_CHECK_FALSE_RETURN(_id_snapshot_map.at(id)->updateByNewData(), false, "Update snapshot style failed");

    return true;
}

bool ESP_Brookesia_RecentsScreen::setMemoryLabel(int internal_free, int internal_total, int external_free, int external_total) const
{
    ESP_BROOKESIA_LOGD("Set memory label");
    ESP_BROOKESIA_CHECK_FALSE_RETURN(_memory_label != nullptr, false, "Memory label is disabled");

    lv_label_set_text_fmt(_memory_label.get(), MEMORY_LABEL_TEXT_FORMAT,
                          internal_free, external_free, _data.memory.label_unit_text,
                          internal_total, external_total, _data.memory.label_unit_text);

    return true;
}

bool ESP_Brookesia_RecentsScreen::checkSnapshotExist(int id) const
{
    auto it = _id_snapshot_map.find(id);

    return (it != _id_snapshot_map.end()) && (it->second != nullptr);
}

bool ESP_Brookesia_RecentsScreen::checkVisible(void) const
{
    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    return lv_obj_is_visible(_main_obj.get());
}

bool ESP_Brookesia_RecentsScreen::checkPointInsideMain(lv_point_t &point) const
{
    bool point_in_main = false;
    bool point_in_trash = false;
    lv_area_t area = {};

    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    lv_obj_refr_pos(_main_obj.get());
    lv_obj_get_coords(_main_obj.get(), &area);
    point_in_main = _lv_area_is_point_on(&area, &point, lv_obj_get_style_radius(_main_obj.get(), 0));

    lv_obj_refr_pos(_trash_obj.get());
    lv_obj_get_coords(_trash_obj.get(), &area);
    point_in_trash = _lv_area_is_point_on(&area, &point, lv_obj_get_style_radius(_trash_obj.get(), 0));

    return point_in_main && !point_in_trash;
}


bool ESP_Brookesia_RecentsScreen::checkPointInsideTable(lv_point_t &point) const
{
    lv_area_t area = {};

    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    lv_obj_refr_pos(_snapshot_table.get());
    lv_obj_get_coords(_snapshot_table.get(), &area);

    return _lv_area_is_point_on(&area, &point, lv_obj_get_style_radius(_snapshot_table.get(), 0));
}

bool ESP_Brookesia_RecentsScreen::checkPointInsideSnapshot(int id, lv_point_t &point) const
{
    lv_area_t area = {};
    lv_obj_t *snapshot_main_obj = NULL;

    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkSnapshotExist(id), false, "Snapshot is not exist");

    snapshot_main_obj = _id_snapshot_map.at(id)->getMainObj();
    ESP_BROOKESIA_CHECK_FALSE_RETURN(snapshot_main_obj != NULL, false, "Invalid snapshot main object");

    lv_obj_refr_pos(snapshot_main_obj);
    lv_obj_get_coords(snapshot_main_obj, &area);

    return _lv_area_is_point_on(&area, &point, lv_obj_get_style_radius(snapshot_main_obj, 0));
}

int ESP_Brookesia_RecentsScreen::getSnapshotOriginY(int id) const
{
    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkSnapshotExist(id), 0, "Snapshot is not exist");

    return _id_snapshot_map.at(id)->getOriginY();
}

int ESP_Brookesia_RecentsScreen::getSnapshotCurrentY(int id) const
{
    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkSnapshotExist(id), 0, "Snapshot is not exist");

    return _id_snapshot_map.at(id)->getCurrentY();
}

int ESP_Brookesia_RecentsScreen::getSnapshotIdPointIn(lv_point_t &point) const
{
    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkInitialized(), -1, "Not initialized");

    for (auto it = _id_snapshot_map.begin(); it != _id_snapshot_map.end(); ++it) {
        if (checkPointInsideSnapshot(it->first, point)) {
            return it->first;
        }
    }

    return -1;
}

bool ESP_Brookesia_RecentsScreen::calibrateData(const ESP_Brookesia_StyleSize_t &screen_size, const ESP_Brookesia_CoreHome &home,
        ESP_Brookesia_RecentsScreenData_t &data)
{
    uint16_t parent_w = 0;
    uint16_t parent_h = 0;
    ESP_Brookesia_RecentsScreenSnapshotData_t &new_snapshot_data = data.snapshot_table.snapshot;
    const ESP_Brookesia_StyleSize_t *parent_size = nullptr;

    ESP_BROOKESIA_LOGD("Calibrate data");

    /* Main */
    parent_size = &screen_size;
    parent_w = parent_size->width;
    parent_h = parent_size->height;
    // Size
    ESP_BROOKESIA_CHECK_FALSE_RETURN(home.calibrateCoreObjectSize(*parent_size, data.main.size), false,
                                     "Invalid main size");
    ESP_BROOKESIA_CHECK_VALUE_RETURN(data.main.y_start, 0, parent_h - 1, false, "Invalid main y start");
    ESP_BROOKESIA_CHECK_VALUE_RETURN(data.main.y_start + data.main.size.height, 1, parent_h, false,
                                     "Main height is out of range");
    parent_size = &data.main.size;
    parent_w = parent_size->width;
    parent_h = parent_size->height;
    // Layout
    ESP_BROOKESIA_CHECK_VALUE_RETURN(data.main.layout_row_pad, 0, parent_h, false, "Invalid main layout row pad");
    ESP_BROOKESIA_CHECK_VALUE_RETURN(data.main.layout_top_pad, 0, parent_h, false, "Invalid main layout top pad");
    ESP_BROOKESIA_CHECK_VALUE_RETURN(data.main.layout_bottom_pad, 0, parent_h, false, "Invalid main layout bottom pad");

    /* Memory */
    if (data.flags.enable_memory) {
        parent_size = &data.main.size;
        parent_w = parent_size->width;
        parent_h = parent_size->height;
        // Main
        ESP_BROOKESIA_CHECK_FALSE_RETURN(home.calibrateCoreObjectSize(*parent_size, data.memory.main_size), false,
                                         "Invalid memory main size");
        parent_size = &data.memory.main_size;
        parent_w = parent_size->width;
        parent_h = parent_size->height;
        ESP_BROOKESIA_CHECK_VALUE_RETURN(data.memory.main_layout_x_right_offset, 0, parent_w, false,
                                         "Invalid memory main layout x right offset");
        // Label
        ESP_BROOKESIA_CHECK_FALSE_RETURN(home.calibrateCoreFont(parent_size, data.memory.label_text_font), false,
                                         "Invalid memory label text font size");
        ESP_BROOKESIA_CHECK_NULL_RETURN(data.memory.label_unit_text, false, "Invalid memory label unit text");
    }

    // Trash
    parent_size = &data.main.size;
    parent_w = parent_size->width;
    parent_h = parent_size->height;
    ESP_BROOKESIA_CHECK_FALSE_RETURN(home.calibrateCoreObjectSize(*parent_size, data.trash_icon.default_size), false,
                                     "Invalid trash icon default size");
    ESP_BROOKESIA_CHECK_FALSE_RETURN(home.calibrateCoreObjectSize(*parent_size, data.trash_icon.press_size), false,
                                     "Invalid trash icon eprbdefault size");
    ESP_BROOKESIA_CHECK_NULL_RETURN(data.trash_icon.image.resource, false, "Invalid trash icon image resource");

    // Table
    parent_size = &data.main.size;
    parent_w = parent_size->width;
    parent_h = parent_size->height;
    if (data.flags.enable_table_height_flex) {
        data.snapshot_table.main_size.height = parent_h - data.memory.main_size.height -
                                               data.trash_icon.default_size.height -
                                               data.main.layout_row_pad * 4 - data.main.layout_top_pad -
                                               data.main.layout_bottom_pad;
        data.snapshot_table.main_size.flags.enable_height_percent = 0;
        data.snapshot_table.main_size.flags.enable_square = 0;
    }
    ESP_BROOKESIA_CHECK_FALSE_RETURN(home.calibrateCoreObjectSize(*parent_size, data.snapshot_table.main_size), false,
                                     "Invalid snapshot_table main size");
    parent_size = &data.snapshot_table.main_size;
    parent_w = parent_size->width;
    ESP_BROOKESIA_CHECK_VALUE_RETURN(data.snapshot_table.main_layout_column_pad, 0, parent_w, false,
                                     "Invalid snapshot_table main layout column pad");

    /* Snapshot */
    // Main
    parent_size = (new_snapshot_data.flags.enable_all_main_size_refer_screen) ? &screen_size :
                  &data.snapshot_table.main_size;
    ESP_BROOKESIA_CHECK_FALSE_RETURN(home.calibrateCoreObjectSize(*parent_size, new_snapshot_data.main_size), false,
                                     "Invalid snapshot main size");
    // If referring to the screen, check with the main size
    if (new_snapshot_data.flags.enable_all_main_size_refer_screen) {
        ESP_BROOKESIA_CHECK_VALUE_RETURN(new_snapshot_data.main_size.width, 1, data.snapshot_table.main_size.width, false,
                                         "Invalid snapshot main width");
        ESP_BROOKESIA_CHECK_VALUE_RETURN(new_snapshot_data.main_size.height, 1, data.snapshot_table.main_size.height, false,
                                         "Invalid snapshot main height");
    }
    // Title
    parent_size = (new_snapshot_data.flags.enable_all_main_size_refer_screen) ? &screen_size :
                  &new_snapshot_data.main_size;
    ESP_BROOKESIA_CHECK_FALSE_RETURN(home.calibrateCoreObjectSize(*parent_size, new_snapshot_data.title.main_size), false,
                                     "Invalid snapshot title size");
    // If referring to the screen, check with the main size
    if (new_snapshot_data.flags.enable_all_main_size_refer_screen) {
        ESP_BROOKESIA_CHECK_VALUE_RETURN(new_snapshot_data.title.main_size.width, 1, new_snapshot_data.main_size.width, false,
                                         "Invalid snapshot title main width");
        ESP_BROOKESIA_CHECK_VALUE_RETURN(new_snapshot_data.title.main_size.height, 1, new_snapshot_data.main_size.height, false,
                                         "Invalid snapshot title main height");
    }
    parent_size = &new_snapshot_data.title.main_size;
    parent_w = parent_size->width;
    ESP_BROOKESIA_CHECK_VALUE_RETURN(new_snapshot_data.title.main_layout_column_pad, 0, parent_w, false,
                                     "Invalid snapshot title layout column pad");
    // Title Icon
    parent_size = &new_snapshot_data.title.main_size;
    ESP_BROOKESIA_CHECK_FALSE_RETURN(home.calibrateCoreObjectSize(*parent_size, new_snapshot_data.title.icon_size), false,
                                     "Invalid snapshot title icon size");
    ESP_BROOKESIA_CHECK_FALSE_RETURN(home.calibrateCoreFont(parent_size, new_snapshot_data.title.text_font), false,
                                     "Invalid snapshot title text font");
    // Image
    parent_size = (new_snapshot_data.flags.enable_all_main_size_refer_screen) ? &screen_size :
                  &new_snapshot_data.main_size;
    ESP_BROOKESIA_CHECK_FALSE_RETURN(home.calibrateCoreObjectSize(*parent_size, new_snapshot_data.image.main_size), false,
                                     "Invalid snapshot image main size");
    // If referring to the screen, check with the main size
    if (new_snapshot_data.flags.enable_all_main_size_refer_screen) {
        ESP_BROOKESIA_CHECK_VALUE_RETURN(new_snapshot_data.image.main_size.width, 1, new_snapshot_data.main_size.width, false,
                                         "Invalid snapshot image main width");
        ESP_BROOKESIA_CHECK_VALUE_RETURN(new_snapshot_data.image.main_size.height, 1, new_snapshot_data.main_size.height, false,
                                         "Invalid snapshot image main height");
    }
    // All
    parent_size = &new_snapshot_data.main_size;
    parent_h = parent_size->height;
    ESP_BROOKESIA_CHECK_VALUE_RETURN(new_snapshot_data.title.main_size.height + new_snapshot_data.image.main_size.height, 1,
                                     parent_h, false, "The sum of snapshot title height(%d) and image height(%d) out of main",
                                     new_snapshot_data.title.main_size.height, new_snapshot_data.image.main_size.height);

    return true;
}

bool ESP_Brookesia_RecentsScreen::updateByNewData(void)
{
    float h_factor = 0;
    float w_factor = 0;

    ESP_BROOKESIA_LOGD("Update(0x%p)", this);
    ESP_BROOKESIA_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    // Main
    lv_obj_set_size(_main_obj.get(), _data.main.size.width, _data.main.size.height);
    lv_obj_set_style_pad_row(_main_obj.get(), _data.main.layout_row_pad, 0);
    lv_obj_set_style_pad_top(_main_obj.get(), _data.main.layout_top_pad, 0);
    lv_obj_set_style_pad_bottom(_main_obj.get(), _data.main.layout_bottom_pad, 0);
    lv_obj_set_style_bg_color(_main_obj.get(), lv_color_hex(_data.main.background_color.color), 0);
    lv_obj_set_style_bg_opa(_main_obj.get(), _data.main.background_color.opacity, 0);
    lv_obj_align(_main_obj.get(), LV_ALIGN_TOP_MID, 0, _data.main.y_start);

    // Label
    if (_data.flags.enable_memory) {
        lv_obj_set_size(_memory_obj.get(), _data.memory.main_size.width, _data.memory.main_size.height);
        lv_obj_align(_memory_label.get(), LV_ALIGN_RIGHT_MID, -_data.memory.main_layout_x_right_offset, 0);
        lv_obj_set_style_text_color(_memory_label.get(), lv_color_hex(_data.memory.label_text_color.color), 0);
        lv_obj_set_style_text_opa(_memory_label.get(), _data.memory.label_text_color.opacity, 0);
        lv_obj_set_style_text_font(_memory_label.get(), (lv_font_t *)_data.memory.label_text_font.font_resource, 0);
    }

    // Table
    lv_obj_set_size(_snapshot_table.get(), _data.snapshot_table.main_size.width, _data.snapshot_table.main_size.height);
    lv_obj_set_style_pad_column(_snapshot_table.get(), _data.snapshot_table.main_layout_column_pad, 0);

    // Trash
    lv_obj_set_size(_trash_obj.get(), _data.trash_icon.default_size.width, _data.trash_icon.default_size.height);
    lv_img_set_src(_trash_icon.get(), _data.trash_icon.image.resource);
    lv_obj_set_style_img_recolor(_trash_icon.get(), lv_color_hex(_data.trash_icon.image.recolor.color), 0);
    lv_obj_set_style_img_recolor_opa(_trash_icon.get(), _data.trash_icon.image.recolor.opacity, 0);
    h_factor = (float)(_data.trash_icon.default_size.height) /
               ((lv_img_dsc_t *)_data.trash_icon.image.resource)->header.h;
    w_factor = (float)(_data.trash_icon.default_size.width) /
               ((lv_img_dsc_t *)_data.trash_icon.image.resource)->header.w;
    if (h_factor < w_factor) {
        _trash_icon_default_zoom = (int)(h_factor * LV_IMG_ZOOM_NONE);
        lv_img_set_zoom(_trash_icon.get(), _trash_icon_default_zoom);
    } else {
        _trash_icon_default_zoom = (int)(w_factor * LV_IMG_ZOOM_NONE);
        lv_img_set_zoom(_trash_icon.get(), _trash_icon_default_zoom);
    }
    h_factor = (float)(_data.trash_icon.press_size.height) /
               ((lv_img_dsc_t *)_data.trash_icon.image.resource)->header.h;
    w_factor = (float)(_data.trash_icon.press_size.width) /
               ((lv_img_dsc_t *)_data.trash_icon.image.resource)->header.w;
    if (h_factor < w_factor) {
        _trash_icon_press_zoom = (int)(h_factor * LV_IMG_ZOOM_NONE);
    } else {
        _trash_icon_press_zoom = (int)(w_factor * LV_IMG_ZOOM_NONE);
    }
    lv_obj_refr_size(_trash_icon.get());

    // Snapshot
    for (auto &it : _id_snapshot_map) {
        ESP_BROOKESIA_CHECK_NULL_RETURN(it.second, false, "Invalid snapshot(%d)", it.first);
        ESP_BROOKESIA_CHECK_FALSE_RETURN(it.second->updateByNewData(), false, "Update snapshot object style failed");
    }

    return true;
}

void ESP_Brookesia_RecentsScreen::onDataUpdateEventCallback(lv_event_t *event)
{
    ESP_Brookesia_RecentsScreen *recents_screen = nullptr;

    ESP_BROOKESIA_LOGD("Data update event");
    ESP_BROOKESIA_CHECK_NULL_EXIT(event, "Invalid event object");

    recents_screen = (ESP_Brookesia_RecentsScreen *)lv_event_get_user_data(event);
    ESP_BROOKESIA_CHECK_NULL_EXIT(recents_screen, "Invalid app snapshot_table object");

    ESP_BROOKESIA_CHECK_FALSE_EXIT(recents_screen->updateByNewData(), "Update object style failed");
}

void ESP_Brookesia_RecentsScreen::onTrashTouchEventCallback(lv_event_t *event)
{
    lv_event_code_t event_code = lv_event_get_code(event);
    ESP_Brookesia_RecentsScreen *recents_screen = (ESP_Brookesia_RecentsScreen *)lv_event_get_user_data(event);
    std::unordered_map<int, std::shared_ptr<ESP_Brookesia_RecentsScreenSnapshot>> id_snapshot_map;

    ESP_BROOKESIA_LOGD("Trash touch event callback");
    ESP_BROOKESIA_CHECK_NULL_EXIT(recents_screen, "Invalid recents_screen object");

    switch (event_code) {
    case LV_EVENT_CLICKED:
        ESP_BROOKESIA_LOGD("Clicked");
        if (recents_screen->_is_trash_pressed_losted) {
            break;
        }
        // Since the snapshot may be deleted during the loop, we need to copy the map first
        id_snapshot_map = recents_screen->_id_snapshot_map;
        for (auto &it : id_snapshot_map) {
            lv_event_send(recents_screen->getEventObject(), recents_screen->getSnapshotDeletedEventCode(),
                          reinterpret_cast<void *>(it.first));
        }
        // Send this event to notify that trash icon is clicked
        lv_event_send(recents_screen->getEventObject(), recents_screen->getSnapshotDeletedEventCode(),
                      reinterpret_cast<void *>(0));
        break;
    case LV_EVENT_PRESSED:
        ESP_BROOKESIA_LOGD("Pressed");
        // Zoom out icon
        lv_img_set_zoom(lv_event_get_target(event), recents_screen->_trash_icon_press_zoom);
        lv_obj_refr_size(lv_event_get_target(event));
        recents_screen->_is_trash_pressed_losted = false;
        break;
    case LV_EVENT_PRESS_LOST:
        ESP_BROOKESIA_LOGD("Press lost");
        recents_screen->_is_trash_pressed_losted = true;
        [[fallthrough]];
    case LV_EVENT_RELEASED:
        ESP_BROOKESIA_LOGD("Released");
        // Zoom in icon
        lv_img_set_zoom(lv_event_get_target(event), recents_screen->_trash_icon_default_zoom);
        lv_obj_refr_size(lv_event_get_target(event));
        break;
    default:
        break;
    }
}
