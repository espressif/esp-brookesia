/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <algorithm>
#include <limits>
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
#include "brookesia/service_display.hpp"
#include "brookesia/service_video/decoder.hpp"

namespace esp_brookesia::service {

namespace {

constexpr uint32_t DISPLAY_DRAW_TIMEOUT_MS_DEFAULT = BROOKESIA_SERVICE_DISPLAY_DRAW_TIMEOUT_MS;

} // namespace

std::string VideoDecoder::get_component_version()
{
    return make_version(
               BROOKESIA_SERVICE_VIDEO_VER_MAJOR, BROOKESIA_SERVICE_VIDEO_VER_MINOR, BROOKESIA_SERVICE_VIDEO_VER_PATCH
           );
}

bool VideoDecoder::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGI(
        "Version: %1%.%2%.%3%", BROOKESIA_SERVICE_VIDEO_VER_MAJOR, BROOKESIA_SERVICE_VIDEO_VER_MINOR,
        BROOKESIA_SERVICE_VIDEO_VER_PATCH
    );

    return true;
}

void VideoDecoder::on_stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_FALSE_EXECUTE(stop_internal(), {}, {
        BROOKESIA_LOGE("Failed to stop video decoder");
    });
    BROOKESIA_CHECK_FALSE_EXECUTE(close_internal(), {}, {
        BROOKESIA_LOGE("Failed to close video decoder");
    });
}

std::expected<void, std::string> VideoDecoder::function_open(const boost::json::object &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: config(%1%)", config);

    if (is_opened()) {
        return std::unexpected("Decoder is already opened, please close it first");
    }

    BaseHelper::DecoderConfig decoder_cfg;
    auto parse_result = BROOKESIA_DESCRIBE_FROM_JSON(config, decoder_cfg);
    if (!parse_result) {
        return std::unexpected("Failed to parse decoder config");
    }

    auto display_result = setup_display_output(decoder_cfg);
    if (!display_result) {
        return std::unexpected(display_result.error());
    }
    lib_utils::FunctionGuard display_cleanup_guard([this]() {
        clear_display_output();
    });

    auto decoder_handle = hal::acquire_interface<hal::video::DecoderIface>(
                              hal::video::DecoderIface::get_default_instance_name(id_)
                          );
    auto decoder_iface = decoder_handle.get();
    if (!decoder_iface) {
        return std::unexpected("Failed to acquire video decoder interface");
    }

    std::string error_message;
    auto callback = [this](uint16_t width, uint16_t height, const uint8_t *data, size_t size) {
        on_decoded_frame(width, height, data, size);
    };
    auto hal_decoder_cfg = to_hal_config(decoder_cfg);
    if (!decoder_iface->open(hal_decoder_cfg, std::move(callback), &error_message)) {
        return std::unexpected(error_message.empty() ? "Failed to open video decoder" : error_message);
    }

    decoder_cfg_ = decoder_cfg;
    decoder_iface_ = std::move(decoder_handle);
    display_cleanup_guard.release();

    return {};
}

std::expected<void, std::string> VideoDecoder::function_close()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!stop_internal()) {
        return std::unexpected("Failed to stop video decoder");
    }

    if (!close_internal()) {
        return std::unexpected("Failed to close video decoder");
    }

    return {};
}

std::expected<void, std::string> VideoDecoder::function_start()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_opened()) {
        return std::unexpected("Decoder is not opened, please open it first");
    }

    if (is_started()) {
        return {};
    }

    std::string error_message;
    if (!decoder_iface_->start(&error_message)) {
        return std::unexpected(error_message.empty() ? "Failed to start video decoder" : error_message);
    }

    return {};
}

std::expected<void, std::string> VideoDecoder::function_stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!stop_internal()) {
        return std::unexpected("Failed to stop video decoder");
    }

    return {};
}

std::expected<void, std::string> VideoDecoder::function_feed_frame(const RawBuffer &frame)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: frame(%1%)", frame);

    if (!is_started()) {
        return std::unexpected("Decoder is not started, please start it first");
    }

    std::string error_message;
    if (!decoder_iface_->feed_frame(frame.data_ptr, frame.data_size, &error_message)) {
        return std::unexpected(error_message.empty() ? "Failed to feed video frame" : error_message);
    }

    return {};
}

