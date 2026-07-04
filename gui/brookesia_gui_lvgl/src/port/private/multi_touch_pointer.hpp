/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "brookesia/service_display.hpp"

#if __has_include("lvgl/lvgl.h")
#   include "lvgl/lvgl.h"
#else
#   include "lvgl.h"
#endif

namespace esp_brookesia::gui::lvgl {

class MultiTouchPointerManager {
public:
    static constexpr size_t MAX_INPUT_COUNT = 5;

    bool start(lv_display_t *display, lv_group_t *group, std::string output_name, size_t input_count)
    {
        stop();

        if (display == nullptr || output_name.empty()) {
            return false;
        }

        display_ = display;
        group_ = group;
        output_name_ = std::move(output_name);
        input_count_ = std::clamp<size_t>(input_count, 1, MAX_INPUT_COUNT);
        for (size_t i = 0; i < input_count_; i++) {
            contexts_[i] = {
                .owner = this,
                .slot_index = i,
            };
            auto *input = lv_indev_create();
            if (input == nullptr) {
                stop();
                return false;
            }
            inputs_[i] = input;
            lv_indev_set_user_data(input, &contexts_[i]);
            lv_indev_set_display(input, display_);
            lv_indev_set_type(input, LV_INDEV_TYPE_POINTER);
            lv_indev_set_read_cb(input, MultiTouchPointerManager::read_callback);
            if (group_ != nullptr) {
                lv_indev_set_group(input, group_);
            }
        }

        return true;
    }

    void stop()
    {
        for (auto *input : inputs_) {
            if (input != nullptr) {
                lv_indev_delete(input);
            }
        }
        inputs_.fill(nullptr);
        contexts_ = {};
        slots_ = {};
        input_count_ = 0;
        output_name_.clear();
        last_snapshot_sequence_ = 0;
        has_snapshot_sequence_ = false;
        next_synthetic_track_id_ = 1;
    }

    lv_indev_t *primary_input() const
    {
        return inputs_[0];
    }

    static size_t resolve_input_count(std::string_view output_name)
    {
        const auto outputs = service::Display::get_instance().get_outputs();
        auto output_it = std::find_if(outputs.begin(), outputs.end(), [output_name](const auto & output) {
            return output.name == output_name;
        });
        if ((output_it == outputs.end()) || !output_it->touch.has_value()) {
            return 1;
        }

        return std::clamp<size_t>(static_cast<size_t>(output_it->touch->max_points), 1, MAX_INPUT_COUNT);
    }

private:
    struct InputContext {
        MultiTouchPointerManager *owner = nullptr;
        size_t slot_index = 0;
    };

    struct Slot {
        bool active = false;
        bool release_pending = false;
        bool has_track = false;
        uint8_t track_id = 0;
        lv_point_t point{};

        bool allocated() const
        {
            return active || release_pending || has_track;
        }
    };

    static constexpr int32_t NEAREST_MATCH_DISTANCE_PX = 96;

    static void read_callback(lv_indev_t *input, lv_indev_data_t *data)
    {
        auto *context = static_cast<InputContext *>(lv_indev_get_user_data(input));
        if (context == nullptr || context->owner == nullptr) {
            if (data != nullptr) {
                data->state = LV_INDEV_STATE_RELEASED;
            }
            return;
        }
        context->owner->read_input(input, context->slot_index, data);
    }

    void read_input(lv_indev_t *input, size_t slot_index, lv_indev_data_t *data)
    {
        if (data == nullptr || slot_index >= input_count_) {
            return;
        }

        data->state = LV_INDEV_STATE_RELEASED;
        data->point = slots_[slot_index].point;

#if LVGL_VERSION_MAJOR >= 9 && LV_USE_GESTURE_RECOGNITION
        auto snapshot = refresh_from_display();
        if (slot_index == 0 && snapshot != nullptr) {
            update_gesture_recognizers(input, data, *snapshot);
        }
#else
        refresh_from_display();
        (void)input;
#endif

        auto &slot = slots_[slot_index];
        data->point = slot.point;
        if (slot.active) {
            data->state = LV_INDEV_STATE_PRESSED;
            return;
        }

        data->state = LV_INDEV_STATE_RELEASED;
        if (slot.release_pending) {
            slot.release_pending = false;
            slot.has_track = false;
            slot.track_id = 0;
        }
    }

    const service::Display::TouchSnapshot *refresh_from_display()
    {
        auto snapshot_result = service::Display::get_instance().get_touch_snapshot(output_name_);
        if (!snapshot_result.has_value()) {
            release_all_slots();
            return nullptr;
        }

        snapshot_ = std::move(snapshot_result.value());
        if (has_snapshot_sequence_ && snapshot_.sequence == last_snapshot_sequence_) {
            return &snapshot_;
        }
        has_snapshot_sequence_ = true;
        last_snapshot_sequence_ = snapshot_.sequence;

        if (!snapshot_.valid) {
            release_all_slots();
            return &snapshot_;
        }

        assign_snapshot_points(snapshot_.points);
        return &snapshot_;
    }

