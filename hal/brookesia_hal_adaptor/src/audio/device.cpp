/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <utility>

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
#include "processor_impl.hpp"

namespace esp_brookesia::hal {

bool AudioDevice::set_codec_recorder_info(audio::CodecRecorderIface::Info info)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: info(%1%)", BROOKESIA_DESCRIBE_TO_STR(info));

    BROOKESIA_CHECK_FALSE_RETURN(
        !is_iface_initialized(CODEC_RECORDER_IMPL_NAME), false, "Should called before initializing codec recorder"
    );

    codec_recorder_info_ = info;

    return true;
}

bool AudioDevice::set_processor_config(AudioProcessorConfig config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: config(%1%)", BROOKESIA_DESCRIBE_TO_STR(config));

    BROOKESIA_CHECK_FALSE_RETURN(
        !is_iface_initialized(PLAYBACK_IMPL_NAME), false, "Should be called before initializing audio playback"
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        !is_iface_initialized(ENCODER_IMPL_NAME), false, "Should be called before initializing audio encoder"
    );
    BROOKESIA_CHECK_FALSE_RETURN(
        !is_iface_initialized(DECODER_IMPL_NAME), false, "Should be called before initializing audio decoder"
    );

    processor_config_ = std::move(config);

    return true;
}

bool AudioDevice::probe()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    return true;
}

std::vector<InterfaceSpec> AudioDevice::get_interface_specs() const
{
    std::vector<InterfaceSpec> specs;
#if BROOKESIA_HAL_ADAPTOR_AUDIO_ENABLE_CODEC_PLAYER_IMPL
    specs.push_back({audio::CodecPlayerIface::NAME, CODEC_PLAYER_IMPL_NAME});
#endif
#if BROOKESIA_HAL_ADAPTOR_AUDIO_ENABLE_CODEC_RECORDER_IMPL
    specs.push_back({audio::CodecRecorderIface::NAME, CODEC_RECORDER_IMPL_NAME});
#endif
#if BROOKESIA_HAL_ADAPTOR_AUDIO_ENABLE_PROCESSOR_IMPL
    specs.push_back({audio::PlaybackIface::NAME, PLAYBACK_IMPL_NAME});
    specs.push_back({audio::EncoderIface::NAME, ENCODER_IMPL_NAME});
    specs.push_back({audio::DecoderIface::NAME, DECODER_IMPL_NAME});
#endif
    return specs;
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
#if BROOKESIA_HAL_ADAPTOR_AUDIO_ENABLE_PROCESSOR_IMPL
    BROOKESIA_CHECK_FALSE_EXECUTE(init_processor(), {}, { BROOKESIA_LOGE("Failed to init audio processor"); });
#endif
    BROOKESIA_CHECK_FALSE_RETURN(!interfaces_.empty(), false, "No valid audio interfaces initialized");

    return true;
}

void AudioDevice::on_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

#if BROOKESIA_HAL_ADAPTOR_AUDIO_ENABLE_PROCESSOR_IMPL
    deinit_processor();
#endif
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

#if BROOKESIA_HAL_ADAPTOR_AUDIO_ENABLE_PROCESSOR_IMPL
bool AudioDevice::init_processor()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (processor_core_ != nullptr) {
        BROOKESIA_LOGD("Audio processor is already initialized, skip");
        return true;
    }

    auto player_iface = std::dynamic_pointer_cast<audio::CodecPlayerIface>(interfaces_[CODEC_PLAYER_IMPL_NAME]);
    auto recorder_iface = std::dynamic_pointer_cast<audio::CodecRecorderIface>(interfaces_[CODEC_RECORDER_IMPL_NAME]);
    BROOKESIA_CHECK_NULL_RETURN(player_iface, false, "Codec player is not initialized");
    BROOKESIA_CHECK_NULL_RETURN(recorder_iface, false, "Codec recorder is not initialized");

    std::shared_ptr<AudioProcessorCore> core = nullptr;
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        core = std::make_shared<AudioProcessorCore>(player_iface, recorder_iface, processor_config_), false,
        "Failed to create audio processor core"
    );
    lib_utils::FunctionGuard iface_delete_guard([this, &core]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        core.reset();
    });

    std::shared_ptr<AudioPlaybackImpl> playback_iface = nullptr;
    std::shared_ptr<AudioEncoderImpl> encoder_iface = nullptr;
    std::shared_ptr<AudioDecoderImpl> decoder_iface = nullptr;
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        playback_iface = std::make_shared<AudioPlaybackImpl>(core), false, "Failed to create audio playback interface"
    );
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        encoder_iface = std::make_shared<AudioEncoderImpl>(core), false, "Failed to create audio encoder interface"
    );
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        decoder_iface = std::make_shared<AudioDecoderImpl>(core), false, "Failed to create audio decoder interface"
    );

    processor_core_ = core;
    interfaces_.emplace(PLAYBACK_IMPL_NAME, playback_iface);
    interfaces_.emplace(ENCODER_IMPL_NAME, encoder_iface);
    interfaces_.emplace(DECODER_IMPL_NAME, decoder_iface);

    iface_delete_guard.release();

    return true;
}

void AudioDevice::deinit_processor()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (processor_core_ == nullptr) {
        BROOKESIA_LOGD("Audio processor is not initialized, skip");
        return;
    }

    interfaces_.erase(DECODER_IMPL_NAME);
    interfaces_.erase(ENCODER_IMPL_NAME);
    interfaces_.erase(PLAYBACK_IMPL_NAME);
    processor_core_.reset();
}
#endif // BROOKESIA_HAL_ADAPTOR_AUDIO_ENABLE_PROCESSOR_IMPL

#if BROOKESIA_HAL_ADAPTOR_ENABLE_AUDIO_DEVICE
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    Device, AudioDevice, AudioDevice::DEVICE_NAME, AudioDevice::get_instance(),
    BROOKESIA_HAL_ADAPTOR_AUDIO_DEVICE_PLUGIN_SYMBOL
);
#endif

} // namespace esp_brookesia::hal
