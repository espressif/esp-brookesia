/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cstdlib>

#include "brookesia/lib_utils/plugin.hpp"
#include "brookesia/hal_adaptor/board_manager.h"
#include "brookesia/hal_custom/board/expansion.h"
#include "brookesia/hal_custom/board/fuel_gauge.h"
#include "brookesia/hal_custom/board/nand.h"
#include "private/utils.hpp"

namespace {

static void register_device_cleanup();

} // namespace

BROOKESIA_PLUGIN_REGISTER_PRE_MAIN_FUNCTION(MOSAICO_DEVICE_CLEANUP_PLUGIN_SYMBOL, register_device_cleanup);

namespace {

static void register_device_cleanup()
{
    const brookesia_hal_board_device_cleanup_t devices[] = {
        {"fs_nand", fs_nand_cleanup_error, fs_nand_cleanup},
        {"bq27220_fuel_gauge", bq27220_fuel_gauge_cleanup_error, bq27220_fuel_gauge_cleanup},
        {"expansion_runtime_pin", esp_mosaico_expansion_cleanup_error, esp_mosaico_expansion_cleanup},
        {"expansion_module_manager", esp_mosaico_expansion_cleanup_error, esp_mosaico_expansion_cleanup},
    };
    for (const auto &device : devices) {
        // Missing cleanup registration would hide errors and orphan retained
        // resources. Stop at startup even when assertions are disabled.
        const auto ret = brookesia_hal_board_manager_register_device_cleanup(&device);
        if (ret != ESP_OK) {
            BROOKESIA_LOGE("Failed to register cleanup for %1%: %2%", device.device_name, ret);
            std::abort();
        }
    }
}

} // namespace
