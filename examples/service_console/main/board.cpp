/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "esp_log.h"
#include "esp_err.h"
#include <string.h>
#include <stdbool.h>
#include "esp_board_manager.h"
#include "esp_board_periph.h"
#include "dev_audio_codec.h"
#include "brookesia/lib_utils.hpp"
#include "board.hpp"

bool board_manager_init()
{
    esp_board_manager_init();
    esp_board_manager_print();
    esp_board_device_init_all();

    return true;
}

// Helper function to check if board name matches
static inline bool is_board_name(const char *board_name, const char *target_name)
{
    if (board_name == nullptr || target_name == nullptr) {
        return false;
    }
    return strcmp(board_name, target_name) == 0;
}

bool board_audio_peripheral_init(esp_brookesia::service::Audio::PeripheralConfig &config)
{
    esp_err_t ret = ESP_OK;
    dev_audio_codec_handles_t *play_dev_handles = NULL;
    dev_audio_codec_handles_t *rec_dev_handles = NULL;
    auto &manager_config = config.manager_config;

    ret = esp_board_device_get_handle("audio_dac", (void **)&play_dev_handles);
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to get audio_dac handle");
    BROOKESIA_CHECK_NULL_RETURN(play_dev_handles, false, "Failed to get audio_dac handle");
    manager_config.play_dev = play_dev_handles->codec_dev;

    ret = esp_board_device_get_handle("audio_adc", (void **)&rec_dev_handles);
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to get audio_adc handle");
    BROOKESIA_CHECK_NULL_RETURN(rec_dev_handles, false, "Failed to get audio_adc handle");
    manager_config.rec_dev = rec_dev_handles->codec_dev;

    config.player_volume_default = 80;
    config.player_volume_min = 0;
    config.player_volume_max = 100;
    config.recorder_gain = 32.0;
    config.recorder_channel_gains[2] = 20.0;

    if (is_board_name(g_esp_board_info.name, "esp32s3_korvo2_v3") ||
            is_board_name(g_esp_board_info.name, "echoear_core_board_v1_2") ||
            is_board_name(g_esp_board_info.name, "esp_box_3")) {
        strncpy(manager_config.mic_layout, "RMNM", sizeof(manager_config.mic_layout));
        manager_config.board_sample_rate = 16000;
        manager_config.board_bits = 32;
        manager_config.board_channels = 2;
    } else if (is_board_name(g_esp_board_info.name, "esp32_s3_korvo2l_v1") ||
               is_board_name(g_esp_board_info.name, "esp32_p4_function_ev")) {
        strncpy(manager_config.mic_layout, "MR", sizeof(manager_config.mic_layout));
        manager_config.board_sample_rate = 16000;
        manager_config.board_bits = 16;
        manager_config.board_channels = 2;
    } else {
        BROOKESIA_LOGE("Unsupported board: %s", g_esp_board_info.name);
        return false;
    }

    return true;
}
