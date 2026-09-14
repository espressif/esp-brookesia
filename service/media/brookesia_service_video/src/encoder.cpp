/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <algorithm>
#include <chrono>
#include <limits>
#include <new>
#include <span>
#include <string_view>
#include <utility>

#include "boost/format.hpp"
#include "brookesia/service_video/macro_configs.h"
#if !BROOKESIA_SERVICE_VIDEO_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/lib_utils/function_guard.hpp"
#include "brookesia/lib_utils/plugin.hpp"
#include "brookesia/hal_interface/interface.hpp"
#include "brookesia/hal_interface/interfaces/video/processor.hpp"
#include "brookesia/service_manager/dataflow/registry.hpp"
#include "brookesia/service_manager/service/manager.hpp"
#include "brookesia/service_video/encoder.hpp"

namespace esp_brookesia::service {

namespace {

constexpr uint32_t VISUAL_DRAW_TIMEOUT_MS_DEFAULT = 5000;
constexpr std::string_view VISUAL_DATAFLOW_PROVIDER_ID_DEFAULT{};
constexpr uint32_t OPERATION_CLOSE_TIMEOUT_MS = 1000;
constexpr uint32_t OPERATION_STOP_TIMEOUT_MS = 1000;

} // namespace

std::string VideoEncoder::get_component_version()
{
    return make_version(
               BROOKESIA_SERVICE_VIDEO_VER_MAJOR, BROOKESIA_SERVICE_VIDEO_VER_MINOR, BROOKESIA_SERVICE_VIDEO_VER_PATCH
           );
}

bool VideoEncoder::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGI(
        "Version: %1%.%2%.%3%", BROOKESIA_SERVICE_VIDEO_VER_MAJOR, BROOKESIA_SERVICE_VIDEO_VER_MINOR,
        BROOKESIA_SERVICE_VIDEO_VER_PATCH
    );

    return true;
}

void VideoEncoder::on_stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto close_result = close_internal(OPERATION_CLOSE_TIMEOUT_MS);
    if (!close_result) {
        BROOKESIA_LOGE("Failed to close video encoder: %1%", close_result.error());
    }
}

std::expected<void, std::string> VideoEncoder::function_open(const boost::json::object &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: config(%1%)", BROOKESIA_DESCRIBE_TO_STR(config));

    if (closing_requested_.load()) {
        return std::unexpected("Encoder is closing");
    }

    std::lock_guard operation_lock(operation_mutex_);
    if (closing_requested_.load()) {
        return std::unexpected("Encoder is closing");
    }

    if (is_opened()) {
        return std::unexpected("Encoder is already opened, please close it first");
    }

    BaseHelper::EncoderConfig encoder_cfg;
    auto parse_result = BROOKESIA_DESCRIBE_FROM_JSON(config, encoder_cfg);
    if (!parse_result) {
        return std::unexpected("Failed to parse encoder config");
    }

    auto display_result = setup_display_output(encoder_cfg);
    if (!display_result) {
        return std::unexpected(display_result.error());
    }
    lib_utils::FunctionGuard display_cleanup_guard([this]() {
        clear_display_output();
    });

    auto encoder_handle = hal::acquire_interface<hal::video::EncoderIface>(
                              hal::video::EncoderIface::get_default_instance_name(id_)
                          );
    auto encoder_iface = encoder_handle.get();
    if (!encoder_iface) {
        return std::unexpected("Failed to acquire video encoder interface");
    }

    std::string error_message;
    auto callback = [this](
                        size_t sink_index,
                        const BaseHelper::EncoderSinkInfo & sink_info,
                        const uint8_t *data,
                        size_t size
    ) {
        on_encoder_frame(Helper::EventId::StreamSinkFrameReady, sink_index, sink_info, data, size);
    };
    auto hal_encoder_cfg = to_hal_config(encoder_cfg);
    if (!encoder_iface->open(hal_encoder_cfg, std::move(callback), &error_message)) {
        return std::unexpected(error_message.empty() ? "Failed to open video encoder" : error_message);
    }
    lib_utils::FunctionGuard encoder_cleanup_guard([&encoder_iface]() {
        encoder_iface->close();
    });

    if (display_activate_pending_ && display_operation_) {
        auto active_result = display_operation_->set_active_source(display_output_name_);
        if (!active_result) {
            return std::unexpected("Failed to activate visual data-flow source: " + active_result.error());
        }
        display_activate_pending_ = false;
    }

    encoder_cfg_ = encoder_cfg;
    encoder_iface_ = std::move(encoder_handle);
    encoder_cleanup_guard.release();
    display_cleanup_guard.release();

    return {};
}