bool VideoDecoder::close_internal()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_opened()) {
        return true;
    }

    decoder_iface_->close();
    decoder_iface_.reset();
    decoder_cfg_ = {};
    clear_display_output();

    return true;
}

bool VideoDecoder::stop_internal()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_started()) {
        return true;
    }

    std::string error_message;
    if (!decoder_iface_->stop(&error_message)) {
        BROOKESIA_LOGE("%1%", error_message.empty() ? "Failed to stop video decoder" : error_message);
        return false;
    }

    return true;
}

std::expected<void, std::string> VideoDecoder::setup_display_output(BaseHelper::DecoderConfig &decoder_cfg)
{
    const auto &display_config = decoder_cfg.display;
    publish_sink_event_ = true;
    if (!display_config.has_value()) {
        return {};
    }

    auto &display_service = Display::get_instance();
    auto binding = ServiceManager::get_instance().bind(Display::Helper::get_name().data());
    if (!binding.is_valid()) {
        return std::unexpected("Failed to bind Display service");
    }

    const auto outputs = display_service.get_outputs();
    if (outputs.empty()) {
        return std::unexpected("No Display output is available");
    }

    const auto output_it = display_config->output_name.empty() ?
                           outputs.begin() :
    std::find_if(outputs.begin(), outputs.end(), [&](const auto & output) {
        return output.name == display_config->output_name;
    });
    if (output_it == outputs.end()) {
        return std::unexpected("Requested Display output is not available");
    }
    if ((output_it->width > std::numeric_limits<uint16_t>::max()) ||
            (output_it->height > std::numeric_limits<uint16_t>::max())) {
        return std::unexpected("Display output size exceeds video decoder config range");
    }
    if ((display_config->x >= output_it->width) || (display_config->y >= output_it->height)) {
        return std::unexpected("Video decoder Display origin is outside the output area");
    }

    auto sink_format_result = to_decoder_sink_format(output_it->pixel_format);
    if (!sink_format_result) {
        return std::unexpected(sink_format_result.error());
    }
    const uint32_t max_width = output_it->width - display_config->x;
    const uint32_t max_height = output_it->height - display_config->y;
    if (decoder_cfg.width == 0) {
        decoder_cfg.width = static_cast<uint16_t>(max_width);
    }
    if (decoder_cfg.height == 0) {
        decoder_cfg.height = static_cast<uint16_t>(max_height);
    }
    if ((decoder_cfg.width == 0) || (decoder_cfg.height == 0)) {
        return std::unexpected("Video decoder Display frame size is invalid");
    }
    if ((decoder_cfg.width > max_width) || (decoder_cfg.height > max_height)) {
        return std::unexpected("Video decoder Display frame size exceeds the output area");
    }
    decoder_cfg.sink_format = sink_format_result.value();

    auto source_name = display_config->source_name.empty() ?
                       std::string(BaseHelper::DISPLAY_SOURCE_NAME) : display_config->source_name;
    auto source_role = display_config->source_role.empty() ?
                       std::string(BaseHelper::DISPLAY_SOURCE_ROLE) : display_config->source_role;
    Display::SourceInfo source_info = {
        .name = source_name,
        .role = source_role,
        .preferred_outputs = {output_it->name},
        .priority = 0,
    };
    auto source_result = display_service.register_source(std::move(source_info));
    if (!source_result) {
        return std::unexpected("Failed to register Display video source: " + source_result.error());
    }

    const auto source_id = source_result.value();
    auto source_cleanup_guard = lib_utils::FunctionGuard([&display_service, source_id]() {
        (void)display_service.unregister_source(source_id);
    });
    auto request_result = display_service.request_output(source_id, output_it->name);
    if (!request_result) {
        return std::unexpected("Failed to request Display output: " + request_result.error());
    }

    display_binding_ = std::move(binding);
    display_source_id_ = source_id;
    display_output_name_ = output_it->name;
    display_pixel_format_ = output_it->pixel_format;
    display_x_ = display_config->x;
    display_y_ = display_config->y;
    display_draw_timeout_ms_ = display_config->draw_timeout_ms == 0 ?
                               DISPLAY_DRAW_TIMEOUT_MS_DEFAULT : display_config->draw_timeout_ms;
    publish_sink_event_ = display_config->publish_sink_event;
    source_cleanup_guard.release();

    BROOKESIA_LOGI(
        "Video decoder Display preview enabled: source(%1%), output(%2%), origin(%3%,%4%), "
        "size(%5%x%6%), sink_format=%7%",
        source_name, display_output_name_, display_x_, display_y_, decoder_cfg.width, decoder_cfg.height,
        BROOKESIA_DESCRIBE_ENUM_TO_STR(decoder_cfg.sink_format)
    );

    return {};
}

