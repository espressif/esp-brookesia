/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/service_device/macro_configs.h"
#if !BROOKESIA_SERVICE_DEVICE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif

#include "brookesia/hal_interface/interfaces/video/camera.hpp"
#include "brookesia/service_device/service_device.hpp"
#include "private/utils.hpp"

namespace esp_brookesia::service {

std::expected<boost::json::array, std::string> Device::function_get_camera_device_infos()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!hal::has_interface(hal::video::CameraIface::NAME)) {
        return std::unexpected("Camera interface is not available");
    }

    auto camera_iface = hal::acquire_first_interface<hal::video::CameraIface>();
    if (!camera_iface) {
        return std::unexpected("Failed to acquire camera interface");
    }

    return BROOKESIA_DESCRIBE_TO_JSON(camera_iface->get_device_infos()).as_array();
}

} // namespace esp_brookesia::service
