/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * The EEPROM descriptor validation and camera slot resource handling are
 * adapted from esp-mosaico-bsp commit
 * bef99672e411101489ed19c40527cca1c1dd5bb1.
 */

#include "brookesia/hal_custom/expansion/mosaico_module_manager.h"

#include <string.h>

#include "driver/i2c_master.h"
#include "esp_bit_defs.h"
#include "brookesia/hal_adaptor/board_manager.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "hal/usb_serial_jtag_ll.h"
#include "soc/soc_caps.h"

#define EEPROM_DESC_CRC_OFFSET          (0x34U)
#define EEPROM_MFG_DATA_OFFSET          (0x36U)
#define EEPROM_MFG_CRC_OFFSET           (0x3EU)
#define EEPROM_PARAM_DATA_OFFSET        (0x40U)
#define EEPROM_PARAM_CRC_OFFSET         (0x84U)
#define EEPROM_DETACH_ATTEMPTS          (5)
#define EEPROM_DETACH_RETRY_MS          (10U)
static const char *TAG = "MOSAICO_MODULE";

_Static_assert(
    sizeof(esp_mosaico_module_eeprom_v1_t) == ESP_MOSAICO_MODULE_EEPROM_IMAGE_SIZE,
    "Mosaico module EEPROM V1 layout mismatch"
);

typedef struct {
    bool context_valid;
    bool initialized;
    bool cleanup_pending;
    bool scan_paused;
    bool address_restore_pending;
    bool camera_resource_claimed;
    esp_err_t peripheral_error;
    bool usj_pad_was_enabled;
    bool usj_clock_was_enabled;
    uint32_t usj_interrupt_mask;
    gpio_num_t camera_flash_gpio_num;
    uint8_t camera_flash_off_level;
    SemaphoreHandle_t lock;
    i2c_master_bus_handle_t i2c_bus;
    i2c_master_dev_handle_t eeprom_devices[ESP_MOSAICO_EXPANSION_SLOT_COUNT];
    esp_mosaico_expansion_manager_config_t config;
    esp_mosaico_expansion_info_t slots[ESP_MOSAICO_EXPANSION_SLOT_COUNT];
} manager_context_t;

static manager_context_t manager;


static bool is_valid_slot(esp_mosaico_expansion_slot_t slot);
static bool is_supported_module(esp_mosaico_expansion_slot_t slot, uint8_t board_type);
static esp_err_t configure_address_select(const esp_mosaico_expansion_slot_config_t *config);
static esp_err_t configure_all_address_selects(void);
static esp_err_t attach_eeprom_device(esp_mosaico_expansion_slot_t slot);
static esp_err_t restore_manager_resources(void);
static esp_err_t detach_eeprom_devices(void);
static esp_err_t cleanup_failed_init_resources(void);
static esp_err_t release_i2c_peripheral(void);
static esp_err_t sample_slot_locked(esp_mosaico_expansion_slot_t slot);
static bool slot_info_changed(const esp_mosaico_expansion_info_t *old_info,
                              const esp_mosaico_expansion_info_t *new_info);
static void invalidate_slots_locked(void);
static void set_slot_error_locked(esp_mosaico_expansion_slot_t slot, esp_err_t error);
static esp_err_t finish_camera_address_restore(esp_mosaico_expansion_slot_t slot,
        esp_err_t restore_result);
static esp_err_t force_flash_off(gpio_num_t gpio_num, uint8_t off_level);
static esp_err_t prepare_camera_sensor_pads(uint32_t xclk_settle_ms);
static void save_and_disable_usb_serial_jtag_locked(void);
static void restore_usb_serial_jtag_locked(void);

uint16_t esp_mosaico_module_crc16(const uint8_t *data, size_t size)
{
    if ((data == NULL) && (size != 0)) {
        return 0;
    }

    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < size; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++) {
            crc = (crc & 1U) ? (uint16_t)((crc >> 1) ^ 0xA001U) : (uint16_t)(crc >> 1);
        }
    }
    return crc;
}

bool esp_mosaico_module_eeprom_valid(const esp_mosaico_module_eeprom_v1_t *image)
{
    if ((image == NULL) ||
            (memcmp(image->magic, ESP_MOSAICO_MODULE_EEPROM_MAGIC,
                    ESP_MOSAICO_MODULE_EEPROM_MAGIC_SIZE) != 0)) {
        return false;
    }

    const uint8_t *raw = (const uint8_t *)image;
    const uint16_t descriptor_crc = esp_mosaico_module_crc16(raw, EEPROM_DESC_CRC_OFFSET);
    const uint16_t manufacturing_crc = esp_mosaico_module_crc16(
                                           raw + EEPROM_MFG_DATA_OFFSET,
                                           EEPROM_MFG_CRC_OFFSET - EEPROM_MFG_DATA_OFFSET
                                       );
    const uint16_t parameter_crc = esp_mosaico_module_crc16(
                                       raw + EEPROM_PARAM_DATA_OFFSET,
                                       EEPROM_PARAM_CRC_OFFSET - EEPROM_PARAM_DATA_OFFSET
                                   );

    return (image->desc_crc16 == descriptor_crc) &&
           (image->mfg_crc16 == manufacturing_crc) &&
           (image->param_crc16 == parameter_crc);
}

