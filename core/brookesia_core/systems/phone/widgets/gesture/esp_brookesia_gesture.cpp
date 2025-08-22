/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <limits>
#include <cmath>
#include "esp_brookesia_systems_internal.h"
#if !ESP_BROOKESIA_PHONE_GESTURE_ENABLE_DEBUG_LOG
#   define ESP_BROOKESIA_UTILS_DISABLE_DEBUG_LOG
#endif
#include "phone/private/esp_brookesia_phone_utils.hpp"
#include "esp_brookesia_gesture.hpp"

using namespace std;
using namespace esp_brookesia::gui;

namespace esp_brookesia::systems::phone {

Gesture::Gesture(base::Context &core_in, const Gesture::Data &data_in)
    : core(core_in)
    , data(data_in)
{
}

Gesture::~Gesture()
{
    ESP_UTILS_LOGD("Destroy(0x%p)", this);
    if (!del()) {
        ESP_UTILS_LOGE("Delete failed");
    }
}

bool Gesture::begin(lv_obj_t *parent)
{
    ESP_Brookesia_LvTimer_t detect_timer = nullptr;
    ESP_Brookesia_LvObj_t event_mask_obj = nullptr;
    array<ESP_Brookesia_LvObj_t, static_cast<int>(Gesture::IndicatorBarType::MAX)> indicator_bars = {};
    array<ESP_Brookesia_LvAnim_t, static_cast<int>(Gesture::IndicatorBarType::MAX)> indicator_bar_scale_back_anims = {};
    lv_event_code_t press_event_code = LV_EVENT_ALL;
    lv_event_code_t pressing_event_code = LV_EVENT_ALL;
    lv_event_code_t release_event_code = LV_EVENT_ALL;

    ESP_UTILS_LOGD("Begin(0x%p)", this);
    ESP_UTILS_CHECK_NULL_RETURN(core.getTouchDevice(), false, "Invalid core touch device");

    /* Create objects */
    detect_timer = ESP_BROOKESIA_LV_TIMER(onTouchDetectTimerCallback, data.detect_period_ms, this);
    ESP_UTILS_CHECK_NULL_RETURN(detect_timer, false, "Create detect timer failed");
    event_mask_obj = ESP_BROOKESIA_LV_OBJ(obj, parent);
    ESP_UTILS_CHECK_NULL_RETURN(event_mask_obj, false, "Create event & mask object failed");
    press_event_code = core.getFreeEventCode();
    ESP_UTILS_CHECK_FALSE_RETURN(esp_brookesia_core_utils_check_event_code_valid(press_event_code), false,
                                 "Invalid press event code");
    pressing_event_code = core.getFreeEventCode();
    ESP_UTILS_CHECK_FALSE_RETURN(esp_brookesia_core_utils_check_event_code_valid(pressing_event_code), false,
                                 "Invalid pressing event code");
    release_event_code = core.getFreeEventCode();
    ESP_UTILS_CHECK_FALSE_RETURN(esp_brookesia_core_utils_check_event_code_valid(release_event_code), false,
                                 "Invalid release event code");
    for (int i = 0; i < static_cast<int>(Gesture::IndicatorBarType::MAX); i++) {
        indicator_bars[i] = ESP_BROOKESIA_LV_OBJ(bar, parent);
        ESP_UTILS_CHECK_NULL_RETURN(indicator_bars[i], false, "Create indicator bar failed");
        indicator_bar_scale_back_anims[i] = ESP_BROOKESIA_LV_ANIM();
        ESP_UTILS_CHECK_NULL_RETURN(indicator_bar_scale_back_anims[i], false, "Create indicator bar animation failed");
        _indicator_bar_anim_var[i] = {
            .gesture = this,
            .type = (Gesture::IndicatorBarType)i,
        };
    }

    /* Setup objects */
    // Event mask
    lv_obj_add_style(event_mask_obj.get(), core.getDisplay().getCoreContainerStyle(), 0);
    lv_obj_add_flag(event_mask_obj.get(), LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_HIDDEN);
    lv_obj_center(event_mask_obj.get());
    // Indicator bar
    for (int i = 0; i < static_cast<int>(Gesture::IndicatorBarType::MAX); i++) {
        // Bar
        lv_obj_add_style(indicator_bars[i].get(), core.getDisplay().getCoreContainerStyle(), 0);
        lv_obj_clear_flag(indicator_bars[i].get(), LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(indicator_bars[i].get(), LV_OBJ_FLAG_HIDDEN);
        lv_bar_set_range(indicator_bars[i].get(), 0, 100);
        lv_bar_set_start_value(indicator_bars[i].get(), 0, LV_ANIM_OFF);
        lv_bar_set_value(indicator_bars[i].get(), 100, LV_ANIM_OFF);
        // Animation
        lv_anim_set_user_data(indicator_bar_scale_back_anims[i].get(), reinterpret_cast<void *>(static_cast<uintptr_t>(i)));
        lv_anim_set_var(indicator_bar_scale_back_anims[i].get(), &_indicator_bar_anim_var[i]);
        lv_anim_set_early_apply(indicator_bar_scale_back_anims[i].get(), false);
        lv_anim_set_exec_cb(indicator_bar_scale_back_anims[i].get(), onIndicatorBarScaleBackAnimationExecuteCallback);
        lv_anim_set_ready_cb(indicator_bar_scale_back_anims[i].get(), onIndicatorBarScaleBackAnimationReadyCallback);
    }

    // Save objects
    _touch_device = core.getTouchDevice();
    _detect_timer = detect_timer;
    _event_mask_obj = event_mask_obj;
    _press_event_code = press_event_code;
    _pressing_event_code = pressing_event_code;
    _release_event_code = release_event_code;
    _indicator_bars = indicator_bars;
    _indicator_bar_scale_back_anims = indicator_bar_scale_back_anims;

    // Update the object style
    ESP_UTILS_CHECK_FALSE_GOTO(updateByNewData(), err, "Update failed");

    return true;

err:
    ESP_UTILS_CHECK_FALSE_RETURN(del(), false, "Delete gesture failed");

    return false;
}

bool Gesture::del(void)
{
    ESP_UTILS_LOGD("Delete(0x%p)", this);

    _direction_tan_threshold = 0;
    _touch_start_tick = 0;
    _detect_timer.reset();
    resetGestureInfo();
    _event_mask_obj.reset();
    for (int i = 0; i < static_cast<int>(Gesture::IndicatorBarType::MAX); i++) {
        _indicator_bar_scale_back_anims[i].reset();
    }

    return true;
}

bool Gesture::readTouchPoint(int &x, int &y) const
{
    lv_point_t point = {};

    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    if (_touch_device->state != LV_INDEV_STATE_PRESSED) {
        return false;
    }

    lv_indev_get_point(_touch_device, &point);
    if ((point.x >= core.getData().screen_size.width) || (point.y >= core.getData().screen_size.height)) {
        return false;
    }

    x = point.x;
    y = point.y;

    return true;
}

bool Gesture::checkMaskVisible(void) const
{
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    return !lv_obj_has_flag(_event_mask_obj.get(), LV_OBJ_FLAG_HIDDEN);
}

bool Gesture::checkIndicatorBarVisible(Gesture::IndicatorBarType type) const
{
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    auto type_int = static_cast<int>(type);
    ESP_UTILS_CHECK_VALUE_RETURN(type_int, 0, static_cast<int>(Gesture::IndicatorBarType::MAX), false, "Invalid indicator bar type");

    return !lv_obj_has_flag(_indicator_bars[type_int].get(), LV_OBJ_FLAG_HIDDEN);
}

int Gesture::getIndicatorBarLength(Gesture::IndicatorBarType type) const
{
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), -1, "Not initialized");
    auto type_int = static_cast<int>(type);
    ESP_UTILS_CHECK_VALUE_RETURN(type_int, 0, static_cast<int>(Gesture::IndicatorBarType::MAX), false, "Invalid indicator bar type");

