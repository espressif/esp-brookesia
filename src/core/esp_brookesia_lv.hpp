/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <memory>
#include "lvgl.h"

using ESP_Brookesia_LvObj_t = std::shared_ptr<lv_obj_t>;
struct LvObjDeleter {
    void operator()(lv_obj_t *obj)
    {
        if (lv_obj_is_valid(obj)) {
            lv_obj_del(obj);
        }
    }
};
#define ESP_BROOKESIA_LV_OBJ(type, parent) ESP_Brookesia_LvObj_t(lv_##type##_create(parent), LvObjDeleter());

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