const char *esp_mosaico_expansion_slot_to_name(esp_mosaico_expansion_slot_t slot)
{
    switch (slot) {
    case ESP_MOSAICO_EXPANSION_SLOT_LEFT:
        return "left";
    case ESP_MOSAICO_EXPANSION_SLOT_RIGHT:
        return "right";
    default:
        return "unknown";
    }
}

const char *esp_mosaico_board_type_to_name(uint8_t board_type)
{
    switch (board_type) {
    case ESP_MOSAICO_BOARD_TYPE_CAMERA:
        return "camera";
    case ESP_MOSAICO_BOARD_TYPE_SENSOR:
        return "sensor";
    case ESP_MOSAICO_BOARD_TYPE_TOF:
        return "tof";
    case ESP_MOSAICO_BOARD_TYPE_MATRIX_LED:
        return "matrix_led";
    case ESP_MOSAICO_BOARD_TYPE_THERMAL:
        return "thermal_camera";
    case ESP_MOSAICO_BOARD_TYPE_RELAY:
        return "relay";
    case ESP_MOSAICO_BOARD_TYPE_BUTTON_LED:
        return "button_led";
    case ESP_MOSAICO_BOARD_TYPE_CORE:
        return "core";
    case ESP_MOSAICO_BOARD_TYPE_POWER:
        return "power";
    case ESP_MOSAICO_BOARD_TYPE_DOCK:
        return "dock";
    case ESP_MOSAICO_BOARD_TYPE_HANDLE:
        return "handle";
    case ESP_MOSAICO_BOARD_TYPE_BALANCE_CAR:
        return "balance_car";
    case ESP_MOSAICO_BOARD_TYPE_DISPLAY:
        return "display";
    case ESP_MOSAICO_BOARD_TYPE_IO_EXP:
        return "io_expansion";
    default:
        return "unknown";
    }
}

esp_err_t esp_mosaico_expansion_manager_init(const esp_mosaico_expansion_manager_config_t *config)
{
    ESP_RETURN_ON_FALSE(config != NULL, ESP_ERR_INVALID_ARG, TAG, "Manager config is null");
    ESP_RETURN_ON_FALSE(config->i2c_name != NULL, ESP_ERR_INVALID_ARG, TAG, "I2C name is null");
    ESP_RETURN_ON_FALSE(config->frequency_hz > 0, ESP_ERR_INVALID_ARG, TAG, "I2C frequency is invalid");
    ESP_RETURN_ON_FALSE(config->timeout_ms > 0, ESP_ERR_INVALID_ARG, TAG, "I2C timeout is invalid");

    if (manager.peripheral_error != ESP_OK) {
        return manager.peripheral_error;
    }
    if (manager.initialized) {
        return ESP_OK;
    }

    for (size_t i = 0; i < ESP_MOSAICO_EXPANSION_SLOT_COUNT; i++) {
        ESP_RETURN_ON_FALSE(GPIO_IS_VALID_OUTPUT_GPIO(config->slots[i].address_gpio_num),
                            ESP_ERR_INVALID_ARG, TAG, "Slot %u address GPIO is invalid", (unsigned)i);
        ESP_RETURN_ON_FALSE(config->slots[i].address_level <= 1, ESP_ERR_INVALID_ARG, TAG,
                            "Slot %u address level is invalid", (unsigned)i);
        ESP_RETURN_ON_FALSE(config->slots[i].eeprom_address <= 0x7F, ESP_ERR_INVALID_ARG, TAG,
                            "Slot %u EEPROM address is invalid", (unsigned)i);
    }
    ESP_RETURN_ON_FALSE(config->slots[ESP_MOSAICO_EXPANSION_SLOT_LEFT].eeprom_address !=
                        config->slots[ESP_MOSAICO_EXPANSION_SLOT_RIGHT].eeprom_address,
                        ESP_ERR_INVALID_ARG, TAG, "Slot EEPROM addresses must be unique");

    if (manager.context_valid) {
        const esp_err_t cleanup_ret = cleanup_failed_init_resources();
        if (cleanup_ret != ESP_OK) {
            ESP_LOGE(TAG, "Retry cleanup from previous manager init failed: %s",
                     esp_err_to_name(cleanup_ret));
            return cleanup_ret;
        }
    }

    SemaphoreHandle_t lock = xSemaphoreCreateMutex();
    ESP_RETURN_ON_FALSE(lock != NULL, ESP_ERR_NO_MEM, TAG, "Create manager lock failed");

    memset(&manager, 0, sizeof(manager));
    manager.context_valid = true;
    manager.lock = lock;
    manager.camera_flash_gpio_num = GPIO_NUM_NC;
    manager.config = *config;
    for (size_t i = 0; i < ESP_MOSAICO_EXPANSION_SLOT_COUNT; i++) {
        manager.slots[i].slot = (esp_mosaico_expansion_slot_t)i;
        manager.slots[i].state = ESP_MOSAICO_EXPANSION_STATE_UNKNOWN;
        manager.slots[i].eeprom_address = manager.config.slots[i].eeprom_address;
    }

    esp_err_t ret = brookesia_hal_board_periph_ref_handle(manager.config.i2c_name, (void **)&manager.i2c_bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Acquire I2C peripheral failed: %s", esp_err_to_name(ret));
        goto fail_cleanup;
    }

    for (size_t i = 0; i < ESP_MOSAICO_EXPANSION_SLOT_COUNT; i++) {
        ret = configure_address_select(&manager.config.slots[i]);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Configure %s address select failed: %s",
                     esp_mosaico_expansion_slot_to_name((esp_mosaico_expansion_slot_t)i),
                     esp_err_to_name(ret));
            goto fail_cleanup;
        }
    }

    for (size_t i = 0; i < ESP_MOSAICO_EXPANSION_SLOT_COUNT; i++) {
        ret = attach_eeprom_device((esp_mosaico_expansion_slot_t)i);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Attach %s EEPROM failed: %s",
                     esp_mosaico_expansion_slot_to_name((esp_mosaico_expansion_slot_t)i),
                     esp_err_to_name(ret));
            goto fail_cleanup;
        }
    }

    manager.initialized = true;
    ESP_LOGI(TAG, "Expansion slots ready: left=0x%02X right=0x%02X",
             manager.config.slots[ESP_MOSAICO_EXPANSION_SLOT_LEFT].eeprom_address,
             manager.config.slots[ESP_MOSAICO_EXPANSION_SLOT_RIGHT].eeprom_address);
    return ESP_OK;