std::expected<void, std::string> VideoEncoder::function_close()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    return close_internal(OPERATION_CLOSE_TIMEOUT_MS);
}

std::expected<void, std::string> VideoEncoder::function_start()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (closing_requested_.load()) {
        return std::unexpected("Encoder is closing");
    }

    std::lock_guard operation_lock(operation_mutex_);
    if (closing_requested_.load()) {
        return std::unexpected("Encoder is closing");
    }

    if (!is_opened()) {
        return std::unexpected("Encoder is not opened, please open it first");
    }

    if (is_started()) {
        return {};
    }

    std::string error_message;
    if (!encoder_iface_->start(&error_message)) {
        return std::unexpected(error_message.empty() ? "Failed to start video encoder" : error_message);
    }

    return {};
}

std::expected<void, std::string> VideoEncoder::function_stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    return stop_internal(OPERATION_STOP_TIMEOUT_MS);
}

std::expected<void, std::string> VideoEncoder::function_fetch_frame(double sink_index)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: sink_index(%1%)", sink_index);

    if (closing_requested_.load()) {
        return std::unexpected("Encoder is closing");
    }

    std::lock_guard operation_lock(operation_mutex_);
    if (closing_requested_.load()) {
        return std::unexpected("Encoder is closing");
    }

    if (!is_started()) {
        return std::unexpected("Encoder is not started, please start it first");
    }

    if (encoder_cfg_.enable_stream_mode) {
        return std::unexpected("This function is not available in stream mode");
    }

    if (sink_index < 0 || sink_index >= encoder_cfg_.sinks.size()) {
        return std::unexpected((
                                   boost::format("Invalid sink index: %1%, should be in range(0~%2%)") % sink_index %
                                   (encoder_cfg_.sinks.size() - 1)
                               ).str());
    }

    std::string error_message;
    auto callback = [this](
                        size_t frame_sink_index,
                        const BaseHelper::EncoderSinkInfo & sink_info,
                        const uint8_t *data,
                        size_t size
    ) {
        on_encoder_frame(Helper::EventId::FetchSinkFrameReady, frame_sink_index, sink_info, data, size);
    };
    const bool fetch_result = encoder_iface_->fetch_frame(
                                  static_cast<size_t>(sink_index), std::move(callback), &error_message
                              );
    if (closing_requested_.load()) {
        close_locked();
        closing_requested_.store(false);
    }
    if (!fetch_result) {
        return std::unexpected(error_message.empty() ? "Failed to fetch video frame" : error_message);
    }

    return {};
}

std::expected<void, std::string> VideoEncoder::close_internal(uint32_t timeout_ms)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    closing_requested_.store(true);
    cancel_pending_calls();

    std::unique_lock operation_lock(operation_mutex_, std::defer_lock);
    if (!operation_lock.try_lock_for(std::chrono::milliseconds(timeout_ms))) {
        return std::unexpected("Video encoder close timed out waiting for active fetch");
    }

    close_locked();
    closing_requested_.store(false);
    return {};
}

std::expected<void, std::string> VideoEncoder::stop_internal(uint32_t timeout_ms)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    cancel_pending_calls();

    std::unique_lock operation_lock(operation_mutex_, std::defer_lock);
    if (!operation_lock.try_lock_for(std::chrono::milliseconds(timeout_ms))) {
        return std::unexpected("Video encoder stop timed out waiting for active fetch");
    }

    if (!stop_locked()) {
        return std::unexpected("Failed to stop video encoder");
    }

    return {};
}

