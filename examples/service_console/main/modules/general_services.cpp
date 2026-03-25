/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "sdkconfig.h"
#include "brookesia/service_manager.hpp"
#include "brookesia/service_helper.hpp"
#include "private/utils.hpp"
#include "general_services.hpp"

using namespace esp_brookesia;

using SNTPHelper = esp_brookesia::service::helper::SNTP;
using AudioHelper = esp_brookesia::service::helper::Audio;
using NVSHelper = esp_brookesia::service::helper::NVS;
using WifiHelper = esp_brookesia::service::helper::Wifi;

constexpr const char *AUDIO_WAKEUP_WORD_MODEL_PARTITION_LABEL = "model";
constexpr const char *AUDIO_WAKEUP_WORD_MN_LANGUAGE = "cn";
constexpr uint32_t AUDIO_WAKEUP_WORD_START_TIMEOUT_MS = 10000;
constexpr uint32_t AUDIO_WAKEUP_WORD_END_TIMEOUT_MS = 30000;

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

    is_initialized_.store(true);
    task_scheduler_ = task_scheduler;

    BROOKESIA_LOGI("Service manager started successfully");

    return true;
}

void GeneralServices::init_audio()
{
    BROOKESIA_CHECK_FALSE_EXIT(is_initialized(), "General services is not initialized");

    if (!AudioHelper::is_available()) {
        BROOKESIA_LOGW("Audio service is not available, skip initialization");
        return;
    }

#if CONFIG_EXAMPLE_SERVICES_ENABLE_AUDIO
    BROOKESIA_LOGI("Initializing audio service...");

#if !CONFIG_EXAMPLE_ENABLE_BOARD_MANAGER
    BROOKESIA_LOGE("Audio service only supported when board manager is enabled, skip initialization");
#else
    // Configure the AFE
    AudioHelper::AFE_Config afe_config{
        .vad = AudioHelper::AFE_VAD_Config{},
#if CONFIG_EXAMPLE_AGENTS_ENABLE_AUDIO_WAKEUP_WORD
        .wakenet = AudioHelper::AFE_WakeNetConfig{
            .model_partition_label = AUDIO_WAKEUP_WORD_MODEL_PARTITION_LABEL,
            .mn_language = AUDIO_WAKEUP_WORD_MN_LANGUAGE,
            .start_timeout_ms = AUDIO_WAKEUP_WORD_START_TIMEOUT_MS,
            .end_timeout_ms = AUDIO_WAKEUP_WORD_END_TIMEOUT_MS,
        },
#endif
    };
    auto set_afe_result = AudioHelper::call_function_sync(
                              AudioHelper::FunctionId::SetAFE_Config, BROOKESIA_DESCRIBE_TO_JSON(afe_config).as_object()
                          );
    if (!set_afe_result) {
        BROOKESIA_LOGE("Failed to set audio AFE config: %1%", set_afe_result.error());
        return;
    }

    BROOKESIA_LOGI("Audio service initialized successfully");
#endif // CONFIG_EXAMPLE_ENABLE_BOARD_MANAGER
#endif // CONFIG_EXAMPLE_SERVICES_ENABLE_AUDIO
}

void GeneralServices::start_sntp()
{
    BROOKESIA_CHECK_FALSE_EXIT(is_initialized(), "General services is not initialized");

    if (!SNTPHelper::is_available()) {
        BROOKESIA_LOGW("SNTP service is not available");
        return;
    }

    auto &service_manager = service::ServiceManager::get_instance();
    auto binding = service_manager.bind(SNTPHelper::get_name().data());
    if (!binding.is_valid()) {
        BROOKESIA_LOGE("Failed to bind SNTP service");
    } else {
        service_bindings_.push_back(std::move(binding));
    }
}

void GeneralServices::start_nvs()
{
    BROOKESIA_CHECK_FALSE_EXIT(is_initialized(), "General services is not initialized");

    if (!NVSHelper::is_available()) {
        BROOKESIA_LOGW("NVS service is not available");
        return;
    }

    auto &service_manager = service::ServiceManager::get_instance();
    auto binding = service_manager.bind(NVSHelper::get_name().data());
    if (!binding.is_valid()) {
        BROOKESIA_LOGE("Failed to bind NVS service");
    } else {
        service_bindings_.push_back(std::move(binding));
    }
}
