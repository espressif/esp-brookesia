/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Board handle structure
 */
typedef struct {
    char *uuid;
    bool initialized;
} board_handle_t;

/**
 * @brief Initialize board handle
 * @param handle Pointer to board handle
 * @return ESP_OK on success
 */
esp_err_t board_init(board_handle_t *handle);

/**
 * @brief Cleanup board handle
 * @param handle Pointer to board handle
 */
void board_cleanup(board_handle_t *handle);

/**
 * @brief Get board UUID
 * @param handle Board handle
 * @param uuid_buffer Buffer to store UUID
 * @param buffer_size Size of the UUID buffer
 * @return ESP_OK on success
 */
esp_err_t board_get_uuid(board_handle_t *handle, char *uuid_buffer, size_t buffer_size);

/**
 * @brief Get system information as JSON string
 * @param handle Board handle
 * @param json_buffer Buffer to store JSON string
 * @param buffer_size Size of the JSON buffer
 * @return ESP_OK on success
 */
esp_err_t board_get_json(board_handle_t *handle, char *json_buffer, size_t buffer_size);

/**
 * @brief Get board-specific JSON information
 * @param handle Board handle
 * @param json_buffer Buffer to store JSON string
 * @param buffer_size Size of the JSON buffer
 * @return ESP_OK on success
 */
esp_err_t board_get_board_json(board_handle_t *handle, char *json_buffer, size_t buffer_size);

// Helper functions for system information
/**
 * @brief Get MAC address
 * @param mac_buffer Buffer to store MAC address
 * @param buffer_size Size of the MAC buffer
 * @return ESP_OK on success
 */
esp_err_t board_get_mac_address(char *mac_buffer);

#ifdef __cplusplus
}
#endif
