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

class ESP_Brookesia_LvObject {
public:
    static void Deleter(lv_obj_t* obj)
    {
        if (lv_obj_is_valid(obj)) {
            lv_obj_del(obj);
        }
    }

    ESP_Brookesia_LvObject() = default;
    ESP_Brookesia_LvObject(const ESP_Brookesia_LvObject& other) {
        _object = other._object;
    }
    ESP_Brookesia_LvObject(ESP_Brookesia_LvObject&& other) noexcept {
        _object = std::move(other._object);
    }
    ESP_Brookesia_LvObject(std::nullptr_t p)
    {
        _object = nullptr;
    }
    explicit ESP_Brookesia_LvObject(ESP_Brookesia_LvObject *parent)
    {
        if (parent != nullptr) {
            _object = std::shared_ptr<lv_obj_t>(lv_obj_create(parent->get()), &Deleter);
        }
    }
    explicit ESP_Brookesia_LvObject(lv_obj_t* p) {
        if ((p == nullptr) || lv_obj_is_valid(p)) {
            _object = std::shared_ptr<lv_obj_t>((p != nullptr) ? p : lv_obj_create(nullptr), &Deleter);
        }
    }

    ESP_Brookesia_LvObject& operator=(std::nullptr_t p) {
        _object = nullptr;
        return *this;
    }
    ESP_Brookesia_LvObject& operator=(const ESP_Brookesia_LvObject& other) {
        if (this != &other) {
            _object = other._object;
        }
        return *this;
    }
    ESP_Brookesia_LvObject& operator=(ESP_Brookesia_LvObject&& other) noexcept {
        if (this != &other) {
            _object = std::move(other._object);
        }
        return *this;
    }
    operator lv_obj_t*() const {
        return get();
    }
    lv_obj_t* operator->() const {
        return get();
    }

    void reset()
    {
        _object.reset();
    }
    lv_obj_t* get() const
    {
        return _object.get();
    }

    bool applyStyle(lv_style_t *style, lv_style_selector_t selector = LV_PART_MAIN);
    bool applyStyleAttribute(const ESP_Brookesia_StyleSize_t &size, lv_style_selector_t selector = LV_PART_MAIN);
    bool applyStyleAttribute(const ESP_Brookesia_StyleFont_t &font, lv_style_selector_t selector = LV_PART_MAIN);
    bool applyStyleAttribute(const ESP_Brookesia_StyleAlign_t &align);
    bool applyStyleAttribute(const ESP_Brookesia_StyleLayoutFlex_t &layout, lv_style_selector_t selector = LV_PART_MAIN);
    bool applyStyleAttribute(const ESP_Brookesia_StyleGap_t &gap, lv_style_selector_t selector = LV_PART_MAIN);
    bool applyStyleAttribute(
        ESP_Brookesia_StyleColorItem_t color_type, const ESP_Brookesia_StyleColor_t &color,
        lv_style_selector_t selector = LV_PART_MAIN
    );
    bool applyStyleAttribute(const ESP_Brookesia_StyleImage_t &image, lv_style_selector_t selector = LV_PART_MAIN);
    bool alignTo(ESP_Brookesia_LvObject &target, const ESP_Brookesia_StyleAlign_t &align);

    bool isChecked() const
    {
        return lv_obj_has_state(get(), LV_STATE_CHECKED);
    }
    bool isValid() const
    {
        return (_object != nullptr) && lv_obj_is_valid(_object.get());
    }

    static lv_color_t getColor(uint32_t color);
    static lv_align_t getAlign(const ESP_Brookesia_StyleAlignType_t &type);
    static lv_flex_flow_t getLayoutFlexFlow(const ESP_Brookesia_StyleFlexFlow_t &flow);
    static lv_flex_align_t getLayoutFlexAlign(const ESP_Brookesia_StyleFlexAlign_t &align);

private:
    std::shared_ptr<lv_obj_t> _object = nullptr;
};

using ESP_Brookesia_LvObj_t = ESP_Brookesia_LvObject;
#define ESP_BROOKESIA_LV_OBJ(type, parent) ESP_Brookesia_LvObject(lv_##type##_create(parent))

class ESP_Brookesia_LvImage: public ESP_Brookesia_LvObject {
public:
    using ESP_Brookesia_LvObject::applyStyleAttribute;

    ESP_Brookesia_LvImage() = default;
    ESP_Brookesia_LvImage(ESP_Brookesia_LvObject &parent):
        ESP_Brookesia_LvObject(lv_obj_create(parent.get())),
        _image(lv_img_create(get()))
    {
        if (isValid()) {
            lv_obj_center(_image.get());
            lv_obj_set_size(_image.get(), LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_clear_flag(_image.get(), LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
            lv_img_set_size_mode(_image.get(), LV_IMG_SIZE_MODE_REAL);
        }
    }

    bool applyStyleAttribute(const ESP_Brookesia_StyleImage_t &image, lv_style_selector_t selector = LV_PART_MAIN);
    bool setImage(const void *image);

    bool isValid() const
    {
        return ESP_Brookesia_LvObject::isValid() && _image.isValid();
    }

private:
    ESP_Brookesia_LvObject _image;
};

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
