/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "sdkconfig.h"
#include "brookesia/hal_interface.hpp"
#include "brookesia/hal_adaptor.hpp"
#include "brookesia/service_manager.hpp"
#include "brookesia/service_helper.hpp"
#include "private/utils.hpp"
#include "general_services.hpp"

using namespace esp_brookesia;

using AudioPlaybackHelper = esp_brookesia::service::helper::AudioPlayback;
using DeviceHelper = esp_brookesia::service::helper::Device;

constexpr const char *AUDIO_WAKEUP_WORD_MODEL_PARTITION_LABEL = "model";
constexpr const char *AUDIO_WAKEUP_WORD_MN_LANGUAGE = "cn";
constexpr uint32_t AUDIO_WAKEUP_START_TIMEOUT_MS = 30000;
constexpr uint32_t AUDIO_WAKEUP_END_TIMEOUT_MS = 10000;

bool GeneralServices::init()
{
    auto &service_manager = service::ServiceManager::get_instance();
    BROOKESIA_CHECK_FALSE_RETURN(service_manager.init(), false, "Failed to initialize service manager");

    BROOKESIA_CHECK_FALSE_RETURN(init_audio(), false, "Failed to initialize audio");
    BROOKESIA_CHECK_FALSE_RETURN(service_manager.start(), false, "Failed to start service manager");

    BROOKESIA_LOGI("Service manager started successfully");

    return true;
}

bool GeneralServices::init_audio()
{
#if CONFIG_BROOKESIA_HAL_ADAPTOR_AUDIO_ENABLE_AUDIO_PROCESSOR_IMPL
    hal::AudioProcessorConfig processor_config {
        .playback = {
            .player_task = {
                .core_id = 0,
                .priority = 5,
                .stack_size = 4 * 1024,
#if CONFIG_SPIRAM_XIP_FROM_PSRAM
                .stack_in_ext = true,
#else
                .stack_in_ext = false,
#endif
            },
        },
        .encoder = {},
        .decoder = {},
        .afe = {
            .vad = hal::AudioProcessorAFE_VAD_Config{},
            .wakenet = hal::AudioProcessorAFE_WakeNetConfig{
                .model_partition_label = AUDIO_WAKEUP_WORD_MODEL_PARTITION_LABEL,
                .mn_language = AUDIO_WAKEUP_WORD_MN_LANGUAGE,
                .start_timeout_ms = AUDIO_WAKEUP_START_TIMEOUT_MS,
                .end_timeout_ms = AUDIO_WAKEUP_END_TIMEOUT_MS,
            },
        },
    };
    BROOKESIA_CHECK_FALSE_RETURN(
        hal::AudioDevice::get_instance().set_processor_config(std::move(processor_config)),
        false,
        "Failed to configure audio processor"
    );
    BROOKESIA_LOGI(
        "Audio processor AFE configured: model_partition(%1%), mn_language(%2%), wake_start_timeout_ms(%3%), "
        "wake_end_timeout_ms(%4%); agent AudioEncoder defaults keep enable_afe=true",
        AUDIO_WAKEUP_WORD_MODEL_PARTITION_LABEL,
        AUDIO_WAKEUP_WORD_MN_LANGUAGE,
        AUDIO_WAKEUP_START_TIMEOUT_MS,
        AUDIO_WAKEUP_END_TIMEOUT_MS
    );
#endif

    return true;
}

bool GeneralServices::start_audio_services()
{
    if (!AudioPlaybackHelper::is_available()) {
        BROOKESIA_LOGW("Audio playback service is not available");
        return true;
    }

    auto &service_manager = service::ServiceManager::get_instance();
    static auto playback_binding = service_manager.bind(AudioPlaybackHelper::get_name().data());
    BROOKESIA_CHECK_FALSE_RETURN(playback_binding.is_valid(), false, "Failed to bind audio playback service");

    return true;
}
