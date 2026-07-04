/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/hal_adaptor/macro_configs.h"
#if !BROOKESIA_HAL_ADAPTOR_VIDEO_ENCODER_IMPL_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif

#include <cstring>
#include <new>
#include <utility>

#include "boost/format.hpp"
#include "impl/esp_capture_video_v4l2_src.h"
#include "video_processor.h"
#include "brookesia/lib_utils/function_guard.hpp"
#include "private/utils.hpp"
#include "encoder_impl.hpp"
#include "processor_type_converter.hpp"

#if BROOKESIA_HAL_ADAPTOR_VIDEO_ENABLE_ENCODER_IMPL

namespace esp_brookesia::hal {

constexpr size_t SINK_NUM_MAX = 1;
constexpr uint8_t V4L2_BUFFER_COUNT_DEFAULT = 2;

static bool prepare_v4l2_camera_config(
    const video::EncoderConfig &encoder_cfg, const std::string &default_device_path, void *&camera_config,
    std::string &error_message
);
static void delete_v4l2_camera_config(void *&camera_config);
static void set_error(std::string *error_message, std::string message);

VideoEncoderImpl::VideoEncoderImpl(size_t id, std::string default_device_path)
    : video::EncoderIface()
    , id_(id)
    , default_device_path_(std::move(default_device_path))
{
}

VideoEncoderImpl::~VideoEncoderImpl()
{
    close();
}

bool VideoEncoderImpl::open(
    const video::EncoderConfig &config, FrameCallback callback, std::string *error_message
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_opened()) {
        set_error(error_message, "Encoder is already opened, please close it first");
        return false;
    }

    auto sink_num = config.sinks.size();
    if ((sink_num == 0) || (sink_num > SINK_NUM_MAX)) {
        set_error(
            error_message,
            (boost::format("The sink number(%1%) is out of range(1~%2%)") % sink_num % SINK_NUM_MAX).str()
        );
        return false;
    }

    std::string error;
    if (!prepare_v4l2_camera_config(config, default_device_path_, camera_config_, error)) {
        set_error(error_message, error);
        return false;
    }

    auto capture_frame_cb = +[](void *ctx, int sink_idx, esp_capture_stream_frame_t *vid_frame) {
        auto self = static_cast<VideoEncoderImpl *>(ctx);
        BROOKESIA_CHECK_NULL_EXIT(self, "Invalid context");
        self->on_capture_frame(sink_idx, vid_frame);
    };
    auto capture_cfg = VideoProcessorTypeConverter::convert(config, camera_config_, capture_frame_cb, this);
    capture_handle_ = video_capture_open(&capture_cfg);
    if (!capture_handle_) {
        delete_v4l2_camera_config(camera_config_);
        set_error(error_message, "Failed to open video capture");
        return false;
    }

    config_ = config;
    stream_callback_ = std::move(callback);

    return true;
}

void VideoEncoderImpl::close()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (capture_handle_ != nullptr) {
        video_capture_close(VideoProcessorTypeConverter::to_capture_handle(capture_handle_));
        capture_handle_ = nullptr;
    }
    is_started_ = false;
    stream_callback_ = nullptr;
    config_ = {};
    delete_v4l2_camera_config(camera_config_);
}

bool VideoEncoderImpl::start(std::string *error_message)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_opened()) {
        set_error(error_message, "Encoder is not opened, please open it first");
        return false;
    }
    if (is_started()) {
        return true;
    }

    auto ret = video_capture_start(VideoProcessorTypeConverter::to_capture_handle(capture_handle_));
    if (ret != ESP_OK) {
        set_error(error_message, (boost::format("Failed to start video capture: %1%") % esp_err_to_name(ret)).str());
        return false;
    }

    is_started_ = true;

    return true;
}

bool VideoEncoderImpl::stop(std::string *error_message)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    (void)error_message;
    set_error(error_message, "This function is not implemented yet");

    return false;
}