fail_cleanup:
    manager.cleanup_pending = true;
    manager.scan_paused = true;
    {
        const esp_err_t cleanup_ret = cleanup_failed_init_resources();
        if (cleanup_ret != ESP_OK) {
            ESP_LOGE(TAG, "Manager init rollback is incomplete and will be retried: %s",
                     esp_err_to_name(cleanup_ret));
        }
    }
    return ret;
}

esp_err_t esp_mosaico_expansion_manager_deinit(void)
{
    if (manager.peripheral_error != ESP_OK) {
        return manager.peripheral_error;
    }
    if (!manager.initialized) {
        return cleanup_failed_init_resources();
    }

    xSemaphoreTake(manager.lock, portMAX_DELAY);
    for (size_t i = 0; i < ESP_MOSAICO_EXPANSION_SLOT_COUNT; i++) {
        if (manager.slots[i].state == ESP_MOSAICO_EXPANSION_STATE_CLAIMED) {
            xSemaphoreGive(manager.lock);
            return ESP_ERR_INVALID_STATE;
        }
    }
    manager.cleanup_pending = true;
    manager.scan_paused = true;
    xSemaphoreGive(manager.lock);

    esp_err_t first_error = detach_eeprom_devices();
    if (first_error != ESP_OK) {
        esp_err_t restore_ret = restore_manager_resources();
        if (restore_ret == ESP_OK) {
            xSemaphoreTake(manager.lock, portMAX_DELAY);
            manager.address_restore_pending = false;
            xSemaphoreGive(manager.lock);
            ESP_LOGW(TAG, "Manager rollback succeeded; deinitialization must still be retried");
        } else {
            ESP_LOGE(TAG, "Restore manager after EEPROM detach failure failed: %s",
                     esp_err_to_name(restore_ret));
        }
        return first_error;
    }

    for (int i = ESP_MOSAICO_EXPANSION_SLOT_COUNT - 1; i >= 0; i--) {
        esp_err_t ret = gpio_reset_pin(manager.config.slots[i].address_gpio_num);
        if ((first_error == ESP_OK) && (ret != ESP_OK)) {
            first_error = ret;
        }
    }
    if (first_error != ESP_OK) {
        esp_err_t restore_ret = restore_manager_resources();
        if (restore_ret == ESP_OK) {
            xSemaphoreTake(manager.lock, portMAX_DELAY);
            manager.address_restore_pending = false;
            xSemaphoreGive(manager.lock);
            ESP_LOGW(TAG, "Manager rollback succeeded; deinitialization must still be retried");
        } else {
            ESP_LOGE(TAG, "Restore manager after GPIO cleanup failure failed: %s",
                     esp_err_to_name(restore_ret));
        }
        return first_error;
    }

    const esp_err_t release_ret = release_i2c_peripheral();
    if (release_ret != ESP_OK) {
        return release_ret;
    }

    manager.initialized = false;
    vSemaphoreDelete(manager.lock);
    memset(&manager, 0, sizeof(manager));
    return ESP_OK;
}

bool esp_mosaico_expansion_manager_is_initialized(void)
{
    return manager.initialized;
}

bool esp_mosaico_expansion_manager_cleanup_pending(void)
{
    if (!manager.context_valid || (manager.lock == NULL)) {
        return false;
    }

    xSemaphoreTake(manager.lock, portMAX_DELAY);
    const bool cleanup_pending = manager.cleanup_pending;
    xSemaphoreGive(manager.lock);
    return cleanup_pending;
}

