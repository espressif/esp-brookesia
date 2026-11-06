/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string_view>

#include "private/props_impl.hpp"
// lv_image_cache_drop() lives in an instance header that lvgl.h does not aggregate.
#if __has_include("lvgl/src/misc/cache/instance/lv_image_cache.h")
#   include "lvgl/src/misc/cache/instance/lv_image_cache.h"
#elif __has_include("src/misc/cache/instance/lv_image_cache.h")
#   include "src/misc/cache/instance/lv_image_cache.h"
#endif

namespace esp_brookesia::gui::lvgl {
using namespace props_detail;

std::expected<std::shared_ptr<BinaryImageSource>, std::string> load_image_source(std::string_view path)
{
    std::string path_string(path);
    if (is_lvgl_bin_path(path_string)) {
        return load_binary_image_source(path_string);
    }
    if (is_png_path(path_string)) {
        return load_encoded_png_image_source(path_string);
    }
    if (is_jpeg_path(path_string)) {
        return load_encoded_jpeg_image_source(path_string);
    }
    return std::unexpected("Unsupported image source format: " + path_string);
}

std::expected<RuntimeImageResource, std::string> resolve_image_resource(RuntimeImageResource resource)
{
    if (resource.native_src != 0 || (resource.width > 0 && resource.height > 0)) {
        return resource;
    }

    if (is_lvgl_bin_path(resource.primary_src)) {
        auto source = load_binary_image_source(resource.primary_src);
        if (!source) {
            return std::unexpected(source.error());
        }

        resource.width = static_cast<int32_t>((*source)->descriptor.header.w);
        resource.height = static_cast<int32_t>((*source)->descriptor.header.h);
        return resource;
    }

    if (is_png_path(resource.primary_src)) {
        auto header = read_png_image_header(resource.primary_src);
        if (!header) {
            return std::unexpected(header.error());
        }
        resource.width = header->w;
        resource.height = header->h;
        return resource;
    }

    if (is_jpeg_path(resource.primary_src)) {
        auto source = load_encoded_jpeg_image_source(resource.primary_src);
        if (!source) {
            return std::unexpected(source.error());
        }
        resource.width = static_cast<int32_t>((*source)->descriptor.header.w);
        resource.height = static_cast<int32_t>((*source)->descriptor.header.h);
        return resource;
    }

    return std::unexpected("Unsupported image metadata format: " + resource.primary_src);
}

bool requires_preloaded_image_resource(const RuntimeImageResource &resource)
{
    return resource.native_src == 0 &&
           (is_lvgl_bin_path(resource.primary_src) || is_png_path(resource.primary_src) ||
            is_jpeg_path(resource.primary_src));
}

std::expected<void, std::string> preload_image_resource(BackendImpl &impl, const RuntimeImageResource &resource)
{
    if (resource.native_src != 0) {
        return {};
    }

    if (is_lvgl_bin_path(resource.primary_src)) {
        auto cache_it = impl.binary_image_cache.find(image_cache_key(resource.primary_src));
        if (cache_it != impl.binary_image_cache.end()) {
            ++cache_it->second.ref_count;
            return {};
        }

        auto source = load_binary_image_source(resource.primary_src);
        if (!source) {
            return std::unexpected(source.error());
        }

        impl.binary_image_cache.emplace(image_cache_key(resource.primary_src), BinaryImageCacheEntry{
            .source = *source,
            .ref_count = 1,
        });
        BROOKESIA_LOGD("Preloaded LVGL image bin: path='%1%'", resource.primary_src);
        return {};
    }

    if (is_png_path(resource.primary_src)) {
        auto cache_it = impl.decoded_image_cache.find(image_cache_key(resource.primary_src));
        if (cache_it != impl.decoded_image_cache.end()) {
            ++cache_it->second.ref_count;
            return {};
        }

        auto source = load_encoded_png_image_source(resource.primary_src);
        if (!source) {
            return std::unexpected(source.error());
        }

        impl.decoded_image_cache.emplace(image_cache_key(resource.primary_src), BinaryImageCacheEntry{
            .source = *source,
            .ref_count = 1,
        });
        BROOKESIA_LOGD("Preloaded PNG image source: path='%1%'", resource.primary_src);
        return {};
    }

    if (is_jpeg_path(resource.primary_src)) {
        auto cache_it = impl.decoded_image_cache.find(image_cache_key(resource.primary_src));
        if (cache_it != impl.decoded_image_cache.end()) {
            ++cache_it->second.ref_count;
            return {};
        }

        auto source = load_encoded_jpeg_image_source(resource.primary_src);
        if (!source) {
            return std::unexpected(source.error());
        }

        impl.decoded_image_cache.emplace(image_cache_key(resource.primary_src), BinaryImageCacheEntry{
            .source = *source,
            .ref_count = 1,
        });
        BROOKESIA_LOGD("Preloaded JPEG image source: path='%1%'", resource.primary_src);
        return {};
    }

    return std::unexpected("Unsupported image preload format: " + resource.primary_src);
}

void release_image_resource(BackendImpl &impl, const RuntimeImageResource &resource)
{
    if (resource.native_src != 0) {
        return;
    }

    if (is_lvgl_bin_path(resource.primary_src)) {
        auto cache_it = impl.binary_image_cache.find(image_cache_key(resource.primary_src));
        if (cache_it == impl.binary_image_cache.end()) {
            return;
        }
        if (cache_it->second.ref_count > 1) {
            --cache_it->second.ref_count;
            return;
        }
        // Invalidate LVGL's internal image/header cache (keyed by the descriptor
        // pointer for LV_IMAGE_SRC_VARIABLE) before freeing the descriptor, so a
        // later allocation reusing this address cannot hit a stale decoded image.
        lv_image_cache_drop(&cache_it->second.source->descriptor);
        impl.binary_image_cache.erase(cache_it);
        BROOKESIA_LOGD("Released LVGL image bin: path='%1%'", resource.primary_src);
        return;
    }

    if (is_png_path(resource.primary_src)) {
        auto cache_it = impl.decoded_image_cache.find(image_cache_key(resource.primary_src));
        if (cache_it == impl.decoded_image_cache.end()) {
            return;
        }
        if (cache_it->second.ref_count > 1) {
            --cache_it->second.ref_count;
            return;
        }
        // Invalidate LVGL's internal image/header cache (keyed by the descriptor
        // pointer for LV_IMAGE_SRC_VARIABLE) before freeing the descriptor, so a
        // later allocation reusing this address cannot hit a stale decoded image.
        lv_image_cache_drop(&cache_it->second.source->descriptor);
        impl.decoded_image_cache.erase(cache_it);
        BROOKESIA_LOGD("Released PNG image source: path='%1%'", resource.primary_src);
        return;
    }

    if (is_jpeg_path(resource.primary_src)) {
        auto cache_it = impl.decoded_image_cache.find(image_cache_key(resource.primary_src));
        if (cache_it == impl.decoded_image_cache.end()) {
            return;
        }
        if (cache_it->second.ref_count > 1) {
            --cache_it->second.ref_count;
            return;
        }
        // Invalidate LVGL's internal image/header cache (keyed by the descriptor
        // pointer for LV_IMAGE_SRC_VARIABLE) before freeing the descriptor, so a
        // later allocation reusing this address cannot hit a stale decoded image.
        lv_image_cache_drop(&cache_it->second.source->descriptor);
        impl.decoded_image_cache.erase(cache_it);
        BROOKESIA_LOGD("Released JPEG image source: path='%1%'", resource.primary_src);
    }
}

static void apply_image_source(BackendImpl &impl, Record &record, const Node &node)
{
    if (node.image_props.src.empty()) {
        return;
    }

    auto &image = record.ensure_type_payload<Record::ImagePayload>();

    const auto next_image_src =
        node.resolved_image.primary_src.empty() ? node.image_props.src : node.resolved_image.primary_src;
    const auto next_image_native_src = node.resolved_image.native_src;
    const bool source_changed =
        image.src != next_image_src || image.native_src != next_image_native_src;
    if (source_changed) {
        lv_image_set_src(record.object, nullptr);
    }
    image.native_src = next_image_native_src;
    if (image.native_src == 0) {
        if (is_lvgl_bin_path(next_image_src)) {
            auto cache_it = impl.binary_image_cache.find(image_cache_key(next_image_src));
            if (cache_it != impl.binary_image_cache.end()) {
                image.binary_src = cache_it->second.source;
            } else if (source_changed || image.binary_src == nullptr) {
                image.binary_src.reset();
                BROOKESIA_LOGW("LVGL image bin was not preloaded: path='%1%'", next_image_src);
            }
        } else {
            auto cache_it = impl.decoded_image_cache.find(image_cache_key(next_image_src));
            if (cache_it != impl.decoded_image_cache.end()) {
                image.binary_src = cache_it->second.source;
            } else {
                image.binary_src.reset();
            }
        }
        if (image.binary_src == nullptr && source_changed) {
            BROOKESIA_LOGW("Image source is not preloaded: path='%1%'", next_image_src);
        }
        image.src = next_image_src;
    } else {
        image.src = next_image_src;
        image.binary_src.reset();
    }
    image.width = node.resolved_image.width;
    image.height = node.resolved_image.height;
    const void *image_src = nullptr;
    if (image.native_src != 0) {
        image_src = reinterpret_cast<const void *>(image.native_src);
    } else if (image.binary_src != nullptr) {
        image_src = static_cast<const void *>(&image.binary_src->descriptor);
    }
    BROOKESIA_LOGD(
        "Applying image source: node='%1%', requested_src='%2%', resolved_src='%3%', native_src=%4%, width=%5%, height=%6%",
        impl.build_absolute_path(record),
        node.image_props.src,
        image.src,
        image.native_src,
        image.width,
        image.height
    );
    lv_image_set_src(record.object, image_src);
    const auto &placement = node.placement;
    const bool match_sized_flow_image = placement.mode == PlacementMode::Flow &&
                                        placement.width.mode == SizeMode::Match &&
                                        placement.height.mode == SizeMode::Match;
    if (match_sized_flow_image) {
        if (auto *parent = lv_obj_get_parent(record.object); parent != nullptr && lv_obj_is_valid(parent)) {
            // A flow image can keep the position computed while its parent still
            // had the placeholder size.  Flush the parent layout and restore the
            // match-sized child to the parent's content origin before drawing it.
            lv_obj_mark_layout_as_dirty(parent);
            lv_obj_update_layout(parent);
            lv_obj_move_to(record.object, 0, 0);
        }
    }
    apply_image_sizing(record, node.placement);
}

void release_frame_view(Record &record)
{
    using DisplayService = esp_brookesia::service::Display;

    auto *frame_view_payload = record.get_type_payload<Record::FrameViewPayload>();
    if (record.type != NodeType::FrameView || frame_view_payload == nullptr) {
        return;
    }

    auto &frame_view = *frame_view_payload;
    const bool should_unregister = frame_view.registered_output && !frame_view.output_name.empty();
    const std::string output_name = frame_view.output_name;
    frame_view.frame_connection.disconnect();
    frame_view.output_registered_connection.disconnect();
    frame_view.output_unregistered_connection.disconnect();
    clear_frame_view_image(record, frame_view);
    frame_view.registered_output = false;
    frame_view.output_name.clear();
    frame_view.buffer.clear();
    frame_view.props = {};

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
                       impl.build_absolute_path(record));
        release_frame_view(record);
        return;
    }

    auto &frame_view = ensure_frame_view_payload(record);
    lv_obj_update_layout(record.object);
    const int32_t width = props.auto_register_output ? lv_obj_get_width(record.object) : frame_view.width;
    const int32_t height = props.auto_register_output ? lv_obj_get_height(record.object) : frame_view.height;
    if (props.auto_register_output && frame_view_matches(frame_view, props, output_name, width, height)) {
        return;
    }
    if (!props.auto_register_output && frame_view.ready &&
            frame_view_props_equal(frame_view.props, props) &&
            frame_view.output_name == output_name) {
        return;
    }

    release_frame_view(record);
    frame_view.props = props;
    frame_view.output_name = output_name;

    if (props.auto_register_output) {
        if (width <= 0 || height <= 0) {
            BROOKESIA_LOGD("Deferring FrameView output registration until size is resolved: node='%1%'",
                           impl.build_absolute_path(record));
            return;
        }

        const auto pixel_format = to_display_pixel_format(props.color_format);
        const size_t bpp = frame_view_bytes_per_pixel(props.color_format);
        if (!pixel_format.has_value() || bpp == 0) {
            BROOKESIA_LOGE("Unsupported FrameView color format: node='%1%', color_format=%2%",
                           impl.build_absolute_path(record), BROOKESIA_DESCRIBE_ENUM_TO_STR(props.color_format));
            return;
        }

        const size_t stride_bytes = static_cast<size_t>(width) * bpp;
        const uint64_t buffer_size = static_cast<uint64_t>(stride_bytes) * static_cast<uint64_t>(height);
        if (buffer_size > std::numeric_limits<size_t>::max()) {
            BROOKESIA_LOGE(
                "FrameView output buffer size overflow: node='%1%'",
                impl.build_absolute_path(record)
            );
            return;
        }
        frame_view.buffer.assign(static_cast<size_t>(buffer_size), 0);
        auto register_result = DisplayService::get_instance().register_output(DisplayService::BufferOutputConfig{
            .name = output_name,
            .width = static_cast<uint32_t>(width),
            .height = static_cast<uint32_t>(height),
            .pixel_format = *pixel_format,
            .buffer = esp_brookesia::service::RawBuffer(
                frame_view.buffer.data(),
                frame_view.buffer.size()
            ),
            .stride_bytes = stride_bytes,
        });
        if (!register_result) {
            BROOKESIA_LOGE("Failed to register FrameView output '%1%': %2%", output_name, register_result.error());
            frame_view.buffer.clear();
            return;
        }
        frame_view.registered_output = true;

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
        if (set_frame_view_image(impl, record, frame_view, *output, *descriptor_format)) {
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
                       impl.build_absolute_path(record), BROOKESIA_DESCRIBE_ENUM_TO_STR(props.color_format));
        return;
    }
    if (output->info.pixel_format != *expected_pixel_format) {
        BROOKESIA_LOGE(
            "FrameView external output format mismatch: node='%1%', output='%2%', expected=%3%, actual=%4%",
            impl.build_absolute_path(record),
            output_name,
            BROOKESIA_DESCRIBE_ENUM_TO_STR(*expected_pixel_format),
            BROOKESIA_DESCRIBE_ENUM_TO_STR(output->info.pixel_format)
        );
        return;
    }
    if (set_frame_view_image(impl, record, frame_view, *output, props.color_format)) {
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
    case NodeType::Line: {
        if (!has_mask(mask, PropsApplyMask::LinePoints)) {
            break;
        }
        auto &line = record.ensure_type_payload<Record::LinePayload>();
        auto &points = line.points;
        points.clear();
        points.reserve(node.line_props.points.size());
        for (const auto &point : node.line_props.points) {
            points.push_back(lv_point_precise_t{
                .x = static_cast<lv_value_precise_t>(point.x),
                .y = static_cast<lv_value_precise_t>(point.y),
            });
        }
        if (!points.empty()) {
            lv_line_set_points(record.object, points.data(), points.size());
        }
        break;
    }
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
    const auto *image_payload = record.get_type_payload<Record::ImagePayload>();
    if (record.type != NodeType::Image || record.object == nullptr || image_payload == nullptr ||
            image_payload->width <= 0 || image_payload->height <= 0) {
        return;
    }
    const auto &image = *image_payload;

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
        const uint32_t scale = calculate_scale(placement.width.value, image.width);
        lv_obj_set_size(record.object, placement.width.value, apply_scale(image.height, scale));
    } else if (fixed_height && !explicit_width) {
        const uint32_t scale = calculate_scale(placement.height.value, image.height);
        lv_obj_set_size(record.object, apply_scale(image.width, scale), placement.height.value);
    } else if (!explicit_width && !explicit_height) {
        lv_obj_set_size(record.object, image.width, image.height);
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
