/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <memory>
#include <cstdlib>
#include <src/core/lv_obj.h>
#include "lvgl.h"
#if __has_include("lvgl_private.h")
#   include "lvgl_private.h"
#   include "lv_api_map_v8.h"
#else
#   include "src/lvgl_private.h"
#   include "src/lv_api_map_v8.h"
#endif
#include "style/esp_brookesia_gui_style.hpp"

namespace esp_brookesia::gui {

// Smart pointer for LVGL objects with automatic cleanup
using LvObjSharedPtr = std::shared_ptr<lv_obj_t>;
struct LvObjDeleter {
    void operator()(lv_obj_t *obj)
    {
        if ((obj != nullptr) && lv_obj_is_valid(obj)) {
            lv_obj_del(obj);
        }
    }
};

// Smart pointer for LVGL timers with automatic cleanup
using LvTimerSharedPtr = std::shared_ptr<lv_timer_t>;
struct LvTimerDeleter {
    void operator()(lv_timer_t *t)
    {
        lv_timer_del(t);
    }
};

// Smart pointer for LVGL animations with automatic cleanup
using LvAnimSharedPtr = std::shared_ptr<lv_anim_t>;
struct LvAnimDeleter {
    void operator()(lv_anim_t *anim)
    {
        lv_anim_del(anim->var, anim->exec_cb);
        delete anim;
    }
};

/**
 * @brief Convert GUI types to LVGL types
 */
lv_color_t toLvColor(uint32_t color);
lv_align_t toLvAlign(const StyleAlignType &type);
lv_text_align_t toLvTextAlign(const StyleAlignType &type);
lv_flex_flow_t toLvFlexFlow(const StyleLayoutFlex::FlowType &flow);
lv_flex_align_t toLvFlexAlign(const StyleLayoutFlex::AlignType &align);
lv_obj_flag_t toLvFlags(const StyleFlag &flags);

/**
 * @brief Helper functions
 */
bool checkLvObjIsValid(lv_obj_t *obj);
bool checkLvObjOutOfParent(lv_obj_t *obj);
bool checkLvEventCodeValid(lv_event_code_t code);
bool getLvInternalFontBySize(uint8_t size_px, const lv_font_t **font);
lv_color_t getLvRandomColor(void);
lv_indev_t *getLvInputDev(const lv_display_t *display, lv_indev_type_t type);
lv_anim_path_cb_t getLvAnimPathCb(esp_brookesia::gui::StyleAnimation::AnimationPathType type);
} // namespace esp_brookesia::gui

#define ESP_BROOKESIA_MAKE_LV_OBJ_PTR(type, parent) \
    esp_brookesia::gui::LvObjSharedPtr(lv_##type##_create(parent), esp_brookesia::gui::LvObjDeleter());
#define ESP_BROOKESIA_MAKE_LV_TIMER_PTR(func, t, data) \
    esp_brookesia::gui::LvTimerSharedPtr(lv_timer_create(func, t, data), esp_brookesia::gui::LvTimerDeleter());
#define ESP_BROOKESIA_MAKE_LV_ANIM_PTR() \
    esp_brookesia::gui::LvAnimSharedPtr( \
        [](){ \
            lv_anim_t *anim = new lv_anim_t; \
            if (anim != nullptr) { \
                lv_anim_init(anim); \
            } \
            return anim; \
        }(), \
        esp_brookesia::gui::LvAnimDeleter() \
    )

/**
 * @brief Backward compatibility for macro definitions
 */
#define ESP_BROOKESIA_LV_OBJ(type, parent)     ESP_BROOKESIA_MAKE_LV_OBJ_PTR(type, parent)
#define ESP_BROOKESIA_LV_TIMER(func, t, data)  ESP_BROOKESIA_MAKE_LV_TIMER_PTR(func, t, data)
#define ESP_BROOKESIA_LV_ANIM()                ESP_BROOKESIA_MAKE_LV_ANIM_PTR()

/**
 * @brief Backward compatibility for type definitions
 */
using ESP_Brookesia_LvObj_t = esp_brookesia::gui::LvObjSharedPtr;
using ESP_Brookesia_LvTimer_t = esp_brookesia::gui::LvTimerSharedPtr;
using ESP_Brookesia_LvAnim_t = esp_brookesia::gui::LvAnimSharedPtr;

/**
 * @brief Backward compatibility for functions
 */
inline bool esp_brookesia_core_utils_get_internal_font_by_size(uint8_t size_px, const lv_font_t **font)
{
    return esp_brookesia::gui::getLvInternalFontBySize(size_px, font);
}
inline lv_color_t esp_brookesia_core_utils_get_random_color(void)
{
    return esp_brookesia::gui::getLvRandomColor();
}
inline bool esp_brookesia_core_utils_check_obj_out_of_parent(lv_obj_t *obj)
{
    return esp_brookesia::gui::checkLvObjOutOfParent(obj);
}
inline bool esp_brookesia_core_utils_check_event_code_valid(lv_event_code_t code)
{
    return esp_brookesia::gui::checkLvEventCodeValid(code);
}
inline lv_indev_t *esp_brookesia_core_utils_get_input_dev(const lv_display_t *display, lv_indev_type_t type)
{
    return esp_brookesia::gui::getLvInputDev(display, type);
}
inline lv_anim_path_cb_t esp_brookesia_core_utils_get_anim_path_cb(esp_brookesia::gui::StyleAnimation::AnimationPathType type)
{
    return esp_brookesia::gui::getLvAnimPathCb(type);
}

/**
 * @brief Overload the `|` operator for `lv_obj_flag_t` to avoid `-fpermissive` errors
 */
inline lv_obj_flag_t operator|(lv_obj_flag_t a, lv_obj_flag_t b)
{
    return static_cast<lv_obj_flag_t>(static_cast<int>(a) | static_cast<int>(b));
}
inline void lv_obj_remove_flag(lv_obj_t *obj, int flag)
{
    lv_obj_remove_flag(obj, static_cast<lv_obj_flag_t>(flag));
}

/**
 * @brief Overload the `|` operator for `lv_buttonmatrix_ctrl_t` to avoid `-fpermissive` errors
 */
inline lv_buttonmatrix_ctrl_t operator|(lv_buttonmatrix_ctrl_t a, int b)
{
    return static_cast<lv_buttonmatrix_ctrl_t>(static_cast<int>(a) | b);
}
inline lv_buttonmatrix_ctrl_t operator|(lv_buttonmatrix_ctrl_t a, lv_buttonmatrix_ctrl_t b)
{
    return static_cast<lv_buttonmatrix_ctrl_t>(static_cast<int>(a) | static_cast<int>(b));
}
inline lv_buttonmatrix_ctrl_t operator|(int a, lv_buttonmatrix_ctrl_t b)
{
    return static_cast<lv_buttonmatrix_ctrl_t>(a | static_cast<int>(b));
}
