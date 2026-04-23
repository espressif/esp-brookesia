/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rainmaker_app_backend.h"
#include "rainmaker_app_backend_util.h"
#include "rainmaker_app_backend_shared.h"
#include "rainmaker_device_group_manager.h"
#include "rainmaker_list_manager.h"
#include "rainmaker_standard_device.h"
#include "rainmaker_api_handle.h"
#include "esp_log.h"

static const char *TAG = "rainmaker_app_backend";
static char *g_current_email = NULL;
static bool g_is_login_success = false;

/* Forward declarations */
static void login_failure_callback(int error_code, const char *failed_reason);
static void login_success_callback(void);

#define GLOBAL_RM_BASE_URL "https://api.rainmaker.espressif.com"

#define RM_APP_NVS_KEY_REFRESH_TOKEN "refresh_token"
#define RM_APP_NVS_KEY_EMAIL "email"

static void login_failure_callback(int error_code, const char *failed_reason)
{
    ESP_LOGI(TAG, "Login failed, error code: %d, reason: %s", error_code, failed_reason);
    g_is_login_success = false;
}

static void login_success_callback(void)
{
    ESP_LOGI(TAG, "Login success");
    g_is_login_success = true;
}

bool rm_app_backend_is_logged_in(void)
{
    /* Check if we have a saved refresh token */
    char *refresh_token = NULL;
    if (rainmaker_app_backend_util_nvs_read_str(RM_APP_NVS_KEY_REFRESH_TOKEN, &refresh_token) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read refresh token from NVS");
        return false;
    }
    if (!refresh_token || !refresh_token[0]) {
        ESP_LOGW(TAG, "Refresh token is empty");
        return false;
    }

    /* Check if we have a saved email */
    rainmaker_app_backend_util_safe_free_and_set_null((void **)&g_current_email);
    if (rainmaker_app_backend_util_nvs_read_str(RM_APP_NVS_KEY_EMAIL, &g_current_email) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read email from NVS");
        return false;
    }
    if (!g_current_email || !g_current_email[0]) {
        ESP_LOGW(TAG, "Email is empty");
        return false;
    }

    /* Use refresh token for future auto-login retries */
    (void)rainmaker_api_set_refresh_token(refresh_token);
    rainmaker_app_backend_util_safe_free(refresh_token);
    return true;
}

esp_err_t rm_app_backend_init(void)
{
    /* Initialize NVS */
    esp_err_t ret = rainmaker_app_backend_util_nvs_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = rm_device_group_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize device group manager: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = rm_list_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize schedule manager: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Init RainMaker API (User API wrapper) */
    ret = rainmaker_api_init(GLOBAL_RM_BASE_URL, login_failure_callback, login_success_callback);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init RainMaker API: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Initialize the current email */
    g_current_email = NULL;

    /* Initialize the login success flag */
    g_is_login_success = false;
    return ESP_OK;
}

bool rm_app_backend_login(const char *email, const char *password, const char **out_err_reason)
{
    if (email == NULL || password == NULL) {
        if (out_err_reason) {
            *out_err_reason = strdup("Email or password is NULL");
        }
        return false;
    }

    esp_err_t ret = rainmaker_api_set_username_password(email, password);
    if (ret != ESP_OK) {
        if (out_err_reason) {
            *out_err_reason = strdup("Failed to set username and password");
        }
        return false;
    }

    /* Initialize login success flag */
    g_is_login_success = false;
    ret = rainmaker_api_login();
    if (ret != ESP_OK) {
        if (out_err_reason) {
            *out_err_reason = strdup("Failed to login");
        }
        return false;
    }

    /* Check if login success */
    if (g_is_login_success) {
        /* Persist refresh token so we can auto-enter Home after restart */
        char *refresh_token = NULL;
        if (rainmaker_api_get_refresh_token(&refresh_token) == ESP_OK && refresh_token && refresh_token[0]) {
            if (rainmaker_app_backend_util_nvs_write_str(RM_APP_NVS_KEY_REFRESH_TOKEN, refresh_token) != ESP_OK) {
                rainmaker_app_backend_util_safe_free(refresh_token);
                if (out_err_reason) {
                    *out_err_reason = strdup("Failed to persist refresh token");
                }
                return false;
            }
            rainmaker_app_backend_util_safe_free(refresh_token);
        } else {
            rainmaker_app_backend_util_safe_free(refresh_token);
            if (out_err_reason) {
                *out_err_reason = strdup("Failed to get refresh token");
            }
            return false;
        }
        if (rainmaker_app_backend_util_nvs_write_str(RM_APP_NVS_KEY_EMAIL, email) != ESP_OK) {
            if (out_err_reason) {
                *out_err_reason = strdup("Failed to persist email");
            }
            return false;
        }
        rainmaker_app_backend_util_safe_free_and_set_null((void **)&g_current_email);
        g_current_email = strdup(email);
        if (g_current_email == NULL) {
            if (out_err_reason) {
                *out_err_reason = strdup("Failed to allocate memory for email");
            }
            return false;
        }
        return true;
    } else {
        if (out_err_reason) {
            *out_err_reason = strdup("Login failed");
        }
        return false;
    }
}

const char *rm_app_backend_get_current_group_name(void)
{
    return rm_app_backend_shared_get_current_group_name();
}

