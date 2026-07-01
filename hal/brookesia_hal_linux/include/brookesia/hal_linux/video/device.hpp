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

class VideoEncoderLinuxImpl;
class VideoDecoderLinuxImpl;

class VideoLinuxDevice: public Device {
public:
    static constexpr const char *DEVICE_NAME = "VideoLinux";

    VideoLinuxDevice(const VideoLinuxDevice &) = delete;
    VideoLinuxDevice &operator=(const VideoLinuxDevice &) = delete;
    VideoLinuxDevice(VideoLinuxDevice &&) = delete;
    VideoLinuxDevice &operator=(VideoLinuxDevice &&) = delete;

    static VideoLinuxDevice &get_instance()
    {
        static VideoLinuxDevice instance;
        return instance;
    }

    static std::string get_encoder_iface_name(size_t id);
    static std::string get_decoder_iface_name(size_t id);

private:
    VideoLinuxDevice()
        : Device(std::string(DEVICE_NAME))
    {
    }
    ~VideoLinuxDevice() = default;

    bool probe() override;
    std::vector<InterfaceSpec> get_interface_specs() const override;
    bool on_init() override;
    void on_deinit() override;

    std::vector<std::shared_ptr<VideoEncoderLinuxImpl>> encoder_ifaces_;
    std::vector<std::shared_ptr<VideoDecoderLinuxImpl>> decoder_ifaces_;
};

} // namespace esp_brookesia::hal