esp_err_t esp_mosaico_expansion_manager_recover(void)
{
    if (manager.peripheral_error != ESP_OK) {
        return manager.peripheral_error;
    }
    ESP_RETURN_ON_FALSE(manager.initialized, ESP_ERR_INVALID_STATE, TAG, "Manager is not initialized");

    xSemaphoreTake(manager.lock, portMAX_DELAY);
    if (!manager.cleanup_pending) {
        xSemaphoreGive(manager.lock);
        return ESP_OK;
    }

    const esp_err_t ret = restore_manager_resources();
    if (ret != ESP_OK) {
        for (size_t i = 0; i < ESP_MOSAICO_EXPANSION_SLOT_COUNT; i++) {
            set_slot_error_locked((esp_mosaico_expansion_slot_t)i, ret);
        }
        xSemaphoreGive(manager.lock);
        return ret;
    }

    invalidate_slots_locked();
    manager.cleanup_pending = false;
    manager.scan_paused = false;
    manager.address_restore_pending = false;
    xSemaphoreGive(manager.lock);
    ESP_LOGI(TAG, "Recovered expansion manager after an incomplete deinitialization");
    return ESP_OK;
}

esp_err_t esp_mosaico_expansion_scan(esp_mosaico_expansion_slot_t slot,
                                     esp_mosaico_expansion_info_t *out_info)
{
    ESP_RETURN_ON_FALSE(manager.initialized, ESP_ERR_INVALID_STATE, TAG, "Manager is not initialized");
    ESP_RETURN_ON_FALSE(is_valid_slot(slot) && (out_info != NULL), ESP_ERR_INVALID_ARG, TAG,
                        "Invalid scan request");

    xSemaphoreTake(manager.lock, portMAX_DELAY);
    if (manager.cleanup_pending) {
        xSemaphoreGive(manager.lock);
        return ESP_ERR_INVALID_STATE;
    }
    if (manager.address_restore_pending) {
        const esp_err_t restore_ret = configure_all_address_selects();
        if (restore_ret != ESP_OK) {
            set_slot_error_locked(slot, restore_ret);
            *out_info = manager.slots[slot];
            xSemaphoreGive(manager.lock);
            return restore_ret;
        }
        manager.address_restore_pending = false;
        manager.scan_paused = false;
        ESP_LOGI(TAG, "Expansion address selects recovered; EEPROM scanning resumed");
    }
    esp_err_t ret = ESP_OK;
    if (!manager.scan_paused) {
        ret = sample_slot_locked(slot);
    }
    *out_info = manager.slots[slot];
    xSemaphoreGive(manager.lock);
    return ret;
}

esp_err_t esp_mosaico_expansion_get_info(esp_mosaico_expansion_slot_t slot,
        esp_mosaico_expansion_info_t *out_info)
{
    ESP_RETURN_ON_FALSE(manager.initialized, ESP_ERR_INVALID_STATE, TAG, "Manager is not initialized");
    ESP_RETURN_ON_FALSE(is_valid_slot(slot) && (out_info != NULL), ESP_ERR_INVALID_ARG, TAG,
                        "Invalid info request");

    xSemaphoreTake(manager.lock, portMAX_DELAY);
    if (manager.cleanup_pending) {
        xSemaphoreGive(manager.lock);
        return ESP_ERR_INVALID_STATE;
    }
    *out_info = manager.slots[slot];
    xSemaphoreGive(manager.lock);
    return ESP_OK;
}

esp_err_t esp_mosaico_expansion_claim(esp_mosaico_expansion_slot_t slot,
                                      uint8_t expected_board_type,
                                      uint32_t expected_generation,
                                      bool pause_all_scanning)
{
    ESP_RETURN_ON_FALSE(manager.initialized, ESP_ERR_INVALID_STATE, TAG, "Manager is not initialized");
    ESP_RETURN_ON_FALSE(is_valid_slot(slot), ESP_ERR_INVALID_ARG, TAG, "Invalid claim slot");

    xSemaphoreTake(manager.lock, portMAX_DELAY);
    if (manager.cleanup_pending) {
        xSemaphoreGive(manager.lock);
        return ESP_ERR_INVALID_STATE;
    }
    esp_mosaico_expansion_info_t *info = &manager.slots[slot];
    if ((info->state != ESP_MOSAICO_EXPANSION_STATE_READY) ||
            (info->eeprom.board_type != expected_board_type) ||
            ((expected_generation != 0) && (info->generation != expected_generation))) {
        xSemaphoreGive(manager.lock);
        return ESP_ERR_INVALID_STATE;
    }

    info->state = ESP_MOSAICO_EXPANSION_STATE_CLAIMED;
    info->generation++;
    if (pause_all_scanning) {
        manager.scan_paused = true;
    }
    xSemaphoreGive(manager.lock);
    ESP_LOGI(TAG, "Claimed %s slot for %s", esp_mosaico_expansion_slot_to_name(slot),
             esp_mosaico_board_type_to_name(expected_board_type));
    return ESP_OK;
}

