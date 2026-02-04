/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <nvs_flash.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Settings handle structure
 */
typedef struct {
    char *namespace_name;
    nvs_handle_t nvs_handle;
    bool read_write;
    bool dirty;
} settings_handle_t;

/**
 * @brief Initialize settings handle
 * @param handle Pointer to settings handle
 * @param namespace_name NVS namespace name
 * @param read_write Whether to open for read/write (true) or read-only (false)
 * @return ESP_OK on success
 */
esp_err_t settings_init(settings_handle_t *handle, const char *namespace_name, bool read_write);

/**
 * @brief Deinitialize settings handle
 * @param handle Pointer to settings handle
 */
void settings_deinit(settings_handle_t *handle);

/**
 * @brief Get string value from NVS
 * @param handle Settings handle
 * @param key Key name
 * @param default_value Default value if key not found
 * @param out_value Buffer to store the value
 * @param max_len Maximum buffer length
 * @return ESP_OK on success
 */
esp_err_t settings_get_string(settings_handle_t *handle, const char *key,
                              const char *default_value, char *out_value, size_t max_len);

/**
 * @brief Set string value in NVS
 * @param handle Settings handle
 * @param key Key name
 * @param value Value to set
 * @return ESP_OK on success
 */
esp_err_t settings_set_string(settings_handle_t *handle, const char *key, const char *value);

/**
 * @brief Get integer value from NVS
 * @param handle Settings handle
 * @param key Key name
 * @param default_value Default value if key not found
 * @param out_value Pointer to store the value
 * @return ESP_OK on success
 */
esp_err_t settings_get_int(settings_handle_t *handle, const char *key,
                           int32_t default_value, int32_t *out_value);

/**
 * @brief Set integer value in NVS
 * @param handle Settings handle
 * @param key Key name
 * @param value Value to set
 * @return ESP_OK on success
 */
esp_err_t settings_set_int(settings_handle_t *handle, const char *key, int32_t value);

/**
 * @brief Erase a key from NVS
 * @param handle Settings handle
 * @param key Key name to erase
 * @return ESP_OK on success
 */
esp_err_t settings_erase_key(settings_handle_t *handle, const char *key);

/**
 * @brief Erase all keys in the namespace
 * @param handle Settings handle
 * @return ESP_OK on success
 */
esp_err_t settings_erase_all(settings_handle_t *handle);

/**
 * @brief Commit changes to NVS
 * @param handle Settings handle
 * @return ESP_OK on success
 */
esp_err_t settings_commit(settings_handle_t *handle);

#ifdef __cplusplus
}
#endif
