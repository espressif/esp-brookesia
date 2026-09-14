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
#include <cerrno>
#include <optional>
#include <string_view>
#include <utility>

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "boost/format.hpp"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_board_device.h"
#include "brookesia/hal_adaptor/board_manager.h"
#include "esp_board_manager_defs.h"
#include "esp_video_device.h"
#include "dev_camera.h"
#include "driver/gpio.h"
#include "brookesia/lib_utils/function_guard.hpp"
#include "brookesia/hal_interface/lifecycle.h"
#include "camera_impl.hpp"
#include "private/utils.hpp"
#if BROOKESIA_HAL_ADAPTOR_VIDEO_CAMERA_REQUIRES_EXPANSION_RUNTIME
#   include "../expansion/runtime.hpp"
#endif

#if BROOKESIA_HAL_ADAPTOR_VIDEO_ENABLE_CAMERA_IMPL

#ifdef CONFIG_ESP_BOARD_DEV_CAMERA_SUPPORT
#   include "linux/videodev2.h"
#endif

namespace esp_brookesia::hal {

namespace {

std::mutex camera_board_manager_mutex;

#ifdef CONFIG_ESP_BOARD_DEV_CAMERA_SUPPORT
// A failed final deinit outlives whichever discovery/encoder interface initiated
// it. No allocation or destructor retry is needed to transfer the remaining pin.
struct PendingCameraCleanup {
    bool camera_initialized = false;
    bool expansion_runtime_retained = false;
    uint64_t generation = 0;
};

PendingCameraCleanup pending_camera_cleanup;
uint64_t next_cleanup_generation = 1;

std::optional<video::EncoderSinkFormat> map_v4l2_pixelformat(uint32_t pixelformat);
std::vector<video::EncoderSinkFormat> enumerate_supported_formats(const char *device_path);
bool deinit_board_camera(std::string *error_message);
#endif

} // namespace

VideoCameraDeviceSession::~VideoCameraDeviceSession()
{
    std::string error_message;
    if (!close(&error_message)) {
        BROOKESIA_LOGE("Camera session cleanup failed: %1%", error_message);
    }
}

bool VideoCameraDeviceSession::is_board_camera_declared()
{
#ifdef CONFIG_ESP_BOARD_DEV_CAMERA_SUPPORT
    detail::LifecycleGuard lifecycle_guard;
    return brookesia_hal_board_manager_check_name(ESP_BOARD_DEVICE_NAME_CAMERA);
#else
    return false;
#endif
}

bool VideoCameraDeviceSession::get_declared_device_path(std::string &device_path)
{
#ifdef CONFIG_ESP_BOARD_DEV_CAMERA_SUPPORT
    detail::LifecycleGuard lifecycle_guard;
    void *config_ptr = nullptr;
    auto ret = brookesia_hal_board_manager_get_device_config(ESP_BOARD_DEVICE_NAME_CAMERA, &config_ptr);
    if ((ret != ESP_OK) || (config_ptr == nullptr)) {
        return false;
    }

    const auto *config = static_cast<const dev_camera_config_t *>(config_ptr);
    if (config->sub_type == nullptr) {
        return false;
    }

    const std::string_view sub_type(config->sub_type);
    if (sub_type == "csi") {
        device_path = ESP_VIDEO_MIPI_CSI_DEVICE_NAME;
    } else if (sub_type == "dvp") {
        device_path = ESP_VIDEO_DVP_DEVICE_NAME;
    } else if (sub_type == "spi") {
        device_path = ESP_VIDEO_SPI_DEVICE_NAME;
    } else {
        return false;
    }
    return true;
#else
    (void)device_path;
    return false;
#endif
}

bool VideoCameraDeviceSession::open(std::string &error_message)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    std::lock_guard lock(camera_board_manager_mutex);
    {
        detail::LifecycleGuard lifecycle_guard;
        if (!recover_pending_cleanup_locked(error_message)) {
            return false;
        }
    }
    if (!retain_expansion_runtime(error_message)) {
        return false;
    }

    bool result;
    {
        // Keep init, handle lookup, node validation and rollback one BM transaction.
        // A board's claim dependency may wait for scan readiness. Its scan path
        // must use already-owned hardware without acquiring this lifecycle gate.
        detail::LifecycleGuard lifecycle_guard;
        result = open_locked(error_message);
    }
    if (!result && !camera_initialized_) {
        release_expansion_runtime();
    }
    return result;
}

