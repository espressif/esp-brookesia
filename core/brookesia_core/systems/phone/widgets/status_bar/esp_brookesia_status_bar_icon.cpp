/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <memory>
#include "esp_brookesia_systems_internal.h"
#if !ESP_BROOKESIA_PHONE_STATUS_BAR_ENABLE_DEBUG_LOG
#   define ESP_BROOKESIA_UTILS_DISABLE_DEBUG_LOG
#endif
#include "phone/private/esp_brookesia_phone_utils.hpp"
#include "systems/base/esp_brookesia_base_context.hpp"
#include "lvgl/esp_brookesia_lv_helper.hpp"
#include "esp_brookesia_status_bar_icon.hpp"

using namespace std;
using namespace esp_brookesia::gui;

namespace esp_brookesia::systems::phone {

StatusBarIcon::StatusBarIcon(const StatusBarIcon::Data &data)
    : _data(data)
{
}

StatusBarIcon::~StatusBarIcon()
{
    ESP_UTILS_LOGD("Destroy(@0x%p)", this);
    if (!del()) {
        ESP_UTILS_LOGE("Delete failed");
    }
}

bool StatusBarIcon::begin(base::Context &core, lv_obj_t *parent)
{
    ESP_Brookesia_LvObj_t main_obj = nullptr;
    ESP_Brookesia_LvObj_t image_obj = nullptr;
    vector<ESP_Brookesia_LvObj_t> image_objs;

    ESP_UTILS_LOGD("Begin(@0x%p)", this);
    ESP_UTILS_CHECK_NULL_RETURN(parent, false, "Invalid parent");
    ESP_UTILS_CHECK_FALSE_RETURN(!checkInitialized(), false, "Icon is already initialized");

    /* Create objects */
    // Main
    main_obj = ESP_BROOKESIA_LV_OBJ(obj, parent);
    ESP_UTILS_CHECK_NULL_RETURN(main_obj, false, "Create main object failed");
    // Image
    for (int i = 0; i < _data.icon.image_num; i++) {
        image_obj = ESP_BROOKESIA_LV_OBJ(img, main_obj.get());
        ESP_UTILS_CHECK_NULL_RETURN(image_obj, false, "Create icon image[%d] failed", i);
        image_objs.push_back(image_obj);
    }

    /* Setup objects style */
    // Main
    lv_obj_add_style(main_obj.get(), core.getDisplay().getCoreContainerStyle(), 0);
    lv_obj_clear_flag(main_obj.get(), LV_OBJ_FLAG_SCROLLABLE);
    // Image
    for (auto &image_obj_tmp : image_objs) {
        lv_obj_add_style(image_obj_tmp.get(), core.getDisplay().getCoreContainerStyle(), 0);
        lv_obj_align(image_obj_tmp.get(), LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_size(image_obj_tmp.get(), LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        // lv_image_set_size_mode(image_obj_tmp.get(), LV_IMG_SIZE_MODE_REAL);
        lv_obj_add_flag(image_obj_tmp.get(), LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_clear_flag(image_objs[0].get(), LV_OBJ_FLAG_HIDDEN);

    // Save objects
    _main_obj = main_obj;
    _image_objs = image_objs;

    // Update style
    ESP_UTILS_CHECK_FALSE_GOTO(updateByNewData(), err, "Update failed");

    return true;

err:
    ESP_UTILS_CHECK_FALSE_RETURN(del(), false, "Delete failed");

    return false;
}

bool StatusBarIcon::del(void)
{
    ESP_UTILS_LOGD("Delete(@0x%p)", this);

    if (!checkInitialized()) {
        return true;
    }

    _main_obj.reset();
    _image_objs.clear();

    return true;
}

bool StatusBarIcon::setCurrentState(int state)
{
    uint8_t image_resource_num = _image_objs.size();
    lv_obj_t *img_obj = nullptr;

    ESP_UTILS_LOGD("Set state(%d)", state);
    ESP_UTILS_CHECK_FALSE_RETURN(state < image_resource_num, false, "Invalid state(%d)", state);
    ESP_UTILS_CHECK_NULL_RETURN(_main_obj, false, "Invalid main object");

    if (state == _current_state) {
        return true;
    }

    if (state < 0) {
        lv_obj_add_flag(_main_obj.get(), LV_OBJ_FLAG_HIDDEN);
        goto end;
    } else if (_current_state < 0) {
        lv_obj_clear_flag(_main_obj.get(), LV_OBJ_FLAG_HIDDEN);
    }

    for (int i = 0; i < image_resource_num; i++) {
        img_obj = _image_objs[i].get();
        if (i == state) {
            lv_obj_clear_flag(img_obj, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(img_obj, LV_OBJ_FLAG_HIDDEN);
        }
    }

end:
    _current_state = state;

    return true;
}

bool StatusBarIcon::updateByNewData(void)
{
    int image_resource_num = _image_objs.size();
    float h_factor = 0;
    float w_factor = 0;
    const lv_img_dsc_t *img_dsc = nullptr;
    ESP_Brookesia_LvObj_t main_obj = _main_obj;
    ESP_Brookesia_LvObj_t image_obj = nullptr;

    ESP_UTILS_LOGD("Update(0x%p)", this);
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Icon is not initialized");

    // Update main object style
    lv_obj_set_size(main_obj.get(), _data.size.width, _data.size.height);
    if (_is_out_of_parent && _current_state >= 0) {
        _is_out_of_parent = false;
        lv_obj_clear_flag(main_obj.get(), LV_OBJ_FLAG_HIDDEN);
    }
    if (esp_brookesia_core_utils_check_obj_out_of_parent(main_obj.get())) {
        _is_out_of_parent = true;
        lv_obj_add_flag(main_obj.get(), LV_OBJ_FLAG_HIDDEN);
        ESP_UTILS_LOGW("Icon out of area, hide it");
    }

    // Update the size of the image object
    for (int i = 0; i < image_resource_num; i++) {
        img_dsc = (const lv_img_dsc_t *)_data.icon.images[i].resource;
        image_obj = _image_objs[i];
        lv_img_set_src(image_obj.get(), img_dsc);
        lv_obj_set_style_img_recolor(image_obj.get(), lv_color_hex(_data.icon.images[i].recolor.color), 0);
        lv_obj_set_style_img_recolor_opa(image_obj.get(), _data.icon.images[i].recolor.opacity, 0);
        // Calculate the multiple of the size between the target and the image.
        h_factor = (float)(_data.size.height) / img_dsc->header.h;
        w_factor = (float)(_data.size.width) / img_dsc->header.w;
        // Scale the image to a suitable size.
        // So you don’t have to consider the size of the source image.
        if (h_factor < w_factor) {
            lv_image_set_scale(image_obj.get(), (int)(h_factor * LV_SCALE_NONE));
        } else {
            lv_image_set_scale(image_obj.get(), (int)(w_factor * LV_SCALE_NONE));
        }
        lv_obj_refr_size(image_obj.get());
    }

    return true;
}

} // namespace esp_brookesia::systems::phone
