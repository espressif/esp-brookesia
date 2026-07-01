/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/hal_adaptor/macro_configs.h"
#if !BROOKESIA_HAL_ADAPTOR_VIDEO_DECODER_IMPL_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif

#include <utility>

#include "boost/format.hpp"
#include "video_processor.h"
#include "private/utils.hpp"
#include "decoder_impl.hpp"
#include "processor_type_converter.hpp"

#if BROOKESIA_HAL_ADAPTOR_VIDEO_ENABLE_DECODER_IMPL

namespace esp_brookesia::hal {

static void set_error(std::string *error_message, std::string message);

VideoDecoderImpl::VideoDecoderImpl(size_t id)
    : video::DecoderIface()
    , id_(id)
{
}

VideoDecoderImpl::~VideoDecoderImpl()
{
    close();
}

bool VideoDecoderImpl::open(
    const video::DecoderConfig &config, FrameCallback callback, std::string *error_message
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_opened()) {
        set_error(error_message, "Decoder is already opened, please close it first");
        return false;
    }

    auto decode_cb = +[](void *ctx, const uint8_t *data, size_t size) {
        auto self = static_cast<VideoDecoderImpl *>(ctx);
        BROOKESIA_CHECK_NULL_EXIT(self, "Invalid context");
        self->on_decoded_frame(data, size);
    };
    auto render_cfg = VideoProcessorTypeConverter::convert(config, decode_cb, this);
    render_handle_ = video_render_open(&render_cfg);
    if (!render_handle_) {
        set_error(error_message, "Failed to open video render");
        return false;
    }

    config_ = config;
    callback_ = std::move(callback);

    return true;
}

void VideoDecoderImpl::close()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (render_handle_ != nullptr) {
        video_render_close(VideoProcessorTypeConverter::to_render_handle(render_handle_));
        render_handle_ = nullptr;
    }
    is_started_ = false;
    callback_ = nullptr;
    config_ = {};
}

bool VideoDecoderImpl::start(std::string *error_message)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_opened()) {
        set_error(error_message, "Decoder is not opened, please open it first");
        return false;
    }
    if (is_started()) {
        return true;
    }

    auto ret = video_render_start(VideoProcessorTypeConverter::to_render_handle(render_handle_));
    if (ret != ESP_OK) {
        set_error(error_message, (boost::format("Failed to start video render: %1%") % esp_err_to_name(ret)).str());
        return false;
    }

    is_started_ = true;

    return true;
}

bool VideoDecoderImpl::stop(std::string *error_message)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_started()) {
        return true;
    }

    auto ret = video_render_stop(VideoProcessorTypeConverter::to_render_handle(render_handle_));
    if (ret != ESP_OK) {
        set_error(error_message, (boost::format("Failed to stop video render: %1%") % esp_err_to_name(ret)).str());
        return false;
    }
    is_started_ = false;

    return true;
}

bool VideoDecoderImpl::feed_frame(const uint8_t *data, size_t size, std::string *error_message)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_started()) {
        set_error(error_message, "Decoder is not started, please start it first");
        return false;
    }
    BROOKESIA_CHECK_NULL_RETURN(data, false, "Invalid video frame");

    video_frame_t video_frame = {
        .data = const_cast<uint8_t *>(data),
        .size = size,
    };
    auto ret = video_render_feed_frame(VideoProcessorTypeConverter::to_render_handle(render_handle_), &video_frame);
    if (ret != ESP_OK) {
        set_error(error_message, (boost::format("Failed to feed video frame: %1%") % esp_err_to_name(ret)).str());
        return false;
    }

    return true;
}

void VideoDecoderImpl::on_decoded_frame(const uint8_t *data, size_t size)
{
    if (callback_) {
        callback_(config_.width, config_.height, data, size);
    }
}

static void set_error(std::string *error_message, std::string message)
{
    if (error_message != nullptr) {
        *error_message = std::move(message);
    }
}

} // namespace esp_brookesia::hal
#endif // BROOKESIA_HAL_ADAPTOR_VIDEO_ENABLE_DECODER_IMPL
