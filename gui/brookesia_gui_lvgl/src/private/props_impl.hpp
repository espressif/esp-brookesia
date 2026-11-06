#pragma once

/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>

#include "private/types.hpp"
#include "port/private/threading.hpp"
#include "brookesia/gui_lvgl/macro_configs.h"
#if !BROOKESIA_GUI_LVGL_PROPS_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#if defined(__EMSCRIPTEN__)
#include "brookesia/gui_interface/wasm/gui_task_queue.hpp"
#endif
#include "brookesia/gui_interface/asset_source.hpp"
#include "brookesia/service_helper/system/storage.hpp"
#include "private/utils.hpp"

namespace esp_brookesia::gui::lvgl {

namespace props_detail {

using StorageHelper = service::helper::Storage;

// Preload stores an image under this key. A document read scope appends its token so two
// documents can keep different bytes for one path. Draw callbacks run after the scope ends,
// so they must use a source captured while the scope was still active.
inline std::string image_cache_key(const std::string &path)
{
    const std::string token = esp_brookesia::gui::bundled_asset_read_token();
    if (token.empty()) {
        return path;
    }
    return path + '\n' + token;
}

bool is_lvgl_bin_path(std::string_view path);
bool is_png_path(std::string_view path);
bool is_jpeg_path(std::string_view path);

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

uint32_t calculate_scale(int32_t target, int32_t source)
;

int32_t apply_scale(int32_t source, uint32_t scale)
;

std::string join_options(const std::vector<std::string> &options)
;

lv_keyboard_mode_t to_keyboard_mode(std::string_view mode)
;

std::optional<size_t> keyboard_layout_index(std::string_view mode)
;

std::optional<size_t> keyboard_layout_index_from_lv_mode(lv_keyboard_mode_t mode)
;

Record::KeyboardPayload &ensure_keyboard_payload(Record &record)
;

bool keyboard_mode_allowed(const Record &record, std::string_view mode)
;

bool has_keyboard_layout(const Record &record, std::string_view mode)
;

std::string view_reference_path_to_absolute(
    std::string_view scope_root_absolute_path,
    std::string_view target
)
;

std::string keyboard_key_label(const KeyboardKey &key)
;

lv_buttonmatrix_ctrl_t keyboard_key_ctrl(const KeyboardKey &key, bool popovers)
;

Record *keyboard_record_from_event(lv_event_t *event)
;

void set_keyboard_mode_if_possible(Record &record, std::string_view mode)
;

std::string keyboard_key_style_class(const Record &record, const KeyboardKey &key)
;

std::optional<KeyboardKey> keyboard_key_from_selected_button(Record &record, uint32_t selected_button)
;

void keyboard_value_changed_event_cb(lv_event_t *event)
;

const KeyboardKeyStyle *find_keyboard_key_style(const Record &record, const std::string &style_class)
;

std::optional<lv_color_t> parse_keyboard_style_color(std::string_view color)
;

void apply_keyboard_part_style(lv_obj_t *object, const KeyboardKeyStyle &style, lv_state_t state)
;

void apply_keyboard_default_item_styles(Record &record)
;

const void *keyboard_key_image_src(
    BackendImpl &impl,
    const KeyboardKey &key,
    const std::shared_ptr<BinaryImageSource> &resolved)
;

bool draw_keyboard_key_image(
    BackendImpl &impl,
    Record &record,
    const KeyboardKey &key,
    std::optional<lv_color_t> recolor,
    lv_draw_dsc_base_t &base_dsc,
    lv_draw_task_t *draw_task)
;

void keyboard_draw_event_cb(lv_event_t *event)
;

void apply_keyboard_layouts(BackendImpl &impl, Record &record, const KeyboardProps &props)
;

void apply_keyboard_target(BackendImpl &impl, Record &record, const KeyboardProps &props)
;

lv_image_align_t to_image_inner_align(std::string_view align)
;

std::string resolve_frame_view_output_name(const Record &record, const FrameViewProps &props)
;

std::optional<esp_brookesia::service::Display::PixelFormat> to_display_pixel_format(FrameColorFormat format)
;

std::optional<FrameColorFormat> to_frame_color_format(
    esp_brookesia::service::Display::PixelFormat pixel_format)
;

std::optional<lv_color_format_t> to_lvgl_color_format(FrameColorFormat format)
;

size_t frame_view_bytes_per_pixel(FrameColorFormat format)
;

bool frame_view_props_equal(const FrameViewProps &lhs, const FrameViewProps &rhs)
;

Record::FrameViewPayload &ensure_frame_view_payload(Record &record)
;

bool frame_view_matches(
    const Record::FrameViewPayload &frame_view,
    const FrameViewProps &props,
    std::string_view output_name,
    int32_t width,
    int32_t height)
;

bool copy_frame_view_buffer_to_shadow(
    Record::FrameViewPayload &frame_view,
    const esp_brookesia::service::Display::BufferOutputView &output,
    std::optional<esp_brookesia::service::Display::FrameInfo> frame = std::nullopt
)
;

void clear_frame_view_image(Record &record, Record::FrameViewPayload &frame_view)
;

bool set_frame_view_image(
    BackendImpl &impl,
    Record &record,
    Record::FrameViewPayload &frame_view,
    const esp_brookesia::service::Display::BufferOutputView &output,
    FrameColorFormat descriptor_format)
;

void refresh_frame_view_shadow(
    BackendImpl &impl,
    BackendHandle handle,
    std::string_view output_name,
    const esp_brookesia::service::Display::FrameInfo &frame
)
;

void connect_frame_view_frame_signal(BackendImpl &impl, Record &record)
;

void connect_external_frame_view_lifecycle(BackendImpl &impl, Record &record)
;

void apply_common_disabled(Record &record, const CommonProps &props)
;

void apply_common_clickable(Record &record, const CommonProps &props)
;

void apply_common_scrollable(Record &record, const CommonProps &props)
;

void apply_common_press_lock(Record &record, const CommonProps &props)
;

void apply_common_hidden(Record &record, const CommonProps &props)
;

int32_t resolve_pivot_value(const PivotValue &pivot, int32_t size)
;

void apply_common_transform(Record &record, const CommonProps &props)
;

void apply_common_flags(Record &record, const CommonProps &props)
;

void apply_toggle_state(lv_obj_t *object, bool checked)
;

void apply_canvas(Record &record, const Node &node)
;

uint16_t read_le16(const std::vector<uint8_t> &data, size_t offset)
;

uint32_t read_be32(const uint8_t *data)
;

bool is_lvgl_bin_path(std::string_view path)
;

bool is_png_path(std::string_view path)
;

bool is_jpeg_path(std::string_view path)
;

bool is_jpeg_decoder_available()
;

std::expected<lv_image_header_t, std::string> make_png_image_header(
    const uint8_t *data, size_t data_size, const std::string &path
)
;

std::expected<lv_image_header_t, std::string> read_png_image_header(const std::string &path)
;

bool is_jpeg_start_of_frame_marker(uint8_t marker)
;

std::expected<lv_image_header_t, std::string> make_jpeg_image_header(
    const uint8_t *data, size_t data_size, const std::string &path
)
;

std::expected<std::vector<uint8_t>, std::string> read_storage_file_bytes(
    const std::string &path, std::string_view format_name
)
;

std::expected<std::shared_ptr<BinaryImageSource>, std::string> load_binary_image_source(const std::string &path)
;

std::expected<std::shared_ptr<BinaryImageSource>, std::string> load_encoded_png_image_source(
    const std::string &path)
;

std::expected<std::shared_ptr<BinaryImageSource>, std::string> load_encoded_jpeg_image_source(
    const std::string &path)
;


} // namespace props_detail

} // namespace esp_brookesia::gui::lvgl
