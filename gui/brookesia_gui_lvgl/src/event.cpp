/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "private/types.hpp"
#include "brookesia/gui_lvgl/macro_configs.h"
#if !BROOKESIA_GUI_LVGL_EVENT_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"

#include <optional>

namespace esp_brookesia::gui::lvgl {

namespace {

static void append_pointer_payload(BackendEvent &event)
{
    auto *indev = lv_indev_active();
    if (indev == nullptr) {
        return;
    }

    lv_point_t point{};
    lv_indev_get_point(indev, &point);
    event.payload["x"] = point.x;
    event.payload["y"] = point.y;
}

static void append_value_payload(const Record &record, BackendEvent &event)
{
    if (record.object == nullptr) {
        return;
    }

    switch (record.type) {
    case NodeType::TextInput:
        event.payload["text"] = lv_textarea_get_text(record.object);
        break;
    case NodeType::Slider:
        event.payload["value"] = lv_slider_get_value(record.object);
        break;
    case NodeType::Switch:
    case NodeType::Checkbox:
        event.payload["checked"] = lv_obj_has_state(record.object, LV_STATE_CHECKED);
        break;
    case NodeType::Dropdown:
        event.payload["selectedIndex"] = static_cast<int32_t>(lv_dropdown_get_selected(record.object));
        break;
    case NodeType::ProgressBar:
        event.payload["value"] = lv_bar_get_value(record.object);
        break;
    case NodeType::Arc:
        event.payload["value"] = lv_arc_get_value(record.object);
        break;
    default:
        break;
    }
}

static void on_lvgl_event(lv_event_t *event)
{
    BROOKESIA_LOG_TRACE_GUARD();

    BROOKESIA_LOGD("Params: event(%1%)", static_cast<void *>(event));

    auto *context = static_cast<EventContext *>(lv_event_get_user_data(event));
    if (context == nullptr || context->impl == nullptr || !context->impl->event_sink) {
        return;
    }

    auto backend_event = BackendEvent{
        .handle = context->handle,
        .type = context->type,
        .action = context->action,
        .payload = {},
    };

    if (
        context->type == EventType::Pressed ||
        context->type == EventType::Pressing ||
        context->type == EventType::PressLost ||
        context->type == EventType::Released
    ) {
        append_pointer_payload(backend_event);
    }

    if (context->type == EventType::Gesture) {
        auto *indev = lv_indev_active();
        if (indev == nullptr) {
            context->impl->event_sink(backend_event);
            return;
        }
        switch (lv_indev_get_gesture_dir(indev)) {
        case LV_DIR_LEFT:
            backend_event.payload["direction"] = "left";
            break;
        case LV_DIR_RIGHT:
            backend_event.payload["direction"] = "right";
            break;
        case LV_DIR_TOP:
            backend_event.payload["direction"] = "up";
            break;
        case LV_DIR_BOTTOM:
            backend_event.payload["direction"] = "down";
            break;
        default:
            break;
        }
    }

    const auto *record = context->impl->find_record(context->handle);
    const bool should_append_value = context->type == EventType::ValueChanged ||
                                     (context->type == EventType::Released &&
                                      record != nullptr && record->type == NodeType::Slider);
    if (should_append_value && record != nullptr) {
        append_value_payload(*record, backend_event);
    }

    if (!should_append_value) {
        context->impl->event_sink(backend_event);
        return;
    }

    context->impl->event_sink(backend_event);
}

static std::optional<lv_event_code_t> to_lvgl_event_code(EventType type)
{
    switch (type) {
    case EventType::Pressed:
        return LV_EVENT_PRESSED;
    case EventType::Pressing:
        return LV_EVENT_PRESSING;
    case EventType::PressLost:
        return LV_EVENT_PRESS_LOST;
    case EventType::Released:
        return LV_EVENT_RELEASED;
    case EventType::Clicked:
        return LV_EVENT_CLICKED;
    case EventType::LongPressed:
        return LV_EVENT_LONG_PRESSED;
    case EventType::LongPressedRepeat:
        return LV_EVENT_LONG_PRESSED_REPEAT;
    case EventType::Focused:
        return LV_EVENT_FOCUSED;
    case EventType::Defocused:
        return LV_EVENT_DEFOCUSED;
    case EventType::ValueChanged:
        return LV_EVENT_VALUE_CHANGED;
    case EventType::Ready:
        return LV_EVENT_READY;
    case EventType::Cancel:
        return LV_EVENT_CANCEL;
    case EventType::Scroll:
        return LV_EVENT_SCROLL;
    case EventType::Gesture:
        return LV_EVENT_GESTURE;
    case EventType::Max:
    default:
        return std::nullopt;
    }
}

static bool event_bindings_equal(
    const std::unique_ptr<std::vector<std::unique_ptr<EventContext>>> &contexts,
    const std::vector<EventBinding> &events
)
{
    if (contexts == nullptr) {
        return events.empty();
    }
    if (contexts->size() != events.size()) {
        return false;
    }

    for (size_t i = 0; i < events.size(); ++i) {
        if ((*contexts)[i] == nullptr ||
                (*contexts)[i]->type != events[i].type ||
                (*contexts)[i]->action != events[i].action) {
            return false;
        }
    }
    return true;
}

static void unbind_events(Record &record)
{
    if (record.event_contexts == nullptr) {
        return;
    }

    if (record.object != nullptr) {
        for (const auto &context : *record.event_contexts) {
            if (context == nullptr) {
                continue;
            }
            lv_obj_remove_event_cb_with_user_data(record.object, on_lvgl_event, context.get());
        }
    }
    record.event_contexts.reset();
}

} // namespace

void bind_events(BackendImpl &impl, Record &record, const std::vector<EventBinding> &events)
{
    BROOKESIA_LOG_TRACE_GUARD();

    BROOKESIA_LOGD(
        "Params: impl(%1%), record(%2%), events(count=%3%)",
        static_cast<void *>(&impl), record, events.size()
    );

    if (event_bindings_equal(record.event_contexts, events)) {
        return;
    }

    unbind_events(record);
    for (const auto &event : events) {
        auto lvgl_event = to_lvgl_event_code(event.type);
        if (!lvgl_event.has_value()) {
            continue;
        }

        auto context = std::make_unique<EventContext>();
        context->impl = &impl;
        context->handle = record.handle;
        context->type = event.type;
        context->action = event.action;
        lv_obj_add_event_cb(record.object, on_lvgl_event, *lvgl_event, context.get());
        if (record.event_contexts == nullptr) {
            record.event_contexts = std::make_unique<std::vector<std::unique_ptr<EventContext>>>();
        }
        record.event_contexts->push_back(std::move(context));
    }
}

void release_events(Record &record)
{
    unbind_events(record);
}

} // namespace esp_brookesia::gui::lvgl