void VideoDecoder::clear_display_output()
{
    if (display_source_id_ != 0) {
        auto &display_service = Display::get_instance();
        if (!display_output_name_.empty()) {
            (void)display_service.release_output(display_source_id_, display_output_name_);
        }
        (void)display_service.unregister_source(display_source_id_);
    }

    display_source_id_ = 0;
    display_output_name_.clear();
    display_pixel_format_ = Display::PixelFormat::Max;
    display_x_ = 0;
    display_y_ = 0;
    display_draw_timeout_ms_ = 0;
    display_binding_.release();
    publish_sink_event_ = true;
}

void VideoDecoder::on_decoded_frame(uint16_t width, uint16_t height, const uint8_t *data, size_t size)
{
    if (display_source_id_ != 0) {
        Display::FrameInfo frame = {
            .x = display_x_,
            .y = display_y_,
            .width = width,
            .height = height,
            .pixel_format = display_pixel_format_,
        };
        auto result = Display::get_instance().present_frame_sync(
                          display_source_id_, display_output_name_, frame, RawBuffer(data, size),
                          display_draw_timeout_ms_
                      );
        if ((result != Display::PresentResult::Presented) &&
                (result != Display::PresentResult::DroppedNotActive)) {
            BROOKESIA_LOGW("Failed to present decoded video frame to Display: %1%",
                           BROOKESIA_DESCRIBE_ENUM_TO_STR(result));
        }
    }

    if (!publish_sink_event_) {
        return;
    }

    auto result = publish_event(
                      BROOKESIA_DESCRIBE_TO_STR(Helper::EventId::SinkFrameReady),
    std::vector<EventItem> {
        width,
        height,
        RawBuffer(data, size),
    }
                  );
    BROOKESIA_CHECK_FALSE_EXIT(result, "Failed to publish sink frame ready event");
}

hal::video::DecoderConfig VideoDecoder::to_hal_config(const BaseHelper::DecoderConfig &decoder_cfg) const
{
    return hal::video::DecoderConfig{
        .width = decoder_cfg.width,
        .height = decoder_cfg.height,
        .source_format = decoder_cfg.source_format,
        .sink_format = decoder_cfg.sink_format,
        .enable_stream_mode = decoder_cfg.enable_stream_mode,
        .enable_hw_acceleration = decoder_cfg.enable_hw_acceleration,
    };
}

std::expected<helper::Video::DecoderSinkFormat, std::string> VideoDecoder::to_decoder_sink_format(
    Display::PixelFormat pixel_format
) const
{
    switch (pixel_format) {
    case Display::PixelFormat::RGB565:
        return BaseHelper::DecoderSinkFormat::RGB565_LE;
    case Display::PixelFormat::RGB888:
        return BaseHelper::DecoderSinkFormat::RGB888;
    default:
        return std::unexpected("Display output pixel format is not supported by Video decoder preview");
    }
}

#define SYMBOL_NAME(i) BROOKESIA_SERVICE_VIDEO_DECODER_PLUGIN_SYMBOL_##i
#define REGISTER_DECODER(i) \
    BROOKESIA_PLUGIN_REGISTER_WITH_SYMBOL( \
        ServiceBase, VideoDecoder, helper::VideoDecoder<i>::get_name().data(), SYMBOL_NAME(i), i \
    )

#if BROOKESIA_SERVICE_VIDEO_DECODER_DEFAULT_NUM >= 1
REGISTER_DECODER(0);
#endif
#if BROOKESIA_SERVICE_VIDEO_DECODER_DEFAULT_NUM >= 2
REGISTER_DECODER(1);
#endif
#if BROOKESIA_SERVICE_VIDEO_DECODER_DEFAULT_NUM >= 3
REGISTER_DECODER(2);
#endif

} // namespace esp_brookesia::service