bool VideoCameraDeviceSession::open_locked(std::string &error_message)
{
    if (camera_initialized_) {
        return true;
    }

#ifdef CONFIG_ESP_BOARD_DEV_CAMERA_SUPPORT
    if (!is_board_camera_declared()) {
        error_message = "Board camera device is not declared";
        return false;
    }

    lib_utils::FunctionGuard rollback_guard([this]() {
        close_locked(nullptr);
    });

    auto ret = brookesia_hal_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_CAMERA);
    if (ret != ESP_OK) {
        error_message = std::string("Failed to initialize board camera: ") + esp_err_to_name(ret);
        return false;
    }
    camera_initialized_ = true;

    dev_camera_handle_t *camera_handle = nullptr;
    ret = brookesia_hal_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_CAMERA, reinterpret_cast<void **>(&camera_handle));
    if (ret != ESP_OK) {
        error_message = std::string("Failed to get board camera handle: ") + esp_err_to_name(ret);
        return false;
    }
    if ((camera_handle == nullptr) || (camera_handle->dev_path == nullptr) || (camera_handle->dev_path[0] == '\0')) {
        error_message = "Board camera device path is not available";
        return false;
    }

    device_path_ = camera_handle->dev_path;
    const int camera_fd = ::open(device_path_.c_str(), O_RDWR);
    if (camera_fd < 0) {
        const int open_errno = errno;
        error_message = (boost::format("Board camera device %1% is unavailable after initialization (errno=%2%)") %
                         device_path_ % open_errno).str();
        return false;
    }
    if (::close(camera_fd) != 0) {
        const int close_errno = errno;
        error_message = (boost::format("Failed to close board camera probe %1% (errno=%2%)") %
                         device_path_ % close_errno).str();
        // IDF VFS unregisters this descriptor even when the driver's close fails.
        // Never retry the descriptor; let board cleanup preserve any live video owner.
        return false;
    }

    rollback_guard.release();
    BROOKESIA_LOGI("Board camera session opened: %1%", device_path_);
    return true;
#else
    error_message = "Board camera support is disabled";
    return false;
#endif
}

bool VideoCameraDeviceSession::close(std::string *error_message)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    std::lock_guard lock(camera_board_manager_mutex);
    {
        detail::LifecycleGuard lifecycle_guard;
        if (!close_locked(error_message)) {
            return false;
        }
    }
    release_expansion_runtime();
    return true;
}

bool VideoCameraDeviceSession::close_locked(std::string *error_message)
{
#ifdef CONFIG_ESP_BOARD_DEV_CAMERA_SUPPORT
    if (pending_cleanup_generation_ != 0) {
        if (pending_camera_cleanup.camera_initialized &&
                (pending_camera_cleanup.generation == pending_cleanup_generation_)) {
            if (error_message) {
                *error_message = "Camera cleanup is pending; a subsequent open must recover it";
            }
            return false;
        }
        pending_cleanup_generation_ = 0;
    }
    if (camera_initialized_) {
        if (!deinit_board_camera(error_message)) {
            // The DVP callback retains its wrapper on errors. It either permits
            // video cleanup on a later open, or returns a latched terminal error
            // without touching an ambiguously consumed I2C handle again.
            pending_camera_cleanup.camera_initialized = true;
            pending_camera_cleanup.expansion_runtime_retained = expansion_runtime_retained_;
            pending_camera_cleanup.generation = next_cleanup_generation++;
            if (next_cleanup_generation == 0) {
                ++next_cleanup_generation;
            }
            pending_cleanup_generation_ = pending_camera_cleanup.generation;
            camera_initialized_ = false;
            expansion_runtime_retained_ = false;
            device_path_.clear();
            return false;
        }
        camera_initialized_ = false;
    }
#endif
    device_path_.clear();
    return true;
}

