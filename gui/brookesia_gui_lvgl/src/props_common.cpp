/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "private/props_impl.hpp"

namespace esp_brookesia::gui::lvgl::props_detail {

uint32_t calculate_scale(int32_t target, int32_t source)
{
    if (target <= 0 || source <= 0) {
        return LV_SCALE_NONE;
    }
    const int64_t scaled = (static_cast<int64_t>(target) * LV_SCALE_NONE + source / 2) / source;
    return static_cast<uint32_t>(std::max<int64_t>(1, scaled));
}

int32_t apply_scale(int32_t source, uint32_t scale)
{
    if (source <= 0) {
        return 0;
    }
    return static_cast<int32_t>((static_cast<int64_t>(source) * scale + LV_SCALE_NONE / 2) / LV_SCALE_NONE);
}

std::string join_options(const std::vector<std::string> &options)
{
    std::ostringstream oss;
    for (size_t i = 0; i < options.size(); ++i) {
        if (i > 0) {
            oss << '\n';
        }
        oss << options[i];
    }
    return oss.str();
}



void apply_common_clickable(Record &record, const CommonProps &props)
{
    if (props.clickable) {
        lv_obj_add_flag(record.object, LV_OBJ_FLAG_CLICKABLE);
    } else {
        lv_obj_remove_flag(record.object, LV_OBJ_FLAG_CLICKABLE);
    }
}

void apply_common_scrollable(Record &record, const CommonProps &props)
{
    if (props.scrollable) {
        lv_obj_add_flag(record.object, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scrollbar_mode(record.object, LV_SCROLLBAR_MODE_AUTO);
    } else {
        lv_obj_remove_flag(record.object, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scrollbar_mode(record.object, LV_SCROLLBAR_MODE_OFF);
    }
}

void apply_common_press_lock(Record &record, const CommonProps &props)
{
    if (props.press_lock) {
        lv_obj_add_flag(record.object, LV_OBJ_FLAG_PRESS_LOCK);
    } else {
        lv_obj_remove_flag(record.object, LV_OBJ_FLAG_PRESS_LOCK);
    }
}

void apply_common_hidden(Record &record, const CommonProps &props)
{
    if (props.hidden) {
        lv_obj_add_flag(record.object, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(record.object, LV_OBJ_FLAG_HIDDEN);
    }
    if (record.hidden != props.hidden) {
        run_animations(record, props.hidden ? AnimationTrigger::Hide : AnimationTrigger::Show);
        record.hidden = props.hidden;
    }
}

int32_t resolve_pivot_value(const PivotValue &pivot, int32_t size)
{
    if (pivot.percent) {
        return static_cast<int32_t>(std::lround(static_cast<float>(size) * static_cast<float>(pivot.value) / 100.0F));
    }
    return pivot.value;
}

bool is_identity_common_transform(const CommonProps &props)
{
    // Matches CommonProps defaults and LVGL's default local transform (angle 0, zoom 256, pivot 0,0).
    return props.angle == 0 && props.zoom == 256 && !props.pivot_x.percent && props.pivot_x.value == 0 &&
           !props.pivot_y.percent && props.pivot_y.value == 0;
}

void apply_common_transform(Record &record, const CommonProps &props)
{
    if (is_identity_common_transform(props)) {
        return;
    }
    lv_obj_update_layout(record.object);
    const auto width = lv_obj_get_width(record.object);
    const auto height = lv_obj_get_height(record.object);
    lv_obj_set_style_transform_pivot_x(record.object, resolve_pivot_value(props.pivot_x, width), LV_PART_MAIN);
    lv_obj_set_style_transform_pivot_y(record.object, resolve_pivot_value(props.pivot_y, height), LV_PART_MAIN);
    lv_obj_set_style_transform_scale(record.object, std::max<int32_t>(1, props.zoom), LV_PART_MAIN);
    lv_obj_set_style_transform_rotation(record.object, props.angle * 10, LV_PART_MAIN);
}

void apply_common_flags(Record &record, const CommonProps &props)
{
    apply_common_disabled(record, props);
    apply_common_clickable(record, props);
    apply_common_scrollable(record, props);
    apply_common_press_lock(record, props);
    apply_common_hidden(record, props);
    apply_common_transform(record, props);
}

void apply_toggle_state(lv_obj_t *object, bool checked)
{
    if (checked) {
        lv_obj_add_state(object, LV_STATE_CHECKED);
    } else {
        lv_obj_remove_state(object, LV_STATE_CHECKED);
    }
}

void apply_canvas(Record &record, const Node &node)
{
    const auto width = node.placement.width.mode == SizeMode::Fixed ? node.placement.width.value : 96;
    const auto height = node.placement.height.mode == SizeMode::Fixed ? node.placement.height.value : 96;
    if (width <= 0 || height <= 0) {
        return;
    }

    auto &canvas = record.ensure_type_payload<Record::CanvasPayload>();
    auto &buffer = canvas.buffer;
    buffer.assign(static_cast<size_t>(width) * static_cast<size_t>(height), lv_color_hex(0xffffff));
    lv_canvas_set_buffer(record.object, buffer.data(), width, height, LV_COLOR_FORMAT_NATIVE);
    lv_canvas_fill_bg(record.object, lv_color_hex(0xffffff), LV_OPA_COVER);
    for (const auto &command : node.canvas_props.commands) {
        if (command.type == "fill") {
            auto color = parse_color(command.color);
            if (color.has_value()) {
                lv_canvas_fill_bg(record.object, lv_color_hex(*color), LV_OPA_COVER);
            }
        } else if (command.type == "pixel") {
            auto color = parse_color(command.color);
            if (color.has_value()) {
                lv_canvas_set_px(record.object, command.x, command.y, lv_color_hex(*color), LV_OPA_COVER);
            }
        }
    }
}

uint16_t read_le16(const std::vector<uint8_t> &data, size_t offset)
{
    return static_cast<uint16_t>(data[offset]) | (static_cast<uint16_t>(data[offset + 1]) << 8);
}
}
