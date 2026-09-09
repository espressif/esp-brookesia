/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/hal_adaptor/board_manager.h"
#include "esp_board_manager.h"
#include "esp_board_periph.h"
#include "esp_board_device.h"

#include <cstring>

extern "C" {
    esp_err_t fs_nand_cleanup_error(void) __attribute__((weak));
    esp_err_t fs_nand_cleanup(void) __attribute__((weak));
    esp_err_t bq27220_fuel_gauge_cleanup_error(void) __attribute__((weak));
    esp_err_t bq27220_fuel_gauge_cleanup(void) __attribute__((weak));
    esp_err_t esp_mosaico_expansion_cleanup_error(void) __attribute__((weak));
    esp_err_t esp_mosaico_expansion_cleanup(void) __attribute__((weak));
}

namespace {

struct CustomCleanup {
    const char *name;
    esp_err_t (*error)();
    esp_err_t (*retry)();
};

const CustomCleanup custom_cleanup[] = {
    {"fs_nand", fs_nand_cleanup_error, fs_nand_cleanup},
    {"bq27220_fuel_gauge", bq27220_fuel_gauge_cleanup_error, bq27220_fuel_gauge_cleanup},
    {"expansion_runtime_pin", esp_mosaico_expansion_cleanup_error, esp_mosaico_expansion_cleanup},
    {"expansion_module_manager", esp_mosaico_expansion_cleanup_error, esp_mosaico_expansion_cleanup},
};

} // namespace

extern "C" esp_err_t brookesia_hal_board_manager_init(void)
{
    esp_brookesia::hal::detail::LifecycleGuard guard;
    return esp_board_manager_init();
}

extern "C" esp_err_t brookesia_hal_board_manager_get_periph_handle(const char *periph_name, void **periph_handle)
{
    esp_brookesia::hal::detail::LifecycleGuard guard;
    return esp_board_manager_get_periph_handle(periph_name, periph_handle);
}

extern "C" esp_err_t brookesia_hal_board_manager_get_device_handle(const char *dev_name, void **device_handle)
{
    esp_brookesia::hal::detail::LifecycleGuard guard;
    return esp_board_manager_get_device_handle(dev_name, device_handle);
}

extern "C" bool brookesia_hal_board_manager_check_name(const char *name)
{
    esp_brookesia::hal::detail::LifecycleGuard guard;
    return esp_board_manager_check_name(name);
}

extern "C" esp_err_t brookesia_hal_board_manager_get_device_config(const char *dev_name, void **config)
{
    esp_brookesia::hal::detail::LifecycleGuard guard;
    return esp_board_manager_get_device_config(dev_name, config);
}

extern "C" esp_err_t brookesia_hal_board_manager_get_periph_config(const char *periph_name, void **config)
{
    esp_brookesia::hal::detail::LifecycleGuard guard;
    return esp_board_manager_get_periph_config(periph_name, config);
}

extern "C" esp_err_t brookesia_hal_board_manager_get_board_info(esp_board_info_t *board_info)
{
    esp_brookesia::hal::detail::LifecycleGuard guard;
    return esp_board_manager_get_board_info(board_info);
}

extern "C" esp_err_t brookesia_hal_board_manager_init_device_by_name(const char *dev_name)
{
    esp_brookesia::hal::detail::LifecycleGuard guard;
    return esp_board_manager_init_device_by_name(dev_name);
}

extern "C" esp_err_t brookesia_hal_board_manager_deinit_device_by_name(const char *dev_name)
{
    esp_brookesia::hal::detail::LifecycleGuard guard;
    if (!dev_name) {
        return ESP_ERR_INVALID_ARG;
    }
    for (const auto &cleanup : custom_cleanup) {
        if ((std::strcmp(dev_name, cleanup.name) != 0) || !cleanup.error || !cleanup.retry) {
            continue;
        }
        // The previous custom callback has already transferred ownership to a
        // board-lifetime cleanup state. Retry it only on a subsequent request.
        void *handle = nullptr;
        if ((cleanup.error() != ESP_OK) &&
                ((esp_board_manager_get_device_handle(dev_name, &handle) != ESP_OK) || !handle)) {
            return cleanup.retry();
        }
        const auto ret = esp_board_manager_deinit_device_by_name(dev_name);
        return (ret != ESP_OK) ? ret : cleanup.error();
    }
    return esp_board_manager_deinit_device_by_name(dev_name);
}

extern "C" esp_err_t brookesia_hal_board_manager_deinit(void)
{
    esp_brookesia::hal::detail::LifecycleGuard guard;
    auto first_error = esp_board_manager_deinit();
    for (const auto &cleanup : custom_cleanup) {
        if (cleanup.error) {
            const auto error = cleanup.error();
            if ((first_error == ESP_OK) && (error != ESP_OK)) {
                first_error = error;
            }
        }
    }
    return first_error;
}

extern "C" esp_err_t brookesia_hal_board_periph_init(const char *name)
{
    esp_brookesia::hal::detail::LifecycleGuard guard;
    return esp_board_periph_init(name);
}

extern "C" esp_err_t brookesia_hal_board_periph_get_handle(const char *name, void **periph_handle)
{
    esp_brookesia::hal::detail::LifecycleGuard guard;
    return esp_board_periph_get_handle(name, periph_handle);
}

extern "C" esp_err_t brookesia_hal_board_periph_ref_handle(const char *name, void **periph_handle)
{
    esp_brookesia::hal::detail::LifecycleGuard guard;
    return esp_board_periph_ref_handle(name, periph_handle);
}

extern "C" esp_err_t brookesia_hal_board_periph_unref_handle(const char *name)
{
    esp_brookesia::hal::detail::LifecycleGuard guard;
    return esp_board_periph_unref_handle(name);
}

extern "C" esp_err_t brookesia_hal_board_periph_get_config(const char *name, void **config)
{
    esp_brookesia::hal::detail::LifecycleGuard guard;
    return esp_board_periph_get_config(name, config);
}

extern "C" esp_err_t brookesia_hal_board_periph_deinit(const char *name)
{
    esp_brookesia::hal::detail::LifecycleGuard guard;
    return esp_board_periph_deinit(name);
}

extern "C" esp_err_t brookesia_hal_board_device_get_handle(const char *name, void **device_handle)
{
    esp_brookesia::hal::detail::LifecycleGuard guard;
    return esp_board_device_get_handle(name, device_handle);
}

extern "C" esp_err_t brookesia_hal_board_device_get_i2c_effective_addr(const char *device_name, uint16_t *addr)
{
    esp_brookesia::hal::detail::LifecycleGuard guard;
    return esp_board_device_get_i2c_effective_addr(device_name, addr);
}

extern "C" esp_err_t brookesia_hal_board_device_show(const char *name)
{
    esp_brookesia::hal::detail::LifecycleGuard guard;
    return esp_board_device_show(name);
}
