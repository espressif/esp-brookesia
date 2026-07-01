/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file camera.hpp
 * @brief Declares camera discovery HAL interface.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/hal_interface/interface.hpp"
#include "brookesia/hal_interface/interfaces/video/processor.hpp"

namespace esp_brookesia::hal::video {

/**
 * @brief Camera discovery interface for querying available video capture devices.
 */
class CameraIface: public Interface {
public:
    static constexpr const char *NAME = "VideoCamera";

    /**
     * @brief Get the default camera discovery interface instance name.
     *
     * @param[in] id Camera discovery interface id.
     * @return Default instance name.
     */
    static std::string get_default_instance_name(size_t id)
    {
        return std::string("Video:Camera:") + std::to_string(id);
    }

    /**
     * @brief One camera device discovered by the platform.
     */
    struct DeviceInfo {
        uint32_t id = 0;
        std::string name;
        std::string device_path;
        std::vector<EncoderSinkFormat> supported_formats;
    };

    CameraIface()
        : Interface(NAME)
    {
    }

    virtual ~CameraIface() = default;

    /**
     * @brief Get discovered camera devices.
     *
     * @return Camera device list. Empty when no camera path is available.
     */
    virtual std::vector<DeviceInfo> get_device_infos() const = 0;

};

BROOKESIA_DESCRIBE_STRUCT(CameraIface::DeviceInfo, (), (id, name, device_path, supported_formats));

} // namespace esp_brookesia::hal::video
