/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>

#include "brookesia/hal_interface/interfaces/video/processor.hpp"

namespace esp_brookesia::hal {

class VideoCameraDeviceSession;

class VideoEncoderImpl: public video::EncoderIface {
public:
    VideoEncoderImpl(size_t id, std::string default_device_path);
    ~VideoEncoderImpl() override;

    bool open(const video::EncoderConfig &config, FrameCallback callback, std::string *error_message) override;
    void close() override;
    bool start(std::string *error_message) override;
    bool stop(std::string *error_message) override;
    bool fetch_frame(size_t sink_index, FrameCallback callback, std::string *error_message) override;
    bool is_opened() const override;
    bool is_started() const override;

private:
    bool begin_frame_operation();
    void end_frame_operation();
    void stop_accepting_frame_operations();
    void start_accepting_frame_operations();
    bool is_in_frame_callback() const;
    void on_capture_frame(int sink_index, void *frame);
#if BROOKESIA_HAL_ADAPTOR_VIDEO_ENABLE_CAMERA_IMPL
    bool release_camera_session(std::string *error_message = nullptr);
#endif

    size_t id_ = 0;
    std::string default_device_path_;
    video::EncoderConfig config_{};
    FrameCallback stream_callback_;
    bool is_started_ = false;
    void *capture_handle_ = nullptr;
    void *camera_config_ = nullptr;
    mutable std::mutex lifecycle_mutex_;
    std::mutex frame_operation_mutex_;
    std::condition_variable frame_operation_cv_;
    size_t active_frame_operations_ = 0;
    bool accepting_frame_operations_ = false;
#if BROOKESIA_HAL_ADAPTOR_VIDEO_ENABLE_CAMERA_IMPL
    std::unique_ptr<VideoCameraDeviceSession> camera_session_;
#endif
};

} // namespace esp_brookesia::hal