    lv_obj_update_layout(_indicator_bars[type_int].get());
    lv_obj_refresh_self_size(_indicator_bars[type_int].get());

    if (type == Gesture::IndicatorBarType::LEFT || type == Gesture::IndicatorBarType::RIGHT) {
        return lv_obj_get_height(_indicator_bars[type_int].get());
    } else if (type == Gesture::IndicatorBarType::BOTTOM) {
        return lv_obj_get_width(_indicator_bars[type_int].get());
    }

    return -1;
}

bool Gesture::calibrateData(const gui::StyleSize &screen_size, const base::Display &display,
                            Gesture::Data &data)
{
    int parent_w = 0;
    int parent_h = 0;
    const gui::StyleSize *parent_size = nullptr;

    ESP_UTILS_LOGD("Calibrate data");

    // Threshold
    parent_size = &screen_size;
    parent_w = parent_size->width;
    parent_h = parent_size->height;
    ESP_UTILS_CHECK_FALSE_RETURN(data.detect_period_ms > 0, false, "Invalid detect period");
    ESP_UTILS_CHECK_VALUE_RETURN(data.threshold.direction_vertical, 1, parent_h, false,
                                 "Invalid vertical direction threshold");
    ESP_UTILS_CHECK_VALUE_RETURN(data.threshold.direction_horizon, 1, parent_w, false,
                                 "Invalid horizon direction threshold");
    ESP_UTILS_CHECK_VALUE_RETURN(data.threshold.direction_angle, 1, 89, false, "Invalid direction angle threshold");
    ESP_UTILS_CHECK_VALUE_RETURN(data.threshold.horizontal_edge, 1, parent_w, false, "Invalid left edge threshold");
    ESP_UTILS_CHECK_VALUE_RETURN(data.threshold.vertical_edge, 1, parent_h, false, "Invalid top edge threshold");
    ESP_UTILS_CHECK_FALSE_RETURN(data.threshold.speed_slow_px_per_ms > 0, false, "Invalid speed slow threshold");
    ESP_UTILS_CHECK_FALSE_RETURN(data.threshold.duration_short_ms > 0, false, "Invalid duration short threshold");
    // Left/Right indicator bar
    for (int i = 0; i < static_cast<int>(Gesture::IndicatorBarType::MAX); i++) {
        if (!data.flags.enable_indicator_bars[i]) {
            continue;
        }
        ESP_UTILS_CHECK_FALSE_RETURN(display.calibrateCoreObjectSize(screen_size, data.indicator_bars[i].main.size_max), false,
                                     "Calibrate indicator bar main size max failed");
        ESP_UTILS_CHECK_FALSE_RETURN(display.calibrateCoreObjectSize(screen_size, data.indicator_bars[i].main.size_min, true),
                                     false, "Calibrate indicator bar main size min failed");
        switch (static_cast<Gesture::IndicatorBarType>(i)) {
        case Gesture::IndicatorBarType::LEFT:
        case Gesture::IndicatorBarType::RIGHT:
            parent_size = &data.indicator_bars[i].main.size_min;
            parent_w = parent_size->width;
            ESP_UTILS_CHECK_VALUE_RETURN(data.indicator_bars[i].main.layout_pad_all, 0, parent_w / 2, false,
                                         "Invalid indicator bar main layout pad all");
            break;
        case Gesture::IndicatorBarType::BOTTOM:
            parent_size = &data.indicator_bars[i].main.size_min;
            parent_h = parent_size->height;
            ESP_UTILS_CHECK_VALUE_RETURN(data.indicator_bars[i].main.layout_pad_all, 0, parent_h / 2, false,
                                         "Invalid indicator bar main layout pad all");
            break;
        default:
            break;
        }
    }

    return true;
}

