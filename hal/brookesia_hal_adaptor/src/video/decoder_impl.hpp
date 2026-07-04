/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "brookesia/hal_interface/interfaces/video/processor.hpp"

namespace esp_brookesia::hal {

class VideoDecoderImpl: public video::DecoderIface {
public:
    explicit VideoDecoderImpl(size_t id);
    ~VideoDecoderImpl() override;

    bool open(const video::DecoderConfig &config, FrameCallback callback, std::string *error_message) override;
    void close() override;
    bool start(std::string *error_message) override;
    bool stop(std::string *error_message) override;
    bool feed_frame(const uint8_t *data, size_t size, std::string *error_message) override;
    bool is_opened() const override
    {
        return render_handle_ != nullptr;
    }
    bool is_started() const override
    {
        return is_started_;
    }

private:
    void on_decoded_frame(const uint8_t *data, size_t size);

    size_t id_ = 0;
    video::DecoderConfig config_{};
    FrameCallback callback_;
    bool is_started_ = false;
    void *render_handle_ = nullptr;
};

} // namespace esp_brookesia::hal
