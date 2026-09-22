/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <new>
#include <string>
#include <utility>

#include "esp_log.h"
#include "brookesia/hal_custom/macro_configs.h"
#include "brookesia/hal_custom/board/expansion.h"

#if CONFIG_BROOKESIA_HAL_ADAPTOR_ENABLE_EXPANSION_MODULES
#include "brookesia/hal_adaptor/expansion/module_provider.hpp"
#include "brookesia/hal_custom/expansion/mosaico_module_manager.h"
#endif

namespace {

constexpr const char *TAG = "MOSAICO_CAMERA_SLOT";

#if CONFIG_BROOKESIA_HAL_ADAPTOR_ENABLE_EXPANSION_MODULES
struct CameraSlotClaimHandle {
    esp_brookesia::hal::expansion::ModuleLease lease;
};

static bool is_expected_config(const esp_mosaico_camera_slot_claim_config_t &config);
#endif

} // namespace

esp_err_t esp_mosaico_camera_slot_claim_init(
    const esp_mosaico_camera_slot_claim_config_t *config, void **device_handle
)
{
    if ((config == nullptr) || (device_handle == nullptr)) {
        ESP_LOGE(TAG, "Invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }

#if CONFIG_BROOKESIA_HAL_ADAPTOR_ENABLE_EXPANSION_MODULES
    if (!is_expected_config(*config)) {
        ESP_LOGE(TAG, "Camera slot configuration does not match the Mosaico wiring");
        return ESP_ERR_INVALID_ARG;
    }

    auto *handle = new (std::nothrow) CameraSlotClaimHandle;
    if (handle == nullptr) {
        return ESP_ERR_NO_MEM;
    }

    std::string error_message;
    auto lease = esp_brookesia::hal::expansion::claim_matching(
                     "mosaico", "left", "camera", 1200, &error_message
                 );
    if (!lease) {
        ESP_LOGE(TAG, "Claim camera module failed: %s", error_message.c_str());
        delete handle;
        return ESP_ERR_NOT_FOUND;
    }

    handle->lease = std::move(lease);
    *device_handle = handle;
    return ESP_OK;
#else
    ESP_LOGE(TAG, "Expansion module support is disabled");
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t esp_mosaico_camera_slot_claim_deinit(void *device_handle)
{
    if (device_handle == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

#if CONFIG_BROOKESIA_HAL_ADAPTOR_ENABLE_EXPANSION_MODULES
    auto *handle = static_cast<CameraSlotClaimHandle *>(device_handle);
    std::string error_message;
    if (!handle->lease.reset(&error_message)) {
        /* ModuleLease always relinquishes its runtime reference even when a
         * best-effort hardware restore reports an error. Do not leave a dead
         * Board Manager reference holding an already-empty lease. */
        ESP_LOGE(TAG, "Release camera module failed: %s", error_message.c_str());
    }
    delete handle;
    return ESP_OK;
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

namespace {

#if CONFIG_BROOKESIA_HAL_ADAPTOR_ENABLE_EXPANSION_MODULES
static bool is_expected_config(const esp_mosaico_camera_slot_claim_config_t &config)
{
    return (config.slot == static_cast<int32_t>(ESP_MOSAICO_CAMERA_SLOT)) &&
           (config.expected_board_type == static_cast<int32_t>(ESP_MOSAICO_CAMERA_BOARD_TYPE)) &&
           (config.flash_gpio_num == static_cast<int32_t>(ESP_MOSAICO_CAMERA_FLASH_GPIO_NUM)) &&
           (config.flash_off_level == static_cast<int32_t>(ESP_MOSAICO_CAMERA_FLASH_OFF_LEVEL)) &&
           (config.settle_time_ms == static_cast<int32_t>(ESP_MOSAICO_CAMERA_SETTLE_TIME_MS));
}
#endif

} // namespace
