/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "sdkconfig.h"
#include "brookesia/lib_utils.hpp"
#include "brookesia/service_manager.hpp"
#include "brookesia/service_helper.hpp"
#if CONFIG_EXAMPLE_ENABLE_BOARD_MANAGER
#   include "board.hpp"
#endif
#include "services.hpp"

using namespace esp_brookesia;
using NVSHelper = service::helper::NVS;
using SNTPHelper = service::helper::SNTP;

/* Keep service bindings to avoid frequent start and stop of services */
static std::vector<service::ServiceBinding> service_bindings;

void services_init()
{
    service::ServiceManager::get_instance().start();

#if CONFIG_EXAMPLE_SERVICES_ENABLE_NVS
    /* NVS service */
    {
        auto binding = service::ServiceManager::get_instance().bind(NVSHelper::get_name().data());
        if (!binding.is_valid()) {
            BROOKESIA_LOGE("Failed to bind NVS service");
        } else {
            service_bindings.push_back(std::move(binding));
        }
    }
#endif // CONFIG_EXAMPLE_SERVICES_ENABLE_NVS

#if CONFIG_EXAMPLE_SERVICES_ENABLE_SNTP
    /* SNTP service */
    {
        auto binding = service::ServiceManager::get_instance().bind(SNTPHelper::get_name().data());
        if (!binding.is_valid()) {
            BROOKESIA_LOGE("Failed to bind SNTP service");
        } else {
            service_bindings.push_back(std::move(binding));
        }
    }
#endif // CONFIG_EXAMPLE_SERVICES_ENABLE_SNTP

#if CONFIG_EXAMPLE_SERVICES_ENABLE_AUDIO && CONFIG_EXAMPLE_ENABLE_BOARD_MANAGER
    /* Audio service */
    // Configure the peripheral before service start
    auto &audio_service = service::Audio::get_instance();
    service::Audio::PeripheralConfig periph_config{};
    BROOKESIA_CHECK_ESP_ERR_EXIT(audio_peripheral_init(periph_config), "Failed to initialize audio peripheral");
    audio_service.configure_peripheral(periph_config);
    // Configure the recorder
    service::Audio::RecorderConfig recorder_config = DEFAULT_AUDIO_RECORDER_CONFIG();
    recorder_config.recorder_task_config.task_core = 1;
    recorder_config.afe_config = DEFAULT_AV_PROCESSOR_AFE_CONFIG();
    recorder_config.afe_fetch_task_config.task_stack = 6 * 1024;
    audio_service.configure_recorder(recorder_config);
    // Configure the feeder
    service::Audio::FeederConfig feeder_config = DEFAULT_AUDIO_FEEDER_CONFIG();
    feeder_config.feeder_task_config.task_core = 1;
    audio_service.configure_feeder(feeder_config);
#endif // CONFIG_EXAMPLE_SERVICES_ENABLE_AUDIO && CONFIG_EXAMPLE_ENABLE_BOARD_MANAGER
}
