/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "rainmaker_user_api_hal.h"

extern esp_err_t app_rmaker_user_api_pc_init(app_rmaker_user_api_config_t *config);
extern esp_err_t app_rmaker_user_api_pc_deinit(void);
extern esp_err_t app_rmaker_user_api_pc_register_login_failure_callback(
    app_rmaker_user_api_login_failure_callback_t callback);
extern esp_err_t app_rmaker_user_api_pc_register_login_success_callback(
    app_rmaker_user_api_login_success_callback_t callback);
extern esp_err_t app_rmaker_user_api_pc_set_refresh_token(const char *refresh_token);
extern esp_err_t app_rmaker_user_api_pc_get_refresh_token(char **refresh_token);
extern esp_err_t app_rmaker_user_api_pc_set_username_password(const char *username, const char *password);
extern esp_err_t app_rmaker_user_api_pc_login(void);
extern esp_err_t app_rmaker_user_api_pc_generic(app_rmaker_user_api_request_config_t *request_config, int *status_code,
        char **response_data);

esp_err_t rainmaker_user_api_hal_init(app_rmaker_user_api_config_t *config)
{
    return app_rmaker_user_api_pc_init(config);
}

esp_err_t rainmaker_user_api_hal_deinit(void)
{
    return app_rmaker_user_api_pc_deinit();
}

esp_err_t rainmaker_user_api_hal_register_login_failure_callback(
    app_rmaker_user_api_login_failure_callback_t callback)
{
    return app_rmaker_user_api_pc_register_login_failure_callback(callback);
}

esp_err_t rainmaker_user_api_hal_register_login_success_callback(
    app_rmaker_user_api_login_success_callback_t callback)
{
    return app_rmaker_user_api_pc_register_login_success_callback(callback);
}

esp_err_t rainmaker_user_api_hal_set_refresh_token(const char *refresh_token)
{
    return app_rmaker_user_api_pc_set_refresh_token(refresh_token);
}

esp_err_t rainmaker_user_api_hal_get_refresh_token(char **refresh_token)
{
    return app_rmaker_user_api_pc_get_refresh_token(refresh_token);
}

esp_err_t rainmaker_user_api_hal_set_username_password(const char *username, const char *password)
{
    return app_rmaker_user_api_pc_set_username_password(username, password);
}

esp_err_t rainmaker_user_api_hal_login(void)
{
    return app_rmaker_user_api_pc_login();
}

esp_err_t rainmaker_user_api_hal_generic(app_rmaker_user_api_request_config_t *request_config, int *status_code,
        char **response_data)
{
    return app_rmaker_user_api_pc_generic(request_config, status_code, response_data);
}
