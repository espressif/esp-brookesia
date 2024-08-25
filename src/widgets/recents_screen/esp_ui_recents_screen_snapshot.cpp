/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_ui_recents_screen_snapshot.hpp"

using namespace std;

#if !ESP_UI_LOG_ENABLE_DEBUG_WIDGETS_RECENTS_SCREEN
#undef ESP_UI_LOGD
#define ESP_UI_LOGD(...)
#endif

ESP_UI_RecentsScreenSnapshot::ESP_UI_RecentsScreenSnapshot(const ESP_UI_Core &core,
        const ESP_UI_RecentsScreenSnapshotConf_t &conf,
        const ESP_UI_RecentsScreenSnapshotData_t &data):
    _core(core),
    _conf(conf),
    _data(data),
    _origin_y(0),
    _main_obj(nullptr),
    _drag_obj(nullptr),
    _title_obj(nullptr),
    _title_icon(nullptr),
    _title_label(nullptr),
    _snapshot_obj(nullptr),
    _snapshot_image(nullptr)
{
}

ESP_UI_RecentsScreenSnapshot::~ESP_UI_RecentsScreenSnapshot()
{
    ESP_UI_LOGD("Destroy(@0x%p)", this);
    if (!del()) {
        ESP_UI_LOGE("Delete failed");
    }
}

