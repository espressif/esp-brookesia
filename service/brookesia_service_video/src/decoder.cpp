/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "video_processor.h"
#include "brookesia/service_video/macro_configs.h"
#if !BROOKESIA_SERVICE_VIDEO_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "private/type_converter.hpp"
#include "brookesia/lib_utils/plugin.hpp"
#include "brookesia/service_video/decoder.hpp"

namespace esp_brookesia::service {

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

    auto decode_cb = +[](void *ctx, const uint8_t *data, size_t size) {
        BROOKESIA_LOG_TRACE_GUARD();

        auto self = static_cast<VideoDecoder *>(ctx);
        BROOKESIA_CHECK_NULL_EXIT(self, "Invalid context");

        RawBuffer buffer(data, size);
        auto result = self->publish_event(BROOKESIA_DESCRIBE_TO_STR(Helper::EventId::SinkFrameReady),
        std::vector<EventItem> {
            self->decoder_cfg_.width,
            self->decoder_cfg_.height,
            buffer
        });
        BROOKESIA_CHECK_FALSE_EXIT(result, "Failed to publish sink frame ready event");
    };
    auto render_cfg = TypeConverter::convert(decoder_cfg, nullptr, decode_cb, this);
    render_handle_ = video_render_open(&render_cfg);
    if (!render_handle_) {
        return std::unexpected("Failed to open video render");
    }

    decoder_cfg_ = decoder_cfg;

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

    auto ret = video_render_start(TypeConverter::to_render_handle(render_handle_));
    if (ret != ESP_OK) {
        return std::unexpected((boost::format("Failed to start video render: %1%") % esp_err_to_name(ret)).str());
    }

    is_started_ = true;

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

    video_frame_t video_frame = {
        .data = const_cast<uint8_t *>(frame.data_ptr),
        .size = frame.data_size,
    };
    auto ret = video_render_feed_frame(TypeConverter::to_render_handle(render_handle_), &video_frame);
    if (ret != ESP_OK) {
        return std::unexpected((boost::format("Failed to feed video frame: %1%") % esp_err_to_name(ret)).str());
    }

    return {};
}

bool VideoDecoder::close_internal()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_opened()) {
        return true;
    }

    video_render_close(TypeConverter::to_render_handle(render_handle_));
    render_handle_ = nullptr;

    return true;
}

bool VideoDecoder::stop_internal()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_started()) {
        return true;
    }

    bool is_success = true;
    auto ret = video_render_stop(TypeConverter::to_render_handle(render_handle_));
    BROOKESIA_CHECK_ESP_ERR_EXECUTE(ret, {
        is_success = false;
    }, {
        BROOKESIA_LOGE("Failed to stop video render");
    });
    is_started_ = false;

    return is_success;
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