bool Gesture::setMaskObjectVisible(bool visible) const
{
    ESP_UTILS_LOGD("Set mask object visible(%d)", visible);
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    if (visible) {
        lv_indev_reset(_touch_device, NULL);
        lv_obj_move_foreground(_event_mask_obj.get());
        lv_obj_clear_flag(_event_mask_obj.get(), LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(_event_mask_obj.get(), LV_OBJ_FLAG_HIDDEN);
    }

    return true;
}

bool Gesture::setIndicatorBarLength(Gesture::IndicatorBarType type, int length) const
{
    ESP_UTILS_LOGD("Set indicator bar(%d) length(%d)", type, length);
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    auto type_int = static_cast<int>(type);
    ESP_UTILS_CHECK_VALUE_RETURN(type_int, 0, static_cast<int>(Gesture::IndicatorBarType::MAX), false, "Invalid indicator bar type");

    if (!data.flags.enable_indicator_bars[type_int]) {
        return true;
    }

    const Gesture::IndicatorBarData &bar_data = data.indicator_bars[type_int];
    if (type == Gesture::IndicatorBarType::LEFT || type == Gesture::IndicatorBarType::RIGHT) {
        length = max(min(length, bar_data.main.size_max.height), bar_data.main.size_min.height);
        lv_obj_set_height(_indicator_bars[type_int].get(), length);
    } else if (type == Gesture::IndicatorBarType::BOTTOM) {
        length = max(min(length, bar_data.main.size_max.width), bar_data.main.size_min.width);
        lv_obj_set_width(_indicator_bars[type_int].get(), length);
    }

    return true;
}

bool Gesture::setIndicatorBarLengthByOffset(Gesture::IndicatorBarType type, int offset) const
{
    ESP_UTILS_LOGD("Set indicator bar(%d) length by offset(%d)", type, offset);
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    auto type_int = static_cast<int>(type);
    ESP_UTILS_CHECK_VALUE_RETURN(type_int, 0, static_cast<int>(Gesture::IndicatorBarType::MAX), false, "Invalid indicator bar type");

    int target_len = 0;
    int max_len = 0;
    float erase_len_ratio = 0;

    if (!data.flags.enable_indicator_bars[type_int]) {
        return true;
    }

    switch (type) {
    case Gesture::IndicatorBarType::LEFT:
    case Gesture::IndicatorBarType::RIGHT:
        offset = max(0, min(offset, (int)data.threshold.direction_horizon));
        max_len = data.indicator_bars[type_int].main.size_max.height;
        break;
    case Gesture::IndicatorBarType::BOTTOM:
        offset = max(0, min(offset, (int)data.threshold.direction_vertical));
        max_len = data.indicator_bars[type_int].main.size_max.width;
        break;
    default:
        ESP_UTILS_CHECK_FALSE_RETURN(false, -1, "Invalid type");
    }
    erase_len_ratio = (offset * _indicator_bar_scale_factors[type_int]) / (float)max_len;
    target_len =  max_len * (1 - erase_len_ratio);

    const Gesture::IndicatorBarData &bar_data = data.indicator_bars[type_int];
    if (type == Gesture::IndicatorBarType::LEFT || type == Gesture::IndicatorBarType::RIGHT) {
        target_len = max(target_len, (int)bar_data.main.size_min.height);
        lv_obj_set_height(_indicator_bars[type_int].get(), target_len);
    } else if (type == Gesture::IndicatorBarType::BOTTOM) {
        target_len = max(target_len, (int)bar_data.main.size_min.width);
        lv_obj_set_width(_indicator_bars[type_int].get(), target_len);
    }

    return true;
}

bool Gesture::setIndicatorBarVisible(Gesture::IndicatorBarType type, bool visible)
{
    ESP_UTILS_LOGD("Set indicator bar(%d) visible(%d)", type, visible);
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    auto type_int = static_cast<int>(type);
    ESP_UTILS_CHECK_VALUE_RETURN(type_int, 0, static_cast<int>(Gesture::IndicatorBarType::MAX), false, "Invalid indicator bar type");

    if (!data.flags.enable_indicator_bars[type_int]) {
        return true;
    }

    if (visible) {
        lv_obj_move_foreground(_indicator_bars[type_int].get());
        lv_obj_clear_flag(_indicator_bars[type_int].get(), LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(_indicator_bars[type_int].get(), LV_OBJ_FLAG_HIDDEN);
        ESP_UTILS_CHECK_FALSE_RETURN(setIndicatorBarLength(type, _indicator_bar_max_lengths[type_int]), false,
                                     "Set indicator bar length failed");
    }

    return true;
}

bool Gesture::controlIndicatorBarScaleBackAnim(Gesture::IndicatorBarType type, bool start)
{
    int length = 0;

    ESP_UTILS_LOGD("Control indicator bar(%d) scale back animation(%d)", type, start);
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");
    auto type_int = static_cast<int>(type);
    ESP_UTILS_CHECK_VALUE_RETURN(type_int, 0, static_cast<int>(Gesture::IndicatorBarType::MAX), false, "Invalid indicator bar type");

    if (!data.flags.enable_indicator_bars[type_int]) {
        return true;
    }

    length = getIndicatorBarLength(type);
    ESP_UTILS_CHECK_FALSE_RETURN(length >= 0, false, "Get indicator bar length failed");

    if (start) {
        if (_flags.is_indicator_bar_scale_back_anim_running[type_int]) {
            return true;
        }
        if (length == _indicator_bar_max_lengths[type_int]) {
            if (type != Gesture::IndicatorBarType::BOTTOM) {
                ESP_UTILS_CHECK_FALSE_RETURN(
                    setIndicatorBarVisible(type, false), false, "Set indicator bar visible failed"
                );
            }
            return true;
        }
        lv_anim_set_values(_indicator_bar_scale_back_anims[type_int].get(), length, _indicator_bar_max_lengths[type_int]);
        ESP_UTILS_CHECK_NULL_RETURN(lv_anim_start(_indicator_bar_scale_back_anims[type_int].get()), false,
                                    "Start animation failed");
        _flags.is_indicator_bar_scale_back_anim_running[type_int] = true;
    } else {
        if (_flags.is_indicator_bar_scale_back_anim_running[type_int]) {
            ESP_UTILS_CHECK_FALSE_RETURN(
                lv_anim_del(_indicator_bar_scale_back_anims[type_int]->var, _indicator_bar_scale_back_anims[type_int]->exec_cb),
                false, "Delete animation failed"
            );
            _flags.is_indicator_bar_scale_back_anim_running[type_int] = false;
        }
    }

    return true;
}

void Gesture::resetGestureInfo(void)
{
    Info reset_info = GESTURE_INFO_INIT;
    _info = reset_info;
}

bool Gesture::updateByNewData(void)
{
    ESP_UTILS_LOGD("Update(0x%p)", this);
    ESP_UTILS_CHECK_FALSE_RETURN(checkInitialized(), false, "Not initialized");

    int bar_range = 0;
    int align_x_offset = 0;
    int align_y_offset = 0;
    lv_align_t align = LV_ALIGN_DEFAULT;
    // Timer
    lv_timer_set_period(_detect_timer.get(), data.detect_period_ms);
    // Mask
    lv_obj_set_size(_event_mask_obj.get(), core.getData().screen_size.width, core.getData().screen_size.height);
    // Indicator bar
    for (int i = 0; i < static_cast<int>(Gesture::IndicatorBarType::MAX); i++) {
        const Gesture::IndicatorBarData &bar_data = data.indicator_bars[i];
        // Main
        lv_obj_set_size(_indicator_bars[i].get(), bar_data.main.size_max.width, bar_data.main.size_max.height);
        lv_obj_set_style_radius(_indicator_bars[i].get(), bar_data.main.radius, 0);
        lv_obj_set_style_pad_all(_indicator_bars[i].get(), bar_data.main.layout_pad_all, 0);
        lv_obj_set_style_bg_color(_indicator_bars[i].get(), lv_color_hex(bar_data.main.color.color), 0);
        lv_obj_set_style_bg_opa(_indicator_bars[i].get(), bar_data.main.color.opacity, 0);
        // Indicator
        lv_obj_set_style_radius(_indicator_bars[i].get(), bar_data.indicator.radius, LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(_indicator_bars[i].get(), lv_color_hex(bar_data.indicator.color.color),
                                  LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(_indicator_bars[i].get(), bar_data.indicator.color.opacity, LV_PART_INDICATOR);
        lv_anim_set_path_cb(_indicator_bar_scale_back_anims[i].get(),
                            esp_brookesia_core_utils_get_anim_path_cb(bar_data.animation.scale_back_path_type));
        lv_anim_set_time(_indicator_bar_scale_back_anims[i].get(), bar_data.animation.scale_back_time_ms);
        // Others
        auto i_type = static_cast<Gesture::IndicatorBarType>(i);
        if (i_type == Gesture::IndicatorBarType::LEFT) {
            align = LV_ALIGN_LEFT_MID;
            align_x_offset = max(data.threshold.horizontal_edge - bar_data.main.size_max.width, 0);
            align_y_offset = 0;
            _indicator_bar_min_lengths[i] = bar_data.main.size_min.height;
            _indicator_bar_max_lengths[i] = bar_data.main.size_max.height;
            bar_range = data.threshold.direction_horizon;
        } else if (i_type == Gesture::IndicatorBarType::RIGHT) {
            align = LV_ALIGN_RIGHT_MID;
            align_x_offset = min(-data.threshold.horizontal_edge + bar_data.main.size_max.width, 0);
            align_y_offset = 0;
            _indicator_bar_min_lengths[i] = bar_data.main.size_min.height;
            _indicator_bar_max_lengths[i] = bar_data.main.size_max.height;
            bar_range = data.threshold.direction_horizon;
        } else if (i_type == Gesture::IndicatorBarType::BOTTOM) {
            align = LV_ALIGN_BOTTOM_MID;
            align_x_offset = 0;
            align_y_offset = min(-data.threshold.vertical_edge + bar_data.main.size_max.height, 0);
            _indicator_bar_min_lengths[i] = bar_data.main.size_min.width;
            _indicator_bar_max_lengths[i] = bar_data.main.size_max.width;
            bar_range = data.threshold.direction_vertical;
        }
        ESP_UTILS_CHECK_FALSE_RETURN(bar_range > 0, false, "Invalid bar range");
        _indicator_bar_scale_factors[i] =  (_indicator_bar_max_lengths[i] - _indicator_bar_min_lengths[i]) /
                                           (float)bar_range;
        lv_obj_align(_indicator_bars[i].get(), align, align_x_offset, align_y_offset);
    }
    // Data
    _direction_tan_threshold = tan((int)data.threshold.direction_angle * M_PI / 180);

    return true;
}

void Gesture::onDataUpdateEventCallback(lv_event_t *event)
{
    Gesture *gesture = nullptr;

    ESP_UTILS_LOGD("Data update event callback");
    ESP_UTILS_CHECK_NULL_EXIT(event, "Invalid event object");

    gesture = (Gesture *)lv_event_get_user_data(event);
    ESP_UTILS_CHECK_NULL_EXIT(gesture, "Invalid gesture object");

    ESP_UTILS_CHECK_FALSE_EXIT(gesture->updateByNewData(), "Update gesture object style failed");
}

void Gesture::onTouchDetectTimerCallback(struct _lv_timer_t *t)
{
    bool touched = false;
    int distance_x = 0;
    int distance_y = 0;
    float distance_tan = numeric_limits<float>::infinity();
    lv_event_code_t event_code = LV_EVENT_ALL;

    Gesture *gesture = (Gesture *)t->user_data;
    ESP_UTILS_CHECK_NULL_EXIT(gesture, "Invalid gesture");

    const Gesture::Data &data = gesture->data;
    const int &display_w = gesture->core.getData().screen_size.width;
    const int &display_h = gesture->core.getData().screen_size.height;
    const float &distance_tan_threshold = gesture->_direction_tan_threshold;
    Gesture::Info &info = gesture->_info;

    // Check if touched and save the last touch point
    touched = gesture->readTouchPoint(info.stop_x, info.stop_y);

    // Process the stop area
    info.stop_area = Gesture::AREA_CENTER;
    info.stop_area |= (info.stop_y < data.threshold.vertical_edge) ? Gesture::AREA_TOP_EDGE : 0;
    info.stop_area |= ((display_h - info.stop_y) < data.threshold.vertical_edge) ? Gesture::AREA_BOTTOM_EDGE : 0;
    info.stop_area |= (info.stop_x < data.threshold.horizontal_edge) ? Gesture::AREA_LEFT_EDGE : 0;
    info.stop_area |= ((display_w - info.stop_x) < data.threshold.horizontal_edge) ? Gesture::AREA_RIGHT_EDGE : 0;

    // If not touched before and now, just ignore and return
    if (!gesture->checkGestureStart() && !touched) {
        return;
    }

    // If not touched before but touched now, it means the gesture is started
    if (!gesture->checkGestureStart() && touched) {
        // Save the first touch point
        gesture->_touch_start_tick = lv_tick_get();
        info.start_x = info.stop_x;
        info.start_y = info.stop_y;

        // Process the start area
        info.start_area = Gesture::AREA_CENTER;
        info.start_area |= (info.start_y < data.threshold.vertical_edge) ? Gesture::AREA_TOP_EDGE : 0;
        info.start_area |= ((display_h - info.start_y) < data.threshold.vertical_edge) ? Gesture::AREA_BOTTOM_EDGE : 0;
        info.start_area |= (info.start_x < data.threshold.horizontal_edge) ? Gesture::AREA_LEFT_EDGE : 0;
        info.start_area |= ((display_w - info.start_x) < data.threshold.horizontal_edge) ? Gesture::AREA_RIGHT_EDGE : 0;

        // Set the press event code
        event_code = gesture->_press_event_code;
        ESP_UTILS_LOGD("Gesture send press event");

        goto event_process;
    }

    // Process the duration
    info.duration_ms = lv_tick_elaps(gesture->_touch_start_tick);
    info.flags.short_duration = (info.duration_ms < data.threshold.duration_short_ms);

    // Set the event code according to the touch status
    if (touched) {
        event_code = gesture->_pressing_event_code;
        ESP_UTILS_LOGD("Gesture send pressing event");
    } else {
        event_code = gesture->_release_event_code;
        ESP_UTILS_LOGD("Gesture send release event");
    }

    // If not touched now but touched before, it means the gesture is finished
    distance_x = info.stop_x - info.start_x;
    distance_y = info.stop_y - info.start_y;
    if ((distance_x == 0) && (distance_y == 0)) {
        // If the distance is too small, just ignore and go to the end
        goto event_process;
    }

    // Process the distance and speed
    info.distance_px = (float)sqrt(distance_x * distance_x + distance_y * distance_y);
    info.speed_px_per_ms = (info.duration_ms > 0) ? (info.distance_px / info.duration_ms) :
                           numeric_limits<float>::infinity();
    info.flags.slow_speed = (info.speed_px_per_ms < data.threshold.speed_slow_px_per_ms);

    /* Process the direction */
    // Calculate the tan value of the gesture
    distance_tan = (distance_x == 0) ? numeric_limits<float>::infinity() : (float)distance_y / distance_x;
    // Check if the tan absolute value is large enoughd
    // if so, it means the gesture is up or down, otherwise, it's left or right
    if ((distance_tan == numeric_limits<float>::infinity()) || (distance_tan > distance_tan_threshold) ||
            (distance_tan < -distance_tan_threshold)) {
        // Check the distance in y axis
        if (distance_y > data.threshold.direction_vertical) {
            info.direction = Gesture::DIR_DOWN;
        } else if (distance_y < -data.threshold.direction_vertical) {
            info.direction = Gesture::DIR_UP;
        }
    } else {
        // Check the distance in x axis
        if (distance_x > data.threshold.direction_horizon) {
            info.direction = Gesture::DIR_RIGHT;
        } else if (distance_x < -data.threshold.direction_horizon) {
            info.direction = Gesture::DIR_LEFT;
        }
    }

event_process:
    if (gesture->checkGestureStart()) {
        ESP_UTILS_LOGD(
            "\n\tpoint(%d,%d->%d,%d), area(%d->%d), dir(%d), distance(%.2f), angle(%d), duration(%dms), speed(%.2f),"
            "event(%d)", info.start_x, info.start_y, info.stop_x, info.stop_y, info.start_area, info.stop_area,
            (int)info.direction, info.distance_px, (int)(atan(distance_tan) * -180 / M_PI), (int)info.duration_ms,
            info.speed_px_per_ms, (int)event_code
        );
    }

    gesture->_event_data = info;
    lv_obj_send_event(gesture->_event_mask_obj.get(), event_code, (void *)&gesture->_event_data);
    if (event_code == gesture->_release_event_code) {
        gesture->resetGestureInfo();
    }
}

void Gesture::onIndicatorBarScaleBackAnimationExecuteCallback(void *var, int32_t value)
{
    auto anim_var = static_cast<IndicatorBarAnimVar_t *>(var);
    Gesture *gesture = nullptr;
    Gesture::IndicatorBarType type = Gesture::IndicatorBarType::MAX;

    ESP_UTILS_CHECK_NULL_EXIT(anim_var, "Invalid var");

    gesture = anim_var->gesture;
    ESP_UTILS_CHECK_NULL_EXIT(gesture, "Invalid gesture");
    type = anim_var->type;
    auto type_int = static_cast<int>(type);
    ESP_UTILS_CHECK_VALUE_EXIT(type_int, 0, static_cast<int>(Gesture::IndicatorBarType::MAX), "Invalid indicator bar type");

    if (type == Gesture::IndicatorBarType::LEFT || type == Gesture::IndicatorBarType::RIGHT) {
        lv_obj_set_height(gesture->_indicator_bars[type_int].get(), value);
    } else if (type == Gesture::IndicatorBarType::BOTTOM) {
        lv_obj_set_width(gesture->_indicator_bars[type_int].get(), value);
    }
}

void Gesture::onIndicatorBarScaleBackAnimationReadyCallback(lv_anim_t *anim)
{
    Gesture *gesture = nullptr;
    Gesture::IndicatorBarType type = Gesture::IndicatorBarType::MAX;

    ESP_UTILS_LOGD("Indicator bar scale back animation ready callback");
    ESP_UTILS_CHECK_NULL_EXIT(anim, "Invalid anim");

    auto anim_var = static_cast<IndicatorBarAnimVar_t *>(anim->var);
    ESP_UTILS_CHECK_NULL_EXIT(anim_var, "Invalid user data");
    gesture = anim_var->gesture;
    ESP_UTILS_CHECK_NULL_EXIT(gesture, "Invalid gesture");
    type = anim_var->type;
    auto type_int = static_cast<int>(type);
    ESP_UTILS_CHECK_VALUE_EXIT(type_int, 0, static_cast<int>(Gesture::IndicatorBarType::MAX), "Invalid indicator bar type");

    gesture->_flags.is_indicator_bar_scale_back_anim_running[type_int] = false;
    // If the animation is finished, hide the indicator bar (except the bottom one)
    if (type != Gesture::IndicatorBarType::BOTTOM) {
        ESP_UTILS_CHECK_FALSE_EXIT(gesture->setIndicatorBarVisible(type, false), "Hide indicator bar failed");
    }
}

} // namespace esp_brookesia::systems::phone
