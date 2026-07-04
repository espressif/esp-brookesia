/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <string>

#include "brookesia/hal_interface/interfaces/video/processor.hpp"

namespace esp_brookesia::hal {

class VideoEncoderImpl: public video::EncoderIface {
public:
    VideoEncoderImpl(size_t id, std::string default_device_path);
    ~VideoEncoderImpl() override;

    bool open(const video::EncoderConfig &config, FrameCallback callback, std::string *error_message) override;
    void close() override;
    bool start(std::string *error_message) override;
    bool stop(std::string *error_message) override;
    bool fetch_frame(size_t sink_index, FrameCallback callback, std::string *error_message) override;
    bool is_opened() const override
    {
        return capture_handle_ != nullptr;
    }
    bool is_started() const override
    {
        return is_started_;
    }

private:
    void on_capture_frame(int sink_index, void *frame);

    size_t id_ = 0;
    std::string default_device_path_;
    video::EncoderConfig config_{};
    FrameCallback stream_callback_;
    bool is_started_ = false;
    void *capture_handle_ = nullptr;
    void *camera_config_ = nullptr;
};

} // namespace esp_brookesia::hal
