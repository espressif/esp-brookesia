/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "esp_codec_dev.h"
#include "bsp/esp-bsp.h"
#include "esp_brookesia.hpp"
#ifdef ESP_UTILS_LOG_TAG
#   undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "Audio"
#include "esp_lib_utils.h"
#include "audio.hpp"

constexpr int SOUND_VOLUME_MIN      = 0;
constexpr int SOUND_VOLUME_MAX      = 100;
constexpr int SOUND_VOLUME_DEFAULT  = 70;

using namespace esp_brookesia::systems::speaker;
using namespace esp_brookesia::services;

static esp_codec_dev_handle_t play_dev = nullptr;
static esp_codec_dev_handle_t rec_dev = nullptr;

bool audio_init()
{
    ESP_UTILS_LOG_TRACE_GUARD();

    ESP_UTILS_CHECK_ERROR_RETURN(bsp_i2c_init(), false, "Initialize I2C failed");

    esp_gmf_setup_periph_hardware_info periph_info = {
        .i2c = {
            .handle = bsp_i2c_get_handle(),
        },
        .codec = {
            .io_pa = BSP_POWER_AMP_IO,
            .type = ESP_GMF_CODEC_TYPE_ES7210_IN_ES8311_OUT,
            .dac = {
                .io_mclk = BSP_I2S_MCLK,
                .io_bclk = BSP_I2S_SCLK,
                .io_ws = BSP_I2S_LCLK,
                .io_do = BSP_I2S_DOUT,
                .io_di = BSP_I2S_DSIN,
                .sample_rate = 16000,
                .channel = 2,
                .bits_per_sample = 32,
                .port_num  = 0,
            },
            .adc = {
                .io_mclk = BSP_I2S_MCLK,
                .io_bclk = BSP_I2S_SCLK,
                .io_ws = BSP_I2S_LCLK,
                .io_do = BSP_I2S_DOUT,
                .io_di = BSP_I2S_DSIN,
                .sample_rate = 16000,
                .channel = 2,
                .bits_per_sample = 32,
                .port_num  = 0,
            },
        },
    };
    ESP_UTILS_CHECK_ERROR_RETURN(
        audio_manager_init(&periph_info, &play_dev, &rec_dev), false, "Initialize audio manager failed"
    );
    ESP_UTILS_CHECK_ERROR_RETURN(audio_prompt_open(), false, "Open audio prompt failed");

    /* Update media sound volume when NVS volume is updated */
    auto &storage_service = StorageNVS::requestInstance();
    storage_service.connectEventSignal([&](const StorageNVS::Event & event) {
        if ((event.operation != StorageNVS::Operation::UpdateNVS) || (event.key != Manager::SETTINGS_VOLUME)) {
            return;
        }

        ESP_UTILS_LOG_TRACE_GUARD();

        StorageNVS::Value value;
        ESP_UTILS_CHECK_FALSE_EXIT(
            storage_service.getLocalParam(Manager::SETTINGS_VOLUME, value), "Get NVS volume failed"
        );

        auto volume = std::clamp(std::get<int>(value), SOUND_VOLUME_MIN, SOUND_VOLUME_MAX);
        ESP_UTILS_LOGI("Set media sound volume to %d", volume);
        ESP_UTILS_CHECK_FALSE_EXIT(
            esp_codec_dev_set_out_vol(play_dev, volume) == ESP_CODEC_DEV_OK, "Set media sound volume failed"
        );
    });

    /* Initialize media sound volume */
    StorageNVS::Value volume = SOUND_VOLUME_DEFAULT;
    if (!storage_service.getLocalParam(Manager::SETTINGS_VOLUME, volume)) {
        ESP_UTILS_LOGW("Volume not found in NVS, set to default value(%d)", std::get<int>(volume));
    }
    storage_service.setLocalParam(Manager::SETTINGS_VOLUME, volume);

    return true;
}
