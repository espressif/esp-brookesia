/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <utility>
#include <vector>

#include "unity.h"
#include "brookesia/hal_interface.hpp"

using namespace esp_brookesia;

namespace {

class TestCameraIface: public hal::video::CameraIface {
public:
    explicit TestCameraIface(std::vector<DeviceInfo> devices)
        : devices_(std::move(devices))
    {
    }

    std::vector<DeviceInfo> get_device_infos() const override
    {
        return devices_;
    }

private:
    std::vector<DeviceInfo> devices_;
};

} // namespace

TEST_CASE("video::CameraIface: device info describe", "[hal][interface]")
{
    std::vector<hal::video::CameraIface::DeviceInfo> devices = {
        {
            .id = 0,
            .name = "camera0",
            .device_path = "/dev/video0",
            .supported_formats = {hal::video::EncoderSinkFormat::MJPEG, hal::video::EncoderSinkFormat::RGB565},
        },
        {
            .id = 1,
            .name = "camera1",
            .device_path = "",
            .supported_formats = {},
        },
    };
    TestCameraIface iface(devices);

    auto device_infos = iface.get_device_infos();
    TEST_ASSERT_EQUAL(devices.size(), device_infos.size());
    TEST_ASSERT_EQUAL_STRING("/dev/video0", device_infos[0].device_path.c_str());

    auto json = BROOKESIA_DESCRIBE_TO_JSON(devices[0]);
    hal::video::CameraIface::DeviceInfo parsed;
    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_FROM_JSON(json, parsed));
    TEST_ASSERT_EQUAL_UINT32(devices[0].id, parsed.id);
    TEST_ASSERT_EQUAL_STRING(devices[0].name.c_str(), parsed.name.c_str());
    TEST_ASSERT_EQUAL_STRING(devices[0].device_path.c_str(), parsed.device_path.c_str());
    TEST_ASSERT_EQUAL(devices[0].supported_formats.size(), parsed.supported_formats.size());
    TEST_ASSERT_EQUAL(
        static_cast<int>(devices[0].supported_formats[0]),
        static_cast<int>(parsed.supported_formats[0])
    );
}