    void assign_snapshot_points(const std::vector<service::Display::TouchPoint> &points)
    {
        std::array<bool, MAX_INPUT_COUNT> seen = {};
        const size_t point_count = std::min(points.size(), input_count_);
        for (size_t i = 0; i < point_count; i++) {
            const auto &point = points[i];
            auto slot_index = find_slot_for_point(point, seen);
            if (slot_index == INVALID_SLOT) {
                continue;
            }

            auto &slot = slots_[slot_index];
            slot.active = true;
            slot.release_pending = false;
            slot.has_track = true;
            if (point.track_id != 0) {
                slot.track_id = point.track_id;
            } else if (slot.track_id == 0) {
                slot.track_id = next_synthetic_track_id();
            }
            slot.point = {
                .x = static_cast<lv_coord_t>(std::max<int16_t>(point.x, 0)),
                .y = static_cast<lv_coord_t>(std::max<int16_t>(point.y, 0)),
            };
            seen[slot_index] = true;
        }

        for (size_t i = 0; i < input_count_; i++) {
            auto &slot = slots_[i];
            if (!seen[i] && slot.active) {
                slot.active = false;
                slot.release_pending = true;
            }
        }
    }

    size_t find_slot_for_point(
        const service::Display::TouchPoint &point, const std::array<bool, MAX_INPUT_COUNT> &seen
    ) const
    {
        if (point.track_id != 0) {
            for (size_t i = 0; i < input_count_; i++) {
                if (!seen[i] && slots_[i].has_track && slots_[i].track_id == point.track_id) {
                    return i;
                }
            }
        }

        auto nearest_slot = find_nearest_slot(point, seen);
        if (nearest_slot != INVALID_SLOT) {
            return nearest_slot;
        }

        for (size_t i = 0; i < input_count_; i++) {
            if (!seen[i] && !slots_[i].allocated()) {
                return i;
            }
        }

        return INVALID_SLOT;
    }

    size_t find_nearest_slot(
        const service::Display::TouchPoint &point, const std::array<bool, MAX_INPUT_COUNT> &seen
    ) const
    {
        size_t best_slot = INVALID_SLOT;
        int64_t best_distance = std::numeric_limits<int64_t>::max();
        const int64_t max_distance =
            static_cast<int64_t>(NEAREST_MATCH_DISTANCE_PX) * static_cast<int64_t>(NEAREST_MATCH_DISTANCE_PX);

        for (size_t i = 0; i < input_count_; i++) {
            const auto &slot = slots_[i];
            if (seen[i] || !slot.active) {
                continue;
            }
            const int64_t dx = static_cast<int64_t>(slot.point.x) - static_cast<int64_t>(point.x);
            const int64_t dy = static_cast<int64_t>(slot.point.y) - static_cast<int64_t>(point.y);
            const int64_t distance = dx * dx + dy * dy;
            if (distance < best_distance && distance <= max_distance) {
                best_distance = distance;
                best_slot = i;
            }
        }

        return best_slot;
    }

    void release_all_slots()
    {
        for (auto &slot : slots_) {
            if (slot.active) {
                slot.active = false;
                slot.release_pending = true;
            }
        }
    }

    uint8_t next_synthetic_track_id()
    {
        const uint8_t id = next_synthetic_track_id_;
        next_synthetic_track_id_ = static_cast<uint8_t>((next_synthetic_track_id_ % 127) + 1);
        return id;
    }

#if LVGL_VERSION_MAJOR >= 9 && LV_USE_GESTURE_RECOGNITION
    void update_gesture_recognizers(
        lv_indev_t *input, lv_indev_data_t *data, const service::Display::TouchSnapshot &snapshot
    )
    {
        if (input == nullptr || data == nullptr || !snapshot.valid) {
            return;
        }

        std::vector<lv_indev_touch_data_t> touches;
        touches.reserve(input_count_);
        const uint32_t timestamp = lv_tick_get();

        for (size_t i = 0; i < input_count_; i++) {
            const auto &slot = slots_[i];
            if (!slot.active && !slot.release_pending) {
                continue;
            }

            lv_indev_touch_data_t touch{};
            touch.id = static_cast<uint8_t>(i);
            touch.point = slot.point;
            touch.state = slot.active ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
            touch.timestamp = timestamp;
            touches.push_back(touch);
        }

        if (!touches.empty()) {
            lv_indev_gesture_recognizers_update(input, touches.data(), touches.size());
            lv_indev_gesture_recognizers_set_data(input, data);
        }
    }
#endif

    static constexpr size_t INVALID_SLOT = MAX_INPUT_COUNT;

    lv_display_t *display_ = nullptr;
    lv_group_t *group_ = nullptr;
    std::string output_name_;
    size_t input_count_ = 0;
    std::array<lv_indev_t *, MAX_INPUT_COUNT> inputs_ = {};
    std::array<InputContext, MAX_INPUT_COUNT> contexts_ = {};
    std::array<Slot, MAX_INPUT_COUNT> slots_ = {};
    service::Display::TouchSnapshot snapshot_;
    uint32_t last_snapshot_sequence_ = 0;
    bool has_snapshot_sequence_ = false;
    uint8_t next_synthetic_track_id_ = 1;
};

} // namespace esp_brookesia::gui::lvgl
