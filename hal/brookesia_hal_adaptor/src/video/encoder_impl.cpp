/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/hal_adaptor/macro_configs.h"
#if !BROOKESIA_HAL_ADAPTOR_VIDEO_ENCODER_IMPL_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif

#include <atomic>
#include <cstring>
#include <new>
#include <utility>

#include "boost/format.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include "impl/esp_capture_video_v4l2_src.h"
#include "video_processor.h"
#include "brookesia/lib_utils/function_guard.hpp"
#include "camera_impl.hpp"
#include "private/utils.hpp"
#include "encoder_impl.hpp"
#include "processor_type_converter.hpp"

#if BROOKESIA_HAL_ADAPTOR_VIDEO_ENABLE_ENCODER_IMPL

namespace esp_brookesia::hal {

constexpr size_t SINK_NUM_MAX = 1;
constexpr uint8_t V4L2_BUFFER_COUNT_DEFAULT = 2;

namespace {

struct FrameCallbackContext {
    const VideoEncoderImpl *encoder;
    const FrameCallbackContext *previous;
};

thread_local const FrameCallbackContext *current_frame_callback_context = nullptr;

} // namespace

static bool prepare_v4l2_camera_config(
    const video::EncoderConfig &encoder_cfg, const std::string &device_path, void *&camera_config,
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

    std::lock_guard lifecycle_lock(lifecycle_mutex_);

    if (capture_handle_ != nullptr) {
        set_error(error_message, "Encoder is already opened, please close it first");
        return false;
    }
#if BROOKESIA_HAL_ADAPTOR_VIDEO_ENABLE_CAMERA_IMPL
    if (!release_camera_session(error_message)) {
        return false;
    }
#endif

    auto sink_num = config.sinks.size();
    if ((sink_num == 0) || (sink_num > SINK_NUM_MAX)) {
        set_error(
            error_message,
            (boost::format("The sink number(%1%) is out of range(1~%2%)") % sink_num % SINK_NUM_MAX).str()
        );
        return false;
    }

    bool has_explicit_device_path = false;
    std::string device_path = default_device_path_;
    if (config.source.has_value() && config.source->device_path.has_value() &&
            !config.source->device_path->empty()) {
        device_path = config.source->device_path.value();
        has_explicit_device_path = true;
    }

#if BROOKESIA_HAL_ADAPTOR_VIDEO_ENABLE_CAMERA_IMPL
    if (VideoCameraDeviceSession::is_board_camera_declared()) {
        bool should_open_board_camera = !has_explicit_device_path;
        if (has_explicit_device_path) {
            std::string declared_device_path;
            should_open_board_camera = VideoCameraDeviceSession::get_declared_device_path(declared_device_path) &&
                                       (device_path == declared_device_path);
        }
        if (should_open_board_camera) {
            auto board_camera_session = std::make_unique<VideoCameraDeviceSession>();
            std::string session_error;
            if (board_camera_session->open(session_error)) {
                if (!has_explicit_device_path) {
                    device_path = board_camera_session->get_device_path();
                    camera_session_ = std::move(board_camera_session);
                } else if (device_path == board_camera_session->get_device_path()) {
                    camera_session_ = std::move(board_camera_session);
                }
            } else {
                std::string cleanup_error;
                if (!board_camera_session->close(&cleanup_error)) {
                    session_error += "; " + cleanup_error;
                    // Preserve ownership so the next open()/close() can retry
                    // the Board Manager and expansion-runtime cleanup.
                    camera_session_ = std::move(board_camera_session);
                }
                set_error(error_message, std::move(session_error));
                return false;
            }
        }
    }
#endif

    std::string error;
    if (!prepare_v4l2_camera_config(config, device_path, camera_config_, error)) {
#if BROOKESIA_HAL_ADAPTOR_VIDEO_ENABLE_CAMERA_IMPL
        std::string cleanup_error;
        if (!release_camera_session(&cleanup_error)) {
            error += "; " + cleanup_error;
        }
#endif
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
#if BROOKESIA_HAL_ADAPTOR_VIDEO_ENABLE_CAMERA_IMPL
        std::string cleanup_error;
        if (!release_camera_session(&cleanup_error)) {
            BROOKESIA_LOGE("Failed to release camera session after capture open failure: %1%", cleanup_error);
        }
#endif
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

    if (is_in_frame_callback()) {
        BROOKESIA_LOGE(
            "Cannot close encoder synchronously from its frame callback; close it after the callback returns"
        );
        return;
    }

    std::lock_guard lifecycle_lock(lifecycle_mutex_);
    stop_accepting_frame_operations();

    if (is_started_) {
        const auto ret = video_capture_stop(VideoProcessorTypeConverter::to_capture_handle(capture_handle_));
        if (ret != ESP_OK) {
            BROOKESIA_LOGW(
                "Video capture stopped with a cleanup error during close: %1%", esp_err_to_name(ret)
            );
        }
        is_started_ = false;
    }
    if (capture_handle_ != nullptr) {
        video_capture_close(VideoProcessorTypeConverter::to_capture_handle(capture_handle_));
        capture_handle_ = nullptr;
    }
    is_started_ = false;
    stream_callback_ = nullptr;
    config_ = {};
    delete_v4l2_camera_config(camera_config_);
#if BROOKESIA_HAL_ADAPTOR_VIDEO_ENABLE_CAMERA_IMPL
    std::string cleanup_error;
    if (!release_camera_session(&cleanup_error)) {
        BROOKESIA_LOGE("Failed to release camera session during encoder close: %1%", cleanup_error);
    }
#endif
}

bool VideoEncoderImpl::start(std::string *error_message)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    std::lock_guard lifecycle_lock(lifecycle_mutex_);

    if (capture_handle_ == nullptr) {
        set_error(error_message, "Encoder is not opened, please open it first");
        return false;
    }
    if (is_started_) {
        return true;
    }

    auto ret = video_capture_start(VideoProcessorTypeConverter::to_capture_handle(capture_handle_));
    if (ret != ESP_OK) {
        set_error(error_message, (boost::format("Failed to start video capture: %1%") % esp_err_to_name(ret)).str());
        return false;
    }

    is_started_ = true;
    start_accepting_frame_operations();

    return true;
}

bool VideoEncoderImpl::stop(std::string *error_message)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_in_frame_callback()) {
        set_error(
            error_message,
            "Cannot stop encoder synchronously from its frame callback; stop it after the callback returns"
        );
        return false;
    }

    std::lock_guard lifecycle_lock(lifecycle_mutex_);
    if (capture_handle_ == nullptr) {
        set_error(error_message, "Encoder is not opened, please open it first");
        return false;
    }
    if (!is_started_) {
        return true;
    }

    stop_accepting_frame_operations();

    auto ret = video_capture_stop(VideoProcessorTypeConverter::to_capture_handle(capture_handle_));
    // video_capture_stop tears down its capture pipeline even when cleanup reports an error.
    is_started_ = false;
    if (ret != ESP_OK) {
        set_error(
            error_message,
            (boost::format("Video capture stopped with a cleanup error: %1%") % esp_err_to_name(ret)).str()
        );
        return false;
    }

    return true;
}

