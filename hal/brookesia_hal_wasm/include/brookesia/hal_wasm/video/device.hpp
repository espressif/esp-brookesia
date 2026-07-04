/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>
#include "brookesia/hal_interface/device.hpp"

namespace esp_brookesia::hal {

class VideoCameraWasmImpl;
class VideoEncoderWasmImpl;
class VideoDecoderWasmImpl;

class VideoWasmDevice: public Device {
public:
    static constexpr const char *DEVICE_NAME = "VideoWasm";

    VideoWasmDevice(const VideoWasmDevice &) = delete;
    VideoWasmDevice &operator=(const VideoWasmDevice &) = delete;
    VideoWasmDevice(VideoWasmDevice &&) = delete;
    VideoWasmDevice &operator=(VideoWasmDevice &&) = delete;

    static VideoWasmDevice &get_instance()
    {
        static VideoWasmDevice instance;
        return instance;
    }

    static std::string get_camera_iface_name(size_t id);
    static std::string get_encoder_iface_name(size_t id);
    static std::string get_decoder_iface_name(size_t id);

private:
    VideoWasmDevice()
        : Device(std::string(DEVICE_NAME))
    {
    }
    ~VideoWasmDevice() = default;

    bool probe() override;
    std::vector<InterfaceSpec> get_interface_specs() const override;
    bool on_init() override;
    void on_deinit() override;

    std::shared_ptr<VideoCameraWasmImpl> camera_iface_;
    std::shared_ptr<VideoEncoderWasmImpl> encoder_iface_;
    std::shared_ptr<VideoDecoderWasmImpl> decoder_iface_;
};

} // namespace esp_brookesia::hal