esp_err_t esp_mosaico_expansion_release(esp_mosaico_expansion_slot_t slot,
                                        bool resume_scanning)
{
    ESP_RETURN_ON_FALSE(manager.initialized, ESP_ERR_INVALID_STATE, TAG, "Manager is not initialized");
    ESP_RETURN_ON_FALSE(is_valid_slot(slot), ESP_ERR_INVALID_ARG, TAG, "Invalid release slot");

    xSemaphoreTake(manager.lock, portMAX_DELAY);
    if (manager.cleanup_pending) {
        xSemaphoreGive(manager.lock);
        return ESP_ERR_INVALID_STATE;
    }
    esp_mosaico_expansion_info_t *info = &manager.slots[slot];
    if (info->state == ESP_MOSAICO_EXPANSION_STATE_CLAIMED) {
        info->state = is_supported_module(slot, info->eeprom.board_type)
                      ? ESP_MOSAICO_EXPANSION_STATE_READY
                      : ESP_MOSAICO_EXPANSION_STATE_UNSUPPORTED;
        info->generation++;
    }
    if (resume_scanning && !manager.address_restore_pending) {
        manager.scan_paused = false;
    }
    xSemaphoreGive(manager.lock);
    ESP_LOGI(TAG, "Released %s slot", esp_mosaico_expansion_slot_to_name(slot));
    return ESP_OK;
}

esp_err_t esp_mosaico_expansion_restore_address_selects(void)
{
    if (manager.peripheral_error != ESP_OK) {
        return manager.peripheral_error;
    }
    ESP_RETURN_ON_FALSE(manager.initialized, ESP_ERR_INVALID_STATE, TAG, "Manager is not initialized");
    return configure_all_address_selects();
}

esp_err_t esp_mosaico_camera_slot_acquire(const esp_mosaico_camera_slot_config_t *config)
{
    if (manager.peripheral_error != ESP_OK) {
        return manager.peripheral_error;
    }
    ESP_RETURN_ON_FALSE(config != NULL, ESP_ERR_INVALID_ARG, TAG, "Camera slot config is null");
    ESP_RETURN_ON_FALSE(config->slot == ESP_MOSAICO_EXPANSION_SLOT_LEFT, ESP_ERR_NOT_SUPPORTED, TAG,
                        "Camera is supported in the left slot only");
    ESP_RETURN_ON_FALSE(GPIO_IS_VALID_OUTPUT_GPIO(config->flash_gpio_num), ESP_ERR_INVALID_ARG, TAG,
                        "Camera flash GPIO is invalid");

    esp_mosaico_expansion_info_t info = {};
    ESP_RETURN_ON_ERROR(esp_mosaico_expansion_get_info(config->slot, &info), TAG,
                        "Get camera slot state failed");
    if (info.state == ESP_MOSAICO_EXPANSION_STATE_UNKNOWN) {
        ESP_RETURN_ON_ERROR(esp_mosaico_expansion_scan(config->slot, &info), TAG,
                            "Scan camera slot failed");
    }
    if (info.state == ESP_MOSAICO_EXPANSION_STATE_READY) {
        ESP_RETURN_ON_ERROR(
            esp_mosaico_expansion_claim(config->slot, config->expected_board_type, info.generation, true),
            TAG, "Claim camera module failed"
        );
    } else {
        ESP_RETURN_ON_FALSE(
            (info.state == ESP_MOSAICO_EXPANSION_STATE_CLAIMED) &&
            (info.eeprom.board_type == config->expected_board_type),
            ESP_ERR_NOT_FOUND, TAG, "A ready camera module was not found in the left slot"
        );
    }

    xSemaphoreTake(manager.lock, portMAX_DELAY);
    if (manager.camera_resource_claimed) {
        xSemaphoreGive(manager.lock);
        return ESP_ERR_INVALID_STATE;
    }
    manager.camera_resource_claimed = true;
    manager.camera_flash_gpio_num = config->flash_gpio_num;
    manager.camera_flash_off_level = config->flash_off_level;
    save_and_disable_usb_serial_jtag_locked();
    xSemaphoreGive(manager.lock);

    /* GPIO33 is USB Serial/JTAG D- and DVP D2. Release the USB pad so DVP can claim it. */
    (void)gpio_reset_pin(GPIO_NUM_33);

    esp_err_t ret = force_flash_off(config->flash_gpio_num, config->flash_off_level);
    if (ret != ESP_OK) {
        xSemaphoreTake(manager.lock, portMAX_DELAY);
        restore_usb_serial_jtag_locked();
        manager.camera_resource_claimed = false;
        manager.camera_flash_gpio_num = GPIO_NUM_NC;
        manager.camera_flash_off_level = 0;
        xSemaphoreGive(manager.lock);
        const esp_err_t restore_ret = esp_mosaico_expansion_restore_address_selects();
        (void)finish_camera_address_restore(config->slot, restore_ret);
        return ret;
    }

    ret = prepare_camera_sensor_pads(config->settle_time_ms);
    if (ret != ESP_OK) {
        xSemaphoreTake(manager.lock, portMAX_DELAY);
        restore_usb_serial_jtag_locked();
        manager.camera_resource_claimed = false;
        manager.camera_flash_gpio_num = GPIO_NUM_NC;
        manager.camera_flash_off_level = 0;
        xSemaphoreGive(manager.lock);
        const esp_err_t restore_ret = esp_mosaico_expansion_restore_address_selects();
        (void)finish_camera_address_restore(config->slot, restore_ret);
        return ret;
    }

    ESP_LOGI(TAG, "Camera slot ready; EEPROM scanning paused and USB Serial/JTAG released");
    return ESP_OK;
}

