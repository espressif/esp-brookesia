/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "private/props_impl.hpp"

namespace esp_brookesia::gui::lvgl::props_detail {



lv_keyboard_mode_t to_keyboard_mode(std::string_view mode)
{
    if (mode == "number") {
        return LV_KEYBOARD_MODE_NUMBER;
    }
    if (mode == "special") {
        return LV_KEYBOARD_MODE_SPECIAL;
    }
    if (mode == "upper" || mode == "textUpper") {
        return LV_KEYBOARD_MODE_TEXT_UPPER;
    }
    return LV_KEYBOARD_MODE_TEXT_LOWER;
}

std::optional<size_t> keyboard_layout_index(std::string_view mode)
{
    if (mode == "text") {
        return 0;
    }
    if (mode == "upper" || mode == "textUpper") {
        return 1;
    }
    if (mode == "number") {
        return 2;
    }
    if (mode == "special") {
        return 3;
    }
    return std::nullopt;
}

std::optional<size_t> keyboard_layout_index_from_lv_mode(lv_keyboard_mode_t mode)
{
    switch (mode) {
    case LV_KEYBOARD_MODE_TEXT_UPPER:
        return 1;
    case LV_KEYBOARD_MODE_NUMBER:
        return 2;
    case LV_KEYBOARD_MODE_SPECIAL:
        return 3;
    case LV_KEYBOARD_MODE_TEXT_LOWER:
    default:
        return 0;
    }
}

Record::KeyboardPayload &ensure_keyboard_payload(Record &record)
{
    return record.ensure_type_payload<Record::KeyboardPayload>();
}

bool keyboard_mode_allowed(const Record &record, std::string_view mode)
{
    const auto *keyboard = record.get_type_payload<Record::KeyboardPayload>();
    if (keyboard == nullptr || keyboard->allowed_modes.empty()) {
        return true;
    }
    return std::find(keyboard->allowed_modes.begin(), keyboard->allowed_modes.end(), mode) !=
           keyboard->allowed_modes.end();
}

bool has_keyboard_layout(const Record &record, std::string_view mode)
{
    auto index = keyboard_layout_index(mode);
    if (!index.has_value()) {
        return false;
    }
    const auto *keyboard = record.get_type_payload<Record::KeyboardPayload>();
    return keyboard != nullptr && !keyboard->layouts[*index].map.empty();
}

std::string view_reference_path_to_absolute(
    std::string_view scope_root_absolute_path,
    std::string_view target
)
{
    if (target.empty() || target.front() == '/') {
        return std::string(target);
    }

    std::string absolute_path(scope_root_absolute_path);
    if (absolute_path.empty() || absolute_path.back() != '/') {
        absolute_path.push_back('/');
    }
    for (char ch : target) {
        absolute_path.push_back(ch == '.' ? '/' : ch);
    }
    return absolute_path;
}

std::string keyboard_key_label(const KeyboardKey &key)
{
    if (!key.text.empty()) {
        return key.text;
    }
    if (key.role == "backspace") {
        return LV_SYMBOL_BACKSPACE;
    }
    if (key.role == "left") {
        return LV_SYMBOL_LEFT;
    }
    if (key.role == "right") {
        return LV_SYMBOL_RIGHT;
    }
    if (key.role == "space") {
        return " ";
    }
    if (key.role == "ok") {
        return LV_SYMBOL_OK;
    }
    if (key.role == "cancel") {
        return LV_SYMBOL_KEYBOARD;
    }
    return key.text;
}

lv_buttonmatrix_ctrl_t keyboard_key_ctrl(const KeyboardKey &key, bool popovers)
{
    const auto width = static_cast<lv_buttonmatrix_ctrl_t>(std::clamp<int32_t>(key.width, 1, 15));
    if (key.role.empty()) {
        return static_cast<lv_buttonmatrix_ctrl_t>((popovers ? LV_BUTTONMATRIX_CTRL_POPOVER : 0) | width);
    }
    if (key.role == "space") {
        return width;
    }
    return static_cast<lv_buttonmatrix_ctrl_t>(LV_BUTTONMATRIX_CTRL_CHECKED | width);
}

Record *keyboard_record_from_event(lv_event_t *event)
{
    auto *context = static_cast<EventContext *>(lv_event_get_user_data(event));
    if (context == nullptr || context->impl == nullptr) {
        return nullptr;
    }
    return context->impl->find_record(context->handle);
}

void set_keyboard_mode_if_possible(Record &record, std::string_view mode)
{
    auto *keyboard = record.get_type_payload<Record::KeyboardPayload>();
    if (keyboard == nullptr || !keyboard_mode_allowed(record, mode) || !has_keyboard_layout(record, mode)) {
        lv_keyboard_set_mode(record.object, to_keyboard_mode(keyboard == nullptr ? "text" : keyboard->current_mode));
        return;
    }
    keyboard->current_mode = std::string(mode);
    lv_keyboard_set_mode(record.object, to_keyboard_mode(mode));
}

std::string keyboard_key_style_class(const Record &record, const KeyboardKey &key)
{
    if (!key.style_class.empty()) {
        return key.style_class;
    }
    if (key.role == "mode" && !keyboard_mode_allowed(record, key.mode)) {
        return "disabled";
    }
    if (key.role == "ok" || key.role == "cancel") {
        return "action";
    }
    if (key.role == "mode") {
        return "mode";
    }
    if (!key.role.empty()) {
        return "special";
    }
    return "default";
}

std::optional<KeyboardKey> keyboard_key_from_selected_button(Record &record, uint32_t selected_button)
{
    const auto *keyboard = record.get_type_payload<Record::KeyboardPayload>();
    if (keyboard == nullptr) {
        return std::nullopt;
    }

    auto index = keyboard_layout_index(keyboard->current_mode);
    if (!index.has_value()) {
        index = keyboard_layout_index_from_lv_mode(lv_keyboard_get_mode(record.object));
    }
    if (!index.has_value()) {
        return std::nullopt;
    }
    const auto &keys = keyboard->layouts[*index].keys;
    if (selected_button >= keys.size()) {
        return std::nullopt;
    }
    return keys[selected_button];
}

void keyboard_value_changed_event_cb(lv_event_t *event)
{
    auto *record = keyboard_record_from_event(event);
    if (record == nullptr || record->object == nullptr) {
        return;
    }
    const auto selected_button = lv_keyboard_get_selected_button(record->object);
    auto key = keyboard_key_from_selected_button(*record, selected_button);
    if (!key.has_value()) {
        return;
    }

    auto *keyboard = record->get_type_payload<Record::KeyboardPayload>();
    if (keyboard == nullptr) {
        return;
    }
    auto *textarea = keyboard->target_object;
    if (textarea != nullptr && !lv_obj_is_valid(textarea)) {
        textarea = nullptr;
        keyboard->target_object = nullptr;
    }

    if (key->role.empty()) {
        if (textarea != nullptr && !key->text.empty()) {
            lv_textarea_add_text(textarea, key->text.c_str());
        }
        return;
    }
    if (key->role == "mode") {
        set_keyboard_mode_if_possible(*record, key->mode);
        return;
    }
    if (key->role == "ok") {
        lv_obj_send_event(record->object, LV_EVENT_READY, nullptr);
        if (textarea != nullptr) {
            lv_obj_send_event(textarea, LV_EVENT_READY, nullptr);
        }
        return;
    }
    if (key->role == "cancel") {
        lv_obj_send_event(record->object, LV_EVENT_CANCEL, nullptr);
        if (textarea != nullptr) {
            lv_obj_send_event(textarea, LV_EVENT_CANCEL, nullptr);
        }
        return;
    }
    if (textarea == nullptr) {
        return;
    }
    if (key->role == "backspace") {
        lv_textarea_delete_char(textarea);
    } else if (key->role == "left") {
        lv_textarea_cursor_left(textarea);
    } else if (key->role == "right") {
        lv_textarea_cursor_right(textarea);
    } else if (key->role == "space") {
        lv_textarea_add_text(textarea, " ");
    }
}

const KeyboardKeyStyle *find_keyboard_key_style(const Record &record, const std::string &style_class)
{
    const auto *keyboard = record.get_type_payload<Record::KeyboardPayload>();
    if (keyboard == nullptr) {
        return nullptr;
    }
    auto it = keyboard->key_styles.find(style_class);
    if (it != keyboard->key_styles.end()) {
        return &it->second;
    }
    return nullptr;
}

std::optional<lv_color_t> parse_keyboard_style_color(std::string_view color)
{
    if (color.empty()) {
        return std::nullopt;
    }
    auto parsed = parse_color(color);
    if (!parsed.has_value()) {
        BROOKESIA_LOGW("Ignoring invalid keyboard key color: %1%", color);
        return std::nullopt;
    }
    return lv_color_hex(*parsed);
}

void apply_keyboard_part_style(lv_obj_t *object, const KeyboardKeyStyle &style, lv_state_t state)
{
    const auto selector = static_cast<lv_style_selector_t>(
                              static_cast<uint32_t>(LV_PART_ITEMS) | static_cast<uint32_t>(state)
                          );
    if (auto color = parse_keyboard_style_color(style.bg_color); color.has_value()) {
        lv_obj_set_style_bg_color(object, *color, selector);
        lv_obj_set_style_bg_opa(object, LV_OPA_COVER, selector);
    }
    if (auto color = parse_keyboard_style_color(style.text_color); color.has_value()) {
        lv_obj_set_style_text_color(object, *color, selector);
        lv_obj_set_style_image_recolor(object, *color, selector);
        lv_obj_set_style_image_recolor_opa(object, LV_OPA_COVER, selector);
    }
    if (style.radius > 0) {
        lv_obj_set_style_radius(object, style.radius, selector);
    }
}

void apply_keyboard_default_item_styles(Record &record)
{
    const auto *keyboard = record.get_type_payload<Record::KeyboardPayload>();
    if (keyboard == nullptr) {
        return;
    }
    auto default_it = keyboard->key_styles.find("default");
    if (default_it == keyboard->key_styles.end()) {
        return;
    }

    apply_keyboard_part_style(record.object, default_it->second, LV_STATE_DEFAULT);

    KeyboardKeyStyle pressed_style = default_it->second;
    if (!pressed_style.pressed_bg_color.empty()) {
        pressed_style.bg_color = pressed_style.pressed_bg_color;
    }
    if (!pressed_style.pressed_text_color.empty()) {
        pressed_style.text_color = pressed_style.pressed_text_color;
    }
    apply_keyboard_part_style(record.object, pressed_style, LV_STATE_PRESSED);

    auto disabled_it = keyboard->key_styles.find("disabled");
    if (disabled_it != keyboard->key_styles.end()) {
        apply_keyboard_part_style(record.object, disabled_it->second, LV_STATE_DISABLED);
    }
}

std::shared_ptr<BinaryImageSource> lookup_keyboard_key_image(BackendImpl &impl, const KeyboardKey &key)
{
    if (key.resolved_image.native_src != 0 || key.resolved_image.primary_src.empty()) {
        return nullptr;
    }
    const auto cache_key = image_cache_key(key.resolved_image.primary_src);
    if (!is_lvgl_bin_path(key.resolved_image.primary_src)) {
        auto decoded_cache_it = impl.decoded_image_cache.find(cache_key);
        if (decoded_cache_it == impl.decoded_image_cache.end()) {
            return nullptr;
        }
        return decoded_cache_it->second.source;
    }
    auto cache_it = impl.binary_image_cache.find(cache_key);
    if (cache_it == impl.binary_image_cache.end()) {
        return nullptr;
    }
    return cache_it->second.source;
}

const void *keyboard_key_image_src(
    BackendImpl &impl,
    const KeyboardKey &key,
    const std::shared_ptr<BinaryImageSource> &resolved)
{
    if (key.resolved_image.native_src != 0) {
        return reinterpret_cast<const void *>(key.resolved_image.native_src);
    }
    if (key.resolved_image.primary_src.empty()) {
        return nullptr;
    }
    const std::shared_ptr<BinaryImageSource> source =
        resolved != nullptr ? resolved : lookup_keyboard_key_image(impl, key);
    if (source == nullptr) {
        BROOKESIA_LOGW("Keyboard key image source is not preloaded: %1%", key.resolved_image.primary_src);
        return nullptr;
    }
    return &source->descriptor;
}

bool draw_keyboard_key_image(
    BackendImpl &impl,
    Record &record,
    const KeyboardKey &key,
    std::optional<lv_color_t> recolor,
    lv_draw_dsc_base_t &base_dsc,
    lv_draw_task_t *draw_task)
{
    if (key.image.empty()) {
        return false;
    }

    const auto *keyboard = record.get_type_payload<Record::KeyboardPayload>();
    if (keyboard == nullptr) {
        return false;
    }

    std::shared_ptr<BinaryImageSource> resolved;
    auto layout_index = keyboard_layout_index(keyboard->current_mode);
    if (!layout_index.has_value()) {
        layout_index = keyboard_layout_index_from_lv_mode(lv_keyboard_get_mode(record.object));
    }
    if (layout_index.has_value() &&
            base_dsc.id1 < keyboard->layouts[*layout_index].image_sources.size()) {
        resolved = keyboard->layouts[*layout_index].image_sources[base_dsc.id1];
    }
    const auto *src = keyboard_key_image_src(impl, key, resolved);
    if (src == nullptr || key.resolved_image.width <= 0 || key.resolved_image.height <= 0) {
        return false;
    }

    lv_area_t key_area {};
    auto area_it = keyboard->key_fill_areas.find(base_dsc.id1);
    if (area_it != keyboard->key_fill_areas.end()) {
        key_area = area_it->second;
    } else {
        lv_draw_task_get_area(draw_task, &key_area);
    }

    const auto source_size = std::max(key.resolved_image.width, key.resolved_image.height);
    const auto target_size = keyboard->icon_size > 0 ? keyboard->icon_size : source_size;
    const auto scale = calculate_scale(target_size, source_size);
    const auto center_x = key_area.x1 + lv_area_get_width(&key_area) / 2;
    const auto center_y = key_area.y1 + lv_area_get_height(&key_area) / 2;

    lv_area_t image_area;
    image_area.x1 = center_x - key.resolved_image.width / 2;
    image_area.y1 = center_y - key.resolved_image.height / 2;
    image_area.x2 = image_area.x1 + key.resolved_image.width - 1;
    image_area.y2 = image_area.y1 + key.resolved_image.height - 1;

    lv_draw_image_dsc_t image_dsc;
    lv_draw_image_dsc_init(&image_dsc);
    image_dsc.base.layer = base_dsc.layer;
    image_dsc.base.obj = record.object;
    image_dsc.src = src;
    image_dsc.scale_x = scale;
    image_dsc.scale_y = scale;
    image_dsc.pivot.x = key.resolved_image.width / 2;
    image_dsc.pivot.y = key.resolved_image.height / 2;
    image_dsc.opa = LV_OPA_COVER;
    if (recolor.has_value()) {
        image_dsc.recolor = *recolor;
        image_dsc.recolor_opa = LV_OPA_COVER;
    }
    image_dsc.antialias = true;

    lv_draw_image(base_dsc.layer, &image_dsc, &image_area);
    return true;
}

void keyboard_draw_event_cb(lv_event_t *event)
{
    auto *context = static_cast<EventContext *>(lv_event_get_user_data(event));
    if (context == nullptr || context->impl == nullptr) {
        return;
    }
    auto *record = context->impl->find_record(context->handle);
    if (record == nullptr || record->object == nullptr) {
        return;
    }

    lv_draw_task_t *draw_task = lv_event_get_draw_task(event);
    auto *base_dsc = static_cast<lv_draw_dsc_base_t *>(lv_draw_task_get_draw_dsc(draw_task));
    if (base_dsc == nullptr || base_dsc->part != LV_PART_ITEMS) {
        return;
    }
    auto key = keyboard_key_from_selected_button(*record, base_dsc->id1);
    if (!key.has_value()) {
        return;
    }
    auto *keyboard = record->get_type_payload<Record::KeyboardPayload>();
    if (keyboard == nullptr) {
        return;
    }

    const auto style_class = keyboard_key_style_class(*record, *key);
    const auto *style = find_keyboard_key_style(*record, style_class);
    const bool pressed = lv_keyboard_get_selected_button(record->object) == base_dsc->id1 &&
                         lv_obj_has_state(record->object, LV_STATE_PRESSED);

    if (auto *fill_dsc = lv_draw_task_get_fill_dsc(draw_task); fill_dsc != nullptr) {
        lv_area_t key_area {};
        lv_draw_task_get_area(draw_task, &key_area);
        keyboard->key_fill_areas.insert_or_assign(base_dsc->id1, key_area);
        if (style != nullptr) {
            const auto color = parse_keyboard_style_color(
                                   pressed && !style->pressed_bg_color.empty() ? style->pressed_bg_color : style->bg_color
                               );
            if (color.has_value()) {
                fill_dsc->color = *color;
                fill_dsc->opa = LV_OPA_COVER;
            }
            if (style->radius > 0) {
                fill_dsc->radius = style->radius;
            }
        }
    }
    if (auto *label_dsc = lv_draw_task_get_label_dsc(draw_task); label_dsc != nullptr) {
        std::optional<lv_color_t> text_color;
        if (style != nullptr) {
            const auto color = parse_keyboard_style_color(
                                   pressed && !style->pressed_text_color.empty() ? style->pressed_text_color :
                                   style->text_color
                               );
            if (color.has_value()) {
                text_color = *color;
            }
        }
        if (!key->image.empty() && draw_keyboard_key_image(
                    *context->impl,
                    *record,
                    *key,
                    text_color,
                    *base_dsc,
                    draw_task
                )) {
            label_dsc->opa = LV_OPA_TRANSP;
            return;
        }
        if (text_color.has_value()) {
            label_dsc->color = *text_color;
            label_dsc->opa = LV_OPA_COVER;
        }
    }
}

void apply_keyboard_layouts(BackendImpl &impl, Record &record, const KeyboardProps &props)
{
    if (props.layouts.empty()) {
        return;
    }

    auto &keyboard = ensure_keyboard_payload(record);
    keyboard.allowed_modes = props.allowed_modes;
    keyboard.icon_size = props.icon_size.mode == SizeMode::Fixed ? std::max<int32_t>(0, props.icon_size.value) : 0;
    keyboard.key_fill_areas.clear();
    keyboard.key_styles.clear();
    const auto &key_styles = props.resolved_key_styles.empty() ? props.key_styles : props.resolved_key_styles;
    for (const auto &[style_class, style] : key_styles) {
        keyboard.key_styles.emplace(style_class, style);
    }
    apply_keyboard_default_item_styles(record);

    std::vector<BackendImpl::KeyboardLayoutBacking> fresh_backing;
    for (const auto &[mode, layout] : props.layouts) {
        auto index = keyboard_layout_index(mode);
        if (!index.has_value()) {
            BROOKESIA_LOGW(
                "Skipping unsupported keyboard layout mode '%1%' for node '%2%'",
                mode,
                impl.build_absolute_path(record)
            );
            continue;
        }

        auto storage_owner = std::make_shared<Record::KeyboardLayoutStorage>();
        auto &storage = *storage_owner;
        storage.labels.clear();
        storage.keys.clear();
        storage.map.clear();
        storage.controls.clear();

        size_t key_count = 0;
        for (const auto &row : layout.rows) {
            key_count += row.size();
        }
        storage.labels.reserve(key_count);
        storage.keys.reserve(key_count);
        storage.map.reserve(key_count + layout.rows.size() + 1);
        storage.controls.reserve(key_count);

        for (size_t row_index = 0; row_index < layout.rows.size(); ++row_index) {
            const auto &row = layout.rows[row_index];
            for (const auto &key : row) {
                storage.labels.push_back(keyboard_key_label(key));
                storage.keys.push_back(key);
                storage.image_sources.push_back(lookup_keyboard_key_image(impl, key));
                storage.map.push_back(storage.labels.back().c_str());
                auto control = keyboard_key_ctrl(key, props.popovers);
                if (key.role == "mode" && !keyboard_mode_allowed(record, key.mode)) {
                    control = static_cast<lv_buttonmatrix_ctrl_t>(control | LV_BUTTONMATRIX_CTRL_DISABLED);
                }
                storage.controls.push_back(control);
            }
            if (row_index + 1 < layout.rows.size()) {
                storage.map.push_back("\n");
            }
        }
        storage.map.push_back("");

        publish_keyboard_map(
            record.object,
            to_keyboard_mode(mode),
            storage.map.data(),
            storage.controls.data()
        );
        // The record payload keeps the decoded images for the draw callbacks, which run after
        // the document read scope ends. Its map and controls copies are never borrowed by
        // LVGL: only the backing entry below owns the arrays LVGL points into.
        keyboard.layouts[*index] = storage;
        // The backing only has to keep the map pointers LVGL borrowed, so its image refs are
        // dropped here: they would otherwise hold decoded bytes alive until the previous
        // backing entry for this mode is released below.
        storage.image_sources.clear();
        fresh_backing.push_back(BackendImpl::KeyboardLayoutBacking{
            .owner = record.handle.value(),
            .layout_index = *index,
            .storage = std::move(storage_owner),
        });
    }
    for (auto &fresh : fresh_backing) {
        impl.keyboard_layout_backing_store.push_back(std::move(fresh));
    }
    // The new arrays are the published globals. Drop the previous owner's arrays only
    // when no surviving keyboard still displays them.
    release_unreferenced_keyboard_layouts(impl);

    if (!keyboard.event_registered) {
        keyboard.value_event_context = std::make_unique<EventContext>(EventContext{
            .impl = &impl,
            .handle = record.handle,
            .type = EventType::ValueChanged,
            .action = {},
        });
        lv_obj_add_event_cb(
            record.object,
            keyboard_value_changed_event_cb,
            LV_EVENT_VALUE_CHANGED,
            keyboard.value_event_context.get()
        );
        keyboard.event_registered = true;
    }
    if (!keyboard.draw_event_registered) {
        keyboard.draw_event_context = std::make_unique<EventContext>(EventContext{
            .impl = &impl,
            .handle = record.handle,
            .type = EventType::ValueChanged,
            .action = {},
        });
        lv_obj_add_event_cb(
            record.object,
            keyboard_draw_event_cb,
            LV_EVENT_DRAW_TASK_ADDED,
            keyboard.draw_event_context.get()
        );
        lv_obj_add_flag(record.object, LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS);
        keyboard.draw_event_registered = true;
    }
}

void apply_keyboard_target(BackendImpl &impl, Record &record, const KeyboardProps &props)
{
    if (props.target_text_input.empty()) {
        return;
    }

    const auto target_path = view_reference_path_to_absolute(
                                 impl.build_scope_root_absolute_path(record),
                                 props.target_text_input
                             );
    auto *target = impl.find_record_by_absolute_path(target_path);
    if (target == nullptr || target->object == nullptr || target->type != NodeType::TextInput ||
            !lv_obj_is_valid(target->object)) {
        BROOKESIA_LOGW(
            "Keyboard target text input not found: node='%1%', target='%2%', target_path='%3%'",
            impl.build_absolute_path(record),
            props.target_text_input,
            target_path
        );
        return;
    }

    auto &keyboard = ensure_keyboard_payload(record);
    keyboard.target_path = target_path;
    keyboard.target_object = target->object;
    lv_keyboard_set_textarea(record.object, nullptr);
}

lv_image_align_t to_image_inner_align(std::string_view align)
{
    if (align == "tile") {
        return LV_IMAGE_ALIGN_TILE;
    }
    if (align == "stretch") {
        return LV_IMAGE_ALIGN_STRETCH;
    }
    if (align == "contain") {
        return LV_IMAGE_ALIGN_CONTAIN;
    }
    if (align == "cover") {
        return LV_IMAGE_ALIGN_COVER;
    }
    if (align == "center") {
        return LV_IMAGE_ALIGN_CENTER;
    }
    return LV_IMAGE_ALIGN_DEFAULT;
}
}

namespace esp_brookesia::gui::lvgl {

void detach_keyboard_callbacks(Record &record)
{
    auto *keyboard = record.get_type_payload<Record::KeyboardPayload>();
    if (keyboard == nullptr || record.object == nullptr || !lv_obj_is_valid(record.object)) {
        return;
    }

    if (keyboard->event_registered && keyboard->value_event_context != nullptr) {
        lv_obj_remove_event_cb_with_user_data(
            record.object,
            &props_detail::keyboard_value_changed_event_cb,
            keyboard->value_event_context.get()
        );
        keyboard->event_registered = false;
    }
    if (keyboard->draw_event_registered && keyboard->draw_event_context != nullptr) {
        lv_obj_remove_event_cb_with_user_data(
            record.object,
            &props_detail::keyboard_draw_event_cb,
            keyboard->draw_event_context.get()
        );
        keyboard->draw_event_registered = false;
        lv_obj_remove_flag(record.object, LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS);
    }
}

} // namespace esp_brookesia::gui::lvgl
