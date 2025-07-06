/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <memory>
#include <cstdlib>
#include "lvgl.h"
#include "esp_brookesia_lv_helper.hpp"
#include "style/esp_brookesia_gui_style.hpp"

namespace esp_brookesia::gui {

class LvObject {
public:
    explicit LvObject(lv_obj_t *p, bool is_auto_delete = true);
    ~LvObject();

    /**
     * @brief Disable copy operations
     */
    LvObject(const LvObject &other) = delete;
    LvObject &operator=(const LvObject &other) = delete;

    /**
     * @brief Enable move operations
     */
    LvObject(LvObject &&other):
        _native_handle(other._native_handle)
    {
        other._native_handle = nullptr;
    }
    LvObject &operator=(LvObject &&other)
    {
        if (this != &other) {
            _native_handle = other._native_handle;
            other._native_handle = nullptr;
        }
        return *this;
    }

    bool setStyle(lv_style_t *style);
    bool removeStyle(lv_style_t *style);

    bool setStyleAttribute(const StyleSize &size);
    bool setStyleAttribute(const StyleFont &font);
    bool setStyleAttribute(const StyleAlign &align);
    bool setStyleAttribute(const StyleLayoutFlex &layout);
    bool setStyleAttribute(const StyleGap &gap);
    bool setStyleAttribute(StyleColorItem color_type, const StyleColor &color);
    bool setStyleAttribute(StyleWidthItem width_type, int width);
    bool setStyleAttribute(const StyleImage &image);
    bool setStyleAttribute(LvObject &target, const StyleAlign &align);
    bool setStyleAttribute(StyleFlag flags, bool enable);
    bool setX(int x);
    bool setY(int y);
    bool scrollY_To(int y, bool is_animated);

    bool moveForeground();
    bool moveBackground();

    bool addEventCallback(lv_event_cb_t cb, lv_event_code_t code, void *user_data);
    bool delEventCallback(lv_event_cb_t cb, lv_event_code_t code, void *user_data);
    bool delEventCallback(lv_event_cb_t cb);

    bool isValid() const
    {
        return (_native_handle != nullptr) && lv_obj_is_valid(_native_handle);
    }
    bool hasState(lv_state_t state) const;
    bool hasFlags(StyleFlag flags) const;

    bool getX(int &x) const;
    bool getY(int &y) const;
    bool getArea(lv_area_t &area) const;

    lv_obj_t *getNativeHandle() const
    {
        return _native_handle;
    }

private:
    bool _is_auto_delete = true;
    lv_obj_t *_native_handle = nullptr;
};

using LvObjectSharedPtr = std::shared_ptr<LvObject>;
using LvObjectUniquePtr = std::unique_ptr<LvObject>;

} // namespace esp_brookesia::gui
