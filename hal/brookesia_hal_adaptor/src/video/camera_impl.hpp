/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <vector>

#include "brookesia/hal_interface/interfaces/video/camera.hpp"

namespace esp_brookesia::hal {

class VideoCameraImpl: public video::CameraIface {
public:
    explicit VideoCameraImpl(std::vector<DeviceInfo> devices);
    ~VideoCameraImpl() override = default;

    std::vector<DeviceInfo> get_device_infos() const override;

    static std::vector<DeviceInfo> discover_device_infos(bool &initialized_device);
    static void deinit_devices();

private:
    std::vector<DeviceInfo> devices_;
};

} // namespace esp_brookesia::hal
