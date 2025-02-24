/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <memory>
#include <cstdlib>
#include "lvgl.h"
#include "esp_brookesia_style_type.h"

class ESP_Brookesia_LvObj {
public:
    ESP_Brookesia_LvObj() = default;

    ESP_Brookesia_LvObj(lv_obj_t* p) {
        if ((p == nullptr) || !lv_obj_is_valid(p)) {
            return;
        }
        _obj = std::shared_ptr<lv_obj_t>(p, [](lv_obj_t* obj) {
            if (lv_obj_is_valid(obj)) {
                lv_obj_del(obj);
            }
        });
    }

    operator lv_obj_t*() const {
        return get();
    }
    lv_obj_t* operator->() const {
        return get();
    }

    bool applyStyleAttribute(ESP_Brookesia_StyleSize_t size, lv_style_selector_t selector = LV_PART_MAIN);
    bool applyStyleAttribute(ESP_Brookesia_StyleFont_t font, lv_style_selector_t selector = LV_PART_MAIN);
    bool applyStyleAttribute(ESP_Brookesia_StyleAlign_t align, lv_style_selector_t selector = LV_PART_MAIN);
    bool applyStyleAttribute(ESP_Brookesia_StyleLayoutFlex_t layout, lv_style_selector_t selector = LV_PART_MAIN);
    bool applyStyleAttribute(ESP_Brookesia_StyleGap_t gap, lv_style_selector_t selector = LV_PART_MAIN);
    // bool applyStyleAttribute(ESP_Brookesia_StyleColor_t color, lv_style_selector_t selector = LV_PART_MAIN);
    // bool applyStyleAttribute(ESP_Brookesia_StyleImage_t image, lv_style_selector_t selector = LV_PART_MAIN);

    void reset()
    {
        _obj.reset();
    }
    lv_obj_t* get() const
    {
        return _obj.get();
    }

    bool isValid() const {
        return (_obj != nullptr) && lv_obj_is_valid(_obj.get());
    }

private:
    std::shared_ptr<lv_obj_t> _obj = nullptr;
};

using ESP_Brookesia_LvObj_t = ESP_Brookesia_LvObj;
#define ESP_BROOKESIA_LV_OBJ(type, parent) \
    ESP_Brookesia_LvObj([&]() { \
        return lv_##type##_create(parent); \
    }())

using ESP_Brookesia_LvTimer_t = std::shared_ptr<lv_timer_t>;
struct LvTimerDeleter {
    void operator()(lv_timer_t *t)
    {
        lv_timer_del(t);
    }
};
#define ESP_BROOKESIA_LV_TIMER(func, t, data) ESP_Brookesia_LvTimer_t(lv_timer_create(func, t, data), LvTimerDeleter());

using ESP_Brookesia_LvAnim_t = std::shared_ptr<lv_anim_t>;
#define LvAnimConstructor()              \
    ({                                   \
        lv_anim_t *anim = new lv_anim_t; \
        if (anim != nullptr) {           \
            lv_anim_init(anim);          \
        };                               \
        anim;                            \
    })
struct LvAnimDeleter {
    void operator()(lv_anim_t *anim)
    {
        lv_anim_del(anim->var, anim->exec_cb);
        free(anim);
    }
};
#define ESP_BROOKESIA_LV_ANIM() ESP_Brookesia_LvAnim_t(LvAnimConstructor(), LvAnimDeleter());