void VideoEncoder::cancel_pending_calls()
{
    auto scheduler = get_task_scheduler();
    if ((scheduler != nullptr) && scheduler->is_running()) {
        scheduler->cancel_group(get_call_task_group());
    }
}

void VideoEncoder::close_locked()
{
    if (encoder_iface_) {
        encoder_iface_->close();
        encoder_iface_.reset();
    }
    encoder_cfg_ = {};
    clear_display_output();
}

bool VideoEncoder::stop_locked()
{
    if (!is_started()) {
        return true;
    }

    std::string error_message;
    if (!encoder_iface_->stop(&error_message)) {
        BROOKESIA_LOGE("%1%", error_message.empty() ? "Failed to stop video encoder" : error_message);
        return false;
    }

    return true;
}

std::expected<void, std::string> VideoEncoder::setup_display_output(BaseHelper::EncoderConfig &encoder_cfg)
{
    publish_sink_event_ = true;
    if (!encoder_cfg.display.has_value()) {
        return {};
    }
    auto &display_cfg = encoder_cfg.display.value();

    if (display_cfg.sink_index >= encoder_cfg.sinks.size()) {
        return std::unexpected("Video encoder Display sink index is out of range");
    }

    const auto source_name = display_cfg.source_name.empty() ?
                             std::string(BaseHelper::DISPLAY_SOURCE_NAME) : display_cfg.source_name;
    const auto source_role = display_cfg.source_role.empty() ?
                             std::string(BaseHelper::DISPLAY_SOURCE_ROLE) : display_cfg.source_role;
    dataflow::VisualOperationConfig operation_config;
    operation_config.owner = get_attributes().name;
    operation_config.provider_id = std::string(VISUAL_DATAFLOW_PROVIDER_ID_DEFAULT);
    operation_config.model = dataflow::Model::Visual;
    operation_config.source = {
        .name = source_name,
        .role = source_role,
        .preferred_outputs = {},
        .priority = 0,
    };
    auto operation_result = ServiceManager::get_instance().get_dataflow_registry().open_visual_operation(
                                std::move(operation_config)
                            );
    if (!operation_result) {
        return std::unexpected("Failed to open visual data-flow operation: " + operation_result.error());
    }
    auto display_operation = std::move(operation_result.value());
    auto operation_cleanup_guard = lib_utils::FunctionGuard([&display_operation]() {
        if (display_operation) {
            display_operation->close();
        }
    });

    const auto outputs = display_operation->get_outputs();
    if (outputs.empty()) {
        return std::unexpected("No visual data-flow output is available");
    }

    const auto output_it = display_cfg.output_name.empty() ?
                           outputs.begin() :
    std::find_if(outputs.begin(), outputs.end(), [&](const auto & output) {
        return output.output.name == display_cfg.output_name;
    });
    if (output_it == outputs.end()) {
        return std::unexpected("Requested visual data-flow output is not available");
    }
    if ((output_it->output.width > std::numeric_limits<uint16_t>::max()) ||
            (output_it->output.height > std::numeric_limits<uint16_t>::max())) {
        return std::unexpected("Visual output size exceeds video encoder config range");
    }

    auto sink_format_result = to_encoder_sink_format(output_it->pixel_format);
    if (!sink_format_result) {
        return std::unexpected(sink_format_result.error());
    }

    auto &sink = encoder_cfg.sinks[display_cfg.sink_index];
    if (sink.format == BaseHelper::EncoderSinkFormat::Max) {
        sink.format = sink_format_result.value();
    } else if (sink.format != sink_format_result.value()) {
        return std::unexpected("Video encoder Display sink format does not match the Display output");
    }

    const uint32_t output_width = output_it->output.width;
    const uint32_t output_height = output_it->output.height;
    uint32_t origin_x = display_cfg.x;
    uint32_t origin_y = display_cfg.y;
    if ((origin_x >= output_width) || (origin_y >= output_height)) {
        origin_x = 0;
        origin_y = 0;
    }

    uint32_t max_width = output_width - origin_x;
    uint32_t max_height = output_height - origin_y;
    if (sink.width == 0) {
        sink.width = static_cast<uint16_t>(max_width);
    }
    if (sink.height == 0) {
        sink.height = static_cast<uint16_t>(max_height);
    }
    if ((sink.width == 0) || (sink.height == 0)) {
        return std::unexpected("Video encoder Display frame size is invalid");
    }
    if ((sink.width > max_width) || (sink.height > max_height)) {
        sink.width = static_cast<uint16_t>(std::min<uint32_t>(sink.width, output_width));
        sink.height = static_cast<uint16_t>(std::min<uint32_t>(sink.height, output_height));
        origin_x = (output_width - sink.width) / 2;
        origin_y = (output_height - sink.height) / 2;
        BROOKESIA_LOGW(
            "Video encoder Display frame clamped to output area: origin(%1%,%2%), size(%3%x%4%)",
            origin_x, origin_y, sink.width, sink.height
        );
    }
    display_cfg.x = origin_x;
    display_cfg.y = origin_y;

    auto request_result = display_operation->request_output(output_it->output.name);
    if (!request_result) {
        return std::unexpected("Failed to request visual data-flow output: " + request_result.error());
    }
    if (display_cfg.activate_source) {
        display_activate_pending_ = true;
    }

    display_operation_ = std::move(display_operation);
    display_output_name_ = output_it->output.name;
    display_pixel_format_ = output_it->pixel_format;
    display_byte_order_ = output_it->byte_order;
    display_x_ = display_cfg.x;
    display_y_ = display_cfg.y;
    display_draw_timeout_ms_ = display_cfg.draw_timeout_ms == 0 ?
                               VISUAL_DRAW_TIMEOUT_MS_DEFAULT : display_cfg.draw_timeout_ms;
    display_sink_index_ = display_cfg.sink_index;
    display_present_warning_count_ = 0;
    const size_t bytes_per_pixel = display_pixel_format_ == dataflow::VisualPixelFormat::RGB565 ? 2U : 3U;
    const size_t pixel_count = static_cast<size_t>(sink.width) * sink.height;
    if (pixel_count > std::numeric_limits<size_t>::max() / bytes_per_pixel) {
        clear_display_output();
        return std::unexpected("Video encoder Display frame size exceeds the addressable range");
    }
    display_present_capacity_ = pixel_count * bytes_per_pixel;
    if ((display_byte_order_ == dataflow::VisualByteOrder::Swap16) &&
            (display_pixel_format_ == dataflow::VisualPixelFormat::RGB565)) {
        display_present_buffer_.reset(new (std::nothrow) uint8_t[display_present_capacity_]);
        if (display_present_buffer_ == nullptr) {
            clear_display_output();
            return std::unexpected("Failed to allocate video encoder Display present buffer");
        }
    }
    publish_sink_event_ = display_cfg.publish_sink_event;
    operation_cleanup_guard.release();

    BROOKESIA_LOGI(
        "Video encoder Display preview enabled: source(%1%), output(%2%), origin(%3%,%4%), "
        "size(%5%x%6%), sink_format=%7%, sink_index=%8%",
        source_name, display_output_name_, display_x_, display_y_, sink.width, sink.height,
        BROOKESIA_DESCRIBE_ENUM_TO_STR(sink.format), display_sink_index_
    );

    return {};
}

