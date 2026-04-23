/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/hal_custom/macro_configs.h"
#if !BROOKESIA_HAL_CUSTOM_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/hal_adaptor.hpp"
#include "esp_board_manager.h"

using namespace esp_brookesia;

constexpr const char *GPIO_EXPANDER_DEVICE_NAME = "gpio_expander";

ESP_BROOKESIA_HAL_DEVICE_REGISTER_PRE_INIT_CALLBACKS(
    BROOKESIA_HAL_CUSTOM_PRE_INIT_CALLBACKS_SYMBOL,
    {
        hal::AudioDevice::get_instance().get_name(),
        []() {
            BROOKESIA_LOG_TRACE_GUARD();
            auto ret = esp_board_manager_init_device_by_name(GPIO_EXPANDER_DEVICE_NAME);
            BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to init codec DAC");
            return true;
        }
    },
    {
        hal::DisplayDevice::get_instance().get_name(),
        []() {
            BROOKESIA_LOG_TRACE_GUARD();
            auto ret = esp_board_manager_init_device_by_name(GPIO_EXPANDER_DEVICE_NAME);
            BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to init codec DAC");
            return true;
        }
    }
)

ESP_BROOKESIA_HAL_DEVICE_REGISTER_ALL_POST_DEINIT_CALLBACK(
    BROOKESIA_HAL_CUSTOM_ALL_POST_DEINIT_CALLBACKS_SYMBOL,
    []() {
        BROOKESIA_LOG_TRACE_GUARD();
        auto ret = esp_board_manager_deinit_device_by_name(GPIO_EXPANDER_DEVICE_NAME);
        BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to deinit codec DAC");
        return true;
    }
)
