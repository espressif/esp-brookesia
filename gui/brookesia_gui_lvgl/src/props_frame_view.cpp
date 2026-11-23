/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "private/props_impl.hpp"
// lv_image_cache_drop() lives in an instance header that lvgl.h does not aggregate.
#if __has_include("lvgl/src/misc/cache/instance/lv_image_cache.h")
#   include "lvgl/src/misc/cache/instance/lv_image_cache.h"
#elif __has_include("src/misc/cache/instance/lv_image_cache.h")
#   include "src/misc/cache/instance/lv_image_cache.h"
#endif

namespace esp_brookesia::gui::lvgl::props_detail {



std::string resolve_frame_view_output_name(const Record &record, const FrameViewProps &props)
{
    if (!props.output_name.empty()) {
        return props.output_name;
    }
    if (!props.auto_register_output) {
        return {};
    }
    return "FrameView" + std::to_string(record.handle.value());
}

std::optional<esp_brookesia::service::Display::PixelFormat> to_display_pixel_format(FrameColorFormat format)
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

std::optional<FrameColorFormat> to_frame_color_format(
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

std::optional<lv_color_format_t> to_lvgl_color_format(FrameColorFormat format)
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

size_t frame_view_bytes_per_pixel(FrameColorFormat format)
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

bool frame_view_props_equal(const FrameViewProps &lhs, const FrameViewProps &rhs)
{
    return lhs.auto_register_output == rhs.auto_register_output &&
           lhs.output_name == rhs.output_name &&
           lhs.color_format == rhs.color_format;
}

Record::FrameViewPayload &ensure_frame_view_payload(Record &record)
{
    return record.ensure_type_payload<Record::FrameViewPayload>();
}

bool frame_view_matches(
    const Record::FrameViewPayload &frame_view,
    const FrameViewProps &props,
    std::string_view output_name,
    int32_t width,
    int32_t height)
{
    return frame_view.ready &&
           frame_view_props_equal(frame_view.props, props) &&
           frame_view.output_name == output_name &&
           frame_view.width == width &&
           frame_view.height == height;
}

bool copy_frame_view_buffer_to_shadow(
    Record::FrameViewPayload &frame_view,
    const esp_brookesia::service::Display::BufferOutputView &output,
    std::optional<esp_brookesia::service::Display::FrameInfo> frame
)
{
    if (output.info.width == 0 || output.info.height == 0 || output.stride_bytes == 0 ||
            output.buffer.data_ptr == nullptr) {
        return false;
    }

    const uint64_t data_size = static_cast<uint64_t>(output.stride_bytes) * output.info.height;
    if (data_size > std::numeric_limits<size_t>::max()) {
        return false;
    }
    if (frame_view.shadow_buffer.size() != static_cast<size_t>(data_size)) {
        frame_view.shadow_buffer.assign(static_cast<size_t>(data_size), 0);
    }
    if (frame_view.shadow_buffer.empty()) {
        return false;
    }

    const auto descriptor_format = to_frame_color_format(output.info.pixel_format);
    const size_t bytes_per_pixel = descriptor_format.has_value() ? frame_view_bytes_per_pixel(*descriptor_format) : 0;
    const auto *source = output.buffer.to_const_ptr<uint8_t>();
    auto *target = frame_view.shadow_buffer.data();
    if (source == nullptr || target == nullptr || bytes_per_pixel == 0) {
        return false;
    }

    if (!frame.has_value() || frame->pixel_format != output.info.pixel_format ||
            frame->x >= output.info.width || frame->y >= output.info.height ||
            frame->width == 0 || frame->height == 0 ||
            frame->width > output.info.width - frame->x ||
            frame->height > output.info.height - frame->y) {
        std::memcpy(target, source, static_cast<size_t>(data_size));
        return true;
    }

    const size_t row_bytes = static_cast<size_t>(frame->width) * bytes_per_pixel;
    const size_t x_offset = static_cast<size_t>(frame->x) * bytes_per_pixel;
    if (frame->x == 0 && frame->y == 0 && frame->width == output.info.width &&
            frame->height == output.info.height && row_bytes == output.stride_bytes) {
        std::memcpy(target, source, static_cast<size_t>(data_size));
        return true;
    }
    for (uint32_t row = 0; row < frame->height; ++row) {
        const size_t offset = (static_cast<size_t>(frame->y) + row) * output.stride_bytes + x_offset;
        std::memcpy(target + offset, source + offset, row_bytes);
    }
    return true;
}

void clear_frame_view_image(Record &record, Record::FrameViewPayload &frame_view)
{
    frame_view.frame_connection.disconnect();
    // The variable-image cache is keyed by this descriptor address. Drop it before the
    // descriptor is cleared, or a later allocation at the same address can reuse a stale decode.
    lv_image_cache_drop(&frame_view.descriptor);
    frame_view.ready = false;
    frame_view.width = 0;
    frame_view.height = 0;
    frame_view.descriptor = {};
    frame_view.shadow_buffer.clear();
    if (record.object != nullptr && lv_obj_is_valid(record.object)) {
        lv_image_set_src(record.object, nullptr);
    }
}

bool set_frame_view_image(
    BackendImpl &impl,
    Record &record,
    Record::FrameViewPayload &frame_view,
    const esp_brookesia::service::Display::BufferOutputView &output,
    FrameColorFormat descriptor_format)
{
    auto lv_format = to_lvgl_color_format(descriptor_format);
    if (!lv_format.has_value()) {
        BROOKESIA_LOGE("Unsupported FrameView color format for node '%1%'", impl.build_absolute_path(record));
        clear_frame_view_image(record, frame_view);
        return false;
    }
    if (output.info.width == 0 || output.info.height == 0 || output.stride_bytes == 0) {
        BROOKESIA_LOGE("Invalid FrameView output dimensions for node '%1%'", impl.build_absolute_path(record));
        clear_frame_view_image(record, frame_view);
        return false;
    }
    if (output.info.width > std::numeric_limits<uint16_t>::max() ||
            output.info.height > std::numeric_limits<uint16_t>::max() ||
            output.stride_bytes > std::numeric_limits<uint16_t>::max()) {
        BROOKESIA_LOGE(
            "FrameView output is too large for LVGL image descriptor: node='%1%'",
            impl.build_absolute_path(record)
        );
        clear_frame_view_image(record, frame_view);
        return false;
    }
    if (output.buffer.data_ptr == nullptr) {
        BROOKESIA_LOGE("FrameView output buffer is null: node='%1%'", impl.build_absolute_path(record));
        clear_frame_view_image(record, frame_view);
        return false;
    }

    const uint64_t data_size = static_cast<uint64_t>(output.stride_bytes) * output.info.height;
    if (data_size > std::numeric_limits<uint32_t>::max()) {
        BROOKESIA_LOGE(
            "FrameView output buffer is too large for LVGL image descriptor: node='%1%'",
            impl.build_absolute_path(record)
        );
        clear_frame_view_image(record, frame_view);
        return false;
    }

    if (!copy_frame_view_buffer_to_shadow(frame_view, output)) {
        BROOKESIA_LOGE(
            "Failed to copy FrameView output buffer for node '%1%'",
            impl.build_absolute_path(record)
        );
        clear_frame_view_image(record, frame_view);
        return false;
    }

    frame_view.descriptor = {};
    frame_view.descriptor.header.magic = LV_IMAGE_HEADER_MAGIC;
    frame_view.descriptor.header.cf = *lv_format;
    frame_view.descriptor.header.flags = 0;
    frame_view.descriptor.header.w = output.info.width;
    frame_view.descriptor.header.h = output.info.height;
    frame_view.descriptor.header.stride = static_cast<uint32_t>(output.stride_bytes);
    frame_view.descriptor.data_size = static_cast<uint32_t>(data_size);
    frame_view.descriptor.data = frame_view.shadow_buffer.data();

    lv_image_set_src(record.object, &frame_view.descriptor);
    frame_view.width = static_cast<int32_t>(output.info.width);
    frame_view.height = static_cast<int32_t>(output.info.height);
    frame_view.ready = true;
    return true;
}

void refresh_frame_view_shadow(
    BackendImpl &impl,
    BackendHandle handle,
    std::string_view output_name,
    const esp_brookesia::service::Display::FrameInfo &frame
)
{
    using DisplayService = esp_brookesia::service::Display;

    ThreadLockGuard lock;
    auto *record = impl.find_record(handle);
    if (record == nullptr || record->object == nullptr || !lv_obj_is_valid(record->object)) {
        return;
    }
    auto *frame_view_payload = record->get_type_payload<Record::FrameViewPayload>();
    if (frame_view_payload == nullptr) {
        return;
    }
    auto &frame_view = *frame_view_payload;
    if (!frame_view.ready || frame_view.output_name != output_name) {
        return;
    }

    auto output = DisplayService::get_instance().get_buffer_output(output_name);
    if (!output) {
        BROOKESIA_LOGD("FrameView output shadow copy skipped: output='%1%', error='%2%'",
                       output_name, output.error());
        return;
    }
    if (!copy_frame_view_buffer_to_shadow(frame_view, *output, frame)) {
        BROOKESIA_LOGW("FrameView output shadow copy failed: output='%1%'", output_name);
        return;
    }
    auto *shadow_data = frame_view.shadow_buffer.data();
    if (frame_view.descriptor.data != shadow_data) {
        frame_view.descriptor.data = shadow_data;
        lv_image_set_src(record->object, &frame_view.descriptor);
    }
    lv_obj_invalidate(record->object);
}

void connect_frame_view_frame_signal(BackendImpl &impl, Record &record)
{
    using DisplayService = esp_brookesia::service::Display;

    auto *frame_view_payload = record.get_type_payload<Record::FrameViewPayload>();
    if (frame_view_payload == nullptr) {
        return;
    }
    auto &frame_view = *frame_view_payload;
    frame_view.frame_connection.disconnect();
    const auto handle = record.handle;
    const auto output_name = frame_view.output_name;
    auto frame_presented_callback = [&impl, handle, output_name](
                                        const std::string &, const DisplayService::FrameInfo & frame
    ) {
#if defined(__EMSCRIPTEN__)
        auto queued = esp_brookesia::gui::wasm::post_gui_task([&impl, handle, output_name, frame]() {
            refresh_frame_view_shadow(impl, handle, output_name, frame);
        });
        if (!queued) {
            BROOKESIA_LOGW("Failed to queue wasm FrameView invalidate");
        }
#else
        refresh_frame_view_shadow(impl, handle, output_name, frame);
#endif
    };
    frame_view.frame_connection =
        DisplayService::get_instance().connect_frame_presented(output_name, frame_presented_callback);
}

void connect_external_frame_view_lifecycle(BackendImpl &impl, Record &record)
{
    using DisplayService = esp_brookesia::service::Display;

    auto *frame_view_payload = record.get_type_payload<Record::FrameViewPayload>();
    if (frame_view_payload == nullptr) {
        return;
    }
    auto &frame_view = *frame_view_payload;
    frame_view.output_registered_connection.disconnect();
    frame_view.output_unregistered_connection.disconnect();
    const auto handle = record.handle;
    const auto output_name = frame_view.output_name;
    auto output_registered_callback = [&impl, handle, output_name](const DisplayService::OutputInfo & info) {
        if (info.name != output_name || info.slot != DisplayService::OutputSlot::Buffer) {
            return;
        }
        ThreadLockGuard lock;
        auto *record = impl.find_record(handle);
        if (record == nullptr) {
            return;
        }
        auto *frame_view = record->get_type_payload<Record::FrameViewPayload>();
        if (frame_view == nullptr || frame_view->props.auto_register_output ||
                frame_view->output_name != output_name) {
            return;
        }
        refresh_frame_view(impl, *record, frame_view->props);
    };
    auto output_unregistered_callback = [&impl, handle, output_name](const std::string & unregistered_output_name) {
        if (unregistered_output_name != output_name) {
            return;
        }
        ThreadLockGuard lock;
        auto *record = impl.find_record(handle);
        if (record == nullptr) {
            return;
        }
        auto *frame_view = record->get_type_payload<Record::FrameViewPayload>();
        if (frame_view == nullptr || frame_view->props.auto_register_output ||
                frame_view->output_name != output_name) {
            return;
        }
        clear_frame_view_image(*record, *frame_view);
    };
    frame_view.output_registered_connection =
        DisplayService::get_instance().connect_output_registered(output_registered_callback);
    frame_view.output_unregistered_connection =
        DisplayService::get_instance().connect_output_unregistered(output_unregistered_callback);
}

void apply_common_disabled(Record &record, const CommonProps &props)
{
    if (props.disabled) {
        lv_obj_add_state(record.object, LV_STATE_DISABLED);
    } else {
        lv_obj_remove_state(record.object, LV_STATE_DISABLED);
    }
}
}