int rm_app_backend_get_groups_count(void)
{
    return rm_app_backend_shared_get_groups_count();
}

bool rm_app_backend_get_group_by_index(int index, rm_app_group_t *group)
{
    return rm_app_backend_shared_get_group_by_index(index, group);
}

void rm_app_backend_set_current_group_by_name(const char *group_name)
{
    rm_app_backend_shared_set_current_group_by_name(group_name);
}

int rm_app_backend_get_devices_count(void)
{
    return rm_app_backend_shared_get_devices_count();
}

rm_device_item_handle_t rm_app_backend_get_device_by_index(int index, rm_app_device_t *device)
{
    rm_device_item_handle_t handle = rm_app_backend_shared_get_device_by_index(index, device);
    if (handle == NULL && (index < 0 || device == NULL)) {
        ESP_LOGE(TAG, "Invalid index or device pointer");
    }
    if (handle == NULL && g_devices_list == NULL) {
        ESP_LOGE(TAG, "Devices list is NULL");
    }
    return handle;
}

int rm_app_backend_get_current_home_devices_count(void)
{
    return rm_app_backend_shared_get_current_home_devices_count();
}

rm_device_item_handle_t rm_app_backend_get_current_home_device_by_index(int index, rm_app_device_t *device)
{
    rm_device_item_handle_t handle = rm_app_backend_shared_get_current_home_device_by_index(index, device);
    if (handle == NULL && (index < 0 || device == NULL)) {
        ESP_LOGE(TAG, "Invalid index or device pointer");
    }
    if (handle == NULL && (g_current_group == NULL || g_current_group->nodes_list == NULL)) {
        ESP_LOGE(TAG, "Current group is NULL");
    }
    return handle;
}

bool rm_app_backend_get_device_info_by_handle(rm_device_item_handle_t handle, rm_app_device_t *device)
{
    return rm_app_backend_shared_get_device_info_by_handle(handle, device);
}

rm_device_item_handle_t rm_app_backend_get_device_handle_by_node_id(const char *node_id)
{
    return rm_app_backend_shared_get_device_handle_by_node_id(node_id);
}

rm_app_device_type_t rm_app_backend_get_device_type(rm_device_item_handle_t device)
{
    return rm_app_backend_shared_get_device_type(device);
}

bool rm_app_backend_refresh_home_screen(bool all)
{
    esp_err_t ret = ESP_FAIL;
    char *saved_group_name = NULL;
    if (all && g_current_group && g_current_group->group_name && g_current_group->group_name[0]) {
        saved_group_name = strdup(g_current_group->group_name);
        if (saved_group_name == NULL) {
            ESP_LOGW(TAG, "Failed to save current group name for refresh");
        }
    }
    if (all) {
        rm_device_group_manager_clear_group();
        ret = rm_device_group_manager_update_groups_list();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to update groups list: %s", esp_err_to_name(ret));
            free(saved_group_name);
            return false;
        }
        if (saved_group_name) {
            ret = rm_device_group_manager_find_group(saved_group_name);
            if (ret != ESP_OK) {
                (void)rm_device_group_manager_select_default_group();
            }
        } else {
            (void)rm_device_group_manager_select_default_group();
        }
        free(saved_group_name);
        saved_group_name = NULL;
    }
    rm_device_group_manager_clear_device();
    rm_list_manager_clear_item();
    ret = rm_device_group_manager_update_devices_list();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to update devices list: %s", esp_err_to_name(ret));
        return false;
    }
    return true;
}

bool rm_app_backend_refresh_scenes(void)
{
    return rm_app_backend_refresh_home_screen(false);
}

bool rm_app_backend_refresh_schedules(void)
{
    return rm_app_backend_refresh_home_screen(false);
}

bool rm_app_backend_add_automations(void)
{
    /* TODO: implement real add automation */
    ESP_LOGI(TAG, "Automations add backend not implemented (stub)");
    return true;
}

const char *rm_app_backend_get_current_user_email(void)
{
    if (g_current_email) {
        return g_current_email;
    }
    return "Unknown";
}

/* When the user logs out, we need to clear the cache, include the devices list, groups list, current group
, current email, login success flag, the refresh token and email in NVS, deinitialize the RainMaker API */
bool rm_app_backend_logout(void)
{
    rm_device_group_manager_deinit();
    rm_list_manager_deinit();
    /* Clear the current email */
    rainmaker_app_backend_util_safe_free_and_set_null((void **)&g_current_email);
    /* Erase the refresh token and email from NVS */
    (void)rainmaker_app_backend_util_nvs_erase_key(RM_APP_NVS_KEY_REFRESH_TOKEN);
    (void)rainmaker_app_backend_util_nvs_erase_key(RM_APP_NVS_KEY_EMAIL);
    /* Clear the login success flag */
    g_is_login_success = false;
    /* Deinitialize the RainMaker API */
    (void)rainmaker_api_deinit();
    return true;
}

void rm_app_backend_deinit(void)
{
    rm_device_group_manager_deinit();
    rm_list_manager_deinit();
    /* Clear the current email */
    rainmaker_app_backend_util_safe_free_and_set_null((void **)&g_current_email);
    /* Clear the login success flag */
    g_is_login_success = false;
    /* Deinitialize the RainMaker API */
    (void)rainmaker_api_deinit();
}