esp_err_t esp_mosaico_camera_slot_release(esp_mosaico_expansion_slot_t slot)
{
    if (manager.peripheral_error != ESP_OK) {
        return manager.peripheral_error;
    }
    ESP_RETURN_ON_FALSE(slot == ESP_MOSAICO_EXPANSION_SLOT_LEFT, ESP_ERR_NOT_SUPPORTED, TAG,
                        "Camera is supported in the left slot only");
    ESP_RETURN_ON_FALSE(manager.initialized, ESP_ERR_INVALID_STATE, TAG, "Manager is not initialized");

    xSemaphoreTake(manager.lock, portMAX_DELAY);
    if (!manager.camera_resource_claimed) {
        const bool restore_pending = manager.address_restore_pending;
        xSemaphoreGive(manager.lock);
        if (restore_pending) {
            const esp_err_t restore_ret = esp_mosaico_expansion_restore_address_selects();
            return finish_camera_address_restore(slot, restore_ret);
        }
        return esp_mosaico_expansion_release(slot, true);
    }
    const gpio_num_t flash_gpio_num = manager.camera_flash_gpio_num;
    const uint8_t flash_off_level = manager.camera_flash_off_level;
    xSemaphoreGive(manager.lock);

    esp_err_t first_error = force_flash_off(flash_gpio_num, flash_off_level);
    const esp_err_t address_restore_ret = esp_mosaico_expansion_restore_address_selects();
    if ((first_error == ESP_OK) && (address_restore_ret != ESP_OK)) {
        first_error = address_restore_ret;
    }

    xSemaphoreTake(manager.lock, portMAX_DELAY);
    restore_usb_serial_jtag_locked();
    manager.camera_resource_claimed = false;
    manager.camera_flash_gpio_num = GPIO_NUM_NC;
    manager.camera_flash_off_level = 0;
    xSemaphoreGive(manager.lock);

    esp_err_t ret = finish_camera_address_restore(slot, address_restore_ret);
    if ((first_error == ESP_OK) && (ret != ESP_OK)) {
        first_error = ret;
    }
    if (address_restore_ret == ESP_OK) {
        ESP_LOGI(TAG, "Camera slot released; EEPROM scanning and USB Serial/JTAG restored");
    } else {
        ESP_LOGE(TAG, "Camera slot released with EEPROM address restore pending: %s",
                 esp_err_to_name(address_restore_ret));
    }
    return first_error;
}

static bool is_valid_slot(esp_mosaico_expansion_slot_t slot)
{
    return (slot >= ESP_MOSAICO_EXPANSION_SLOT_LEFT) && (slot < ESP_MOSAICO_EXPANSION_SLOT_COUNT);
}

static bool is_supported_module(esp_mosaico_expansion_slot_t slot, uint8_t board_type)
{
    return (slot == ESP_MOSAICO_EXPANSION_SLOT_LEFT) && (board_type == ESP_MOSAICO_BOARD_TYPE_CAMERA);
}

static esp_err_t configure_address_select(const esp_mosaico_expansion_slot_config_t *config)
{
    const gpio_config_t address_gpio_config = {
        .pin_bit_mask = BIT64(config->address_gpio_num),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_set_level(config->address_gpio_num, config->address_level), TAG,
                        "Preset address GPIO%d failed", config->address_gpio_num);
    return gpio_config(&address_gpio_config);
}

static esp_err_t configure_all_address_selects(void)
{
    esp_err_t first_error = ESP_OK;
    for (size_t i = 0; i < ESP_MOSAICO_EXPANSION_SLOT_COUNT; i++) {
        esp_err_t ret = configure_address_select(&manager.config.slots[i]);
        if ((first_error == ESP_OK) && (ret != ESP_OK)) {
            first_error = ret;
        }
    }
    return first_error;
}

static void set_slot_error_locked(esp_mosaico_expansion_slot_t slot, esp_err_t error)
{
    esp_mosaico_expansion_info_t *info = &manager.slots[slot];
    if ((info->state != ESP_MOSAICO_EXPANSION_STATE_ERROR) || (info->last_error != error)) {
        info->generation++;
    }
    info->state = ESP_MOSAICO_EXPANSION_STATE_ERROR;
    info->last_error = error;
}

static void invalidate_slots_locked(void)
{
    for (size_t i = 0; i < ESP_MOSAICO_EXPANSION_SLOT_COUNT; i++) {
        esp_mosaico_expansion_info_t *info = &manager.slots[i];
        info->state = ESP_MOSAICO_EXPANSION_STATE_UNKNOWN;
        info->last_error = ESP_OK;
        memset(&info->eeprom, 0, sizeof(info->eeprom));
        info->generation++;
    }
}

static esp_err_t finish_camera_address_restore(esp_mosaico_expansion_slot_t slot,
        esp_err_t restore_result)
{
    if (restore_result == ESP_OK) {
        xSemaphoreTake(manager.lock, portMAX_DELAY);
        manager.address_restore_pending = false;
        xSemaphoreGive(manager.lock);
        return esp_mosaico_expansion_release(slot, true);
    }

    xSemaphoreTake(manager.lock, portMAX_DELAY);
    manager.address_restore_pending = true;
    manager.scan_paused = true;
    set_slot_error_locked(slot, restore_result);
    xSemaphoreGive(manager.lock);
    return restore_result;
}

