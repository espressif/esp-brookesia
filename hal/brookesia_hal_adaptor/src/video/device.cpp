/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/hal_adaptor/macro_configs.h"
#if !BROOKESIA_HAL_ADAPTOR_VIDEO_DEVICE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif

#include <memory>

#include "private/utils.hpp"
#include "brookesia/lib_utils/plugin.hpp"
#include "brookesia/hal_adaptor/video/device.hpp"
#include "camera_impl.hpp"
#include "decoder_impl.hpp"
#include "encoder_impl.hpp"

namespace esp_brookesia::hal {

static std::string make_default_device_path(size_t id);

std::string VideoDevice::get_encoder_iface_name(size_t id)
{
    return video::EncoderIface::get_default_instance_name(id);
}

std::string VideoDevice::get_decoder_iface_name(size_t id)
{
    return video::DecoderIface::get_default_instance_name(id);
}

std::string VideoDevice::get_camera_iface_name(size_t id)
{
    return video::CameraIface::get_default_instance_name(id);
}

bool VideoDevice::probe()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    return true;
}

std::vector<InterfaceSpec> VideoDevice::get_interface_specs() const
{
    std::vector<InterfaceSpec> specs;
#if BROOKESIA_HAL_ADAPTOR_VIDEO_ENABLE_ENCODER_IMPL
    for (size_t i = 0; i < BROOKESIA_HAL_ADAPTOR_VIDEO_ENCODER_DEFAULT_NUM; i++) {
        specs.push_back({video::EncoderIface::NAME, get_encoder_iface_name(i)});
    }
#endif
#if BROOKESIA_HAL_ADAPTOR_VIDEO_ENABLE_DECODER_IMPL
    for (size_t i = 0; i < BROOKESIA_HAL_ADAPTOR_VIDEO_DECODER_DEFAULT_NUM; i++) {
        specs.push_back({video::DecoderIface::NAME, get_decoder_iface_name(i)});
    }
#endif
#if BROOKESIA_HAL_ADAPTOR_VIDEO_ENABLE_CAMERA_IMPL
    specs.push_back({video::CameraIface::NAME, get_camera_iface_name(0)});
#endif
    return specs;
}

bool VideoDevice::deinit_on_zero_references() const
{
    return !BROOKESIA_HAL_ADAPTOR_VIDEO_KEEP_DEVICE_INITIALIZED_ON_RELEASE;
}

bool VideoDevice::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

#if BROOKESIA_HAL_ADAPTOR_VIDEO_ENABLE_ENCODER_IMPL
    BROOKESIA_CHECK_FALSE_RETURN(init_cameras(), false, "Failed to init video cameras");
    BROOKESIA_CHECK_FALSE_RETURN(init_encoders(), false, "Failed to init video encoders");
#elif BROOKESIA_HAL_ADAPTOR_VIDEO_ENABLE_CAMERA_IMPL
    BROOKESIA_CHECK_FALSE_RETURN(init_cameras(), false, "Failed to init video cameras");
#endif
#if BROOKESIA_HAL_ADAPTOR_VIDEO_ENABLE_DECODER_IMPL
    BROOKESIA_CHECK_FALSE_RETURN(init_decoders(), false, "Failed to init video decoders");
#endif
    BROOKESIA_CHECK_FALSE_RETURN(!interfaces_.empty(), false, "No valid video interfaces initialized");

    return true;
}

void VideoDevice::on_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

#if BROOKESIA_HAL_ADAPTOR_VIDEO_ENABLE_CAMERA_IMPL
    if (camera_device_initialized_) {
        VideoCameraImpl::deinit_devices();
        camera_device_initialized_ = false;
    }
#endif
    interfaces_.clear();
    camera_device_paths_.clear();
}

bool VideoDevice::init_encoders()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

#if BROOKESIA_HAL_ADAPTOR_VIDEO_ENABLE_ENCODER_IMPL
    for (size_t i = 0; i < BROOKESIA_HAL_ADAPTOR_VIDEO_ENCODER_DEFAULT_NUM; i++) {
        auto iface_name = get_encoder_iface_name(i);
        if (is_iface_initialized(iface_name)) {
            continue;
        }

        std::shared_ptr<VideoEncoderImpl> iface = nullptr;
        BROOKESIA_CHECK_EXCEPTION_RETURN(
            iface = std::make_shared<VideoEncoderImpl>(i, get_default_encoder_device_path(i)), false,
            "Failed to create video encoder interface"
        );
        interfaces_.emplace(std::move(iface_name), std::move(iface));
    }
#endif

    return true;
}

bool VideoDevice::init_decoders()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

#if BROOKESIA_HAL_ADAPTOR_VIDEO_ENABLE_DECODER_IMPL
    for (size_t i = 0; i < BROOKESIA_HAL_ADAPTOR_VIDEO_DECODER_DEFAULT_NUM; i++) {
        auto iface_name = get_decoder_iface_name(i);
        if (is_iface_initialized(iface_name)) {
            continue;
        }

        std::shared_ptr<VideoDecoderImpl> iface = nullptr;
        BROOKESIA_CHECK_EXCEPTION_RETURN(
            iface = std::make_shared<VideoDecoderImpl>(i), false, "Failed to create video decoder interface"
        );
        interfaces_.emplace(std::move(iface_name), std::move(iface));
    }
#endif

    return true;
}

bool VideoDevice::init_cameras()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

#if BROOKESIA_HAL_ADAPTOR_VIDEO_ENABLE_CAMERA_IMPL
    auto iface_name = get_camera_iface_name(0);
    if (is_iface_initialized(iface_name)) {
        return true;
    }

    auto device_infos = VideoCameraImpl::discover_device_infos(camera_device_initialized_);
    camera_device_paths_.clear();
    for (const auto &info : device_infos) {
        if (!info.device_path.empty()) {
            camera_device_paths_.push_back(info.device_path);
        }
    }

    std::shared_ptr<VideoCameraImpl> iface = nullptr;
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        iface = std::make_shared<VideoCameraImpl>(std::move(device_infos)), false,
        "Failed to create video camera interface"
    );
    interfaces_.emplace(std::move(iface_name), std::move(iface));
#endif

    return true;
}

std::string VideoDevice::get_default_encoder_device_path(size_t id) const
{
    if (id < camera_device_paths_.size()) {
        return camera_device_paths_[id];
    }
    return make_default_device_path(id);
}

static std::string make_default_device_path(size_t id)
{
    return std::string(BROOKESIA_HAL_ADAPTOR_VIDEO_ENCODER_DEFAULT_DEVICE_PATH_PREFIX) + std::to_string(id);
}

#if BROOKESIA_HAL_ADAPTOR_ENABLE_VIDEO_DEVICE
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    Device, VideoDevice, VideoDevice::DEVICE_NAME, VideoDevice::get_instance(),
    BROOKESIA_HAL_ADAPTOR_VIDEO_DEVICE_PLUGIN_SYMBOL
);
#endif

} // namespace esp_brookesia::hal
