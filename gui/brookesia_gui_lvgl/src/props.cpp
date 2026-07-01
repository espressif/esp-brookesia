/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <unordered_map>
#include <unordered_set>

#include "private/types.hpp"
#include "port/private/threading.hpp"
#include "brookesia/gui_lvgl/macro_configs.h"
#if !BROOKESIA_GUI_LVGL_PROPS_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#if defined(__EMSCRIPTEN__)
#include "brookesia/gui_interface/wasm/gui_task_queue.hpp"
#endif
#include "private/utils.hpp"

namespace esp_brookesia::gui::lvgl {

namespace {

static bool is_lvgl_bin_path(std::string_view path);
static void log_image_file_diagnostics_once(std::string_view path);

class ThreadLockGuard {
public:
    ThreadLockGuard()
    {
        lock_thread();
    }

    ~ThreadLockGuard()
    {
        unlock_thread();
    }

    ThreadLockGuard(const ThreadLockGuard &) = delete;
    ThreadLockGuard &operator=(const ThreadLockGuard &) = delete;
};

static uint32_t calculate_scale(int32_t target, int32_t source)
{
    if (target <= 0 || source <= 0) {
        return LV_SCALE_NONE;
    }
    const int64_t scaled = (static_cast<int64_t>(target) * LV_SCALE_NONE + source / 2) / source;
    return static_cast<uint32_t>(std::max<int64_t>(1, scaled));
}

static int32_t apply_scale(int32_t source, uint32_t scale)
{
    if (source <= 0) {
        return 0;
    }
    return static_cast<int32_t>((static_cast<int64_t>(source) * scale + LV_SCALE_NONE / 2) / LV_SCALE_NONE);
}

static std::string join_options(const std::vector<std::string> &options)
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

static lv_keyboard_mode_t to_keyboard_mode(std::string_view mode)
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

static std::optional<size_t> keyboard_layout_index(std::string_view mode)
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

static std::optional<size_t> keyboard_layout_index_from_lv_mode(lv_keyboard_mode_t mode)
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

static bool keyboard_mode_allowed(const Record &record, std::string_view mode)
{
    if (record.keyboard_allowed_modes.empty()) {
        return true;
    }
    return std::find(record.keyboard_allowed_modes.begin(), record.keyboard_allowed_modes.end(), mode) !=
           record.keyboard_allowed_modes.end();
}

static bool has_keyboard_layout(const Record &record, std::string_view mode)
{
    auto index = keyboard_layout_index(mode);
    if (!index.has_value()) {
        return false;
    }
    return !record.keyboard_layouts[*index].map.empty();
}

static Record *find_record_by_absolute_path(BackendImpl &impl, std::string_view absolute_path)
{
    for (auto &[unused_handle, record] : impl.records) {
        (void)unused_handle;
        if (record.absolute_path == absolute_path) {
            return &record;
        }
    }
    return nullptr;
}

static std::string view_reference_path_to_absolute(
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

static std::string keyboard_key_label(const KeyboardKey &key)
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

static lv_buttonmatrix_ctrl_t keyboard_key_ctrl(const KeyboardKey &key, bool popovers)
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

static Record *keyboard_record_from_event(lv_event_t *event)
{
    auto *context = static_cast<EventContext *>(lv_event_get_user_data(event));
    if (context == nullptr || context->impl == nullptr) {
        return nullptr;
    }
    return context->impl->find_record(context->handle);
}

static void set_keyboard_mode_if_possible(Record &record, std::string_view mode)
{
    if (!keyboard_mode_allowed(record, mode) || !has_keyboard_layout(record, mode)) {
        lv_keyboard_set_mode(record.object, to_keyboard_mode(record.keyboard_current_mode));
        return;
    }
    record.keyboard_current_mode = std::string(mode);
    lv_keyboard_set_mode(record.object, to_keyboard_mode(mode));
}

static std::string keyboard_key_style_class(const Record &record, const KeyboardKey &key)
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

static std::optional<KeyboardKey> keyboard_key_from_selected_button(Record &record, uint32_t selected_button)
{
    auto index = keyboard_layout_index(record.keyboard_current_mode);
    if (!index.has_value()) {
        index = keyboard_layout_index_from_lv_mode(lv_keyboard_get_mode(record.object));
    }
    if (!index.has_value()) {
        return std::nullopt;
    }
    const auto &keys = record.keyboard_layouts[*index].keys;
    if (selected_button >= keys.size()) {
        return std::nullopt;
    }
    return keys[selected_button];
}

static void keyboard_value_changed_event_cb(lv_event_t *event)
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

    auto *textarea = record->keyboard_target_object;
    if (textarea != nullptr && !lv_obj_is_valid(textarea)) {
        textarea = nullptr;
        record->keyboard_target_object = nullptr;
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

static const KeyboardKeyStyle *find_keyboard_key_style(const Record &record, const std::string &style_class)
{
    auto it = record.keyboard_key_styles.find(style_class);
    if (it != record.keyboard_key_styles.end()) {
        return &it->second;
    }
    return nullptr;
}

static std::optional<lv_color_t> parse_keyboard_style_color(std::string_view color)
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

static void apply_keyboard_part_style(lv_obj_t *object, const KeyboardKeyStyle &style, lv_state_t state)
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

static void apply_keyboard_default_item_styles(Record &record)
{
    auto default_it = record.keyboard_key_styles.find("default");
    if (default_it == record.keyboard_key_styles.end()) {
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

    auto disabled_it = record.keyboard_key_styles.find("disabled");
    if (disabled_it != record.keyboard_key_styles.end()) {
        apply_keyboard_part_style(record.object, disabled_it->second, LV_STATE_DISABLED);
    }
}

static const void *keyboard_key_image_src(BackendImpl &impl, Record &record, const KeyboardKey &key)
{
    if (key.resolved_image.native_src != 0) {
        return reinterpret_cast<const void *>(key.resolved_image.native_src);
    }
    if (key.resolved_image.primary_src.empty()) {
        return nullptr;
    }
    if (!is_lvgl_bin_path(key.resolved_image.primary_src)) {
        auto &src_storage = record.keyboard_image_src_storages[key.resolved_image.image_id];
        if (src_storage == nullptr || *src_storage != key.resolved_image.primary_src) {
            src_storage = std::make_shared<std::string>(key.resolved_image.primary_src);
        }
        log_image_file_diagnostics_once(*src_storage);
        return src_storage->c_str();
    }

    auto cache_it = impl.binary_image_cache.find(key.resolved_image.primary_src);
    if (cache_it == impl.binary_image_cache.end() || cache_it->second.source == nullptr) {
        BROOKESIA_LOGW("Keyboard key image binary source is not preloaded: %1%", key.resolved_image.primary_src);
        return nullptr;
    }
    return &cache_it->second.source->descriptor;
}

static bool draw_keyboard_key_image(
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

    const auto *src = keyboard_key_image_src(impl, record, key);
    if (src == nullptr || key.resolved_image.width <= 0 || key.resolved_image.height <= 0) {
        return false;
    }

    lv_area_t key_area {};
    auto area_it = record.keyboard_key_fill_areas.find(base_dsc.id1);
    if (area_it != record.keyboard_key_fill_areas.end()) {
        key_area = area_it->second;
    } else {
        lv_draw_task_get_area(draw_task, &key_area);
    }

    const auto source_size = std::max(key.resolved_image.width, key.resolved_image.height);
    const auto target_size = record.keyboard_icon_size > 0 ? record.keyboard_icon_size : source_size;
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

static void keyboard_draw_event_cb(lv_event_t *event)
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

    const auto style_class = keyboard_key_style_class(*record, *key);
    const auto *style = find_keyboard_key_style(*record, style_class);
    const bool pressed = lv_keyboard_get_selected_button(record->object) == base_dsc->id1 &&
                         lv_obj_has_state(record->object, LV_STATE_PRESSED);

    if (auto *fill_dsc = lv_draw_task_get_fill_dsc(draw_task); fill_dsc != nullptr) {
        lv_area_t key_area {};
        lv_draw_task_get_area(draw_task, &key_area);
        record->keyboard_key_fill_areas.insert_or_assign(base_dsc->id1, key_area);
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

static void apply_keyboard_layouts(BackendImpl &impl, Record &record, const KeyboardProps &props)
{
    if (props.layouts.empty()) {
        return;
    }

    record.keyboard_allowed_modes = props.allowed_modes;
    record.keyboard_icon_size = props.icon_size.mode == SizeMode::Fixed ? std::max<int32_t>(0, props.icon_size.value) : 0;
    record.keyboard_key_fill_areas.clear();
    record.keyboard_image_src_storages.clear();
    record.keyboard_key_styles.clear();
    const auto &key_styles = props.resolved_key_styles.empty() ? props.key_styles : props.resolved_key_styles;
    for (const auto &[style_class, style] : key_styles) {
        record.keyboard_key_styles.emplace(style_class, style);
    }
    apply_keyboard_default_item_styles(record);

    for (const auto &[mode, layout] : props.layouts) {
        auto index = keyboard_layout_index(mode);
        if (!index.has_value()) {
            BROOKESIA_LOGW("Skipping unsupported keyboard layout mode '%1%' for node '%2%'", mode, record.absolute_path);
            continue;
        }

        auto &storage = record.keyboard_layouts[*index];
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

        lv_keyboard_set_map(
            record.object,
            to_keyboard_mode(mode),
            storage.map.data(),
            storage.controls.data()
        );
    }

    if (!record.keyboard_event_registered) {
        record.keyboard_value_event_context = std::make_unique<EventContext>(EventContext{
            .impl = &impl,
            .handle = record.handle,
            .type = EventType::ValueChanged,
            .action = {},
        });
        lv_obj_add_event_cb(
            record.object,
            keyboard_value_changed_event_cb,
            LV_EVENT_VALUE_CHANGED,
            record.keyboard_value_event_context.get()
        );
        record.keyboard_event_registered = true;
    }
    if (!record.keyboard_draw_event_registered) {
        record.keyboard_draw_event_context = std::make_unique<EventContext>(EventContext{
            .impl = &impl,
            .handle = record.handle,
            .type = EventType::ValueChanged,
            .action = {},
        });
        lv_obj_add_event_cb(
            record.object,
            keyboard_draw_event_cb,
            LV_EVENT_DRAW_TASK_ADDED,
            record.keyboard_draw_event_context.get()
        );
        lv_obj_add_flag(record.object, LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS);
        record.keyboard_draw_event_registered = true;
    }
}

static void apply_keyboard_target(BackendImpl &impl, Record &record, const KeyboardProps &props)
{
    if (props.target_text_input.empty()) {
        return;
    }

    const auto target_path = view_reference_path_to_absolute(
                                 record.scope_root_absolute_path,
                                 props.target_text_input
                             );
    auto *target = find_record_by_absolute_path(impl, target_path);
    if (target == nullptr || target->object == nullptr || target->type != NodeType::TextInput ||
            !lv_obj_is_valid(target->object)) {
        BROOKESIA_LOGW(
            "Keyboard target text input not found: node='%1%', target='%2%', target_path='%3%'",
            record.absolute_path,
            props.target_text_input,
            target_path
        );
        return;
    }

    record.keyboard_target_path = target_path;
    record.keyboard_target_object = target->object;
    lv_keyboard_set_textarea(record.object, nullptr);
}

static lv_image_align_t to_image_inner_align(std::string_view align)
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

static std::string resolve_frame_view_output_name(const Record &record, const FrameViewProps &props)
{
    if (!props.output_name.empty()) {
        return props.output_name;
    }
    if (!props.auto_register_output) {
        return {};
    }
    return "FrameView" + std::to_string(record.handle.value());
}

static std::optional<esp_brookesia::service::Display::PixelFormat> to_display_pixel_format(FrameColorFormat format)
{
    using DisplayService = esp_brookesia::service::Display;

    switch (format) {
    case FrameColorFormat::RGB565:
        return DisplayService::PixelFormat::RGB565;
    case FrameColorFormat::RGB888:
        return DisplayService::PixelFormat::RGB888;
    case FrameColorFormat::Max:
    default:
        return std::nullopt;
    }
}

static std::optional<FrameColorFormat> to_frame_color_format(
    esp_brookesia::service::Display::PixelFormat pixel_format)
{
    using DisplayService = esp_brookesia::service::Display;

    switch (pixel_format) {
    case DisplayService::PixelFormat::RGB565:
        return FrameColorFormat::RGB565;
    case DisplayService::PixelFormat::RGB888:
        return FrameColorFormat::RGB888;
    default:
        return std::nullopt;
    }
}

static std::optional<lv_color_format_t> to_lvgl_color_format(FrameColorFormat format)
{
    switch (format) {
    case FrameColorFormat::RGB565:
        return LV_COLOR_FORMAT_RGB565;
    case FrameColorFormat::RGB888:
        return LV_COLOR_FORMAT_RGB888;
    case FrameColorFormat::Max:
    default:
        return std::nullopt;
    }
}

static size_t frame_view_bytes_per_pixel(FrameColorFormat format)
{
    switch (format) {
    case FrameColorFormat::RGB565:
        return 2;
    case FrameColorFormat::RGB888:
        return 3;
    case FrameColorFormat::Max:
    default:
        return 0;
    }
}

static bool frame_view_props_equal(const FrameViewProps &lhs, const FrameViewProps &rhs)
{
    return lhs.auto_register_output == rhs.auto_register_output &&
           lhs.output_name == rhs.output_name &&
           lhs.color_format == rhs.color_format;
}

static bool frame_view_matches(
    const Record &record,
    const FrameViewProps &props,
    std::string_view output_name,
    int32_t width,
    int32_t height)
{
    return record.frame_view_ready &&
           frame_view_props_equal(record.frame_view_props, props) &&
           record.frame_view_output_name == output_name &&
           record.frame_view_width == width &&
           record.frame_view_height == height;
}

static void clear_frame_view_image(Record &record)
{
    record.frame_view_frame_connection.disconnect();
    record.frame_view_ready = false;
    record.frame_view_width = 0;
    record.frame_view_height = 0;
    record.frame_view_descriptor = {};
    if (record.object != nullptr && lv_obj_is_valid(record.object)) {
        lv_image_set_src(record.object, nullptr);
    }
}

static bool set_frame_view_image(
    Record &record,
    const esp_brookesia::service::Display::BufferOutputView &output,
    FrameColorFormat descriptor_format)
{
    auto lv_format = to_lvgl_color_format(descriptor_format);
    if (!lv_format.has_value()) {
        BROOKESIA_LOGE("Unsupported FrameView color format for node '%1%'", record.absolute_path);
        clear_frame_view_image(record);
        return false;
    }
    if (output.info.width == 0 || output.info.height == 0 || output.stride_bytes == 0) {
        BROOKESIA_LOGE("Invalid FrameView output dimensions for node '%1%'", record.absolute_path);
        clear_frame_view_image(record);
        return false;
    }
    if (output.info.width > std::numeric_limits<uint16_t>::max() ||
            output.info.height > std::numeric_limits<uint16_t>::max() ||
            output.stride_bytes > std::numeric_limits<uint16_t>::max()) {
        BROOKESIA_LOGE("FrameView output is too large for LVGL image descriptor: node='%1%'", record.absolute_path);
        clear_frame_view_image(record);
        return false;
    }
    if (output.buffer.data_ptr == nullptr) {
        BROOKESIA_LOGE("FrameView output buffer is null: node='%1%'", record.absolute_path);
        clear_frame_view_image(record);
        return false;
    }

    const uint64_t data_size = static_cast<uint64_t>(output.stride_bytes) * output.info.height;
    if (data_size > std::numeric_limits<uint32_t>::max()) {
        BROOKESIA_LOGE(
            "FrameView output buffer is too large for LVGL image descriptor: node='%1%'",
            record.absolute_path
        );
        clear_frame_view_image(record);
        return false;
    }

    record.frame_view_descriptor = {};
    record.frame_view_descriptor.header.magic = LV_IMAGE_HEADER_MAGIC;
    record.frame_view_descriptor.header.cf = *lv_format;
    record.frame_view_descriptor.header.flags = 0;
    record.frame_view_descriptor.header.w = output.info.width;
    record.frame_view_descriptor.header.h = output.info.height;
    record.frame_view_descriptor.header.stride = static_cast<uint32_t>(output.stride_bytes);
    record.frame_view_descriptor.data_size = static_cast<uint32_t>(data_size);
    record.frame_view_descriptor.data = output.buffer.to_const_ptr<uint8_t>();

    lv_image_set_src(record.object, &record.frame_view_descriptor);
    record.frame_view_width = static_cast<int32_t>(output.info.width);
    record.frame_view_height = static_cast<int32_t>(output.info.height);
    record.frame_view_ready = true;
    return true;
}

static void connect_frame_view_frame_signal(BackendImpl &impl, Record &record)
{
    using DisplayService = esp_brookesia::service::Display;

    record.frame_view_frame_connection.disconnect();
    const auto handle = record.handle;
    const auto output_name = record.frame_view_output_name;
    auto frame_presented_callback = [&impl, handle](const std::string &, const DisplayService::FrameInfo &) {
#if defined(__EMSCRIPTEN__)
        auto queued = esp_brookesia::gui::wasm::post_gui_task([&impl, handle]() {
            ThreadLockGuard lock;
            auto *record = impl.find_record(handle);
            if (record == nullptr || record->object == nullptr || !lv_obj_is_valid(record->object)) {
                return;
            }
            lv_obj_invalidate(record->object);
        });
        if (!queued) {
            BROOKESIA_LOGW("Failed to queue wasm FrameView invalidate");
        }
#else
        ThreadLockGuard lock;
        auto *record = impl.find_record(handle);
        if (record == nullptr || record->object == nullptr || !lv_obj_is_valid(record->object)) {
            return;
        }
        lv_obj_invalidate(record->object);
#endif
    };
    record.frame_view_frame_connection =
        DisplayService::get_instance().connect_frame_presented(output_name, frame_presented_callback);
}

static void connect_external_frame_view_lifecycle(BackendImpl &impl, Record &record)
{
    using DisplayService = esp_brookesia::service::Display;

    record.frame_view_output_registered_connection.disconnect();
    record.frame_view_output_unregistered_connection.disconnect();
    const auto handle = record.handle;
    const auto output_name = record.frame_view_output_name;
    auto output_registered_callback = [&impl, handle, output_name](const DisplayService::OutputInfo & info) {
        if (info.name != output_name || info.slot != DisplayService::OutputSlot::Buffer) {
            return;
        }
        ThreadLockGuard lock;
        auto *record = impl.find_record(handle);
        if (record == nullptr || record->frame_view_props.auto_register_output ||
                record->frame_view_output_name != output_name) {
            return;
        }
        refresh_frame_view(impl, *record, record->frame_view_props);
    };
    auto output_unregistered_callback = [&impl, handle, output_name](const std::string & unregistered_output_name) {
        if (unregistered_output_name != output_name) {
            return;
        }
        ThreadLockGuard lock;
        auto *record = impl.find_record(handle);
        if (record == nullptr || record->frame_view_props.auto_register_output ||
                record->frame_view_output_name != output_name) {
            return;
        }
        clear_frame_view_image(*record);
    };
    record.frame_view_output_registered_connection =
        DisplayService::get_instance().connect_output_registered(output_registered_callback);
    record.frame_view_output_unregistered_connection =
        DisplayService::get_instance().connect_output_unregistered(output_unregistered_callback);
}

static void apply_common_disabled(Record &record, const CommonProps &props)
{
    if (props.disabled) {
        lv_obj_add_state(record.object, LV_STATE_DISABLED);
    } else {
        lv_obj_remove_state(record.object, LV_STATE_DISABLED);
    }
}

static void apply_common_clickable(Record &record, const CommonProps &props)
{
    if (props.clickable) {
        lv_obj_add_flag(record.object, LV_OBJ_FLAG_CLICKABLE);
    } else {
        lv_obj_remove_flag(record.object, LV_OBJ_FLAG_CLICKABLE);
    }
}

static void apply_common_scrollable(Record &record, const CommonProps &props)
{
    if (props.scrollable) {
        lv_obj_add_flag(record.object, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scrollbar_mode(record.object, LV_SCROLLBAR_MODE_AUTO);
    } else {
        lv_obj_remove_flag(record.object, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scrollbar_mode(record.object, LV_SCROLLBAR_MODE_OFF);
    }
}

static void apply_common_press_lock(Record &record, const CommonProps &props)
{
    if (props.press_lock) {
        lv_obj_add_flag(record.object, LV_OBJ_FLAG_PRESS_LOCK);
    } else {
        lv_obj_remove_flag(record.object, LV_OBJ_FLAG_PRESS_LOCK);
    }
}

static void apply_common_hidden(Record &record, const CommonProps &props)
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

static int32_t resolve_pivot_value(const PivotValue &pivot, int32_t size)
{
    if (pivot.percent) {
        return static_cast<int32_t>(std::lround(static_cast<float>(size) * static_cast<float>(pivot.value) / 100.0F));
    }
    return pivot.value;
}

static void apply_common_transform(Record &record, const CommonProps &props)
{
    lv_obj_update_layout(record.object);
    const auto width = lv_obj_get_width(record.object);
    const auto height = lv_obj_get_height(record.object);
    lv_obj_set_style_transform_pivot_x(record.object, resolve_pivot_value(props.pivot_x, width), LV_PART_MAIN);
    lv_obj_set_style_transform_pivot_y(record.object, resolve_pivot_value(props.pivot_y, height), LV_PART_MAIN);
    lv_obj_set_style_transform_scale(record.object, std::max<int32_t>(1, props.zoom), LV_PART_MAIN);
    lv_obj_set_style_transform_rotation(record.object, props.angle * 10, LV_PART_MAIN);
}

static void apply_common_flags(Record &record, const CommonProps &props)
{
    apply_common_disabled(record, props);
    apply_common_clickable(record, props);
    apply_common_scrollable(record, props);
    apply_common_press_lock(record, props);
    apply_common_hidden(record, props);
    apply_common_transform(record, props);
}

static void apply_toggle_state(lv_obj_t *object, bool checked)
{
    if (checked) {
        lv_obj_add_state(object, LV_STATE_CHECKED);
    } else {
        lv_obj_remove_state(object, LV_STATE_CHECKED);
    }
}

static void apply_canvas(Record &record, const Node &node)
{
    const auto width = node.placement.width.mode == SizeMode::Fixed ? node.placement.width.value : 96;
    const auto height = node.placement.height.mode == SizeMode::Fixed ? node.placement.height.value : 96;
    if (width <= 0 || height <= 0) {
        return;
    }

    record.canvas_buffer.assign(static_cast<size_t>(width) * static_cast<size_t>(height), lv_color_hex(0xffffff));
    lv_canvas_set_buffer(record.object, record.canvas_buffer.data(), width, height, LV_COLOR_FORMAT_NATIVE);
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

static void log_image_file_diagnostics_once(std::string_view path)
{
    static std::unordered_set<std::string> checked_paths;

    if (path.empty()) {
        return;
    }

    std::string path_string(path);
    const auto [unused_it, inserted] = checked_paths.emplace(path_string);
    if (!inserted) {
        return;
    }

    struct stat file_stat {};
    errno = 0;
    if (::stat(path_string.c_str(), &file_stat) != 0) {
        BROOKESIA_LOGW(
            "Image file stat failed: path='%1%', errno=%2%, reason='%3%'",
            path_string,
            errno,
            std::strerror(errno)
        );
        return;
    }

    errno = 0;
    std::FILE *image_file = std::fopen(path_string.c_str(), "rb");
    if (image_file == nullptr) {
        BROOKESIA_LOGW(
            "Image file fopen failed: path='%1%', errno=%2%, reason='%3%'",
            path_string,
            errno,
            std::strerror(errno)
        );
        return;
    }
    std::fclose(image_file);

    BROOKESIA_LOGD("Image file is readable: path='%1%', size=%2%", path_string, file_stat.st_size);
}

static uint16_t read_le16(const std::vector<uint8_t> &data, size_t offset)
{
    return static_cast<uint16_t>(data[offset]) | (static_cast<uint16_t>(data[offset + 1]) << 8);
}

static bool is_lvgl_bin_path(std::string_view path)
{
    return path.ends_with(".bin") || path.ends_with(".BIN");
}

static std::expected<std::shared_ptr<BinaryImageSource>, std::string> load_binary_image_source(const std::string &path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return std::unexpected("Failed to open LVGL image bin file: " + path);
    }
    file.seekg(0, std::ios::end);
    const auto file_size = file.tellg();
    if (file_size < 12) {
        return std::unexpected("Invalid LVGL image bin size: " + path);
    }
    file.seekg(0, std::ios::beg);

    auto source = std::make_shared<BinaryImageSource>();
    source->data.resize(static_cast<size_t>(file_size));
    file.read(reinterpret_cast<char *>(source->data.data()), static_cast<std::streamsize>(source->data.size()));
    if (!file) {
        return std::unexpected("Failed to read LVGL image bin file: " + path);
    }
    if (source->data[0] != LV_IMAGE_HEADER_MAGIC) {
        return std::unexpected("Invalid LVGL image bin magic: " + path);
    }

    source->descriptor.header.magic = LV_IMAGE_HEADER_MAGIC;
    source->descriptor.header.cf = source->data[1];
    source->descriptor.header.flags = read_le16(source->data, 2);
    source->descriptor.header.w = read_le16(source->data, 4);
    source->descriptor.header.h = read_le16(source->data, 6);
    source->descriptor.header.stride = read_le16(source->data, 8);
    source->descriptor.header.reserved_2 = 0;
    source->descriptor.data_size = static_cast<uint32_t>(source->data.size() - 12);
    source->descriptor.data = source->data.data() + 12;
    source->descriptor.reserved = nullptr;
    source->descriptor.reserved_2 = nullptr;
    return source;
}

} // namespace

std::expected<RuntimeImageResource, std::string> resolve_image_resource(RuntimeImageResource resource)
{
    if (resource.native_src != 0 || !is_lvgl_bin_path(resource.primary_src) ||
            (resource.width > 0 && resource.height > 0)) {
        return resource;
    }

    auto source = load_binary_image_source(resource.primary_src);
    if (!source) {
        return std::unexpected(source.error());
    }

    resource.width = static_cast<int32_t>((*source)->descriptor.header.w);
    resource.height = static_cast<int32_t>((*source)->descriptor.header.h);
    return resource;
}

bool requires_preloaded_image_resource(const RuntimeImageResource &resource)
{
    return resource.native_src == 0 && is_lvgl_bin_path(resource.primary_src);
}

std::expected<void, std::string> preload_image_resource(BackendImpl &impl, const RuntimeImageResource &resource)
{
    if (resource.native_src != 0 || !is_lvgl_bin_path(resource.primary_src)) {
        return {};
    }

    auto cache_it = impl.binary_image_cache.find(resource.primary_src);
    if (cache_it != impl.binary_image_cache.end()) {
        ++cache_it->second.ref_count;
        return {};
    }

    auto source = load_binary_image_source(resource.primary_src);
    if (!source) {
        return std::unexpected(source.error());
    }

    impl.binary_image_cache.emplace(resource.primary_src, BinaryImageCacheEntry{
        .source = *source,
        .ref_count = 1,
    });
    BROOKESIA_LOGD("Preloaded LVGL image bin: path='%1%'", resource.primary_src);
    return {};
}

void release_image_resource(BackendImpl &impl, const RuntimeImageResource &resource)
{
    if (resource.native_src != 0 || !is_lvgl_bin_path(resource.primary_src)) {
        return;
    }

    auto cache_it = impl.binary_image_cache.find(resource.primary_src);
    if (cache_it == impl.binary_image_cache.end()) {
        return;
    }
    if (cache_it->second.ref_count > 1) {
        --cache_it->second.ref_count;
        return;
    }
    impl.binary_image_cache.erase(cache_it);
    BROOKESIA_LOGD("Released LVGL image bin: path='%1%'", resource.primary_src);
}

static void apply_image_source(BackendImpl &impl, Record &record, const Node &node)
{
    if (node.image_props.src.empty()) {
        return;
    }

    const auto next_image_src =
        node.resolved_image.primary_src.empty() ? node.image_props.src : node.resolved_image.primary_src;
    const auto next_image_native_src = node.resolved_image.native_src;
    const bool source_changed =
        record.image_src != next_image_src || record.image_native_src != next_image_native_src;
    if (source_changed) {
        lv_image_set_src(record.object, nullptr);
    }
    record.image_native_src = next_image_native_src;
    if (record.image_native_src == 0) {
        if (is_lvgl_bin_path(next_image_src)) {
            auto cache_it = impl.binary_image_cache.find(next_image_src);
            if (cache_it != impl.binary_image_cache.end()) {
                record.image_binary_src = cache_it->second.source;
            } else if (source_changed || record.image_binary_src == nullptr) {
                record.image_binary_src.reset();
                BROOKESIA_LOGW("LVGL image bin was not preloaded: path='%1%'", next_image_src);
            }
        } else {
            record.image_binary_src.reset();
        }
        if (record.image_binary_src != nullptr) {
            record.image_src_storage.reset();
        } else if (source_changed || record.image_src_storage == nullptr) {
            if (is_lvgl_bin_path(next_image_src)) {
                record.image_src_storage.reset();
            } else {
                record.image_src_storage = std::make_shared<std::string>(next_image_src);
            }
        }
        record.image_src = next_image_src;
    } else {
        record.image_src = next_image_src;
        record.image_src_storage.reset();
        record.image_binary_src.reset();
    }
    record.image_width = node.resolved_image.width;
    record.image_height = node.resolved_image.height;
    const void *image_src = nullptr;
    if (record.image_native_src != 0) {
        image_src = reinterpret_cast<const void *>(record.image_native_src);
    } else if (record.image_binary_src != nullptr) {
        image_src = static_cast<const void *>(&record.image_binary_src->descriptor);
    } else if (record.image_src_storage != nullptr) {
        image_src = static_cast<const void *>(record.image_src_storage->c_str());
    }
    if (record.image_native_src == 0 && !is_lvgl_bin_path(record.image_src)) {
        log_image_file_diagnostics_once(record.image_src);
    }
    BROOKESIA_LOGD(
        "Applying image source: node='%1%', requested_src='%2%', resolved_src='%3%', native_src=%4%, width=%5%, height=%6%",
        record.absolute_path,
        node.image_props.src,
        record.image_src,
        record.image_native_src,
        record.image_width,
        record.image_height
    );
    lv_image_set_src(record.object, image_src);
    apply_image_sizing(record, node.placement);
}

void release_frame_view(Record &record)
{
    using DisplayService = esp_brookesia::service::Display;

    if (record.type != NodeType::FrameView) {
        return;
    }

    const bool should_unregister = record.frame_view_registered_output && !record.frame_view_output_name.empty();
    const std::string output_name = record.frame_view_output_name;
    record.frame_view_frame_connection.disconnect();
    record.frame_view_output_registered_connection.disconnect();
    record.frame_view_output_unregistered_connection.disconnect();
    clear_frame_view_image(record);
    record.frame_view_registered_output = false;
    record.frame_view_output_name.clear();
    record.frame_view_buffer.clear();
    record.frame_view_props = {};

    if (should_unregister) {
        auto result = DisplayService::get_instance().unregister_output(output_name);
        if (!result) {
            BROOKESIA_LOGW("Failed to unregister FrameView output '%1%': %2%", output_name, result.error());
        }
    }
}

void refresh_frame_view(BackendImpl &impl, Record &record, FrameViewProps props)
{
    using DisplayService = esp_brookesia::service::Display;

    if (record.type != NodeType::FrameView || record.object == nullptr || !lv_obj_is_valid(record.object)) {
        return;
    }

    const auto output_name = resolve_frame_view_output_name(record, props);
    if (!props.auto_register_output && output_name.empty()) {
        BROOKESIA_LOGE("FrameView outputName is required when autoRegisterOutput is false: node='%1%'",
                       record.absolute_path);
        release_frame_view(record);
        return;
    }

    lv_obj_update_layout(record.object);
    const int32_t width = props.auto_register_output ? lv_obj_get_width(record.object) : record.frame_view_width;
    const int32_t height = props.auto_register_output ? lv_obj_get_height(record.object) : record.frame_view_height;
    if (props.auto_register_output && frame_view_matches(record, props, output_name, width, height)) {
        return;
    }
    if (!props.auto_register_output && record.frame_view_ready &&
            frame_view_props_equal(record.frame_view_props, props) &&
            record.frame_view_output_name == output_name) {
        return;
    }

    release_frame_view(record);
    record.frame_view_props = props;
    record.frame_view_output_name = output_name;

    if (props.auto_register_output) {
        if (width <= 0 || height <= 0) {
            BROOKESIA_LOGD("Deferring FrameView output registration until size is resolved: node='%1%'",
                           record.absolute_path);
            return;
        }

        const auto pixel_format = to_display_pixel_format(props.color_format);
        const size_t bpp = frame_view_bytes_per_pixel(props.color_format);
        if (!pixel_format.has_value() || bpp == 0) {
            BROOKESIA_LOGE("Unsupported FrameView color format: node='%1%', color_format=%2%",
                           record.absolute_path, BROOKESIA_DESCRIBE_ENUM_TO_STR(props.color_format));
            return;
        }

        const size_t stride_bytes = static_cast<size_t>(width) * bpp;
        const uint64_t buffer_size = static_cast<uint64_t>(stride_bytes) * static_cast<uint64_t>(height);
        if (buffer_size > std::numeric_limits<size_t>::max()) {
            BROOKESIA_LOGE("FrameView output buffer size overflow: node='%1%'", record.absolute_path);
            return;
        }
        record.frame_view_buffer.assign(static_cast<size_t>(buffer_size), 0);
        auto register_result = DisplayService::get_instance().register_output(DisplayService::BufferOutputConfig{
            .name = output_name,
            .width = static_cast<uint32_t>(width),
            .height = static_cast<uint32_t>(height),
            .pixel_format = *pixel_format,
            .buffer = esp_brookesia::service::RawBuffer(
                record.frame_view_buffer.data(),
                record.frame_view_buffer.size()
            ),
            .stride_bytes = stride_bytes,
        });
        if (!register_result) {
            BROOKESIA_LOGE("Failed to register FrameView output '%1%': %2%", output_name, register_result.error());
            record.frame_view_buffer.clear();
            return;
        }
        record.frame_view_registered_output = true;

        auto output = DisplayService::get_instance().get_buffer_output(output_name);
        if (!output) {
            BROOKESIA_LOGE("Failed to query registered FrameView output '%1%': %2%", output_name, output.error());
            return;
        }
        const auto descriptor_format = to_frame_color_format(output->info.pixel_format);
        if (!descriptor_format.has_value()) {
            BROOKESIA_LOGE("Registered FrameView output uses unsupported pixel format: output='%1%'", output_name);
            return;
        }
        if (set_frame_view_image(record, *output, *descriptor_format)) {
            connect_frame_view_frame_signal(impl, record);
        }
        return;
    }

    connect_external_frame_view_lifecycle(impl, record);
    auto output = DisplayService::get_instance().get_buffer_output(output_name);
    if (!output) {
        BROOKESIA_LOGW("FrameView external output is not available yet: output='%1%', error='%2%'",
                       output_name, output.error());
        return;
    }

    const auto expected_pixel_format = to_display_pixel_format(props.color_format);
    if (!expected_pixel_format.has_value()) {
        BROOKESIA_LOGE("Unsupported FrameView color format: node='%1%', color_format=%2%",
                       record.absolute_path, BROOKESIA_DESCRIBE_ENUM_TO_STR(props.color_format));
        return;
    }
    if (output->info.pixel_format != *expected_pixel_format) {
        BROOKESIA_LOGE(
            "FrameView external output format mismatch: node='%1%', output='%2%', expected=%3%, actual=%4%",
            record.absolute_path,
            output_name,
            BROOKESIA_DESCRIBE_ENUM_TO_STR(*expected_pixel_format),
            BROOKESIA_DESCRIBE_ENUM_TO_STR(output->info.pixel_format)
        );
        return;
    }
    if (set_frame_view_image(record, *output, props.color_format)) {
        connect_frame_view_frame_signal(impl, record);
    }
}

void apply_props(BackendImpl &impl, Record &record, const Node &node, PropsApplyMask mask)
{
    BROOKESIA_LOG_TRACE_GUARD();

    // BROOKESIA_LOGD("Params: record(%1%), node(%2%), mask(%3%)", record, node, static_cast<uint64_t>(mask));

    if (mask == PropsApplyMask::All) {
        apply_common_flags(record, node.common_props);
    } else {
        if (has_mask(mask, PropsApplyMask::CommonHidden)) {
            apply_common_hidden(record, node.common_props);
        }
        if (has_mask(mask, PropsApplyMask::CommonDisabled)) {
            apply_common_disabled(record, node.common_props);
        }
        if (has_mask(mask, PropsApplyMask::CommonClickable)) {
            apply_common_clickable(record, node.common_props);
        }
        if (has_mask(mask, PropsApplyMask::CommonScrollable)) {
            apply_common_scrollable(record, node.common_props);
        }
        if (has_mask(mask, PropsApplyMask::CommonPressLock)) {
            apply_common_press_lock(record, node.common_props);
        }
        if (has_mask(mask, PropsApplyMask::CommonTransform)) {
            apply_common_transform(record, node.common_props);
        }
    }

    switch (record.type) {
    case NodeType::Label:
        if (has_mask(mask, PropsApplyMask::LabelText)) {
            lv_label_set_text(record.object, node.label_props.text.c_str());
        }
        break;
    case NodeType::Image:
        if (has_mask(mask, PropsApplyMask::ImageSource)) {
            apply_image_source(impl, record, node);
        }
        if (has_mask(mask, PropsApplyMask::ImageInnerAlign)) {
            lv_image_set_inner_align(record.object, to_image_inner_align(node.image_props.inner_align));
        }
        if (has_mask(mask, PropsApplyMask::ImageRecolor)) {
            const auto recolor = parse_color(node.image_props.recolor);
            if (recolor.has_value()) {
                lv_obj_set_style_image_recolor(record.object, lv_color_hex(*recolor), LV_PART_MAIN);
                lv_obj_set_style_image_recolor_opa(
                    record.object,
                    static_cast<lv_opa_t>(node.image_props.recolor_opacity),
                    LV_PART_MAIN
                );
            } else {
                lv_obj_remove_local_style_prop(record.object, LV_STYLE_IMAGE_RECOLOR, LV_PART_MAIN);
                lv_obj_remove_local_style_prop(record.object, LV_STYLE_IMAGE_RECOLOR_OPA, LV_PART_MAIN);
            }
        }
        if (has_mask(mask, PropsApplyMask::ImageAngle)) {
            lv_image_set_rotation(record.object, node.image_props.angle * 10);
        }
        if (has_mask(mask, PropsApplyMask::ImageOffsetX)) {
            lv_image_set_offset_x(record.object, node.image_props.offset_x);
        }
        if (has_mask(mask, PropsApplyMask::ImageOffsetY)) {
            lv_image_set_offset_y(record.object, node.image_props.offset_y);
        }
        if (has_mask(mask, PropsApplyMask::ImageZoom)) {
            lv_image_set_scale(
                record.object,
                static_cast<uint32_t>(std::max<int32_t>(1, node.image_props.zoom))
            );
        }
        if (has_mask(mask, PropsApplyMask::ImagePivot)) {
            lv_image_set_pivot(
                record.object,
                resolve_pivot_value(node.image_props.pivot_x, lv_obj_get_width(record.object)),
                resolve_pivot_value(node.image_props.pivot_y, lv_obj_get_height(record.object))
            );
        }
        break;
    case NodeType::FrameView:
        if (has_mask(mask, PropsApplyMask::FrameViewConfig)) {
            refresh_frame_view(impl, record, node.frame_view_props);
        }
        break;
    case NodeType::TextInput:
        if (has_mask(mask, PropsApplyMask::TextInputText)) {
            lv_textarea_set_text(record.object, node.text_input_props.text.c_str());
            lv_textarea_set_cursor_pos(record.object, LV_TEXTAREA_CURSOR_LAST);
        }
        if (has_mask(mask, PropsApplyMask::TextInputPlaceholder)) {
            lv_textarea_set_placeholder_text(record.object, node.text_input_props.placeholder.c_str());
        }
        if (has_mask(mask, PropsApplyMask::TextInputPassword)) {
            lv_textarea_set_password_mode(record.object, node.text_input_props.password);
        }
        if (has_mask(mask, PropsApplyMask::TextInputMultiline)) {
            lv_textarea_set_one_line(record.object, !node.text_input_props.multiline);
        }
        if (has_mask(mask, PropsApplyMask::TextInputMaxLength) && node.text_input_props.max_length > 0) {
            lv_textarea_set_max_length(record.object, static_cast<uint32_t>(node.text_input_props.max_length));
        }
        refresh_text_input_inner_layout(record);
        break;
    case NodeType::Slider:
        if (has_mask(mask, PropsApplyMask::RangeRange)) {
            lv_slider_set_range(record.object, node.range_props.min, node.range_props.max);
        }
        if (has_mask(mask, PropsApplyMask::RangeValue)) {
            lv_slider_set_value(record.object, node.range_props.value, LV_ANIM_OFF);
        }
        break;
    case NodeType::Switch:
        if (has_mask(mask, PropsApplyMask::ToggleChecked)) {
            apply_toggle_state(record.object, node.toggle_props.checked);
        }
        break;
    case NodeType::Checkbox:
        if (has_mask(mask, PropsApplyMask::LabelText)) {
            lv_checkbox_set_text(record.object, node.label_props.text.c_str());
        }
        if (has_mask(mask, PropsApplyMask::ToggleChecked)) {
            apply_toggle_state(record.object, node.toggle_props.checked);
        }
        break;
    case NodeType::Dropdown: {
        if (has_mask(mask, PropsApplyMask::DropdownOptions)) {
            const auto options = join_options(node.dropdown_props.options);
            lv_dropdown_set_options(record.object, options.c_str());
        }
        if (has_mask(mask, PropsApplyMask::DropdownSelectedIndex)) {
            lv_dropdown_set_selected(
                record.object, static_cast<uint32_t>(std::max<int32_t>(0, node.dropdown_props.selected_index))
            );
        }
        break;
    }
    case NodeType::ProgressBar:
        if (has_mask(mask, PropsApplyMask::RangeRange)) {
            lv_bar_set_range(record.object, node.range_props.min, node.range_props.max);
        }
        if (has_mask(mask, PropsApplyMask::RangeValue)) {
            lv_bar_set_value(record.object, node.range_props.value, LV_ANIM_OFF);
        }
        break;
    case NodeType::Arc:
        if (has_mask(mask, PropsApplyMask::RangeRange)) {
            lv_arc_set_range(record.object, node.range_props.min, node.range_props.max);
        }
        if (has_mask(mask, PropsApplyMask::RangeValue)) {
            lv_arc_set_value(record.object, node.range_props.value);
        }
        break;
    case NodeType::Line:
        if (!has_mask(mask, PropsApplyMask::LinePoints)) {
            break;
        }
        record.line_points.clear();
        record.line_points.reserve(node.line_props.points.size());
        for (const auto &point : node.line_props.points) {
            record.line_points.push_back(lv_point_precise_t{
                .x = static_cast<lv_value_precise_t>(point.x),
                .y = static_cast<lv_value_precise_t>(point.y),
            });
        }
        if (!record.line_points.empty()) {
            lv_line_set_points(record.object, record.line_points.data(), record.line_points.size());
        }
        break;
    case NodeType::Table:
        if (has_mask(mask, PropsApplyMask::TableRows) && node.table_props.rows > 0) {
            lv_table_set_row_count(record.object, static_cast<uint32_t>(node.table_props.rows));
        }
        if (has_mask(mask, PropsApplyMask::TableColumns) && node.table_props.columns > 0) {
            lv_table_set_column_count(record.object, static_cast<uint32_t>(node.table_props.columns));
        }
        if (has_mask(mask, PropsApplyMask::TableCells)) {
            for (const auto &cell : node.table_props.cells) {
                lv_table_set_cell_value(
                    record.object,
                    static_cast<uint32_t>(std::max<int32_t>(0, cell.row)),
                    static_cast<uint32_t>(std::max<int32_t>(0, cell.column)),
                    cell.text.c_str()
                );
            }
        }
        break;
    case NodeType::Keyboard:
        if (has_mask(mask, PropsApplyMask::KeyboardConfig)) {
            apply_keyboard_layouts(impl, record, node.keyboard_props);
        }
        if (has_mask(mask, PropsApplyMask::KeyboardMode) || has_mask(mask, PropsApplyMask::KeyboardConfig)) {
            set_keyboard_mode_if_possible(record, node.keyboard_props.mode);
        }
        if (has_mask(mask, PropsApplyMask::KeyboardPopovers)) {
            lv_keyboard_set_popovers(record.object, node.keyboard_props.popovers);
            auto *parent = lv_obj_get_parent(record.object);
            if (parent != nullptr && lv_obj_is_valid(parent)) {
                if (node.keyboard_props.popovers) {
                    lv_obj_add_flag(parent, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
                } else {
                    lv_obj_remove_flag(parent, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
                }
            }
        }
        if (has_mask(mask, PropsApplyMask::KeyboardConfig)) {
            apply_keyboard_target(impl, record, node.keyboard_props);
        }
        break;
    case NodeType::Canvas:
        if (has_mask(mask, PropsApplyMask::CanvasCommands)) {
            apply_canvas(record, node);
        }
        break;
    case NodeType::Button:
    case NodeType::Container:
    case NodeType::Screen:
    case NodeType::Spinner:
    case NodeType::Max:
    default:
        break;
    }
}

void apply_image_sizing(Record &record, const Placement &placement)
{
    if (record.type != NodeType::Image || record.object == nullptr ||
            record.image_width <= 0 || record.image_height <= 0) {
        return;
    }

    const bool fixed_width = placement.width.mode == SizeMode::Fixed && placement.width.value > 0;
    const bool fixed_height = placement.height.mode == SizeMode::Fixed && placement.height.value > 0;
    const bool explicit_width = placement.width.mode != SizeMode::Wrap;
    const bool explicit_height = placement.height.mode != SizeMode::Wrap;
    lv_image_set_antialias(record.object, true);

    if (explicit_width && explicit_height) {
        if (placement.aspect_ratio.has_value()) {
            return;
        }
        if (!fixed_width || !fixed_height) {
            return;
        }
        lv_obj_set_size(record.object, placement.width.value, placement.height.value);
    } else if (fixed_width && !explicit_height) {
        const uint32_t scale = calculate_scale(placement.width.value, record.image_width);
        lv_obj_set_size(record.object, placement.width.value, apply_scale(record.image_height, scale));
    } else if (fixed_height && !explicit_width) {
        const uint32_t scale = calculate_scale(placement.height.value, record.image_height);
        lv_obj_set_size(record.object, apply_scale(record.image_width, scale), placement.height.value);
    } else if (!explicit_width && !explicit_height) {
        lv_obj_set_size(record.object, record.image_width, record.image_height);
        lv_image_set_scale(record.object, LV_SCALE_NONE);
    }
}

void refresh_text_input_inner_layout(Record &record)
{
    if (record.type != NodeType::TextInput || record.object == nullptr || !lv_obj_is_valid(record.object)) {
        return;
    }
    if (!lv_textarea_get_one_line(record.object)) {
        return;
    }

    auto *label = lv_textarea_get_label(record.object);
    if (label == nullptr || !lv_obj_is_valid(label)) {
        return;
    }

    lv_obj_update_layout(record.object);
    lv_obj_update_layout(label);
    lv_obj_set_style_align(label, LV_ALIGN_LEFT_MID, LV_PART_MAIN);
    lv_obj_set_style_pad_top(label, 0, LV_PART_TEXTAREA_PLACEHOLDER);
    lv_obj_set_style_pad_bottom(label, 0, LV_PART_TEXTAREA_PLACEHOLDER);
    lv_obj_set_style_bg_color(
        record.object,
        lv_obj_get_style_text_color(record.object, LV_PART_MAIN),
        LV_PART_CURSOR
    );
    lv_obj_set_style_bg_opa(record.object, LV_OPA_TRANSP, LV_PART_CURSOR);
    lv_obj_set_style_border_color(
        record.object,
        lv_obj_get_style_text_color(record.object, LV_PART_MAIN),
        LV_PART_CURSOR
    );
    lv_obj_set_style_border_opa(record.object, LV_OPA_COVER, LV_PART_CURSOR);
    lv_obj_set_style_border_width(record.object, 1, LV_PART_CURSOR);
    lv_obj_set_style_border_side(record.object, LV_BORDER_SIDE_LEFT, LV_PART_CURSOR);
    lv_obj_set_style_anim_duration(record.object, 450, LV_PART_CURSOR);
    lv_obj_set_style_pad_left(record.object, -1, LV_PART_CURSOR);
    lv_obj_set_style_pad_right(record.object, 0, LV_PART_CURSOR);
    lv_obj_set_style_pad_top(record.object, 0, LV_PART_CURSOR);
    lv_obj_set_style_pad_bottom(record.object, 0, LV_PART_CURSOR);
    lv_textarea_set_cursor_pos(record.object, lv_textarea_get_cursor_pos(record.object));
    lv_obj_send_event(record.object, LV_EVENT_FOCUSED, nullptr);
    lv_obj_invalidate(record.object);
}

} // namespace esp_brookesia::gui::lvgl