static esp_err_t attach_eeprom_device(esp_mosaico_expansion_slot_t slot)
{
    if (manager.eeprom_devices[slot] != NULL) {
        return ESP_OK;
    }
    const i2c_device_config_t config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = manager.config.slots[slot].eeprom_address,
        .scl_speed_hz = manager.config.frequency_hz,
    };
    return i2c_master_bus_add_device(manager.i2c_bus, &config, &manager.eeprom_devices[slot]);
}

static esp_err_t restore_manager_resources(void)
{
    esp_err_t first_error = configure_all_address_selects();
    for (size_t i = 0; i < ESP_MOSAICO_EXPANSION_SLOT_COUNT; i++) {
        esp_err_t ret = attach_eeprom_device((esp_mosaico_expansion_slot_t)i);
        if ((first_error == ESP_OK) && (ret != ESP_OK)) {
            first_error = ret;
        }
    }
    return first_error;
}

static esp_err_t detach_eeprom_devices(void)
{
    esp_err_t first_error = ESP_OK;
    for (int i = ESP_MOSAICO_EXPANSION_SLOT_COUNT - 1; i >= 0; i--) {
        i2c_master_dev_handle_t device = manager.eeprom_devices[i];
        if (device == NULL) {
            continue;
        }
        esp_err_t ret = ESP_ERR_INVALID_STATE;
        for (int attempt = 0; attempt < EEPROM_DETACH_ATTEMPTS; attempt++) {
            ret = i2c_master_bus_rm_device(device);
            if (ret == ESP_OK) {
                break;
            }
            if (attempt + 1 < EEPROM_DETACH_ATTEMPTS) {
                vTaskDelay(pdMS_TO_TICKS(EEPROM_DETACH_RETRY_MS));
            }
        }
        if ((first_error == ESP_OK) && (ret != ESP_OK)) {
            first_error = ret;
        }
        if (ret == ESP_OK) {
            manager.eeprom_devices[i] = NULL;
        } else {
            ESP_LOGE(TAG, "Detach %s EEPROM failed after %d attempts: %s",
                     esp_mosaico_expansion_slot_to_name((esp_mosaico_expansion_slot_t)i),
                     EEPROM_DETACH_ATTEMPTS, esp_err_to_name(ret));
        }
    }
    return first_error;
}

static esp_err_t cleanup_failed_init_resources(void)
{
    if (manager.peripheral_error != ESP_OK) {
        return manager.peripheral_error;
    }
    if (!manager.context_valid) {
        return ESP_OK;
    }

    esp_err_t first_error = detach_eeprom_devices();
    if (first_error != ESP_OK) {
        /* EEPROM handles still depend on the I2C bus. Keep the complete
         * context intact so the next initialization can retry safely. */
        return first_error;
    }

    for (int i = ESP_MOSAICO_EXPANSION_SLOT_COUNT - 1; i >= 0; i--) {
        const esp_err_t ret = gpio_reset_pin(manager.config.slots[i].address_gpio_num);
        if ((first_error == ESP_OK) && (ret != ESP_OK)) {
            first_error = ret;
        }
    }

    const esp_err_t release_ret = release_i2c_peripheral();
    if (release_ret != ESP_OK) {
        return release_ret;
    }

    if (first_error != ESP_OK) {
        return first_error;
    }

    vSemaphoreDelete(manager.lock);
    memset(&manager, 0, sizeof(manager));
    return ESP_OK;
}

static esp_err_t release_i2c_peripheral(void)
{
    if (manager.peripheral_error != ESP_OK) {
        return manager.peripheral_error;
    }
    if (manager.i2c_bus == NULL) {
        return ESP_OK;
    }

    /* Bus deinit errors do not guarantee that the driver handle survived.
     * Retire our cached pointer before release and never retry an unknown owner. */
    manager.i2c_bus = NULL;
    const esp_err_t ret = brookesia_hal_board_periph_unref_handle(manager.config.i2c_name);
    if (ret != ESP_OK) {
        manager.peripheral_error = ret;
        manager.cleanup_pending = true;
        manager.scan_paused = true;
        ESP_LOGE(TAG, "I2C release outcome is unknown; restart required: %s", esp_err_to_name(ret));
    }
    return ret;
}

