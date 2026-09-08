/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sdkconfig.h"

#include <new>
#include <string>
#include <utility>

#include "esp_err.h"
#include "esp_log.h"
#include "gen_board_device_custom.h"

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

bool is_expected_config(const dev_custom_camera_slot_claim_config_t &config)
{
    return (config.slot == ESP_MOSAICO_CAMERA_SLOT) &&
           (config.expected_board_type == ESP_MOSAICO_CAMERA_BOARD_TYPE) &&
           (config.flash_gpio_num == ESP_MOSAICO_CAMERA_FLASH_GPIO_NUM) &&
           (config.flash_off_level == ESP_MOSAICO_CAMERA_FLASH_OFF_LEVEL) &&
           (config.settle_time_ms == ESP_MOSAICO_CAMERA_SETTLE_TIME_MS);
}
#endif

int camera_slot_claim_init(void *config, int cfg_size, void **device_handle)
{
    if ((config == nullptr) || (device_handle == nullptr) ||
            (cfg_size != static_cast<int>(sizeof(dev_custom_camera_slot_claim_config_t)))) {
        ESP_LOGE(TAG, "Invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }

#if CONFIG_BROOKESIA_HAL_ADAPTOR_ENABLE_EXPANSION_MODULES
    const auto &slot_config = *static_cast<const dev_custom_camera_slot_claim_config_t *>(config);
    if (!is_expected_config(slot_config)) {
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

int camera_slot_claim_deinit(void *device_handle)
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

} // namespace

CUSTOM_DEVICE_IMPLEMENT(camera_slot_claim, camera_slot_claim_init, camera_slot_claim_deinit);