void VideoEncoder::clear_display_output()
{
    if (display_operation_) {
        display_operation_->close();
        display_operation_.reset();
    }

    display_output_name_.clear();
    display_pixel_format_ = dataflow::VisualPixelFormat::Unknown;
    display_byte_order_ = dataflow::VisualByteOrder::Native;
    display_x_ = 0;
    display_y_ = 0;
    display_draw_timeout_ms_ = 0;
    display_sink_index_ = 0;
    display_present_warning_count_ = 0;
    display_present_buffer_.reset();
    display_present_capacity_ = 0;
    display_activate_pending_ = false;
    publish_sink_event_ = true;
}

void VideoEncoder::on_encoder_frame(
    Helper::EventId event_id, size_t sink_index, const BaseHelper::EncoderSinkInfo &sink_info, const uint8_t *data,
    size_t size
)
{
    if (display_operation_ && display_operation_->is_available() && (sink_index == display_sink_index_) &&
            (data != nullptr) && (size > 0)) {
        auto result = dataflow::VisualPresentResult::DroppedInvalidFrame;
        const auto &display_sink = encoder_cfg_.sinks[display_sink_index_];
        if ((size == display_present_capacity_) && (sink_info.width == display_sink.width) &&
                (sink_info.height == display_sink.height)) {
            const uint8_t *present_data = data;
            if (display_present_buffer_) {
                // The capture pipeline and event subscribers share the original frame.
                auto *pixels = display_present_buffer_.get();
                for (size_t i = 0; i < size; i += 2) {
                    pixels[i] = data[i + 1];
                    pixels[i + 1] = data[i];
                }
                present_data = pixels;
            }
            dataflow::VisualFrameInfo frame = {
                .x = display_x_,
                .y = display_y_,
                .width = sink_info.width,
                .height = sink_info.height,
                .pixel_format = display_pixel_format_,
            };
            result = display_operation_->present_frame_sync(
                         display_output_name_, frame, std::span<const uint8_t>(present_data, size),
                         display_draw_timeout_ms_
                     );
        }
        if ((result != dataflow::VisualPresentResult::Presented) &&
                (result != dataflow::VisualPresentResult::DroppedNotActive)) {
            if ((display_present_warning_count_++ % 30) == 0) {
                BROOKESIA_LOGW("Failed to present encoded video frame to Display: %1%",
                               BROOKESIA_DESCRIBE_ENUM_TO_STR(result));
            }
        }
    }

    if (!publish_sink_event_) {
        return;
    }

    auto result = publish_event(
                      BROOKESIA_DESCRIBE_TO_STR(event_id),
    std::vector<EventItem> {
        sink_index,
        BROOKESIA_DESCRIBE_TO_JSON(sink_info).as_object(),
        RawBuffer(data, size),
    }
                  );
    BROOKESIA_CHECK_FALSE_EXIT(result, "Failed to publish encoder sink frame ready event");
}

