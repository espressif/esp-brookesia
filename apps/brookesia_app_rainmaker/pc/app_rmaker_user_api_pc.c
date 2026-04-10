/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* RainMaker User API core (init, login, execute request, token/URL config) */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>
#include <esp_log.h>
#include <cJSON.h>
#include "app_rmaker_user_api.h"
#include "app_rmaker_user_transport_pc.h"

/* Forward declarations (mutual recursion with pc_generic / login / get_api_version) */
esp_err_t app_rmaker_user_api_pc_generic(app_rmaker_user_api_request_config_t *request_config, int *status_code,
        char **response_data);
esp_err_t app_rmaker_user_api_pc_login(void);

/**
 * @brief RainMaker User API context structure
 */
typedef struct {
    bool            is_initialized;      /**< Whether the User API is initialized */
    bool            api_version_checked; /**< Whether the API version has been checked */
    pthread_mutex_t api_lock;            /**< Mutex for API serialization (recursive) */
    char           *api_version;         /**< API version string */
    char           *access_token;        /**< Current access token */
    char           *refresh_token;       /**< Stored refresh token */
    char           *base_url;            /**< Base URL for RainMaker User API */
    char           *username;           /**< Username for login */
    char           *password;            /**< Password for login */
    void           *http_client;         /**< Persistent curl handle for optional session reuse */
    app_rmaker_user_api_login_failure_callback_t login_failure_callback; /**< Login failure callback */
    app_rmaker_user_api_login_success_callback_t login_success_callback;  /**< Login success callback */
} app_rmaker_user_api_context_t;

/* Global context */
static const char *TAG = "rmaker_user_api_pc";
static const char *rmaker_user_api_versions_url = "apiversions";
static const char *rmaker_user_api_login_url = "login2";

static app_rmaker_user_api_context_t g_rmaker_user_api_ctx = {0};
static bool s_api_mutex_inited;

#define APP_RMAKER_USER_API_URL_LEN                 256
#define APP_RMAKER_USER_API_DEFAULT_BASE_URL        "https://api.rainmaker.espressif.com"
#define APP_RMAKER_USER_API_LOCK()                  pthread_mutex_lock(&g_rmaker_user_api_ctx.api_lock)
#define APP_RMAKER_USER_API_UNLOCK()                pthread_mutex_unlock(&g_rmaker_user_api_ctx.api_lock)

/**
 * @brief Safe string copy helper
 */
char *app_rmaker_user_api_safe_strdup(const char *str)
{
    if (!str) {
        return NULL;
    }
    return strdup(str);
}

/**
 * @brief Safe string free helper
 */
void app_rmaker_user_api_safe_free(char **ptr)
{
    if (ptr && *ptr) {
        free(*ptr);
        *ptr = NULL;
    }
}

/**
 * @brief Check whether base URL and (username/password or refresh token) are set
 */
static bool pc_credentials_check(void)
{
    if (!g_rmaker_user_api_ctx.base_url) {
        ESP_LOGE(TAG, "Base URL is not set, please use app_rmaker_user_api_pc_set_base_url to set it");
        return false;
    }
    if (!g_rmaker_user_api_ctx.username || !g_rmaker_user_api_ctx.password) {
        if (!g_rmaker_user_api_ctx.refresh_token) {
            ESP_LOGE(TAG,
                     "Username and password or refresh token is not set, please use "
                     "app_rmaker_user_api_pc_set_username_password or app_rmaker_user_api_pc_set_refresh_token to set it");
            return false;
        }
    }
    return true;
}

/**
 * @brief Clear user info when the user account or base URL is changed
 */
static void pc_clear_user_info(void)
{
    app_rmaker_user_api_safe_free(&g_rmaker_user_api_ctx.access_token);
}

/**
 * @brief After HTTP body is read, check for auth errors and whether to retry the same request.
 */
static esp_err_t pc_handle_http_response_body(int *status_code, char **response_data, bool *need_retry)
{
    *need_retry = false;
    /* Never log full body: user/nodes can be multi‑MB and overwhelms the terminal. */
    if (*response_data) {
        size_t len = strlen(*response_data);
        int n = (len > 200u) ? 200 : (int)len;
        ESP_LOGD(TAG, "Status code: %d, len=%zu, body_prefix=%.*s%s", *status_code, len, n, *response_data,
                 (len > 200u) ? "..." : "");
    } else {
        ESP_LOGD(TAG, "Status code: %d, response_data: (null)", *status_code);
    }

    if (*status_code == 401 && *response_data && strstr(*response_data, "Unauthorized")) {
        ESP_LOGW(TAG, "Access token expired, attempting re-login");
        if (app_rmaker_user_api_pc_login() == ESP_OK) {
            *need_retry = true;
        } else {
            *need_retry = false;
        }
    }

    return ESP_OK;
}