static esp_err_t sample_slot_locked(esp_mosaico_expansion_slot_t slot)
{
    esp_mosaico_expansion_info_t next = {
        .slot = slot,
        .state = ESP_MOSAICO_EXPANSION_STATE_UNKNOWN,
        .generation = manager.slots[slot].generation,
        .eeprom_address = manager.config.slots[slot].eeprom_address,
        .last_error = ESP_OK,
    };

    if ((manager.i2c_bus == NULL) || (manager.eeprom_devices[slot] == NULL)) {
        next.state = ESP_MOSAICO_EXPANSION_STATE_ERROR;
        next.last_error = ESP_ERR_INVALID_STATE;
        if (slot_info_changed(&manager.slots[slot], &next)) {
            next.generation++;
        }
        manager.slots[slot] = next;
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = i2c_master_probe(manager.i2c_bus, next.eeprom_address, manager.config.timeout_ms);
    if (ret == ESP_ERR_NOT_FOUND) {
        next.state = ESP_MOSAICO_EXPANSION_STATE_EMPTY;
    } else if (ret != ESP_OK) {
        next.state = ESP_MOSAICO_EXPANSION_STATE_ERROR;
        next.last_error = ret;
    } else {
        const uint8_t memory_address = 0;
        ret = i2c_master_transmit_receive(
                  manager.eeprom_devices[slot], &memory_address, sizeof(memory_address),
                  (uint8_t *)&next.eeprom, sizeof(next.eeprom), manager.config.timeout_ms
              );
        if (ret != ESP_OK) {
            next.state = ESP_MOSAICO_EXPANSION_STATE_ERROR;
            next.last_error = ret;
        } else if (!esp_mosaico_module_eeprom_valid(&next.eeprom)) {
            next.state = ESP_MOSAICO_EXPANSION_STATE_INVALID;
        } else if (!is_supported_module(slot, next.eeprom.board_type)) {
            next.state = ESP_MOSAICO_EXPANSION_STATE_UNSUPPORTED;
        } else {
            next.state = ESP_MOSAICO_EXPANSION_STATE_READY;
        }
    }

    if (slot_info_changed(&manager.slots[slot], &next)) {
        next.generation++;
        ESP_LOGI(TAG, "Slot %s state=%d type=%s(0x%02X) generation=%lu",
                 esp_mosaico_expansion_slot_to_name(slot), next.state,
                 esp_mosaico_board_type_to_name(next.eeprom.board_type), next.eeprom.board_type,
                 (unsigned long)next.generation);
    }
    manager.slots[slot] = next;
    return ret == ESP_ERR_NOT_FOUND ? ESP_OK : ret;
}

static bool slot_info_changed(const esp_mosaico_expansion_info_t *old_info,
                              const esp_mosaico_expansion_info_t *new_info)
{
    return (old_info->state != new_info->state) ||
           (old_info->last_error != new_info->last_error) ||
           (memcmp(&old_info->eeprom, &new_info->eeprom, sizeof(old_info->eeprom)) != 0);
}

static esp_err_t force_flash_off(gpio_num_t gpio_num, uint8_t off_level)
{
    const gpio_config_t config = {
        .pin_bit_mask = BIT64(gpio_num),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_set_level(gpio_num, off_level), TAG, "Preset camera flash off failed");
    return gpio_config(&config);
}

static esp_err_t prepare_camera_sensor_pads(uint32_t xclk_settle_ms)
{
    const gpio_num_t pwdn_io = ESP_MOSAICO_CAMERA_PWDN_GPIO_NUM;
    const gpio_num_t reset_io = ESP_MOSAICO_CAMERA_RESET_GPIO_NUM;
    const uint32_t settle_ms = (xclk_settle_ms > 0) ? xclk_settle_ms : ESP_MOSAICO_CAMERA_SETTLE_TIME_MS;
    gpio_io_config_t pwdn_io_cfg = { 0 };
    gpio_io_config_t reset_io_cfg = { 0 };

    /*
     * XCLK comes from the CameraBoard 24 MHz oscillator, so wait for it to
     * stabilize before any SCCB access. The sensor driver owns PWDN and RESET.
     */
    vTaskDelay(pdMS_TO_TICKS(settle_ms));
    (void)gpio_get_io_config(pwdn_io, &pwdn_io_cfg);
    (void)gpio_get_io_config(reset_io, &reset_io_cfg);
    return ESP_OK;
}

static void save_and_disable_usb_serial_jtag_locked(void)
{
#if SOC_USB_SERIAL_JTAG_SUPPORTED
    manager.usj_pad_was_enabled = usb_serial_jtag_ll_phy_is_pad_enabled();
    manager.usj_clock_was_enabled = usb_serial_jtag_ll_module_is_enabled();
    manager.usj_interrupt_mask = manager.usj_clock_was_enabled
                                 ? usb_serial_jtag_ll_get_intr_ena_status()
                                 : 0;
    usb_serial_jtag_ll_disable_intr_mask(USB_SERIAL_JTAG_LL_INTR_MASK);
    usb_serial_jtag_ll_phy_enable_pad(false);
    usb_serial_jtag_ll_enable_bus_clock(false);
#endif
}

static void restore_usb_serial_jtag_locked(void)
{
#if SOC_USB_SERIAL_JTAG_SUPPORTED
    if (manager.usj_clock_was_enabled) {
        usb_serial_jtag_ll_enable_bus_clock(true);
    }
    usb_serial_jtag_ll_phy_enable_pad(manager.usj_pad_was_enabled);
    if (manager.usj_clock_was_enabled && (manager.usj_interrupt_mask != 0)) {
        usb_serial_jtag_ll_ena_intr_mask(manager.usj_interrupt_mask);
    }
    manager.usj_pad_was_enabled = false;
    manager.usj_clock_was_enabled = false;
    manager.usj_interrupt_mask = 0;
#endif
}
