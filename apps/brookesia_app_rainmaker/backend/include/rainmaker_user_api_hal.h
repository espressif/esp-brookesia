/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file rainmaker_user_api_hal.h
 *
 * Platform HAL for RainMaker User API calls used by rainmaker_api_handle.c.
 * - On ESP-IDF: forwards to the espressif/rmaker_user_api component (app_rmaker_user_api_*).
 * - On PC (RM_HOST_BUILD): implemented in pc/rainmaker_user_api_hal_pc.c using libcurl.
 */

#pragma once

#include <esp_err.h>
#include "app_rmaker_user_api.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t rainmaker_user_api_hal_init(app_rmaker_user_api_config_t *config);
esp_err_t rainmaker_user_api_hal_deinit(void);

esp_err_t rainmaker_user_api_hal_register_login_failure_callback(
    app_rmaker_user_api_login_failure_callback_t callback);
esp_err_t rainmaker_user_api_hal_register_login_success_callback(
    app_rmaker_user_api_login_success_callback_t callback);

esp_err_t rainmaker_user_api_hal_set_refresh_token(const char *refresh_token);
esp_err_t rainmaker_user_api_hal_get_refresh_token(char **refresh_token);
esp_err_t rainmaker_user_api_hal_set_username_password(const char *username, const char *password);

esp_err_t rainmaker_user_api_hal_login(void);

/**
 * Same contract as app_rmaker_user_api_generic(): execute User API request (GET/POST/PUT/DELETE).
 */
esp_err_t rainmaker_user_api_hal_generic(app_rmaker_user_api_request_config_t *request_config, int *status_code,
        char **response_data);

#ifdef __cplusplus
}
#endif
