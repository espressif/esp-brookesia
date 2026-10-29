/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "private/style_impl.hpp"

namespace esp_brookesia::gui::lvgl::style_detail {



void draw_arc_gradient_segmented(
    const lv_draw_arc_dsc_t &source,
    const Record::ArcGradientRecord &gradient)
{
    auto span = static_cast<int32_t>(source.end_angle - source.start_angle);
    if (span < 0) {
        span += 360;
    }
    if (span <= 0) {
        return;
    }

    const auto segments = std::clamp<int32_t>(gradient.segments, 2, 128);
    for (int32_t index = 0; index < segments; ++index) {
        auto segment = source;
        const auto start_offset = (span * index) / segments;
        const auto end_offset = (span * (index + 1)) / segments;
        segment.start_angle = source.start_angle + start_offset;
        segment.end_angle = source.start_angle + end_offset;
        segment.color = mix_color(gradient.start_color, gradient.end_color, index, segments - 1);
        lv_draw_arc(source.base.layer, &segment);
    }
}

void on_arc_gradient_draw_task_added(lv_event_t *event)
{
    auto *context = static_cast<ArcGradientContext *>(lv_event_get_user_data(event));
    if (context == nullptr || context->impl == nullptr) {
        return;
    }
    auto *record = context->impl->find_record(context->handle);
    if (record == nullptr || record->object == nullptr || record->type != NodeType::Arc ||
            record->style_extras_payload == nullptr) {
        return;
    }

    auto *task = lv_event_get_draw_task(event);
    if (task == nullptr || lv_draw_task_get_type(task) != LV_DRAW_TASK_TYPE_ARC) {
        return;
    }
    auto *arc = lv_draw_task_get_arc_dsc(task);
    if (arc == nullptr || arc->base.obj != record->object) {
        return;
    }

    const char *part_name = nullptr;
    if (arc->base.part == LV_PART_MAIN) {
        part_name = "main";
    } else if (arc->base.part == LV_PART_INDICATOR) {
        part_name = "indicator";
    } else {
        return;
    }

    auto gradient_it = record->style_extras_payload->arc_gradients.find(part_name);
    if (gradient_it == record->style_extras_payload->arc_gradients.end() || !gradient_it->second.enabled) {
        return;
    }

    const auto source = *arc;
    arc->opa = LV_OPA_TRANSP;
    draw_arc_gradient_segmented(source, gradient_it->second);
}

std::optional<Record::ArcGradientRecord> make_arc_gradient_record(const Style &style)
{
    if (!style.arc_gradient_color.has_value()) {
        return std::nullopt;
    }
    auto end_color = parse_color(*style.arc_gradient_color);
    if (!end_color.has_value()) {
        return std::nullopt;
    }

    std::optional<uint32_t> start_color;
    if (style.arc_color.has_value()) {
        start_color = parse_color(*style.arc_color);
    }
    if (!start_color.has_value() && style.line_color.has_value()) {
        start_color = parse_color(*style.line_color);
    }
    if (!start_color.has_value() && style.bg_color.has_value()) {
        start_color = parse_color(*style.bg_color);
    }
    if (!start_color.has_value()) {
        return std::nullopt;
    }

    return Record::ArcGradientRecord{
        .enabled = true,
        .start_color = *start_color,
        .end_color = *end_color,
        .segments = style.arc_gradient_segments.value_or(32),
    };
}

void apply_arc_gradient(BackendImpl &impl, Record &record, const ResolvedStyle &style)
{
    const auto main_gradient = make_arc_gradient_record(style.style);
    std::optional<Record::ArcGradientRecord> indicator_gradient;
    if (auto part_it = style.part_styles.find("indicator"); part_it != style.part_styles.end()) {
        indicator_gradient = make_arc_gradient_record(part_it->second.style);
    }

    const bool has_gradient = main_gradient.has_value() || indicator_gradient.has_value();
    if (!has_gradient) {
        if (record.style_extras_payload != nullptr) {
            record.style_extras_payload->arc_gradients.clear();
        }
        lv_obj_remove_flag(record.object, LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS);
        return;
    }

    auto &style_extras = ensure_style_extras_payload(record);
    style_extras.arc_gradients.clear();
    if (main_gradient.has_value()) {
        style_extras.arc_gradients.emplace("main", *main_gradient);
    }
    if (indicator_gradient.has_value()) {
        style_extras.arc_gradients.emplace("indicator", *indicator_gradient);
    }

    if (!style_extras.arc_gradient_event_registered) {
        style_extras.arc_gradient_context = std::make_unique<ArcGradientContext>(ArcGradientContext{
            .impl = &impl,
            .handle = record.handle,
        });
        lv_obj_add_event_cb(
            record.object,
            on_arc_gradient_draw_task_added,
            LV_EVENT_DRAW_TASK_ADDED,
            style_extras.arc_gradient_context.get()
        );
        style_extras.arc_gradient_event_registered = true;
    }

    lv_obj_add_flag(record.object, LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS);
}
}