bool VideoCameraDeviceSession::recover_pending_cleanup_locked(std::string &error_message)
{
#ifdef CONFIG_ESP_BOARD_DEV_CAMERA_SUPPORT
    if (pending_camera_cleanup.camera_initialized) {
        if (!deinit_board_camera(&error_message)) {
            return false;
        }
        // Transfer the existing runtime consumer instead of briefly stopping
        // scanning between recovery and this session's new camera initialization.
        expansion_runtime_retained_ = pending_camera_cleanup.expansion_runtime_retained;
        pending_camera_cleanup = {};
    }
    pending_cleanup_generation_ = 0;
#else
    (void)error_message;
#endif
    return true;
}

bool VideoCameraDeviceSession::retain_expansion_runtime(std::string &error_message)
{
#if BROOKESIA_HAL_ADAPTOR_VIDEO_CAMERA_REQUIRES_EXPANSION_RUNTIME
    if (expansion_runtime_retained_) {
        return true;
    }

    auto &runtime = expansion::detail::ExpansionRuntime::get_instance();
    if (runtime.has_providers()) {
        if (!runtime.retain_consumer(&error_message)) {
            return false;
        }
        expansion_runtime_retained_ = true;
    }
#else
    (void)error_message;
#endif
    return true;
}

void VideoCameraDeviceSession::release_expansion_runtime()
{
#if BROOKESIA_HAL_ADAPTOR_VIDEO_CAMERA_REQUIRES_EXPANSION_RUNTIME
    if (expansion_runtime_retained_) {
        expansion_runtime_retained_ = false;
        expansion::detail::ExpansionRuntime::get_instance().release_consumer();
    }
#endif
}

VideoCameraImpl::~VideoCameraImpl() = default;

std::vector<VideoCameraImpl::DeviceInfo> VideoCameraImpl::get_device_infos() const
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    std::lock_guard discovery_lock(discovery_mutex_);
    std::vector<DeviceInfo> devices;
#ifdef CONFIG_ESP_BOARD_DEV_CAMERA_SUPPORT
    std::string error_message;
    VideoCameraDeviceSession session;
    if (!session.open(error_message)) {
        BROOKESIA_LOGD("Camera discovery skipped: %1%", error_message);
        return devices;
    }

    auto supported_formats = enumerate_supported_formats(session.get_device_path().c_str());
    devices.push_back({
        .id = 0,
        .name = ESP_BOARD_DEVICE_NAME_CAMERA,
        .device_path = session.get_device_path(),
        .supported_formats = std::move(supported_formats),
    });
    BROOKESIA_LOGI(
        "Discovered camera device path: %1% (supported source formats: %2%)",
        devices.back().device_path, devices.back().supported_formats.size()
    );

    if (!session.close(&error_message)) {
        BROOKESIA_LOGW("Camera discovery cleanup failed: %1%", error_message);
        devices.clear();
    }
#else
    BROOKESIA_LOGD("Board camera support is disabled");
#endif
    return devices;
}

#ifdef CONFIG_ESP_BOARD_DEV_CAMERA_SUPPORT
namespace {

bool deinit_board_camera(std::string *error_message)
{
    const auto ret = brookesia_hal_board_manager_deinit_device_by_name(ESP_BOARD_DEVICE_NAME_CAMERA);
    if (ret == ESP_OK) {
        return true;
    }
    if (error_message != nullptr) {
        *error_message = std::string("Failed to deinitialize board camera: ") + esp_err_to_name(ret);
    }
    return false;
}

std::optional<video::EncoderSinkFormat> map_v4l2_pixelformat(uint32_t pixelformat)
{
    switch (pixelformat) {
    case V4L2_PIX_FMT_YUYV:
    case V4L2_PIX_FMT_UYVY:
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

std::vector<video::EncoderSinkFormat> enumerate_supported_formats(const char *device_path)
{
    std::vector<video::EncoderSinkFormat> formats;

    int fd = ::open(device_path, O_RDONLY);
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
        if (mapped.has_value() &&
                (std::find(formats.begin(), formats.end(), mapped.value()) == formats.end())) {
            formats.push_back(mapped.value());
        }
    }

    ::close(fd);
    return formats;
}

} // namespace
#endif // CONFIG_ESP_BOARD_DEV_CAMERA_SUPPORT

} // namespace esp_brookesia::hal

#endif // BROOKESIA_HAL_ADAPTOR_VIDEO_ENABLE_CAMERA_IMPL
