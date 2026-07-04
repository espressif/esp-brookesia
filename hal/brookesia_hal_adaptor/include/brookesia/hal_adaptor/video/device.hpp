/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <string>
#include <vector>

#include "brookesia/hal_interface/device.hpp"
#include "brookesia/hal_interface/interfaces/video/camera.hpp"
#include "brookesia/hal_interface/interfaces/video/processor.hpp"

namespace esp_brookesia::hal {

class VideoDevice: public Device {
public:
    static constexpr const char *DEVICE_NAME = "Video";

    VideoDevice(const VideoDevice &) = delete;
    VideoDevice &operator=(const VideoDevice &) = delete;
    VideoDevice(VideoDevice &&) = delete;
    VideoDevice &operator=(VideoDevice &&) = delete;

    static std::string get_encoder_iface_name(size_t id);
    static std::string get_decoder_iface_name(size_t id);
    static std::string get_camera_iface_name(size_t id);

    static VideoDevice &get_instance()
    {
        static VideoDevice instance;
        return instance;
    }

private:
    VideoDevice()
        : Device(std::string(DEVICE_NAME))
    {
    }
    ~VideoDevice() = default;

    bool probe() override;
    std::vector<InterfaceSpec> get_interface_specs() const override;
    bool deinit_on_zero_references() const override;
    bool on_init() override;
    void on_deinit() override;

    bool init_encoders();
    bool init_decoders();
    bool init_cameras();
    std::string get_default_encoder_device_path(size_t id) const;

    std::vector<std::string> camera_device_paths_;
    bool camera_device_initialized_ = false;
};

} // namespace esp_brookesia::hal
