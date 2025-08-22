/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <cmath>
#include "esp_brookesia_systems_internal.h"
#if !ESP_BROOKESIA_PHONE_RECENTS_SCREEN_ENABLE_DEBUG_LOG
#   define ESP_BROOKESIA_UTILS_DISABLE_DEBUG_LOG
#endif
#include "phone/private/esp_brookesia_phone_utils.hpp"
#include "esp_brookesia_recents_screen.hpp"

#define MEMORY_LABEL_TEXT_FORMAT        "%d + %d %s of %d + %d %s available"
#define MEMORY_LABEL_TEXT_UNIT          "KB"

using namespace std;
using namespace esp_brookesia::gui;

namespace esp_brookesia::systems::phone {

RecentsScreen::RecentsScreen(base::Context &core, const RecentsScreen::Data &data)
    : _system_context(core)
    , _data(data)
{
}

RecentsScreen::~RecentsScreen()
{
    ESP_UTILS_LOGD("Destroy(0x%p)", this);
    if (!del()) {
        ESP_UTILS_LOGE("Delete failed");
    }
}

bool RecentsScreen::begin(lv_obj_t *parent)
{
    ESP_Brookesia_LvObj_t main_obj = nullptr;
    ESP_Brookesia_LvObj_t memory_obj = nullptr;
    ESP_Brookesia_LvObj_t memory_label = nullptr;
    ESP_Brookesia_LvObj_t snapshot_table = nullptr;
    ESP_Brookesia_LvObj_t trash_obj = nullptr;
    ESP_Brookesia_LvObj_t trash_icon = nullptr;

    ESP_UTILS_LOGD("Begin(0x%p)", this);
    ESP_UTILS_CHECK_NULL_RETURN(parent, false, "Invalid parent object");
    ESP_UTILS_CHECK_FALSE_RETURN(!checkInitialized(), false, "RecentsScreen is already initialized");

    /* Create objects */
    // Main
    main_obj = ESP_BROOKESIA_LV_OBJ(obj, parent);
    ESP_UTILS_CHECK_NULL_RETURN(main_obj, false, "Create main object failed");
    // Memory
    if (_data.flags.enable_memory) {
        ESP_UTILS_LOGD("Enable memory label");
        memory_obj = ESP_BROOKESIA_LV_OBJ(obj, main_obj.get());
        ESP_UTILS_CHECK_NULL_RETURN(main_obj, false, "Create main object failed");
        memory_label = ESP_BROOKESIA_LV_OBJ(label, memory_obj.get());
        ESP_UTILS_CHECK_NULL_RETURN(memory_label, false, "Create memory label failed");
    }
    // Snapshot snapshot_table
    snapshot_table = ESP_BROOKESIA_LV_OBJ(obj, main_obj.get());
    ESP_UTILS_CHECK_NULL_RETURN(snapshot_table, false, "Create snapshot snapshot_table failed");
    // Trash
    trash_obj = ESP_BROOKESIA_LV_OBJ(obj, main_obj.get());
    ESP_UTILS_CHECK_NULL_RETURN(trash_obj, false, "Create trash_icon object failed");
    trash_icon = ESP_BROOKESIA_LV_OBJ(img, trash_obj.get());
    ESP_UTILS_CHECK_NULL_RETURN(trash_icon, false, "Create trash_icon icon failed");

    /* Setup objects style */
    // Main
    lv_obj_add_style(main_obj.get(), _system_context.getDisplay().getCoreContainerStyle(), 0);
    lv_obj_set_flex_flow(main_obj.get(), LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(main_obj.get(), LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(main_obj.get(), LV_OBJ_FLAG_SCROLLABLE);
    // Memory
    if (_data.flags.enable_memory) {
        // Object
        lv_obj_add_style(memory_obj.get(), _system_context.getDisplay().getCoreContainerStyle(), 0);
        lv_obj_clear_flag(memory_obj.get(), LV_OBJ_FLAG_SCROLLABLE);
        // Label
        lv_obj_add_style(memory_label.get(), _system_context.getDisplay().getCoreContainerStyle(), 0);
        lv_obj_clear_flag(memory_label.get(), LV_OBJ_FLAG_SCROLLABLE);
    }
    // Snapshot snapshot_table
    lv_obj_add_style(snapshot_table.get(), _system_context.getDisplay().getCoreContainerStyle(), 0);
    lv_obj_set_flex_flow(snapshot_table.get(), LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(snapshot_table.get(), LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(snapshot_table.get(), LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_snap_x(snapshot_table.get(), LV_SCROLL_SNAP_CENTER);
    lv_obj_clear_flag(snapshot_table.get(), LV_OBJ_FLAG_SCROLLABLE);
    // Trash
    lv_obj_add_style(trash_obj.get(), _system_context.getDisplay().getCoreContainerStyle(), 0);
    lv_obj_clear_flag(trash_obj.get(), LV_OBJ_FLAG_SCROLLABLE);
    // Transh icon
    lv_obj_center(trash_icon.get());
    lv_obj_add_style(trash_icon.get(), _system_context.getDisplay().getCoreContainerStyle(), 0);
    // lv_obj_set_size(trash_icon.get(), LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_image_set_inner_align(trash_icon.get(), LV_IMAGE_ALIGN_CENTER);
    lv_obj_add_flag(trash_icon.get(), LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(trash_icon.get(), LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_add_event_cb(trash_icon.get(), onTrashTouchEventCallback, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(trash_icon.get(), onTrashTouchEventCallback, LV_EVENT_PRESSED, this);
    lv_obj_add_event_cb(trash_icon.get(), onTrashTouchEventCallback, LV_EVENT_PRESS_LOST, this);
    lv_obj_add_event_cb(trash_icon.get(), onTrashTouchEventCallback, LV_EVENT_RELEASED, this);
    // Event
    ESP_UTILS_CHECK_FALSE_RETURN(_system_context.registerDateUpdateEventCallback(onDataUpdateEventCallback, this), false,
                                 "Register data update event callback failed");

    // Save objects
    _main_obj = main_obj;
    _memory_obj = memory_obj;
    _memory_label = memory_label;
    _snapshot_table = snapshot_table;
    _trash_obj = trash_obj;
    _trash_icon = trash_icon;
    _snapshot_deleted_event_code = _system_context.getFreeEventCode();

    // Update
    ESP_UTILS_CHECK_FALSE_GOTO(updateByNewData(), err, "Update failed");
    lv_label_set_text_fmt(_memory_label.get(), MEMORY_LABEL_TEXT_FORMAT, 0, 0, _data.memory.label_unit_text,
                          0, 0, _data.memory.label_unit_text);

    return true;

err:
    ESP_UTILS_CHECK_FALSE_RETURN(del(), false, "Delete failed");

    return false;
}

bool RecentsScreen::del(void)
{
    bool ret = true;

    ESP_UTILS_LOGD("Delete(0x%p)", this);

    if (!checkInitialized()) {
        return true;
    }

    if (_system_context.checkCoreInitialized() && !_system_context.unregisterDateUpdateEventCallback(onDataUpdateEventCallback, this)) {
        ESP_UTILS_LOGE("Unregister data update event callback failed");
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

bool RecentsScreen::setVisible(bool visible) const
{
    ESP_UTILS_LOGD("Set visible(%d)", visible);
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    if (visible) {
        lv_obj_clear_flag(_main_obj.get(), LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(_main_obj.get(), LV_OBJ_FLAG_HIDDEN);
    }

    return true;
}

bool RecentsScreen::addSnapshot(const RecentsScreenSnapshot::Conf &conf)
{
    shared_ptr<RecentsScreenSnapshot> snapshot = nullptr;

    ESP_UTILS_LOGD("Add snapshot(%d)", conf.id);
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    snapshot = make_shared<RecentsScreenSnapshot>(_system_context, conf, _data.snapshot_table.snapshot);
    ESP_UTILS_CHECK_NULL_RETURN(snapshot, false, "Create snapshot failed");

    ESP_UTILS_CHECK_FALSE_RETURN(snapshot->begin(_snapshot_table.get()), false, "Begin snapshot failed");

    if (checkSnapshotExist(conf.id)) {
        ESP_UTILS_LOGW("Already exist, override it");
        _id_snapshot_map[conf.id] = snapshot;
    } else {
        auto ret = _id_snapshot_map.insert(std::pair<int, shared_ptr<RecentsScreenSnapshot>>(conf.id, snapshot));
        ESP_UTILS_CHECK_FALSE_RETURN(ret.second, false, "Insert snapshot failed");
    }

    ESP_UTILS_CHECK_FALSE_RETURN(scrollToSnapshotById(conf.id), false, "Scroll to snapshot failed");

    return true;
}

bool RecentsScreen::removeSnapshot(int id)
{
    ESP_UTILS_LOGD("Remove snapshot(%d)", id);
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_UTILS_CHECK_FALSE_RETURN(checkSnapshotExist(id), false, "Snapshot is not exist");

    int num = _id_snapshot_map.erase(id);
    ESP_UTILS_CHECK_FALSE_RETURN(num > 0, false, "Remove snapshot failed");

    return true;
}

bool RecentsScreen::scrollToSnapshotById(int id)
{
    lv_obj_t *snapshot_main_obj = nullptr;

    ESP_UTILS_LOGD("Scroll to snapshot id(%d)", id);
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_UTILS_CHECK_FALSE_RETURN(checkSnapshotExist(id), false, "Snapshot is not exist");

    snapshot_main_obj = _id_snapshot_map.at(id)->getMainObj();
    ESP_UTILS_CHECK_NULL_RETURN(snapshot_main_obj, false, "Invalid snapshot main object");

    lv_obj_add_flag(_snapshot_table.get(), LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_scroll_to_view(snapshot_main_obj, _data.flags.enable_table_scroll_anim ? LV_ANIM_ON : LV_ANIM_OFF);
    lv_obj_clear_flag(_snapshot_table.get(), LV_OBJ_FLAG_SCROLLABLE);

    return true;
}

bool RecentsScreen::scrollToSnapshotByIndex(uint8_t index)
{
    lv_obj_t *snapshot_main_obj = nullptr;

    ESP_UTILS_LOGD("Scroll to snapshot index(%d)", index);
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_UTILS_CHECK_FALSE_RETURN(index < _id_snapshot_map.size(), false, "Invalid snapshot index");

    auto it = _id_snapshot_map.begin();
    advance(it, _id_snapshot_map.size() - index - 1);
    snapshot_main_obj = it->second->getMainObj();
    ESP_UTILS_CHECK_NULL_RETURN(snapshot_main_obj, false, "Invalid snapshot main object");

    lv_obj_add_flag(_snapshot_table.get(), LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_scroll_to_view(snapshot_main_obj, _data.flags.enable_table_scroll_anim ? LV_ANIM_ON : LV_ANIM_OFF);
    lv_obj_clear_flag(_snapshot_table.get(), LV_OBJ_FLAG_SCROLLABLE);

    return true;
}

bool RecentsScreen::moveSnapshotY(int id, int y)
{
    lv_obj_t *drag_obj = NULL;

    ESP_UTILS_LOGD("Move snapshot(%d) to y(%d)", id, y);
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_UTILS_CHECK_FALSE_RETURN(checkSnapshotExist(id), false, "Snapshot is not exist");

    drag_obj = _id_snapshot_map.at(id)->getDragObj();
    ESP_UTILS_CHECK_NULL_RETURN(drag_obj, false, "Invalid snapshot drag object");

    lv_obj_set_y(drag_obj, y);

    return true;
}

bool RecentsScreen::updateSnapshotImage(int id)
{
    ESP_UTILS_LOGD("Update snapshot(%d) image", id);
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_UTILS_CHECK_FALSE_RETURN(checkSnapshotExist(id), false, "Snapshot is not exist");

    ESP_UTILS_CHECK_FALSE_RETURN(_id_snapshot_map.at(id)->updateByNewData(), false, "Update snapshot style failed");

    return true;
}

bool RecentsScreen::setMemoryLabel(int internal_free, int internal_total, int external_free, int external_total) const
{
    ESP_UTILS_LOGD("Set memory label");
    ESP_UTILS_CHECK_FALSE_RETURN(_memory_label != nullptr, false, "Memory label is disabled");

    lv_label_set_text_fmt(_memory_label.get(), MEMORY_LABEL_TEXT_FORMAT,
                          internal_free, external_free, _data.memory.label_unit_text,
                          internal_total, external_total, _data.memory.label_unit_text);

    return true;
}

bool RecentsScreen::checkSnapshotExist(int id) const
{
    auto it = _id_snapshot_map.find(id);

    return (it != _id_snapshot_map.end()) && (it->second != nullptr);
}

bool RecentsScreen::checkVisible(void) const
{
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    return lv_obj_is_visible(_main_obj.get());
}

bool RecentsScreen::checkPointInsideMain(lv_point_t &point) const
{
    bool point_in_main = false;
    bool point_in_trash = false;
    lv_area_t area = {};

    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    lv_obj_refr_pos(_main_obj.get());
    lv_obj_get_coords(_main_obj.get(), &area);
    point_in_main = _lv_area_is_point_on(&area, &point, lv_obj_get_style_radius(_main_obj.get(), 0));

    lv_obj_refr_pos(_trash_obj.get());
    lv_obj_get_coords(_trash_obj.get(), &area);
    point_in_trash = _lv_area_is_point_on(&area, &point, lv_obj_get_style_radius(_trash_obj.get(), 0));

    return point_in_main && !point_in_trash;
}


bool RecentsScreen::checkPointInsideTable(lv_point_t &point) const
{
    lv_area_t area = {};

    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    lv_obj_refr_pos(_snapshot_table.get());
    lv_obj_get_coords(_snapshot_table.get(), &area);

    return _lv_area_is_point_on(&area, &point, lv_obj_get_style_radius(_snapshot_table.get(), 0));
}

bool RecentsScreen::checkPointInsideSnapshot(int id, lv_point_t &point) const
{
    lv_area_t area = {};
    lv_obj_t *snapshot_main_obj = NULL;

    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    ESP_UTILS_CHECK_FALSE_RETURN(checkSnapshotExist(id), false, "Snapshot is not exist");

    snapshot_main_obj = _id_snapshot_map.at(id)->getMainObj();
    ESP_UTILS_CHECK_FALSE_RETURN(snapshot_main_obj != NULL, false, "Invalid snapshot main object");

    lv_obj_refr_pos(snapshot_main_obj);
    lv_obj_get_coords(snapshot_main_obj, &area);

    return _lv_area_is_point_on(&area, &point, lv_obj_get_style_radius(snapshot_main_obj, 0));
}

int RecentsScreen::getSnapshotOriginY(int id) const
{
    ESP_UTILS_CHECK_FALSE_RETURN(checkSnapshotExist(id), 0, "Snapshot is not exist");

    return _id_snapshot_map.at(id)->getOriginY();
}

int RecentsScreen::getSnapshotCurrentY(int id) const
{
    ESP_UTILS_CHECK_FALSE_RETURN(checkSnapshotExist(id), 0, "Snapshot is not exist");

    return _id_snapshot_map.at(id)->getCurrentY();
}

int RecentsScreen::getSnapshotIdPointIn(lv_point_t &point) const
{
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), -1, "Not initialized");

    for (auto it = _id_snapshot_map.begin(); it != _id_snapshot_map.end(); ++it) {
        if (checkPointInsideSnapshot(it->first, point)) {
            return it->first;
        }
    }

    return -1;
}

bool RecentsScreen::calibrateData(const gui::StyleSize &screen_size, const base::Display &display,
                                  RecentsScreen::Data &data)
{
    int parent_w = 0;
    int parent_h = 0;
    RecentsScreenSnapshot::Data &new_snapshot_data = data.snapshot_table.snapshot;
    const gui::StyleSize *parent_size = nullptr;

    ESP_UTILS_LOGD("Calibrate data");

    /* Main */
    parent_size = &screen_size;
    parent_w = parent_size->width;
    parent_h = parent_size->height;
    // Size
    ESP_UTILS_CHECK_FALSE_RETURN(display.calibrateCoreObjectSize(*parent_size, data.main.size), false,
                                 "Invalid main size");
    ESP_UTILS_CHECK_VALUE_RETURN(data.main.y_start, 0, parent_h - 1, false, "Invalid main y start");
    ESP_UTILS_CHECK_VALUE_RETURN(data.main.y_start + data.main.size.height, 1, parent_h, false,
                                 "Main height is out of range");
    parent_size = &data.main.size;
    parent_w = parent_size->width;
    parent_h = parent_size->height;
    // Layout
    ESP_UTILS_CHECK_VALUE_RETURN(data.main.layout_row_pad, 0, parent_h, false, "Invalid main layout row pad");
    ESP_UTILS_CHECK_VALUE_RETURN(data.main.layout_top_pad, 0, parent_h, false, "Invalid main layout top pad");
    ESP_UTILS_CHECK_VALUE_RETURN(data.main.layout_bottom_pad, 0, parent_h, false, "Invalid main layout bottom pad");

    /* Memory */
    if (data.flags.enable_memory) {
        parent_size = &data.main.size;
        parent_w = parent_size->width;
        parent_h = parent_size->height;
        // Main
        ESP_UTILS_CHECK_FALSE_RETURN(display.calibrateCoreObjectSize(*parent_size, data.memory.main_size), false,
                                     "Invalid memory main size");
        parent_size = &data.memory.main_size;
        parent_w = parent_size->width;
        parent_h = parent_size->height;
        ESP_UTILS_CHECK_VALUE_RETURN(data.memory.main_layout_x_right_offset, 0, parent_w, false,
                                     "Invalid memory main layout x right offset");
        // Label
        ESP_UTILS_CHECK_FALSE_RETURN(display.calibrateCoreFont(parent_size, data.memory.label_text_font), false,
                                     "Invalid memory label text font size");
        ESP_UTILS_CHECK_NULL_RETURN(data.memory.label_unit_text, false, "Invalid memory label unit text");
    }

    // Trash
    parent_size = &data.main.size;
    parent_w = parent_size->width;
    parent_h = parent_size->height;
    ESP_UTILS_CHECK_FALSE_RETURN(display.calibrateCoreObjectSize(*parent_size, data.trash_icon.default_size), false,
                                 "Invalid trash icon default size");
    ESP_UTILS_CHECK_FALSE_RETURN(display.calibrateCoreObjectSize(*parent_size, data.trash_icon.press_size), false,
                                 "Invalid trash icon eprbdefault size");
    ESP_UTILS_CHECK_NULL_RETURN(data.trash_icon.image.resource, false, "Invalid trash icon image resource");

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
    ESP_UTILS_CHECK_FALSE_RETURN(display.calibrateCoreObjectSize(*parent_size, data.snapshot_table.main_size), false,
                                 "Invalid snapshot_table main size");
    parent_size = &data.snapshot_table.main_size;
    parent_w = parent_size->width;
    ESP_UTILS_CHECK_VALUE_RETURN(data.snapshot_table.main_layout_column_pad, 0, parent_w, false,
                                 "Invalid snapshot_table main layout column pad");

    /* Snapshot */
    // Main
    parent_size = (new_snapshot_data.flags.enable_all_main_size_refer_screen) ? &screen_size :
                  &data.snapshot_table.main_size;
    ESP_UTILS_CHECK_FALSE_RETURN(display.calibrateCoreObjectSize(*parent_size, new_snapshot_data.main_size), false,
                                 "Invalid snapshot main size");
    // If referring to the screen, check with the main size
    if (new_snapshot_data.flags.enable_all_main_size_refer_screen) {
        ESP_UTILS_CHECK_VALUE_RETURN(new_snapshot_data.main_size.width, 1, data.snapshot_table.main_size.width, false,
                                     "Invalid snapshot main width");
        ESP_UTILS_CHECK_VALUE_RETURN(new_snapshot_data.main_size.height, 1, data.snapshot_table.main_size.height, false,
                                     "Invalid snapshot main height");
    }
    // Title
    parent_size = (new_snapshot_data.flags.enable_all_main_size_refer_screen) ? &screen_size :
                  &new_snapshot_data.main_size;
    ESP_UTILS_CHECK_FALSE_RETURN(display.calibrateCoreObjectSize(*parent_size, new_snapshot_data.title.main_size), false,
                                 "Invalid snapshot title size");
    // If referring to the screen, check with the main size
    if (new_snapshot_data.flags.enable_all_main_size_refer_screen) {
        ESP_UTILS_CHECK_VALUE_RETURN(new_snapshot_data.title.main_size.width, 1, new_snapshot_data.main_size.width, false,
                                     "Invalid snapshot title main width");
        ESP_UTILS_CHECK_VALUE_RETURN(new_snapshot_data.title.main_size.height, 1, new_snapshot_data.main_size.height, false,
                                     "Invalid snapshot title main height");
    }
    parent_size = &new_snapshot_data.title.main_size;
    parent_w = parent_size->width;
    ESP_UTILS_CHECK_VALUE_RETURN(new_snapshot_data.title.main_layout_column_pad, 0, parent_w, false,
                                 "Invalid snapshot title layout column pad");
    // Title Icon
    parent_size = &new_snapshot_data.title.main_size;
    ESP_UTILS_CHECK_FALSE_RETURN(display.calibrateCoreObjectSize(*parent_size, new_snapshot_data.title.icon_size), false,
                                 "Invalid snapshot title icon size");
    ESP_UTILS_CHECK_FALSE_RETURN(display.calibrateCoreFont(parent_size, new_snapshot_data.title.text_font), false,
                                 "Invalid snapshot title text font");
    // Image
    parent_size = (new_snapshot_data.flags.enable_all_main_size_refer_screen) ? &screen_size :
                  &new_snapshot_data.main_size;
    ESP_UTILS_CHECK_FALSE_RETURN(display.calibrateCoreObjectSize(*parent_size, new_snapshot_data.image.main_size), false,
                                 "Invalid snapshot image main size");
    // If referring to the screen, check with the main size
    if (new_snapshot_data.flags.enable_all_main_size_refer_screen) {
        ESP_UTILS_CHECK_VALUE_RETURN(new_snapshot_data.image.main_size.width, 1, new_snapshot_data.main_size.width, false,
                                     "Invalid snapshot image main width");
        ESP_UTILS_CHECK_VALUE_RETURN(new_snapshot_data.image.main_size.height, 1, new_snapshot_data.main_size.height, false,
                                     "Invalid snapshot image main height");
    }
    // All
    parent_size = &new_snapshot_data.main_size;
    parent_h = parent_size->height;
    ESP_UTILS_CHECK_VALUE_RETURN(new_snapshot_data.title.main_size.height + new_snapshot_data.image.main_size.height, 1,
                                 parent_h, false, "The sum of snapshot title height(%d) and image height(%d) out of main",
                                 new_snapshot_data.title.main_size.height, new_snapshot_data.image.main_size.height);

    return true;
}

bool RecentsScreen::updateByNewData(void)
{
    float h_factor = 0;
    float w_factor = 0;

    ESP_UTILS_LOGD("Update(0x%p)", this);
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

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
        _trash_icon_default_zoom = (int)(h_factor * LV_SCALE_NONE);
        lv_image_set_scale(_trash_icon.get(), _trash_icon_default_zoom);
    } else {
        _trash_icon_default_zoom = (int)(w_factor * LV_SCALE_NONE);
        lv_image_set_scale(_trash_icon.get(), _trash_icon_default_zoom);
    }
    h_factor = (float)(_data.trash_icon.press_size.height) /
               ((lv_img_dsc_t *)_data.trash_icon.image.resource)->header.h;
    w_factor = (float)(_data.trash_icon.press_size.width) /
               ((lv_img_dsc_t *)_data.trash_icon.image.resource)->header.w;
    if (h_factor < w_factor) {
        _trash_icon_press_zoom = (int)(h_factor * LV_SCALE_NONE);
    } else {
        _trash_icon_press_zoom = (int)(w_factor * LV_SCALE_NONE);
    }
    lv_obj_set_size(_trash_icon.get(), _data.trash_icon.default_size.width, _data.trash_icon.default_size.height);
    lv_obj_refr_size(_trash_icon.get());

    // Snapshot
    for (auto &it : _id_snapshot_map) {
        ESP_UTILS_CHECK_NULL_RETURN(it.second, false, "Invalid snapshot(%d)", it.first);
        ESP_UTILS_CHECK_FALSE_RETURN(it.second->updateByNewData(), false, "Update snapshot object style failed");
    }

    return true;
}

void RecentsScreen::onDataUpdateEventCallback(lv_event_t *event)
{
    RecentsScreen *recents_screen = nullptr;

    ESP_UTILS_LOGD("Data update event");
    ESP_UTILS_CHECK_NULL_EXIT(event, "Invalid event object");

    recents_screen = (RecentsScreen *)lv_event_get_user_data(event);
    ESP_UTILS_CHECK_NULL_EXIT(recents_screen, "Invalid app snapshot_table object");

    ESP_UTILS_CHECK_FALSE_EXIT(recents_screen->updateByNewData(), "Update object style failed");
}

void RecentsScreen::onTrashTouchEventCallback(lv_event_t *event)
{
    lv_event_code_t event_code = lv_event_get_code(event);
    RecentsScreen *recents_screen = (RecentsScreen *)lv_event_get_user_data(event);
    std::unordered_map<int, std::shared_ptr<RecentsScreenSnapshot>> id_snapshot_map;

    ESP_UTILS_LOGD("Trash touch event callback");
    ESP_UTILS_CHECK_NULL_EXIT(recents_screen, "Invalid recents_screen object");

    switch (event_code) {
    case LV_EVENT_CLICKED:
        ESP_UTILS_LOGD("Clicked");
        if (recents_screen->_is_trash_pressed_losted) {
            break;
        }
        // Since the snapshot may be deleted during the loop, we need to copy the map first
        id_snapshot_map = recents_screen->_id_snapshot_map;
        for (auto &it : id_snapshot_map) {
            lv_obj_send_event(recents_screen->getEventObject(), recents_screen->getSnapshotDeletedEventCode(),
                              reinterpret_cast<void *>(it.first));
        }
        // Send this event to notify that trash icon is clicked
        lv_obj_send_event(recents_screen->getEventObject(), recents_screen->getSnapshotDeletedEventCode(),
                          reinterpret_cast<void *>(0));
        break;
    case LV_EVENT_PRESSED:
        ESP_UTILS_LOGD("Pressed");
        // Zoom out icon
        lv_image_set_scale((lv_obj_t *)lv_event_get_target(event), recents_screen->_trash_icon_press_zoom);
        lv_obj_refr_size((lv_obj_t *)lv_event_get_target(event));
        recents_screen->_is_trash_pressed_losted = false;
        break;
    case LV_EVENT_PRESS_LOST:
        ESP_UTILS_LOGD("Press lost");
        recents_screen->_is_trash_pressed_losted = true;
        [[fallthrough]];
    case LV_EVENT_RELEASED:
        ESP_UTILS_LOGD("Released");
        // Zoom in icon
        lv_image_set_scale((lv_obj_t *)lv_event_get_target(event), recents_screen->_trash_icon_default_zoom);
        lv_obj_refr_size((lv_obj_t *)lv_event_get_target(event));
        break;
    default:
        break;
    }
}

} // namespace esp_brookesia::systems::phone
