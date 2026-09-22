/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_MOSAICO_MODULE_EEPROM_MAGIC          "ESP"
#define ESP_MOSAICO_MODULE_EEPROM_MAGIC_SIZE     (3U)
#define ESP_MOSAICO_MODULE_EEPROM_IMAGE_SIZE     (0x86U)
#define ESP_MOSAICO_MODULE_BOARD_NAME_SIZE       (32U)

#define ESP_MOSAICO_CAMERA_SLOT                   ESP_MOSAICO_EXPANSION_SLOT_LEFT
#define ESP_MOSAICO_CAMERA_BOARD_TYPE             ESP_MOSAICO_BOARD_TYPE_CAMERA
#define ESP_MOSAICO_CAMERA_FLASH_GPIO_NUM         GPIO_NUM_34
#define ESP_MOSAICO_CAMERA_FLASH_OFF_LEVEL        (1U)
#define ESP_MOSAICO_CAMERA_SETTLE_TIME_MS         (20U)
#define ESP_MOSAICO_CAMERA_PWDN_GPIO_NUM          GPIO_NUM_48
#define ESP_MOSAICO_CAMERA_RESET_GPIO_NUM         GPIO_NUM_53

typedef enum {
    ESP_MOSAICO_EXPANSION_SLOT_LEFT = 0,
    ESP_MOSAICO_EXPANSION_SLOT_RIGHT,
    ESP_MOSAICO_EXPANSION_SLOT_COUNT,
} esp_mosaico_expansion_slot_t;

typedef enum {
    ESP_MOSAICO_EXPANSION_STATE_UNKNOWN = 0,
    ESP_MOSAICO_EXPANSION_STATE_EMPTY,
    ESP_MOSAICO_EXPANSION_STATE_INVALID,
    ESP_MOSAICO_EXPANSION_STATE_UNSUPPORTED,
    ESP_MOSAICO_EXPANSION_STATE_READY,
    ESP_MOSAICO_EXPANSION_STATE_CLAIMED,
    ESP_MOSAICO_EXPANSION_STATE_ERROR,
} esp_mosaico_expansion_state_t;

typedef enum {
    ESP_MOSAICO_BOARD_TYPE_CORE = 0x01,
    ESP_MOSAICO_BOARD_TYPE_POWER = 0x02,
    ESP_MOSAICO_BOARD_TYPE_DOCK = 0x03,
    ESP_MOSAICO_BOARD_TYPE_HANDLE = 0x04,
    ESP_MOSAICO_BOARD_TYPE_BALANCE_CAR = 0x05,
    ESP_MOSAICO_BOARD_TYPE_DISPLAY = 0x06,
    ESP_MOSAICO_BOARD_TYPE_CAMERA = 0x07,
    ESP_MOSAICO_BOARD_TYPE_SENSOR = 0x08,
    ESP_MOSAICO_BOARD_TYPE_IO_EXP = 0x09,
    ESP_MOSAICO_BOARD_TYPE_TOF = 0x10,
    ESP_MOSAICO_BOARD_TYPE_MATRIX_LED = 0x11,
    ESP_MOSAICO_BOARD_TYPE_THERMAL = 0x12,
    ESP_MOSAICO_BOARD_TYPE_RELAY = 0x13,
    ESP_MOSAICO_BOARD_TYPE_BUTTON_LED = 0x14,
} esp_mosaico_board_type_t;

typedef struct __attribute__((packed))
{
    char magic[ESP_MOSAICO_MODULE_EEPROM_MAGIC_SIZE];
    uint8_t board_type;
    uint16_t board_id;
    uint16_t hw_version;
    uint16_t sw_version;
    uint16_t vendor_id;
    uint32_t board_flags;
    uint32_t serial_number;
    char board_name[ESP_MOSAICO_MODULE_BOARD_NAME_SIZE];
    uint16_t desc_crc16;
    uint32_t manufacture_date;
    uint16_t batch_number;
    uint16_t factory_id;
    uint16_t mfg_crc16;
    uint16_t param_version;
    uint16_t param_length;
    uint8_t param_data[64];
    uint16_t param_crc16;
} esp_mosaico_module_eeprom_v1_t;

typedef struct {
    gpio_num_t address_gpio_num;
    uint8_t address_level;
    uint8_t eeprom_address;
} esp_mosaico_expansion_slot_config_t;

typedef struct {
    const char *i2c_name;
    uint32_t frequency_hz;
    uint32_t timeout_ms;
    esp_mosaico_expansion_slot_config_t slots[ESP_MOSAICO_EXPANSION_SLOT_COUNT];
} esp_mosaico_expansion_manager_config_t;

typedef struct {
    esp_mosaico_expansion_slot_t slot;
    esp_mosaico_expansion_state_t state;
    uint32_t generation;
    uint8_t eeprom_address;
    esp_err_t last_error;
    esp_mosaico_module_eeprom_v1_t eeprom;
} esp_mosaico_expansion_info_t;

typedef struct {
    esp_mosaico_expansion_slot_t slot;
    uint8_t expected_board_type;
    gpio_num_t flash_gpio_num;
    uint8_t flash_off_level;
    uint32_t settle_time_ms;
} esp_mosaico_camera_slot_config_t;

esp_err_t esp_mosaico_expansion_manager_init(const esp_mosaico_expansion_manager_config_t *config);
esp_err_t esp_mosaico_expansion_manager_deinit(void);
bool esp_mosaico_expansion_manager_is_initialized(void);
bool esp_mosaico_expansion_manager_cleanup_pending(void);

/**
 * @brief Restore a manager whose previous deinitialization could not complete
 *
 * A successful recovery keeps the manager initialized, restores both EEPROM
 * devices and address-select GPIOs, and invalidates cached slot samples. The
 * caller must still balance the Board Manager reference left by the failed
 * deinitialization.
 */
esp_err_t esp_mosaico_expansion_manager_recover(void);

/**
 * @brief Perform one synchronous EEPROM sample for a slot
 *
 * The common expansion layer owns periodic scheduling and debounce. When a
 * camera owns the connector this function returns the cached state without
 * accessing either EEPROM.
 */
esp_err_t esp_mosaico_expansion_scan(esp_mosaico_expansion_slot_t slot,
                                     esp_mosaico_expansion_info_t *out_info);

esp_err_t esp_mosaico_expansion_get_info(esp_mosaico_expansion_slot_t slot,
        esp_mosaico_expansion_info_t *out_info);

esp_err_t esp_mosaico_expansion_claim(esp_mosaico_expansion_slot_t slot,
                                      uint8_t expected_board_type,
                                      uint32_t expected_generation,
                                      bool pause_all_scanning);

esp_err_t esp_mosaico_expansion_release(esp_mosaico_expansion_slot_t slot,
                                        bool resume_scanning);

esp_err_t esp_mosaico_expansion_restore_address_selects(void);

esp_err_t esp_mosaico_camera_slot_acquire(const esp_mosaico_camera_slot_config_t *config);
esp_err_t esp_mosaico_camera_slot_release(esp_mosaico_expansion_slot_t slot);

bool esp_mosaico_module_eeprom_valid(const esp_mosaico_module_eeprom_v1_t *image);
uint16_t esp_mosaico_module_crc16(const uint8_t *data, size_t size);
const char *esp_mosaico_expansion_slot_to_name(esp_mosaico_expansion_slot_t slot);
const char *esp_mosaico_board_type_to_name(uint8_t board_type);

#ifdef __cplusplus
}
#endif