hal::video::EncoderConfig VideoEncoder::to_hal_config(const BaseHelper::EncoderConfig &encoder_cfg) const
{
    return hal::video::EncoderConfig{
        .sinks = encoder_cfg.sinks,
        .enable_stream_mode = encoder_cfg.enable_stream_mode,
        .source = encoder_cfg.source,
    };
}

std::expected<helper::Video::EncoderSinkFormat, std::string> VideoEncoder::to_encoder_sink_format(
    dataflow::VisualPixelFormat pixel_format
) const
{
    switch (pixel_format) {
    case dataflow::VisualPixelFormat::RGB565:
        return BaseHelper::EncoderSinkFormat::RGB565;
    case dataflow::VisualPixelFormat::RGB888:
        return BaseHelper::EncoderSinkFormat::RGB888;
    default:
        return std::unexpected("Visual output pixel format is not supported by Video encoder preview");
    }
}

#define SYMBOL_NAME(i) BROOKESIA_SERVICE_VIDEO_ENCODER_PLUGIN_SYMBOL_##i
#define REGISTER_ENCODER(i) \
    BROOKESIA_PLUGIN_REGISTER_WITH_SYMBOL( \
        ServiceBase, VideoEncoder, helper::VideoEncoder<i>::get_name().data(), SYMBOL_NAME(i), i \
    )

#if BROOKESIA_SERVICE_VIDEO_ENCODER_DEFAULT_NUM >= 1
REGISTER_ENCODER(0);
#endif
#if BROOKESIA_SERVICE_VIDEO_ENCODER_DEFAULT_NUM >= 2
REGISTER_ENCODER(1);
#endif
#if BROOKESIA_SERVICE_VIDEO_ENCODER_DEFAULT_NUM >= 3
REGISTER_ENCODER(2);
#endif

} // namespace esp_brookesia::service
