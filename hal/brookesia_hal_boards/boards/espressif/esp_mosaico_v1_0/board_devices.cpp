/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_err.h"
#include "gen_board_device_custom.h"
#include "brookesia/hal_custom/board/audio.h"
#include "brookesia/hal_custom/board/expansion.h"
#include "brookesia/hal_custom/board/fuel_gauge.h"
#include "brookesia/hal_custom/board/nand.h"
#include "brookesia/hal_custom/board/power.h"

namespace {

static int board_power_init(void *config, int cfg_size, void **device_handle);
static int fuel_gauge_init(void *config, int cfg_size, void **device_handle);
static int nand_init(void *config, int cfg_size, void **device_handle);
static int expansion_manager_init(void *config, int cfg_size, void **device_handle);
static int expansion_runtime_pin_init(void *config, int cfg_size, void **device_handle);
static int camera_slot_claim_init(void *config, int cfg_size, void **device_handle);
static int audio_init(void *config, int cfg_size, void **device_handle);

} // namespace

CUSTOM_DEVICE_IMPLEMENT(board_power, board_power_init, esp_mosaico_power_deinit);
CUSTOM_DEVICE_IMPLEMENT(bq27220_fuel_gauge, fuel_gauge_init, esp_mosaico_fuel_gauge_deinit);
CUSTOM_DEVICE_IMPLEMENT(fs_nand, nand_init, esp_mosaico_nand_deinit);
CUSTOM_DEVICE_IMPLEMENT(expansion_module_manager, expansion_manager_init, esp_mosaico_expansion_module_manager_deinit);
CUSTOM_DEVICE_IMPLEMENT(expansion_runtime_pin, expansion_runtime_pin_init, esp_mosaico_expansion_runtime_pin_deinit);
CUSTOM_DEVICE_IMPLEMENT(camera_slot_claim, camera_slot_claim_init, esp_mosaico_camera_slot_claim_deinit);
CUSTOM_DEVICE_IMPLEMENT(audio_codec_power, audio_init, esp_mosaico_audio_power_deinit);

namespace {

// Generated field widths can differ between board revisions. Convert fields explicitly
// so the shared component depends only on its own configuration types.
template <typename Config>
static const Config *get_config(void *config, int cfg_size)
{
    return (config != nullptr && cfg_size == static_cast<int>(sizeof(Config))) ?
           static_cast<const Config *>(config) : nullptr;
}

static int board_power_init(void *config, int cfg_size, void **device_handle)
{
    const auto *source = get_config<dev_custom_board_power_config_t>(config, cfg_size);
    if (source == nullptr) {
        return esp_mosaico_power_init(nullptr, device_handle);
    }
    const esp_mosaico_power_config_t parameters = {
        .vcc_gpio_num = static_cast<gpio_num_t>(source->vcc_gpio_num),
        .shutdown_gpio_num = static_cast<gpio_num_t>(source->shutdown_gpio_num),
        .ramp_frequency_hz = static_cast<uint32_t>(source->ramp_frequency_hz),
        .ramp_time_ms = static_cast<uint32_t>(source->ramp_time_ms),
    };
    return esp_mosaico_power_init(&parameters, device_handle);
}

static int fuel_gauge_init(void *config, int cfg_size, void **device_handle)
{
    const auto *source = get_config<dev_custom_bq27220_fuel_gauge_config_t>(config, cfg_size);
    if (source == nullptr) {
        return esp_mosaico_fuel_gauge_init(nullptr, device_handle);
    }
    const esp_mosaico_fuel_gauge_config_t parameters = {
        .peripheral_name = source->peripheral_name,
        .i2c_addr = static_cast<uint16_t>(source->i2c_addr),
        .frequency = static_cast<uint32_t>(source->frequency),
    };
    return esp_mosaico_fuel_gauge_init(&parameters, device_handle);
}

static int nand_init(void *config, int cfg_size, void **device_handle)
{
    const auto *source = get_config<dev_custom_fs_nand_config_t>(config, cfg_size);
    if (source == nullptr) {
        return esp_mosaico_nand_init(nullptr, device_handle);
    }
    const esp_mosaico_nand_config_t parameters = {
        .peripheral_name = source->peripheral_name,
        .cs_gpio_num = static_cast<gpio_num_t>(source->cs_gpio_num),
        .hold_gpio_num = static_cast<gpio_num_t>(source->hold_gpio_num),
        .wp_gpio_num = static_cast<gpio_num_t>(source->wp_gpio_num),
        .clock_speed_hz = source->clock_speed_hz,
        .queue_size = source->queue_size,
        .gc_factor = static_cast<uint8_t>(source->gc_factor),
    };
    return esp_mosaico_nand_init(&parameters, device_handle);
}

static int expansion_manager_init(void *config, int cfg_size, void **device_handle)
{
    const auto *source = get_config<dev_custom_expansion_module_manager_config_t>(config, cfg_size);
    if (source == nullptr) {
        return esp_mosaico_expansion_module_manager_init(nullptr, device_handle);
    }
    const esp_mosaico_expansion_module_manager_config_t parameters = {
        .peripheral_name = source->peripheral_name,
        .peripheral_count = source->peripheral_count,
        .frequency_hz = source->frequency_hz,
        .timeout_ms = source->timeout_ms,
        .left_address_gpio_num = source->left_address_gpio_num,
        .left_address_level = source->left_address_level,
        .left_eeprom_address = source->left_eeprom_address,
        .right_address_gpio_num = source->right_address_gpio_num,
        .right_address_level = source->right_address_level,
        .right_eeprom_address = source->right_eeprom_address,
    };
    return esp_mosaico_expansion_module_manager_init(&parameters, device_handle);
}

static int expansion_runtime_pin_init(void *config, int cfg_size, void **device_handle)
{
    if (get_config<dev_custom_expansion_runtime_pin_config_t>(config, cfg_size) == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    return esp_mosaico_expansion_runtime_pin_init(device_handle);
}

static int camera_slot_claim_init(void *config, int cfg_size, void **device_handle)
{
    const auto *source = get_config<dev_custom_camera_slot_claim_config_t>(config, cfg_size);
    if (source == nullptr) {
        return esp_mosaico_camera_slot_claim_init(nullptr, device_handle);
    }
    const esp_mosaico_camera_slot_claim_config_t parameters = {
        .slot = source->slot,
        .expected_board_type = source->expected_board_type,
        .flash_gpio_num = source->flash_gpio_num,
        .flash_off_level = source->flash_off_level,
        .settle_time_ms = source->settle_time_ms,
    };
    return esp_mosaico_camera_slot_claim_init(&parameters, device_handle);
}

static int audio_init(void *config, int cfg_size, void **device_handle)
{
    const auto *source = get_config<dev_custom_audio_codec_power_config_t>(config, cfg_size);
    if (source == nullptr) {
        return esp_mosaico_audio_power_init(nullptr, device_handle);
    }
    const esp_mosaico_audio_power_config_t parameters = {
        .codec_gpio_num = static_cast<gpio_num_t>(source->codec_gpio_num),
        .settle_time_ms = source->settle_time_ms,
    };
    return esp_mosaico_audio_power_init(&parameters, device_handle);
}

} // namespace
