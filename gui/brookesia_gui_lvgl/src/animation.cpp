/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "private/types.hpp"
#include "brookesia/gui_lvgl/macro_configs.h"
#if !BROOKESIA_GUI_LVGL_BACKEND_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"

#include <algorithm>

namespace esp_brookesia::gui::lvgl {

namespace {

struct RuntimeAnimationContext {
    lv_obj_t *object = nullptr;
    lv_anim_exec_xcb_t exec = nullptr;
    std::shared_ptr<bool> active = std::make_shared<bool>(true);
    std::function<void()> completed_handler;
};

static lv_anim_path_cb_t to_lvgl_anim_path(AnimationEasing easing)
{
    switch (easing) {
    case AnimationEasing::EaseIn:
        return lv_anim_path_ease_in;
    case AnimationEasing::EaseOut:
        return lv_anim_path_ease_out;
    case AnimationEasing::EaseInOut:
        return lv_anim_path_ease_in_out;
    case AnimationEasing::Overshoot:
        return lv_anim_path_overshoot;
    case AnimationEasing::Bounce:
        return lv_anim_path_bounce;
    case AnimationEasing::Step:
        return lv_anim_path_step;
    case AnimationEasing::Linear:
    case AnimationEasing::Max:
    default:
        return lv_anim_path_linear;
    }
}

static lv_anim_exec_xcb_t to_lvgl_anim_exec(AnimationProperty property)
{
    switch (property) {
    case AnimationProperty::Opacity:
        return [](void *object, int32_t value) {
            lv_obj_set_style_opa(static_cast<lv_obj_t *>(object), static_cast<lv_opa_t>(value), LV_PART_MAIN);
        };
    case AnimationProperty::X:
        return [](void *object, int32_t value) {
            lv_obj_set_x(static_cast<lv_obj_t *>(object), value);
        };
    case AnimationProperty::Y:
        return [](void *object, int32_t value) {
            lv_obj_set_y(static_cast<lv_obj_t *>(object), value);
        };
    case AnimationProperty::Width:
        return [](void *object, int32_t value) {
            lv_obj_set_width(static_cast<lv_obj_t *>(object), value);
        };
    case AnimationProperty::Height:
        return [](void *object, int32_t value) {
            lv_obj_set_height(static_cast<lv_obj_t *>(object), value);
        };
    case AnimationProperty::Scale:
        return [](void *object, int32_t value) {
            lv_obj_set_style_transform_scale(static_cast<lv_obj_t *>(object), value, LV_PART_MAIN);
        };
    case AnimationProperty::Angle:
        return [](void *object, int32_t value) {
            auto *lv_object = static_cast<lv_obj_t *>(object);
            lv_obj_set_style_transform_rotation(lv_object, value * 10, LV_PART_MAIN);
        };
    case AnimationProperty::OffsetX:
        return [](void *object, int32_t value) {
            lv_image_set_offset_x(static_cast<lv_obj_t *>(object), value);
        };
    case AnimationProperty::OffsetY:
        return [](void *object, int32_t value) {
            lv_image_set_offset_y(static_cast<lv_obj_t *>(object), value);
        };
    case AnimationProperty::Max:
    default:
        return nullptr;
    }
}

static int32_t get_current_animation_value(lv_obj_t *object, AnimationProperty property)
{
    if (object == nullptr) {
        return 0;
    }

    switch (property) {
    case AnimationProperty::Opacity:
        return lv_obj_get_style_opa(object, LV_PART_MAIN);
    case AnimationProperty::X:
        return lv_obj_get_x(object);
    case AnimationProperty::Y:
        return lv_obj_get_y(object);
    case AnimationProperty::Width:
        return lv_obj_get_width(object);
    case AnimationProperty::Height:
        return lv_obj_get_height(object);
    case AnimationProperty::Scale:
        return lv_obj_get_style_transform_scale_x(object, LV_PART_MAIN);
    case AnimationProperty::Angle: {
        int32_t angle = lv_obj_get_style_transform_rotation(object, LV_PART_MAIN) / 10;
        while (angle > 180) {
            angle -= 360;
        }
        while (angle <= -180) {
            angle += 360;
        }
        return angle;
    }
    case AnimationProperty::OffsetX:
        return lv_image_get_offset_x(object);
    case AnimationProperty::OffsetY:
        return lv_image_get_offset_y(object);
    case AnimationProperty::Max:
    default:
        return 0;
    }
}

static std::pair<int32_t, int32_t> resolve_animation_values(lv_obj_t *object, const Animation &animation)
{
    int32_t from = animation.from;
    if (animation.from_mode == AnimationValueMode::Current) {
        from = get_current_animation_value(object, animation.property);
    }

    int32_t to = animation.to;
    if (animation.to_mode == AnimationValueMode::Relative) {
        to = from + animation.to;
    }

    return {from, to};
}

static void runtime_animation_completed_cb(lv_anim_t *lv_anim)
{
    auto *context = static_cast<RuntimeAnimationContext *>(lv_anim_get_user_data(lv_anim));
    if (context == nullptr) {
        return;
    }

    auto active = context->active;
    if (active != nullptr && *active) {
        *active = false;
        if (context->completed_handler) {
            context->completed_handler();
        }
    }
}

static bool animation_equals(const Animation &lhs, const Animation &rhs)
{
    return lhs.id == rhs.id &&
           lhs.trigger == rhs.trigger &&
           lhs.property == rhs.property &&
           lhs.from_mode == rhs.from_mode &&
           lhs.from == rhs.from &&
           lhs.to_mode == rhs.to_mode &&
           lhs.to == rhs.to &&
           lhs.duration == rhs.duration &&
           lhs.delay == rhs.delay &&
           lhs.easing == rhs.easing &&
           lhs.repeat == rhs.repeat &&
           lhs.playback == rhs.playback;
}

static bool animations_equal(const std::vector<Animation> &lhs, const std::vector<Animation> &rhs)
{
    if (lhs.size() != rhs.size()) {
        return false;
    }
    for (size_t i = 0; i < lhs.size(); ++i) {
        if (!animation_equals(lhs[i], rhs[i])) {
            return false;
        }
    }
    return true;
}

} // namespace

void apply_animations(Record &record, const std::vector<Animation> &animations)
{
    BROOKESIA_LOG_TRACE_GUARD();

    BROOKESIA_LOGD("Params: record(%1%), animations(count=%2%)", record, animations.size());

    if (animations_equal(record.animations, animations)) {
        return;
    }

    record.animations = animations;
    run_animations(record, AnimationTrigger::Mount);
    if (!record.hidden) {
        run_animations(record, AnimationTrigger::Show);
    }
}

void run_animations(Record &record, AnimationTrigger trigger)
{
    if (record.object == nullptr) {
        return;
    }

    for (const auto &animation : record.animations) {
        if (animation.trigger != trigger) {
            continue;
        }
        auto exec = to_lvgl_anim_exec(animation.property);
        if (exec == nullptr) {
            continue;
        }

        lv_anim_t lv_anim;
        lv_anim_init(&lv_anim);
        lv_anim_set_var(&lv_anim, record.object);
        lv_anim_set_exec_cb(&lv_anim, exec);
        auto [from, to] = resolve_animation_values(record.object, animation);
        exec(record.object, from);
        lv_anim_set_values(&lv_anim, from, to);
        lv_anim_set_duration(&lv_anim, static_cast<uint32_t>(std::max<int32_t>(0, animation.duration)));
        lv_anim_set_delay(&lv_anim, static_cast<uint32_t>(std::max<int32_t>(0, animation.delay)));
        lv_anim_set_path_cb(&lv_anim, to_lvgl_anim_path(animation.easing));
        lv_anim_set_repeat_count(&lv_anim, static_cast<uint32_t>(animation.repeat));
        if (animation.playback) {
            lv_anim_set_reverse_duration(&lv_anim, static_cast<uint32_t>(std::max<int32_t>(0, animation.duration)));
        }
        lv_anim_start(&lv_anim);
    }
}

std::optional<BackendAnimationStartResult> start_animation(
    Record &record, const Animation &animation, std::function<void()> completed_handler
)
{
    if (record.object == nullptr) {
        return std::nullopt;
    }

    auto exec = to_lvgl_anim_exec(animation.property);
    if (exec == nullptr) {
        return std::nullopt;
    }

    lv_anim_delete(record.object, exec);
    auto [from, to] = resolve_animation_values(record.object, animation);

    auto context = std::make_shared<RuntimeAnimationContext>();
    context->object = record.object;
    context->exec = exec;
    context->completed_handler = std::move(completed_handler);

    lv_anim_t lv_anim;
    lv_anim_init(&lv_anim);
    lv_anim_set_var(&lv_anim, record.object);
    lv_anim_set_exec_cb(&lv_anim, exec);
    lv_anim_set_values(&lv_anim, from, to);
    exec(record.object, from);
    lv_anim_set_duration(&lv_anim, static_cast<uint32_t>(std::max<int32_t>(0, animation.duration)));
    lv_anim_set_delay(&lv_anim, static_cast<uint32_t>(std::max<int32_t>(0, animation.delay)));
    lv_anim_set_path_cb(&lv_anim, to_lvgl_anim_path(animation.easing));
    lv_anim_set_user_data(&lv_anim, context.get());
    lv_anim_set_completed_cb(&lv_anim, runtime_animation_completed_cb);
    lv_anim_start(&lv_anim);

    BackendAnimationStartResult result;
    result.resolved_from = from;
    result.resolved_to = to;
    result.connection = ScopedConnection([context = std::move(context)]() mutable {
        if (context == nullptr || context->active == nullptr || !*context->active)
        {
            return;
        }
        *context->active = false;
        lv_anim_delete(context->object, context->exec);
    });
    return result;
}

} // namespace esp_brookesia::gui::lvgl
