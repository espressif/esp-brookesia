/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "esp_brookesia_conf_internal.h"
#include "esp_brookesia_core_utils.h"

const char *esp_brookesia_core_utils_path_to_file_name(const char *path)
{
    size_t i = 0;
    size_t pos = 0;
    char *p = (char *)path;

    while (*p) {
        i++;
        if (*p == '/' || *p == '\\') {
            pos = i;
        }
        p++;
    }

    return path + pos;
}

#define FONT_SWITH_CASE(size)           \
    case (size):                        \
    ret_font = &lv_font_montserrat_##size;   \
    break

bool esp_brookesia_core_utils_get_internal_font_by_size(uint8_t size_px, const lv_font_t **font)
{
    bool ret = true;
    const lv_font_t *ret_font = LV_FONT_DEFAULT;

    uint8_t temp_size = size_px;
    size_px = (size_px << 2) >> 2;
    if (size_px < ESP_BROOKESIA_STYLE_FONT_SIZE_MIN) {
        size_px = ESP_BROOKESIA_STYLE_FONT_SIZE_MIN;
    } else if (size_px > ESP_BROOKESIA_STYLE_FONT_SIZE_MAX) {
        size_px = ESP_BROOKESIA_STYLE_FONT_SIZE_MAX;
    }
    if (temp_size != size_px) {
        ESP_BROOKESIA_LOGW("Font size(%d) not support, use the nearest size(%d)", temp_size, size_px);
    }

    switch (size_px) {
#if LV_FONT_MONTSERRAT_8
        FONT_SWITH_CASE(8);
#endif
#if LV_FONT_MONTSERRAT_10
        FONT_SWITH_CASE(10);
#endif
#if LV_FONT_MONTSERRAT_12
        FONT_SWITH_CASE(12);
#endif
#if LV_FONT_MONTSERRAT_14
        FONT_SWITH_CASE(14);
#endif
#if LV_FONT_MONTSERRAT_16
        FONT_SWITH_CASE(16);
#endif
#if LV_FONT_MONTSERRAT_18
        FONT_SWITH_CASE(18);
#endif
#if LV_FONT_MONTSERRAT_20
        FONT_SWITH_CASE(20);
#endif
#if LV_FONT_MONTSERRAT_22
        FONT_SWITH_CASE(22);
#endif
#if LV_FONT_MONTSERRAT_24
        FONT_SWITH_CASE(24);
#endif
#if LV_FONT_MONTSERRAT_26
        FONT_SWITH_CASE(26);
#endif
#if LV_FONT_MONTSERRAT_28
        FONT_SWITH_CASE(28);
#endif
#if LV_FONT_MONTSERRAT_30
        FONT_SWITH_CASE(30);
#endif
#if LV_FONT_MONTSERRAT_32
        FONT_SWITH_CASE(32);
#endif
#if LV_FONT_MONTSERRAT_34
        FONT_SWITH_CASE(34);
#endif
#if LV_FONT_MONTSERRAT_36
        FONT_SWITH_CASE(36);
#endif
#if LV_FONT_MONTSERRAT_38
        FONT_SWITH_CASE(38);
#endif
#if LV_FONT_MONTSERRAT_40
        FONT_SWITH_CASE(40);
#endif
#if LV_FONT_MONTSERRAT_42
        FONT_SWITH_CASE(42);
#endif
#if LV_FONT_MONTSERRAT_44
        FONT_SWITH_CASE(44);
#endif
#if LV_FONT_MONTSERRAT_46
        FONT_SWITH_CASE(46);
#endif
#if LV_FONT_MONTSERRAT_48
        FONT_SWITH_CASE(48);
#endif
    default:
        ret = false;
        ESP_BROOKESIA_LOGE("No internal font size(%d) found, use default instead", temp_size);
        break;
    }

    if (font != NULL) {
        *font = ret_font;
    }

    return ret;
}

lv_color_t esp_brookesia_core_utils_get_random_color(void)
{
    srand((unsigned int)time(NULL));

    uint8_t r = rand() & 0xff;
    uint8_t g = rand() & 0xff;
    uint8_t b = rand() & 0xff;

    return lv_color_make(r, g, b);
}

bool esp_brookesia_core_utils_check_obj_out_of_parent(lv_obj_t *obj)
{
    lv_area_t child_coords;
    lv_area_t parent_coords;
    lv_obj_t *parent = lv_obj_get_parent(obj);

    lv_obj_refr_pos(obj);
    lv_obj_refr_pos(parent);
    lv_obj_update_layout(obj);
    lv_obj_update_layout(parent);
    lv_obj_get_coords(obj, &child_coords);
    lv_obj_get_coords(parent, &parent_coords);

    if ((child_coords.x1 < parent_coords.x1) || (child_coords.y1 < parent_coords.y1) ||
            (child_coords.x2 > parent_coords.x2) || (child_coords.y2 > parent_coords.y2)) {

        return true;
    }

    return false;
}

bool esp_brookesia_core_utils_check_event_code_valid(lv_event_code_t code)
{
    return ((code > _LV_EVENT_LAST) && (code < LV_EVENT_PREPROCESS));
}

lv_indev_t *esp_brookesia_core_utils_get_input_dev(const lv_disp_t *display, lv_indev_type_t type)
{
    lv_indev_t *indev = NULL;
    lv_indev_t *indev_tmp = lv_indev_get_next(NULL);

    while (indev_tmp != NULL) {
        if (indev_tmp->driver->disp == display && indev_tmp->driver->type == type) {
            indev = indev_tmp;
            break;
        }
        indev_tmp = lv_indev_get_next(indev_tmp);
    }

    return indev;
}

lv_anim_path_cb_t esp_brookesia_core_utils_get_anim_path_cb(ESP_Brookesia_LvAnimationPathType_t type)
{
    ESP_BROOKESIA_CHECK_FALSE_RETURN(type < ESP_BROOKESIA_LV_ANIM_PATH_TYPE_MAX, NULL, "Invalid animation path type(%d)", type);

    switch (type) {
    case ESP_BROOKESIA_LV_ANIM_PATH_TYPE_LINEAR:
        return lv_anim_path_linear;
        break;
    case ESP_BROOKESIA_LV_ANIM_PATH_TYPE_EASE_IN:
        return lv_anim_path_ease_in;
        break;
    case ESP_BROOKESIA_LV_ANIM_PATH_TYPE_EASE_OUT:
        return lv_anim_path_ease_out;
        break;
    case ESP_BROOKESIA_LV_ANIM_PATH_TYPE_EASE_IN_OUT:
        return lv_anim_path_ease_in_out;
        break;
    case ESP_BROOKESIA_LV_ANIM_PATH_TYPE_OVERSHOOT:
        return lv_anim_path_overshoot;
        break;
    case ESP_BROOKESIA_LV_ANIM_PATH_TYPE_BOUNCE:
        return lv_anim_path_bounce;
        break;
    case ESP_BROOKESIA_LV_ANIM_PATH_TYPE_STEP:
        return lv_anim_path_step;
        break;
    default:
        break;
    }
    ESP_BROOKESIA_LOGE("Invalid animation path type(%d)", type);

    return NULL;
}