/**
 * @brief Create JSON payload with refresh token
 */
static char *pc_create_login_payload(void)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        ESP_LOGE(TAG, "Failed to create JSON object");
        return NULL;
    }

    /* If username and password are set, use them to login */
    if (g_rmaker_user_api_ctx.username && g_rmaker_user_api_ctx.password) {
        cJSON_AddStringToObject(root, "user_name", g_rmaker_user_api_ctx.username);
        cJSON_AddStringToObject(root, "password", g_rmaker_user_api_ctx.password);
    } else if (g_rmaker_user_api_ctx.refresh_token) {
        cJSON_AddStringToObject(root, "refreshtoken", g_rmaker_user_api_ctx.refresh_token);
    } else {
        ESP_LOGE(TAG,
                 "Username and password or refresh token is not set, please use app_rmaker_user_api_pc_set_username_password "
                 "or app_rmaker_user_api_pc_set_refresh_token to set it");
        return NULL;
    }
    char *json_string = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_string;
}

/**
 * @brief Parse login response
 */
static esp_err_t pc_parse_login_response(const char *response_data)
{
    cJSON *response = cJSON_Parse(response_data);
    if (!response) {
        ESP_LOGE(TAG, "Failed to parse response JSON");
        return ESP_FAIL;
    }

    cJSON *status = cJSON_GetObjectItem(response, "status");
    if (!status || !status->valuestring || strcmp(status->valuestring, "success") != 0) {
        cJSON *description = cJSON_GetObjectItem(response, "description");
        ESP_LOGE(TAG, "Login failed: %s",
                 description && description->valuestring ? description->valuestring : "Unknown error");
        cJSON_Delete(response);
        return ESP_FAIL;
    }

    cJSON *access_token_json = cJSON_GetObjectItem(response, "accesstoken");
    if (!access_token_json || !access_token_json->valuestring) {
        ESP_LOGE(TAG, "No access token in response");
        cJSON_Delete(response);
        return ESP_FAIL;
    }

    app_rmaker_user_api_safe_free(&g_rmaker_user_api_ctx.access_token);
    g_rmaker_user_api_ctx.access_token = app_rmaker_user_api_safe_strdup(access_token_json->valuestring);
    ESP_LOGD(TAG, "Access token saved successfully");

    /* Save refresh token if provided (login2 response includes refreshtoken on username/password login). */
    cJSON *refresh_token_json = cJSON_GetObjectItem(response, "refreshtoken");
    if (refresh_token_json && cJSON_IsString(refresh_token_json) && refresh_token_json->valuestring) {
        app_rmaker_user_api_safe_free(&g_rmaker_user_api_ctx.refresh_token);
        g_rmaker_user_api_ctx.refresh_token = app_rmaker_user_api_safe_strdup(refresh_token_json->valuestring);
        if (g_rmaker_user_api_ctx.refresh_token) {
            ESP_LOGD(TAG, "Refresh token saved successfully");
        }
    }

    if (g_rmaker_user_api_ctx.login_success_callback) {
        g_rmaker_user_api_ctx.login_success_callback();
    }
    cJSON_Delete(response);
    return ESP_OK;
}

/**
 * @brief Cleanup RainMaker User API context
 */
static void pc_cleanup_context(void)
{
    pc_clear_user_info();
    app_rmaker_user_api_safe_free(&g_rmaker_user_api_ctx.username);
    app_rmaker_user_api_safe_free(&g_rmaker_user_api_ctx.password);
    app_rmaker_user_api_safe_free(&g_rmaker_user_api_ctx.refresh_token);
    app_rmaker_user_api_safe_free(&g_rmaker_user_api_ctx.base_url);
    app_rmaker_user_api_safe_free(&g_rmaker_user_api_ctx.api_version);
    app_rmaker_user_transport_pc_cleanup(&g_rmaker_user_api_ctx.http_client);
    g_rmaker_user_api_ctx.http_client = NULL;
    if (s_api_mutex_inited) {
        (void)pthread_mutex_destroy(&g_rmaker_user_api_ctx.api_lock);
        s_api_mutex_inited = false;
    }
    g_rmaker_user_api_ctx.api_version_checked = false;
    g_rmaker_user_api_ctx.login_failure_callback = NULL;
    g_rmaker_user_api_ctx.login_success_callback = NULL;
}

/* ---------- Public API implementation ---------- */

