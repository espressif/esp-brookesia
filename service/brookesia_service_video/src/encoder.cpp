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
#include "brookesia/lib_utils/function_guard.hpp"
#include "brookesia/lib_utils/plugin.hpp"
#include "brookesia/service_video/encoder.hpp"

namespace esp_brookesia::service {

constexpr size_t SINK_NUM_MAX = 1;

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

    // BROOKESIA_CHECK_FALSE_EXECUTE(stop_internal(), {}, {
    //     BROOKESIA_LOGE("Failed to stop video encoder");
    // });
    BROOKESIA_CHECK_FALSE_EXECUTE(close_internal(), {}, {
        BROOKESIA_LOGE("Failed to close video encoder");
    });
}

std::expected<void, std::string> VideoEncoder::function_open(const boost::json::object &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: config(%1%)", BROOKESIA_DESCRIBE_TO_STR(config));

    if (is_opened()) {
        return std::unexpected("Encoder is already opened, please close it first");
    }

    BaseHelper::EncoderConfig encoder_cfg;
    auto parse_result = BROOKESIA_DESCRIBE_FROM_JSON(config, encoder_cfg);
    if (!parse_result) {
        return std::unexpected("Failed to parse encoder config");
    }

    auto sink_num = encoder_cfg.sinks.size();
    if ((sink_num == 0) || (sink_num > SINK_NUM_MAX)) {
        return std::unexpected((
                                   boost::format("The sink number(%1%) is out of range(1~%2%)") % sink_num % SINK_NUM_MAX
                               ).str());
    }

    auto capture_frame_cb = +[](void *ctx, int sink_idx, esp_capture_stream_frame_t *vid_frame) {
        BROOKESIA_LOG_TRACE_GUARD();

        auto self = static_cast<VideoEncoder *>(ctx);
        BROOKESIA_CHECK_NULL_EXIT(self, "Invalid context");

        auto &sink_info = self->encoder_cfg_.sinks[sink_idx];
        RawBuffer buffer(const_cast<const uint8_t *>(vid_frame->data), vid_frame->size);
        auto result = self->publish_event(BROOKESIA_DESCRIBE_TO_STR(Helper::EventId::StreamSinkFrameReady),
        std::vector<EventItem> {
            sink_idx,
            BROOKESIA_DESCRIBE_TO_JSON(sink_info).as_object(),
            buffer
        });
        BROOKESIA_CHECK_FALSE_EXIT(result, "Failed to publish stream sink frame ready event");
    };
    auto capture_cfg = TypeConverter::convert(
                           encoder_cfg, const_cast<char *>(get_device_path().c_str()), capture_frame_cb, this
                       );
    capture_handle_ = video_capture_open(&capture_cfg);
    if (!capture_handle_) {
        return std::unexpected("Failed to open video capture");
    }

    encoder_cfg_ = encoder_cfg;

    return {};
}

std::expected<void, std::string> VideoEncoder::function_close()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!close_internal()) {
        return std::unexpected("Failed to close video encoder");
    }

    return {};
}

std::expected<void, std::string> VideoEncoder::function_start()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_opened()) {
        return std::unexpected("Encoder is not opened, please open it first");
    }

    if (is_started()) {
        return {};
    }

    auto ret = video_capture_start(TypeConverter::to_capture_handle(capture_handle_));
    if (ret != ESP_OK) {
        return std::unexpected((boost::format("Failed to start video capture: %1%") % esp_err_to_name(ret)).str());
    }

    is_started_ = true;

    return {};
}

std::expected<void, std::string> VideoEncoder::function_stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!stop_internal()) {
        return std::unexpected("Failed to stop video encoder");
    }

    return {};
}

std::expected<void, std::string> VideoEncoder::function_fetch_frame(double sink_index)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: sink_index(%1%)", sink_index);

    if (!is_started()) {
        return std::unexpected("Encoder is not started, please start it first");
    }

    if (encoder_cfg_.enable_stream_mode) {
        return std::unexpected("This function is not available in non-stream mode");
    }

    if (sink_index < 0 || sink_index >= encoder_cfg_.sinks.size()) {
        return std::unexpected((
                                   boost::format("Invalid sink index: %1%, should be in range(0~%2%)") % sink_index %
                                   (encoder_cfg_.sinks.size() - 1)
                               ).str());
    }

    esp_capture_stream_frame_t frame = {
        .stream_type = ESP_CAPTURE_STREAM_TYPE_VIDEO,
        .pts = 0,
        .data = nullptr,
        .size = 0,
    };
    auto acquire_ret = video_capture_fetch_frame_acquire(
                           TypeConverter::to_capture_handle(capture_handle_), sink_index, &frame
                       );
    if (acquire_ret != ESP_OK) {
        return std::unexpected(
                   (boost::format("Failed to acquire video frame: %1%") % esp_err_to_name(acquire_ret)).str()
               );
    }

    lib_utils::FunctionGuard release_guard([this, sink_index, &frame]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        auto release_ret = video_capture_fetch_frame_release(
                               TypeConverter::to_capture_handle(capture_handle_), sink_index, &frame
                           );
        BROOKESIA_CHECK_ESP_ERR_EXECUTE(release_ret, {}, {
            BROOKESIA_LOGE("Failed to release video frame");
        });
    });

    auto publish_ret = publish_event(BROOKESIA_DESCRIBE_TO_STR(Helper::EventId::FetchSinkFrameReady),
    std::vector<EventItem> {
        sink_index,
        BROOKESIA_DESCRIBE_TO_JSON(encoder_cfg_.sinks[sink_index]).as_object(),
        RawBuffer(const_cast<const uint8_t *>(frame.data), frame.size)
    });
    if (!publish_ret) {
        return std::unexpected("Failed to publish fetch sink frame ready event");
    }

    return {};
}

bool VideoEncoder::close_internal()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_opened()) {
        return true;
    }

    video_capture_close(TypeConverter::to_capture_handle(capture_handle_));
    capture_handle_ = nullptr;

    return true;
}

bool VideoEncoder::stop_internal()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    // if (!is_started()) {
    //     return true;
    // }

    // TODO: Since video_processor is not supported to stop the capture, we need to implement this function in the future
    // bool is_success = true;
    // auto ret = video_capture_stop(TypeConverter::to_capture_handle(capture_handle_));
    // BROOKESIA_CHECK_ESP_ERR_EXECUTE(ret, {
    //     is_success = false;
    // }, {
    //     BROOKESIA_LOGE("Failed to stop video capture");
    // });
    // is_started_ = false;

    BROOKESIA_LOGE("This function is not implemented yet");

    return false;
}

// Adjacent string literal concatenation: PREFIX "i" → e.g. "/dev/video" "0" → "/dev/video0"
#define _STRINGIFY(x) #x
#define DEVICE_PATH(i) BROOKESIA_SERVICE_VIDEO_ENCODER_DEFAULT_DEVICE_PATH_PREFIX _STRINGIFY(i)
#define SYMBOL_NAME(i) BROOKESIA_SERVICE_VIDEO_ENCODER_PLUGIN_SYMBOL_##i
#define REGISTER_ENCODER(i) \
    BROOKESIA_PLUGIN_REGISTER_WITH_SYMBOL( \
        ServiceBase, VideoEncoder, helper::VideoEncoder<i>::get_name().data(), SYMBOL_NAME(i), \
        i, DEVICE_PATH(i) \
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
