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

bool GeneralServices::init(std::shared_ptr<esp_brookesia::lib_utils::TaskScheduler> task_scheduler)
{
    BROOKESIA_CHECK_NULL_RETURN(task_scheduler, false, "Task scheduler is not available");

    if (is_initialized()) {
        BROOKESIA_LOGW("General services is already initialized");
        return true;
    }

    BROOKESIA_LOGI("Initializing service manager...");

    auto &service_manager = service::ServiceManager::get_instance();
    BROOKESIA_CHECK_FALSE_RETURN(service_manager.init(), false, "Failed to initialize service manager");
    BROOKESIA_CHECK_FALSE_RETURN(service_manager.start(), false, "Failed to start service manager");

    task_scheduler_ = task_scheduler;

    BROOKESIA_LOGI("Service manager started successfully");

    return true;
}

void GeneralServices::init_audio()
{
    BROOKESIA_CHECK_FALSE_EXIT(is_initialized(), "General services is not initialized");

    hal::AudioProcessorConfig processor_config{
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
    BROOKESIA_CHECK_FALSE_EXIT(
        hal::AudioDevice::get_instance().set_processor_config(std::move(processor_config)),
        "Failed to configure audio processor"
    );

    if (AudioPlaybackHelper::is_available()) {
        auto &service_manager = service::ServiceManager::get_instance();
        auto playback_binding = service_manager.bind(AudioPlaybackHelper::get_name().data());
        BROOKESIA_CHECK_FALSE_EXIT(playback_binding.is_valid(), "Failed to bind audio playback service");
        service_bindings_.push_back(std::move(playback_binding));
    }
}

void GeneralServices::start_device()
{
    BROOKESIA_CHECK_FALSE_EXIT(is_initialized(), "General services is not initialized");

    if (!DeviceHelper::is_available()) {
        BROOKESIA_LOGW("Device service is not available");
        return;
    }

    auto &service_manager = service::ServiceManager::get_instance();
    auto binding = service_manager.bind(DeviceHelper::get_name().data());
    if (!binding.is_valid()) {
        BROOKESIA_LOGE("Failed to bind Device service");
    } else {
        service_bindings_.push_back(std::move(binding));
    }
}