esp_err_t app_rmaker_user_api_pc_init(app_rmaker_user_api_config_t *config)
{
    if (!config) {
        ESP_LOGE(TAG, "Configuration is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (g_rmaker_user_api_ctx.is_initialized) {
        ESP_LOGE(TAG, "RainMaker User API is already initialized");
        return ESP_ERR_INVALID_STATE;
    }
    g_rmaker_user_api_ctx.refresh_token = app_rmaker_user_api_safe_strdup(config->refresh_token);
    g_rmaker_user_api_ctx.base_url = app_rmaker_user_api_safe_strdup(config->base_url ? config->base_url : APP_RMAKER_USER_API_DEFAULT_BASE_URL);
    g_rmaker_user_api_ctx.username = app_rmaker_user_api_safe_strdup(config->username);
    g_rmaker_user_api_ctx.password = app_rmaker_user_api_safe_strdup(config->password);
    g_rmaker_user_api_ctx.api_version = app_rmaker_user_api_safe_strdup(config->api_version);
    /* If the API version is set, set the API version checked to true, otherwise set it to false */
    g_rmaker_user_api_ctx.api_version_checked = g_rmaker_user_api_ctx.api_version ? true : false;
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    if (pthread_mutex_init(&g_rmaker_user_api_ctx.api_lock, &attr) != 0) {
        pthread_mutexattr_destroy(&attr);
        ESP_LOGE(TAG, "Failed to create API lock");
        pc_cleanup_context();
        return ESP_ERR_NO_MEM;
    }
    pthread_mutexattr_destroy(&attr);
    s_api_mutex_inited = true;
    g_rmaker_user_api_ctx.is_initialized = true;
    return ESP_OK;
}

esp_err_t app_rmaker_user_api_pc_deinit(void)
{
    if (!g_rmaker_user_api_ctx.is_initialized) {
        ESP_LOGE(TAG, "RainMaker User API is not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    pc_cleanup_context();
    g_rmaker_user_api_ctx.is_initialized = false;
    return ESP_OK;
}

esp_err_t app_rmaker_user_api_pc_register_login_failure_callback(app_rmaker_user_api_login_failure_callback_t callback)
{
    if (!callback) {
        ESP_LOGE(TAG, "Callback is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (g_rmaker_user_api_ctx.login_failure_callback) {
        ESP_LOGW(TAG, "Login failure callback is already registered, overriding it");
    }
    g_rmaker_user_api_ctx.login_failure_callback = callback;
    return ESP_OK;
}

esp_err_t app_rmaker_user_api_pc_register_login_success_callback(app_rmaker_user_api_login_success_callback_t callback)
{
    if (!callback) {
        ESP_LOGE(TAG, "Callback is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (g_rmaker_user_api_ctx.login_success_callback) {
        ESP_LOGW(TAG, "Login success callback is already registered, overriding it");
    }
    g_rmaker_user_api_ctx.login_success_callback = callback;
    return ESP_OK;
}

/* GET /apiversions */
esp_err_t app_rmaker_user_api_pc_get_api_version(char **api_version)
{
    if (!api_version) {
        ESP_LOGE(TAG, "API version is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    char *response_data = NULL;
    *api_version = NULL;
    int status_code = 0;

    app_rmaker_user_api_request_config_t request_config = {
        .reuse_session = true,
        .no_need_authorize = true,
        .api_type = APP_RMAKER_USER_API_TYPE_GET,
        .api_name = rmaker_user_api_versions_url,
        .api_version = NULL,
        .api_query_params = NULL,
        .api_payload = NULL,
    };

    esp_err_t err = app_rmaker_user_api_pc_generic(&request_config, &status_code, &response_data);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to execute get API version request");
        return err;
    }

    cJSON *response = cJSON_Parse(response_data);
    app_rmaker_user_api_safe_free(&response_data);
    if (response) {
        cJSON *supported_versions = cJSON_GetObjectItem(response, "supported_versions");
        if (supported_versions && cJSON_IsArray(supported_versions)) {
            cJSON *version = cJSON_GetArrayItem(supported_versions, 0);
            if (version && cJSON_IsString(version)) {
                *api_version = app_rmaker_user_api_safe_strdup(version->valuestring);
            }
        } else {
            ESP_LOGE(TAG, "No supported versions found in response");
            err = ESP_FAIL;
        }
        cJSON_Delete(response);
    } else {
        ESP_LOGE(TAG, "Failed to parse API version response");
        err = ESP_FAIL;
    }
    return err;
}

/* POST /{version}/login2 */
esp_err_t app_rmaker_user_api_pc_login(void)
{
    if (!pc_credentials_check()) {
        return ESP_ERR_INVALID_STATE;
    }

    const char *payload = pc_create_login_payload();
    if (!payload) {
        ESP_LOGE(TAG, "Failed to create login payload");
        return ESP_FAIL;
    }
    app_rmaker_user_api_request_config_t request_config = {
        .reuse_session = true,
        .no_need_authorize = true,
        .payload_is_json = true,
        .api_type = APP_RMAKER_USER_API_TYPE_POST,
        .api_name = rmaker_user_api_login_url,
        .api_version = g_rmaker_user_api_ctx.api_version,
        .api_query_params = NULL,
        .api_payload = payload,
    };

    int status_code = 0;
    char *response_data = NULL;
    esp_err_t err = app_rmaker_user_api_pc_generic(&request_config, &status_code, &response_data);
    free((char *)payload);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to execute login request");
        return err;
    }

    /* Get response status code */
    if (status_code != 200) {
        ESP_LOGE(TAG, "HTTP POST request failed with status code: %d", status_code);
        if (response_data) {
            cJSON *response = cJSON_Parse(response_data);
            app_rmaker_user_api_safe_free(&response_data);
            if (response) {
                cJSON *error = cJSON_GetObjectItem(response, "error_code");
                cJSON *description = cJSON_GetObjectItem(response, "description");
                if (error && cJSON_IsNumber(error) && description && cJSON_IsString(description)) {
                    if (g_rmaker_user_api_ctx.login_failure_callback) {
                        g_rmaker_user_api_ctx.login_failure_callback(error->valueint, description->valuestring);
                    }
                }
                cJSON_Delete(response);
            }
        }
        return ESP_FAIL;
    }

    /* Parse response and update access token */
    err = pc_parse_login_response(response_data);
    app_rmaker_user_api_safe_free(&response_data);
    return err;
}

esp_err_t app_rmaker_user_api_pc_set_refresh_token(const char *refresh_token)
{
    if (!refresh_token) {
        ESP_LOGE(TAG, "Refresh token is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    app_rmaker_user_api_safe_free(&g_rmaker_user_api_ctx.refresh_token);
    /* Set refresh token */
    g_rmaker_user_api_ctx.refresh_token = app_rmaker_user_api_safe_strdup(refresh_token);
    if (!g_rmaker_user_api_ctx.refresh_token) {
        ESP_LOGE(TAG, "Failed to allocate memory for refresh token");
        return ESP_ERR_NO_MEM;
    }

    /* Clear user info */
    pc_clear_user_info();
    return ESP_OK;
}

esp_err_t app_rmaker_user_api_pc_get_refresh_token(char **refresh_token)
{
    if (!refresh_token) {
        ESP_LOGE(TAG, "Refresh token output is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    *refresh_token = NULL;
    if (!g_rmaker_user_api_ctx.refresh_token) {
        return ESP_ERR_INVALID_STATE;
    }
    *refresh_token = app_rmaker_user_api_safe_strdup(g_rmaker_user_api_ctx.refresh_token);
    if (!*refresh_token) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t app_rmaker_user_api_pc_set_username_password(const char *username, const char *password)
{
    if (!username || !password) {
        ESP_LOGE(TAG, "Username or password is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    app_rmaker_user_api_safe_free(&g_rmaker_user_api_ctx.username);
    app_rmaker_user_api_safe_free(&g_rmaker_user_api_ctx.password);
    g_rmaker_user_api_ctx.username = app_rmaker_user_api_safe_strdup(username);
    g_rmaker_user_api_ctx.password = app_rmaker_user_api_safe_strdup(password);
    if (!g_rmaker_user_api_ctx.username || !g_rmaker_user_api_ctx.password) {
        ESP_LOGE(TAG, "Failed to allocate memory for username or password");
        app_rmaker_user_api_safe_free(&g_rmaker_user_api_ctx.username);
        app_rmaker_user_api_safe_free(&g_rmaker_user_api_ctx.password);
        return ESP_ERR_NO_MEM;
    }

    /* Clear user info */
    pc_clear_user_info();
    return ESP_OK;
}

esp_err_t app_rmaker_user_api_pc_set_base_url(const char *base_url)
{
    if (!base_url) {
        ESP_LOGE(TAG, "Base URL is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    app_rmaker_user_api_safe_free(&g_rmaker_user_api_ctx.base_url);
    g_rmaker_user_api_ctx.base_url = app_rmaker_user_api_safe_strdup(base_url);
    if (!g_rmaker_user_api_ctx.base_url) {
        ESP_LOGE(TAG, "Failed to allocate memory for base URL");
        return ESP_ERR_NO_MEM;
    }

    /* Clear user info */
    pc_clear_user_info();
    return ESP_OK;
}

esp_err_t app_rmaker_user_api_pc_generic(app_rmaker_user_api_request_config_t *request_config, int *status_code, char **response_data)
{
    if (!request_config || !request_config->api_name || !response_data) {
        ESP_LOGE(TAG, "Request config or response data is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    *response_data = NULL;
    *status_code = 0;
    /* Check the api type, query params and api payload */
    if (request_config->api_type == APP_RMAKER_USER_API_TYPE_GET) {
        if (!request_config->api_query_params) {
            ESP_LOGW(TAG, "Query params are empty for GET %s API, please check the API documentation", request_config->api_name);
        }
    } else if (request_config->api_type == APP_RMAKER_USER_API_TYPE_POST) {
        if (!request_config->api_payload) {
            ESP_LOGE(TAG, "API payload is required for POST %s API", request_config->api_name);
            return ESP_ERR_INVALID_ARG;
        }
    } else if (request_config->api_type == APP_RMAKER_USER_API_TYPE_PUT) {
        if (!request_config->api_payload) {
            ESP_LOGE(TAG, "API payload is required for PUT %s API", request_config->api_name);
            return ESP_ERR_INVALID_ARG;
        }
    } else if (request_config->api_type == APP_RMAKER_USER_API_TYPE_DELETE) {
        if (!request_config->api_query_params) {
            ESP_LOGW(TAG, "Query params are required for DELETE %s API", request_config->api_name);
            return ESP_ERR_INVALID_ARG;
        }
    } else {
        ESP_LOGE(TAG, "Invalid API type for %s API", request_config->api_name);
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = ESP_FAIL;
    /* Lock the API to make sure the API is not executed concurrently */
    APP_RMAKER_USER_API_LOCK();
    /* Check whether API version is set; if not, get it first */
    if (!request_config->api_version && !g_rmaker_user_api_ctx.api_version && !g_rmaker_user_api_ctx.api_version_checked) {
        g_rmaker_user_api_ctx.api_version_checked = true;
        err = app_rmaker_user_api_pc_get_api_version(&g_rmaker_user_api_ctx.api_version);
        if (err != ESP_OK) {
            g_rmaker_user_api_ctx.api_version_checked = false;
            goto unlock;
        }
    }

    /* Check access token and whether need authorization */
    if (!request_config->no_need_authorize) {
        if (!g_rmaker_user_api_ctx.access_token) {
            ESP_LOGW(TAG, "Access token not available, need login first");
            err = app_rmaker_user_api_pc_login();
            if (err != ESP_OK) {
                goto unlock;
            }
            return app_rmaker_user_api_pc_generic(request_config, status_code, response_data);
        }
    }

    /* Create URL */
    char url[APP_RMAKER_USER_API_URL_LEN] = {0};
    if (strncmp(request_config->api_name, rmaker_user_api_versions_url, strlen(rmaker_user_api_versions_url)) == 0) {
        snprintf(url, sizeof(url), "%s/%s", g_rmaker_user_api_ctx.base_url, request_config->api_name);
    } else {
        if (request_config->api_query_params) {
            snprintf(url, sizeof(url), "%s/%s/%s?%s", g_rmaker_user_api_ctx.base_url, request_config->api_version ? request_config->api_version : g_rmaker_user_api_ctx.api_version, request_config->api_name, request_config->api_query_params);
        } else {
            snprintf(url, sizeof(url), "%s/%s/%s", g_rmaker_user_api_ctx.base_url, request_config->api_version ? request_config->api_version : g_rmaker_user_api_ctx.api_version, request_config->api_name);
        }
    }

    bool recoverable = false;
    err = app_rmaker_user_transport_pc_execute(
              &g_rmaker_user_api_ctx.http_client,
              url,
              request_config,
              g_rmaker_user_api_ctx.access_token,
              !request_config->no_need_authorize,
              status_code,
              response_data,
              &recoverable);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP transport failed");
        if (recoverable) {
            ESP_LOGI(TAG, "Retrying with new connection");
            return app_rmaker_user_api_pc_generic(request_config, status_code, response_data);
        }
        goto unlock;
    }

    bool need_retry = false;
    err = pc_handle_http_response_body(status_code, response_data, &need_retry);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to handle HTTP response");
        goto unlock;
    }

    if (need_retry) {
        app_rmaker_user_api_safe_free(response_data);
        *status_code = 0;
        return app_rmaker_user_api_pc_generic(request_config, status_code, response_data);
    }

unlock:
    APP_RMAKER_USER_API_UNLOCK();
    return err;
}
