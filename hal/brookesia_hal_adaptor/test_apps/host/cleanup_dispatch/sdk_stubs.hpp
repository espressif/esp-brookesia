/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>

typedef int esp_err_t;
constexpr esp_err_t ESP_OK = 0;
constexpr esp_err_t ESP_FAIL = -1;
constexpr esp_err_t ESP_ERR_NO_MEM = 0x101;
constexpr esp_err_t ESP_ERR_INVALID_ARG = 0x102;
constexpr esp_err_t ESP_ERR_INVALID_STATE = 0x103;
struct esp_board_info;

extern "C" {
    esp_err_t esp_board_manager_init(void);
    esp_err_t esp_board_manager_get_periph_handle(const char *, void **);
    esp_err_t esp_board_manager_get_device_handle(const char *, void **);
    bool esp_board_manager_check_name(const char *);
    esp_err_t esp_board_manager_get_device_config(const char *, void **);
    esp_err_t esp_board_manager_get_periph_config(const char *, void **);
    esp_err_t esp_board_manager_get_board_info(esp_board_info *);
    esp_err_t esp_board_manager_init_device_by_name(const char *);
    esp_err_t esp_board_manager_deinit_device_by_name(const char *);
    esp_err_t esp_board_manager_deinit(void);
    esp_err_t esp_board_periph_init(const char *);
    esp_err_t esp_board_periph_get_handle(const char *, void **);
    esp_err_t esp_board_periph_ref_handle(const char *, void **);
    esp_err_t esp_board_periph_unref_handle(const char *);
    esp_err_t esp_board_periph_get_config(const char *, void **);
    esp_err_t esp_board_periph_deinit(const char *);
    esp_err_t esp_board_device_get_handle(const char *, void **);
    esp_err_t esp_board_device_get_i2c_effective_addr(const char *, uint16_t *);
    esp_err_t esp_board_device_show(const char *);
}