bool VideoEncoderImpl::fetch_frame(size_t sink_index, FrameCallback callback, std::string *error_message)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    std::unique_lock lifecycle_lock(lifecycle_mutex_);
    if (!is_started_) {
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

    if (!begin_frame_operation()) {
        set_error(error_message, "Encoder is stopping");
        return false;
    }
    lib_utils::FunctionGuard frame_operation_guard([this]() {
        end_frame_operation();
    });

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

    lifecycle_lock.unlock();

    if (callback) {
        const FrameCallbackContext callback_context = {
            .encoder = this,
            .previous = current_frame_callback_context,
        };
        current_frame_callback_context = &callback_context;
        lib_utils::FunctionGuard callback_guard([&callback_context]() {
            current_frame_callback_context = callback_context.previous;
        });
        callback(sink_index, config_.sinks[sink_index], frame.data, frame.size);
    }

    return true;
}

void VideoEncoderImpl::on_capture_frame(int sink_index, void *frame)
{
    if (!begin_frame_operation()) {
        return;
    }
    lib_utils::FunctionGuard frame_operation_guard([this]() {
        end_frame_operation();
    });

    auto *video_frame = static_cast<esp_capture_stream_frame_t *>(frame);
    BROOKESIA_CHECK_NULL_EXIT(video_frame, "Invalid video frame");
    BROOKESIA_CHECK_OUT_RANGE_EXIT(sink_index, 0, config_.sinks.size() - 1, "Invalid sink index: %1%", sink_index);

    if (stream_callback_) {
        const FrameCallbackContext callback_context = {
            .encoder = this,
            .previous = current_frame_callback_context,
        };
        current_frame_callback_context = &callback_context;
        lib_utils::FunctionGuard callback_guard([&callback_context]() {
            current_frame_callback_context = callback_context.previous;
        });
        stream_callback_(sink_index, config_.sinks[sink_index], video_frame->data, video_frame->size);
    }
}

bool VideoEncoderImpl::is_opened() const
{
    std::lock_guard lifecycle_lock(lifecycle_mutex_);
    return capture_handle_ != nullptr;
}

bool VideoEncoderImpl::is_started() const
{
    std::lock_guard lifecycle_lock(lifecycle_mutex_);
    return is_started_;
}

bool VideoEncoderImpl::begin_frame_operation()
{
    std::lock_guard lock(frame_operation_mutex_);
    if (!accepting_frame_operations_) {
        return false;
    }
    ++active_frame_operations_;
    return true;
}

void VideoEncoderImpl::end_frame_operation()
{
    std::lock_guard lock(frame_operation_mutex_);
    if (active_frame_operations_ > 0) {
        --active_frame_operations_;
    }
    frame_operation_cv_.notify_all();
}

void VideoEncoderImpl::stop_accepting_frame_operations()
{
    std::unique_lock lock(frame_operation_mutex_);
    accepting_frame_operations_ = false;
    frame_operation_cv_.wait(lock, [this]() {
        return active_frame_operations_ == 0;
    });
}

void VideoEncoderImpl::start_accepting_frame_operations()
{
    std::lock_guard lock(frame_operation_mutex_);
    accepting_frame_operations_ = true;
}

bool VideoEncoderImpl::is_in_frame_callback() const
{
    for (auto *context = current_frame_callback_context; context != nullptr; context = context->previous) {
        if (context->encoder == this) {
            return true;
        }
    }
    return false;
}

#if BROOKESIA_HAL_ADAPTOR_VIDEO_ENABLE_CAMERA_IMPL
bool VideoEncoderImpl::release_camera_session(std::string *error_message)
{
    if (!camera_session_) {
        return true;
    }

    std::string cleanup_error;
    if (!camera_session_->close(&cleanup_error)) {
        set_error(error_message, std::move(cleanup_error));
        return false;
    }

    camera_session_.reset();
    return true;
}
#endif

static bool prepare_v4l2_camera_config(
    const video::EncoderConfig &encoder_cfg, const std::string &device_path, void *&camera_config,
    std::string &error_message
)
{
    auto buffer_count = V4L2_BUFFER_COUNT_DEFAULT;
    if (encoder_cfg.source.has_value()) {
        auto &source = encoder_cfg.source.value();
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
