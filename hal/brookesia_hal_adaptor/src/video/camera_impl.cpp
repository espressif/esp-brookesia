/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/hal_adaptor/macro_configs.h"
#if !BROOKESIA_HAL_ADAPTOR_VIDEO_DEVICE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif

#include <algorithm>
#include <optional>
#include <utility>

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "esp_err.h"
#include "esp_board_manager.h"
#include "esp_board_manager_defs.h"
#include "dev_camera.h"
#include "camera_impl.hpp"
#include "private/utils.hpp"

#if BROOKESIA_HAL_ADAPTOR_VIDEO_ENABLE_CAMERA_IMPL

#ifdef CONFIG_ESP_BOARD_DEV_CAMERA_SUPPORT
#   include "linux/videodev2.h"
#endif

namespace esp_brookesia::hal {

#ifdef CONFIG_ESP_BOARD_DEV_CAMERA_SUPPORT
namespace {

/**
 * @brief Map a V4L2 pixel format (fourcc) to the framework encoder source format.
 *
 * The mapping mirrors esp_capture's V4L2 source so that the formats reported here
 * match the ones the capture pipeline can actually negotiate.
 */
std::optional<video::EncoderSinkFormat> map_v4l2_pixelformat(uint32_t pixelformat)
{
    switch (pixelformat) {
    case V4L2_PIX_FMT_YUYV:
        return video::EncoderSinkFormat::YUV422;
    case V4L2_PIX_FMT_RGB565:
        return video::EncoderSinkFormat::RGB565;
    case V4L2_PIX_FMT_MJPEG:
    case V4L2_PIX_FMT_JPEG:
        return video::EncoderSinkFormat::MJPEG;
    case V4L2_PIX_FMT_YUV420:
        return video::EncoderSinkFormat::O_UYY_E_VYY;
    default:
        return std::nullopt;
    }
}

/**
 * @brief Enumerate the source formats supported by a V4L2 capture device.
 *
 * The device node must already be registered (board camera initialized). The
 * descriptor is opened read-only and closed again, keeping the esp_video
 * reference count balanced.
 */
std::vector<video::EncoderSinkFormat> enumerate_supported_formats(const char *device_path)
{
    std::vector<video::EncoderSinkFormat> formats;

    int fd = open(device_path, O_RDONLY);
    if (fd < 0) {
        BROOKESIA_LOGW("Failed to open camera device %1% for format enumeration", device_path);
        return formats;
    }

    for (uint32_t index = 0; ; index++) {
        struct v4l2_fmtdesc fmtdesc = {};
        fmtdesc.index = index;
        fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) != 0) {
            break;
        }
        auto mapped = map_v4l2_pixelformat(fmtdesc.pixelformat);
        if (!mapped.has_value()) {
            continue;
        }
        if (std::find(formats.begin(), formats.end(), mapped.value()) == formats.end()) {
            formats.push_back(mapped.value());
        }
    }

    close(fd);

    return formats;
}

} // namespace
#endif // CONFIG_ESP_BOARD_DEV_CAMERA_SUPPORT

VideoCameraImpl::VideoCameraImpl(std::vector<DeviceInfo> devices)
    : devices_(std::move(devices))
{
}

std::vector<VideoCameraImpl::DeviceInfo> VideoCameraImpl::get_device_infos() const
{
    return devices_;
}

std::vector<VideoCameraImpl::DeviceInfo> VideoCameraImpl::discover_device_infos(bool &initialized_device)
{
    BROOKESIA_LOG_TRACE_GUARD();

    initialized_device = false;
    std::vector<DeviceInfo> devices;

#ifdef CONFIG_ESP_BOARD_DEV_CAMERA_SUPPORT
    if (!esp_board_manager_check_name(ESP_BOARD_DEVICE_NAME_CAMERA)) {
        BROOKESIA_LOGW("Camera device not found, skip discovery");
        return devices;
    }

    auto ret = esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_CAMERA);
    if (ret != ESP_OK) {
        BROOKESIA_LOGW("Failed to initialize camera device: %1%", esp_err_to_name(ret));
        return devices;
    }
    initialized_device = true;

    dev_camera_handle_t *camera_handle = nullptr;
    ret = esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_CAMERA, reinterpret_cast<void **>(&camera_handle));
    if (ret != ESP_OK) {
        BROOKESIA_LOGW("Failed to get camera device handle: %1%", esp_err_to_name(ret));
        return devices;
    }
    if ((camera_handle == nullptr) || (camera_handle->dev_path == nullptr) || (camera_handle->dev_path[0] == '\0')) {
        BROOKESIA_LOGW("Camera device path is not available");
        return devices;
    }

    auto supported_formats = enumerate_supported_formats(camera_handle->dev_path);

    devices.push_back({
        .id = 0,
        .name = ESP_BOARD_DEVICE_NAME_CAMERA,
        .device_path = camera_handle->dev_path,
        .supported_formats = std::move(supported_formats),
    });
    BROOKESIA_LOGI(
        "Discovered camera device path: %1% (supported source formats: %2%)",
        camera_handle->dev_path, devices.back().supported_formats.size()
    );
#else
    BROOKESIA_LOGD("Board camera support is disabled");
#endif

    return devices;
}

void VideoCameraImpl::deinit_devices()
{
    BROOKESIA_LOG_TRACE_GUARD();

#ifdef CONFIG_ESP_BOARD_DEV_CAMERA_SUPPORT
    auto ret = esp_board_manager_deinit_device_by_name(ESP_BOARD_DEVICE_NAME_CAMERA);
    if (ret != ESP_OK) {
        BROOKESIA_LOGW("Failed to deinitialize camera device: %1%", esp_err_to_name(ret));
    }
#endif
}

} // namespace esp_brookesia::hal

#endif // BROOKESIA_HAL_ADAPTOR_VIDEO_ENABLE_CAMERA_IMPL