bool ESP_UI_RecentsScreenSnapshot::begin(lv_obj_t *parent)
{
    ESP_UI_LvObj_t main_obj = NULL;
    ESP_UI_LvObj_t drag_obj = NULL;
    ESP_UI_LvObj_t title_obj = NULL;
    ESP_UI_LvObj_t title_icon = NULL;
    ESP_UI_LvObj_t title_label = NULL;
    ESP_UI_LvObj_t snapshot_obj = NULL;
    ESP_UI_LvObj_t snapshot_image = NULL;

    ESP_UI_LOGD("Begin@0x%p)", this);
    ESP_UI_CHECK_NULL_RETURN(parent, false, "Invalid parent object");
    ESP_UI_CHECK_NULL_RETURN(_conf.name, false, "Invalid name");
    ESP_UI_CHECK_NULL_RETURN(_conf.snapshot_image_resource, false, "Invalid snapshot image");
    ESP_UI_CHECK_NULL_RETURN(_conf.icon_image_resource, false, "Invalid icon image");
    ESP_UI_CHECK_FALSE_RETURN(!checkInitialized(), false, "Snapshot is already initialized");

    /* Create objects */
    main_obj = ESP_UI_LV_OBJ(obj, parent);
    ESP_UI_CHECK_NULL_RETURN(main_obj, false, "Create main_obj failed");
    drag_obj = ESP_UI_LV_OBJ(obj, main_obj.get());
    ESP_UI_CHECK_NULL_RETURN(drag_obj, false, "Create drag obj failed");
    title_obj = ESP_UI_LV_OBJ(obj, drag_obj.get());
    ESP_UI_CHECK_NULL_RETURN(title_obj, false, "Create title obj failed");
    title_icon = ESP_UI_LV_OBJ(img, title_obj.get());
    ESP_UI_CHECK_NULL_RETURN(title_icon, false, "Create title icon failed");
    title_label = ESP_UI_LV_OBJ(label, title_obj.get());
    ESP_UI_CHECK_NULL_RETURN(title_label, false, "Create title label failed");
    snapshot_obj = ESP_UI_LV_OBJ(obj, drag_obj.get());
    ESP_UI_CHECK_NULL_RETURN(snapshot_obj, false, "Create snapshot obj failed");
    snapshot_image = ESP_UI_LV_OBJ(img, snapshot_obj.get());
    ESP_UI_CHECK_NULL_RETURN(snapshot_image, false, "Create snapshot image failed");

    /* Setup objects style */
    // Main
    lv_obj_add_style(main_obj.get(), _core.getCoreHome().getCoreContainerStyle(), 0);
    lv_obj_clear_flag(main_obj.get(), LV_OBJ_FLAG_SCROLLABLE);
    // Drag
    lv_obj_add_style(drag_obj.get(), _core.getCoreHome().getCoreContainerStyle(), 0);
    lv_obj_center(drag_obj.get());
    lv_obj_clear_flag(drag_obj.get(), LV_OBJ_FLAG_SCROLLABLE);
    // Title
    lv_obj_add_style(title_obj.get(), _core.getCoreHome().getCoreContainerStyle(), 0);
    lv_obj_align(title_obj.get(), LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_flex_flow(title_obj.get(), LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(title_obj.get(), LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(title_obj.get(), LV_OBJ_FLAG_SCROLLABLE);
    // Title icon
    lv_obj_add_style(title_icon.get(), _core.getCoreHome().getCoreContainerStyle(), 0);
    lv_obj_set_size(title_icon.get(), LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_img_set_size_mode(title_icon.get(), LV_IMG_SIZE_MODE_REAL);
    lv_img_set_src(title_icon.get(), _conf.icon_image_resource);
    // Tile label
    lv_obj_add_style(title_label.get(), _core.getCoreHome().getCoreContainerStyle(), 0);
    lv_label_set_text_static(title_label.get(), _conf.name);
    // Snapshot
    lv_obj_add_style(snapshot_obj.get(), _core.getCoreHome().getCoreContainerStyle(), 0);
    lv_obj_align(snapshot_obj.get(), LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_clear_flag(snapshot_obj.get(), LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_clip_corner(snapshot_obj.get(), true, 0);
    // Snapshot image
    lv_obj_add_style(snapshot_image.get(), _core.getCoreHome().getCoreContainerStyle(), 0);
    lv_obj_center(snapshot_image.get());
    lv_img_set_size_mode(snapshot_image.get(), LV_IMG_SIZE_MODE_REAL);
    lv_obj_clear_flag(snapshot_image.get(), LV_OBJ_FLAG_SCROLLABLE);

    /* Save objects */
    _main_obj = main_obj;
    _drag_obj = drag_obj;
    _title_obj = title_obj;
    _title_icon = title_icon;
    _title_label = title_label;
    _snapshot_obj = snapshot_obj;
    _snapshot_image = snapshot_image;

    // Update style
    ESP_UI_CHECK_FALSE_GOTO(updateByNewData(), err, "Update failed");

    // Other operations
    _origin_y = getCurrentY();

    return true;

err:
    ESP_UI_CHECK_FALSE_RETURN(del(), false, "Delete failed");

    return false;
}

bool ESP_UI_RecentsScreenSnapshot::del(void)
{
    ESP_UI_LOGD("Delete@0x%p)", this);

    if (!checkInitialized()) {
        return true;
    }

    _main_obj.reset();
    _drag_obj.reset();
    _title_obj.reset();
    _title_icon.reset();
    _title_label.reset();
    _snapshot_obj.reset();
    _snapshot_image.reset();

    return true;
}

int ESP_UI_RecentsScreenSnapshot::getCurrentY(void) const
{
    ESP_UI_CHECK_FALSE_RETURN(checkInitialized(), 0, "Not initialized");

    lv_obj_update_layout(_drag_obj.get());
    lv_obj_refr_pos(_drag_obj.get());

    return lv_obj_get_y(_drag_obj.get());
}

bool ESP_UI_RecentsScreenSnapshot::updateByNewData(void)
{
    int app_img_zoom = 0;
    float h_factor = 0;
    float w_factor = 0;

    ESP_UI_LOGD("Update(@0x%p)", this);
    ESP_UI_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    // Main
    lv_obj_set_size(_main_obj.get(), _data.main_size.width, _data.main_size.height);
    // Drag
    lv_obj_set_size(_drag_obj.get(), _data.main_size.width, _data.main_size.height);
    // Title
    lv_obj_set_size(_title_obj.get(), _data.title.main_size.width, _data.title.main_size.height);
    lv_obj_set_style_pad_column(_title_obj.get(), _data.title.main_layout_column_pad, 0);
    // Title icon
    h_factor = (float)(_data.title.icon_size.height) / ((const lv_img_dsc_t *)_conf.icon_image_resource)->header.h;
    w_factor = (float)(_data.title.icon_size.width) / ((const lv_img_dsc_t *)_conf.icon_image_resource)->header.w;
    if (h_factor < w_factor) {
        lv_img_set_zoom(_title_icon.get(), (int)(h_factor * LV_IMG_ZOOM_NONE));
    } else {
        lv_img_set_zoom(_title_icon.get(), (int)(w_factor * LV_IMG_ZOOM_NONE));
    }
    lv_obj_refr_size(_title_icon.get());
    // Title label
    lv_obj_set_style_text_font(_title_label.get(), (lv_font_t *)_data.title.text_font.font_resource, 0);
    lv_obj_set_style_text_color(_title_label.get(), lv_color_hex(_data.title.text_color.color), 0);
    lv_obj_set_style_text_opa(_title_label.get(), _data.title.text_color.opacity, 0);
    // Snapshot
    lv_obj_set_size(_snapshot_obj.get(), _data.image.main_size.width, _data.image.main_size.height);
    lv_obj_set_style_radius(_snapshot_obj.get(), _data.image.radius, 0);
    // Snapshot image
    if (_conf.snapshot_image_resource != _conf.icon_image_resource) {
        h_factor = (float)(_data.image.main_size.height) / ((const lv_img_dsc_t *)_conf.snapshot_image_resource)->header.h;
        w_factor = (float)(_data.image.main_size.width) / ((const lv_img_dsc_t *)_conf.snapshot_image_resource)->header.w;
        if (h_factor < w_factor) {
            app_img_zoom = (int)(h_factor * LV_IMG_ZOOM_NONE);
        } else {
            app_img_zoom = (int)(w_factor * LV_IMG_ZOOM_NONE);
        }
        lv_img_set_zoom(_snapshot_image.get(), app_img_zoom);
        lv_obj_align(_snapshot_image.get(), LV_ALIGN_TOP_MID, 0, 0);
    } else {
        lv_img_set_zoom(_snapshot_image.get(), LV_IMG_ZOOM_NONE);
        lv_obj_center(_snapshot_image.get());
    }
    lv_img_set_src(_snapshot_image.get(), _conf.snapshot_image_resource);

    return true;
}
