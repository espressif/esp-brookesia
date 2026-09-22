/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "brookesia/hal_interface/lifecycle.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct esp_board_info esp_board_info_t;

/**
 * @brief Device-owned cleanup retained after Board Manager consumes its handle.
 *
 * Both callbacks run under the HAL lifecycle lock. get_error() only observes the
 * last cleanup result; it must not release resources. retry() finishes cleanup
 * whose ownership the device retained, preserving errors that require a restart.
 * The facade invokes retry() on an explicit device-release request only when
 * get_error() reports a failure and Board Manager cannot return a live handle.
 * Whole-manager deinitialization queries errors without invoking retry().
 * Neither callback may initialize/deinitialize Board Manager devices or wait for
 * work that needs the lifecycle lock.
 */
typedef struct {
    const char *device_name;              /*!< Board Manager device name; must have static storage duration. */
    esp_err_t (*get_error)(void);         /*!< Return ESP_OK when no cleanup error remains. */
    esp_err_t (*retry)(void);             /*!< Retry retained cleanup without repeating consumed releases. */
} brookesia_hal_board_device_cleanup_t;

/**
 * @brief Register device cleanup before the first HAL Board Manager init/deinit.
 *
 * The descriptor is copied; its name and callbacks must remain valid for the
 * firmware lifetime. An identical registration is idempotent. Registrations
 * persist across Board Manager restarts and cannot be replaced or removed.
 * Register during startup, including on paths where device initialization may
 * subsequently fail. Registration may allocate; cleanup dispatch does not.
 * For automatic startup registration, use BROOKESIA_PLUGIN_REGISTER_PRE_MAIN_FUNCTION
 * and preserve its registrar symbol with a linker -u option. Handle registration
 * failure before starting devices; error returns do not depend on the check policy.
 *
 * @param[in] cleanup Descriptor with a nonempty name and both callbacks.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG for an incomplete descriptor,
 *         ESP_ERR_INVALID_STATE for a conflicting or late registration, or
 *         ESP_ERR_NO_MEM when the descriptor cannot be retained.
 */
esp_err_t brookesia_hal_board_manager_register_device_cleanup(const brookesia_hal_board_device_cleanup_t *cleanup);

/**
 * @brief Board Manager operations serialized by the shared HAL lifecycle lock.
 *
 * Hold the lifecycle lock around compound init/get transactions. This facade
 * covers Brookesia callers; third-party direct calls require their own coordination.
 */
esp_err_t brookesia_hal_board_manager_init(void);
esp_err_t brookesia_hal_board_manager_get_periph_handle(const char *periph_name, void **periph_handle);
esp_err_t brookesia_hal_board_manager_get_device_handle(const char *dev_name, void **device_handle);
bool brookesia_hal_board_manager_check_name(const char *name);
esp_err_t brookesia_hal_board_manager_get_device_config(const char *dev_name, void **config);
esp_err_t brookesia_hal_board_manager_get_periph_config(const char *periph_name, void **config);
esp_err_t brookesia_hal_board_manager_get_board_info(esp_board_info_t *board_info);
esp_err_t brookesia_hal_board_manager_init_device_by_name(const char *dev_name);
esp_err_t brookesia_hal_board_manager_deinit_device_by_name(const char *dev_name);
esp_err_t brookesia_hal_board_manager_deinit(void);
esp_err_t brookesia_hal_board_periph_init(const char *name);
esp_err_t brookesia_hal_board_periph_get_handle(const char *name, void **periph_handle);
esp_err_t brookesia_hal_board_periph_ref_handle(const char *name, void **periph_handle);
esp_err_t brookesia_hal_board_periph_unref_handle(const char *name);
esp_err_t brookesia_hal_board_periph_get_config(const char *name, void **config);
esp_err_t brookesia_hal_board_periph_deinit(const char *name);
esp_err_t brookesia_hal_board_device_get_handle(const char *name, void **device_handle);
esp_err_t brookesia_hal_board_device_get_i2c_effective_addr(const char *device_name, uint16_t *addr);
esp_err_t brookesia_hal_board_device_show(const char *name);

#ifdef __cplusplus
}
#endif
