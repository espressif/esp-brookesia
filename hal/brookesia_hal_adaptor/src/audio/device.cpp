/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_board_manager_includes.h"
#include "brookesia/hal_adaptor/macro_configs.h"
#if !BROOKESIA_HAL_ADAPTOR_AUDIO_DEVICE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/lib_utils/function_guard.hpp"
#include "brookesia/hal_adaptor/audio/device.hpp"
#include "codec_player_impl.hpp"
#include "codec_recorder_impl.hpp"

namespace esp_brookesia::hal {

bool AudioDevice::set_codec_recorder_info(AudioCodecRecorderIface::Info info)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: info(%1%)", BROOKESIA_DESCRIBE_TO_STR(info));

    BROOKESIA_CHECK_FALSE_RETURN(
        !is_iface_initialized(CODEC_RECORDER_IMPL_NAME), false, "Should called before initializing codec recorder"
    );

    codec_recorder_info_ = info;

    return true;
}

bool AudioDevice::probe()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    return true;
}

bool AudioDevice::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

#if BROOKESIA_HAL_ADAPTOR_AUDIO_ENABLE_CODEC_PLAYER_IMPL
    BROOKESIA_CHECK_FALSE_EXECUTE(init_codec_player(), {}, { BROOKESIA_LOGE("Failed to init codec player"); });
#endif
#if BROOKESIA_HAL_ADAPTOR_AUDIO_ENABLE_CODEC_RECORDER_IMPL
    BROOKESIA_CHECK_FALSE_EXECUTE(init_codec_recorder(), {}, { BROOKESIA_LOGE("Failed to init codec recorder"); });
#endif
    BROOKESIA_CHECK_FALSE_RETURN(!interfaces_.empty(), false, "No valid audio interfaces initialized");

    return true;
}

void AudioDevice::on_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

#if BROOKESIA_HAL_ADAPTOR_AUDIO_ENABLE_CODEC_RECORDER_IMPL
    deinit_codec_recorder();
#endif
#if BROOKESIA_HAL_ADAPTOR_AUDIO_ENABLE_CODEC_PLAYER_IMPL
    deinit_codec_player();
#endif
}

#if BROOKESIA_HAL_ADAPTOR_AUDIO_ENABLE_CODEC_PLAYER_IMPL
bool AudioDevice::init_codec_player()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_iface_initialized(CODEC_PLAYER_IMPL_NAME)) {
        BROOKESIA_LOGD("Codec player is already initialized, skip");
        return true;
    }

    std::shared_ptr<AudioCodecPlayerImpl> iface = nullptr;
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        iface = std::make_shared<AudioCodecPlayerImpl>(), false, "Failed to create codec player interface"
    );
    lib_utils::FunctionGuard iface_delete_guard([this, &iface]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        iface.reset();
    });

    BROOKESIA_CHECK_FALSE_RETURN(iface->is_valid(), false, "Failed to create codec player interface");

    interfaces_.emplace(CODEC_PLAYER_IMPL_NAME, iface);

    iface_delete_guard.release();

    return true;
}

void AudioDevice::deinit_codec_player()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_iface_initialized(CODEC_PLAYER_IMPL_NAME)) {
        BROOKESIA_LOGD("Codec player is not initialized, skip");
        return;
    }

    interfaces_.erase(CODEC_PLAYER_IMPL_NAME);
}
#endif // BROOKESIA_HAL_ADAPTOR_AUDIO_ENABLE_CODEC_PLAYER_IMPL

#if BROOKESIA_HAL_ADAPTOR_AUDIO_ENABLE_CODEC_RECORDER_IMPL
bool AudioDevice::init_codec_recorder()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_iface_initialized(CODEC_RECORDER_IMPL_NAME)) {
        BROOKESIA_LOGD("Codec recorder is already initialized, skip");
        return true;
    }

    std::shared_ptr<AudioCodecRecorderImpl> iface = nullptr;
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        iface = std::make_shared<AudioCodecRecorderImpl>(codec_recorder_info_), false,
        "Failed to create codec recorder interface"
    );
    lib_utils::FunctionGuard iface_delete_guard([this, &iface]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        iface.reset();
    });

    BROOKESIA_CHECK_FALSE_RETURN(iface->is_valid(), false, "Failed to create codec recorder interface");

    interfaces_.emplace(CODEC_RECORDER_IMPL_NAME, iface);

    iface_delete_guard.release();

    return true;
}

void AudioDevice::deinit_codec_recorder()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_iface_initialized(CODEC_RECORDER_IMPL_NAME)) {
        BROOKESIA_LOGD("Codec recorder is not initialized, skip");
        return;
    }

    interfaces_.erase(CODEC_RECORDER_IMPL_NAME);
}
#endif // BROOKESIA_HAL_ADAPTOR_AUDIO_ENABLE_CODEC_RECORDER_IMPL

#if BROOKESIA_HAL_ADAPTOR_ENABLE_AUDIO_DEVICE
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    Device, AudioDevice, AudioDevice::DEVICE_NAME, AudioDevice::get_instance(),
    BROOKESIA_HAL_ADAPTOR_AUDIO_DEVICE_PLUGIN_SYMBOL
);
#endif

} // namespace esp_brookesia::hal