bool VideoEncoderImpl::fetch_frame(size_t sink_index, FrameCallback callback, std::string *error_message)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_started()) {
        set_error(error_message, "Encoder is not started, please start it first");
        return false;
    }
    if (config_.enable_stream_mode) {
        set_error(error_message, "This function is not available in stream mode");
        return false;
    }
    if (sink_index >= config_.sinks.size()) {
        set_error(
            error_message,
            (boost::format("Invalid sink index: %1%, should be in range(0~%2%)") % sink_index %
             (config_.sinks.size() - 1)).str()
        );
        return false;
    }

    esp_capture_stream_frame_t frame = {
        .stream_type = ESP_CAPTURE_STREAM_TYPE_VIDEO,
        .pts = 0,
        .data = nullptr,
        .size = 0,
    };
    auto acquire_ret = video_capture_fetch_frame_acquire(
                           VideoProcessorTypeConverter::to_capture_handle(capture_handle_), sink_index, &frame
                       );
    if (acquire_ret != ESP_OK) {
        set_error(
            error_message, (boost::format("Failed to acquire video frame: %1%") % esp_err_to_name(acquire_ret)).str()
        );
        return false;
    }

    lib_utils::FunctionGuard release_guard([this, sink_index, &frame]() {
        auto release_ret = video_capture_fetch_frame_release(
                               VideoProcessorTypeConverter::to_capture_handle(capture_handle_), sink_index, &frame
                           );
        BROOKESIA_CHECK_ESP_ERR_EXECUTE(release_ret, {}, {
            BROOKESIA_LOGE("Failed to release video frame");
        });
    });

    if (callback) {
        callback(sink_index, config_.sinks[sink_index], frame.data, frame.size);
    }

    return true;
}

void VideoEncoderImpl::on_capture_frame(int sink_index, void *frame)
{
    auto *video_frame = static_cast<esp_capture_stream_frame_t *>(frame);
    BROOKESIA_CHECK_NULL_EXIT(video_frame, "Invalid video frame");
    BROOKESIA_CHECK_OUT_RANGE_EXIT(sink_index, 0, config_.sinks.size() - 1, "Invalid sink index: %1%", sink_index);

    if (stream_callback_) {
        stream_callback_(sink_index, config_.sinks[sink_index], video_frame->data, video_frame->size);
    }
}

static bool prepare_v4l2_camera_config(
    const video::EncoderConfig &encoder_cfg, const std::string &default_device_path, void *&camera_config,
    std::string &error_message
)
{
    auto device_path = default_device_path;
    auto buffer_count = V4L2_BUFFER_COUNT_DEFAULT;
    if (encoder_cfg.source.has_value()) {
        auto &source = encoder_cfg.source.value();
        if (source.device_path.has_value() && !source.device_path.value().empty()) {
            device_path = source.device_path.value();
        }
        if (source.v4l2_buffer_count.has_value()) {
            buffer_count = source.v4l2_buffer_count.value();
        }
    }

    if (device_path.empty()) {
        error_message = "Camera device path is empty";
        return false;
    }
    if (buffer_count == 0) {
        error_message = "V4L2 buffer count must be greater than 0";
        return false;
    }

    auto *v4l2_config = new (std::nothrow) esp_capture_video_v4l2_src_cfg_t {};
    if (v4l2_config == nullptr) {
        error_message = "Failed to allocate V4L2 camera config";
        return false;
    }
    if (device_path.size() >= sizeof(v4l2_config->dev_name)) {
        delete v4l2_config;
        error_message = (boost::format("Camera device path is too long: %1%") % device_path).str();
        return false;
    }

    std::strncpy(v4l2_config->dev_name, device_path.c_str(), sizeof(v4l2_config->dev_name) - 1);
    v4l2_config->dev_name[sizeof(v4l2_config->dev_name) - 1] = '\0';
    v4l2_config->buf_count = buffer_count;

    camera_config = v4l2_config;

    return true;
}

static void delete_v4l2_camera_config(void *&camera_config)
{
    delete static_cast<esp_capture_video_v4l2_src_cfg_t *>(camera_config);
    camera_config = nullptr;
}

static void set_error(std::string *error_message, std::string message)
{
    if (error_message != nullptr) {
        *error_message = std::move(message);
    }
}

} // namespace esp_brookesia::hal
#endif // BROOKESIA_HAL_ADAPTOR_VIDEO_ENABLE_ENCODER_IMPL
