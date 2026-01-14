/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "brookesia/service_manager.hpp"
#include "brookesia/service_helper.hpp"
#include "private/utils.hpp"
#if CONFIG_EXAMPLE_SERVICES_ENABLE_AUDIO
#   include "brookesia/service_audio.hpp"
#endif
#if CONFIG_EXAMPLE_ENABLE_BOARD_MANAGER
#   include "board.hpp"
#endif
#include "audio_service.hpp"

using namespace esp_brookesia;

using AudioHelper = service::helper::Audio;

void audio_service_init()
{
    if (!AudioHelper::is_available()) {
        BROOKESIA_LOGW("Audio service is not enabled");
        return;
    }

    BROOKESIA_LOGI("Initializing audio service...");

#if CONFIG_EXAMPLE_SERVICES_ENABLE_AUDIO
#if !CONFIG_EXAMPLE_ENABLE_BOARD_MANAGER
    BROOKESIA_LOGE("Only supported when board manager is enabled, skip");
#else
    // Configure the peripheral before service start
    auto &audio_service = service::Audio::get_instance();
    service::Audio::PeripheralConfig periph_config {};
    BROOKESIA_CHECK_FALSE_EXIT(board_audio_peripheral_init(periph_config), "Failed to initialize audio peripheral");
    audio_service.configure_peripheral(periph_config);
    // Configure the recorder
    service::Audio::RecorderConfig recorder_config = DEFAULT_AUDIO_RECORDER_CONFIG();
    recorder_config.recorder_task_config.task_core = 0;
    recorder_config.afe_config = DEFAULT_AV_PROCESSOR_AFE_CONFIG();
    recorder_config.afe_config.ai_mode_wakeup = false;
    recorder_config.afe_fetch_task_config.task_stack = 6 * 1024;
    audio_service.configure_recorder(recorder_config);
    // Configure the feeder
    service::Audio::FeederConfig feeder_config = DEFAULT_AUDIO_FEEDER_CONFIG();
    feeder_config.feeder_task_config.task_core = 1;
    audio_service.configure_feeder(feeder_config);
#endif // CONFIG_EXAMPLE_ENABLE_BOARD_MANAGER
#endif // CONFIG_EXAMPLE_SERVICES_ENABLE_AUDIO
}
