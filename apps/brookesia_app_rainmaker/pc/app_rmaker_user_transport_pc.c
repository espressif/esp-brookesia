/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "app_rmaker_user_transport_pc.h"
#include <curl/curl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct curl_mem {
    char *data;
    size_t len;
};

static pthread_once_t s_curl_once = PTHREAD_ONCE_INIT;

static void curl_global_init_once(void)
{
    (void)curl_global_init(CURL_GLOBAL_DEFAULT);
}

static bool is_recoverable_curl_error(CURLcode code)
{
    switch (code) {
    case CURLE_COULDNT_CONNECT:
    case CURLE_OPERATION_TIMEDOUT:
    case CURLE_GOT_NOTHING:
    case CURLE_SEND_ERROR:
    case CURLE_RECV_ERROR:
    case CURLE_PARTIAL_FILE:
        return true;
    default:
        return false;
    }
}

static size_t curl_write_mem_cb(void *ptr, size_t size, size_t nmemb, void *userdata)
{
    struct curl_mem *m = (struct curl_mem *)userdata;
    size_t add = size * nmemb;
    char *n = realloc(m->data, m->len + add + 1);
    if (!n) {
        return 0;
    }
    m->data = n;
    memcpy(m->data + m->len, (const char *)ptr, add);
    m->len += add;
    m->data[m->len] = '\0';
    return add;
}

static const char *method_str(app_rmaker_user_api_type_t t)
{
    switch (t) {
    case APP_RMAKER_USER_API_TYPE_POST:
        return "POST";
    case APP_RMAKER_USER_API_TYPE_PUT:
        return "PUT";
    case APP_RMAKER_USER_API_TYPE_DELETE:
        return "DELETE";
    case APP_RMAKER_USER_API_TYPE_GET:
    default:
        return "GET";
    }
}

esp_err_t app_rmaker_user_transport_pc_execute(void **persist_curl, const char *url,
        app_rmaker_user_api_request_config_t *request_config,
        const char *access_token, bool need_authorize, int *status_code,
        char **response_data, bool *out_recoverable)
{
    if (!url || !request_config || !status_code || !response_data) {
        return ESP_ERR_INVALID_ARG;
    }
    if (out_recoverable) {
        *out_recoverable = false;
    }
    *response_data = NULL;
    *status_code = 0;

    (void)pthread_once(&s_curl_once, curl_global_init_once);

    bool use_persistent = (request_config->reuse_session && persist_curl != NULL);
    CURL *curl = NULL;

    if (use_persistent) {
        curl = (CURL *)(*persist_curl);
        if (!curl) {
            curl = curl_easy_init();
            if (!curl) {
                return ESP_FAIL;
            }
            *persist_curl = (void *)curl;
        }
    } else {
        curl = curl_easy_init();
    }

    if (!curl) {
        return ESP_FAIL;
    }

    /* Keep per-easy connection cache (TCP/TLS session reuse), while resetting request-specific state. */
    curl_easy_reset(curl);

    struct curl_mem chunk = {NULL, 0};
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method_str(request_config->api_type));
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(curl, CURLOPT_FORBID_REUSE, use_persistent ? 0L : 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_mem_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &chunk);

    struct curl_slist *hdr = NULL;
    if (need_authorize && access_token) {
        char auth[4608];
        snprintf(auth, sizeof(auth), "Authorization: %s", access_token);
        hdr = curl_slist_append(hdr, auth);
    }
    if (request_config->payload_is_json) {
        hdr = curl_slist_append(hdr, "Content-Type: application/json");
    } else {
        hdr = curl_slist_append(hdr, "Content-Type: application/x-www-form-urlencoded");
    }
    hdr = curl_slist_append(hdr, "accept: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdr);

    if (request_config->api_type == APP_RMAKER_USER_API_TYPE_POST
            || request_config->api_type == APP_RMAKER_USER_API_TYPE_PUT) {
        if (request_config->api_payload) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_config->api_payload);
        }
    }

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_slist_free_all(hdr);

    if (res != CURLE_OK) {
        if (use_persistent && is_recoverable_curl_error(res)) {
            if (out_recoverable) {
                *out_recoverable = true;
            }
            /* Drop broken session so caller retry builds a fresh TCP/TLS connection. */
            curl_easy_cleanup(curl);
            *persist_curl = NULL;
        } else if (!use_persistent) {
            curl_easy_cleanup(curl);
        }
        free(chunk.data);
        return ESP_FAIL;
    }

    if (!use_persistent) {
        curl_easy_cleanup(curl);
    }

    *status_code = (int)http_code;
    *response_data = chunk.data;
    return ESP_OK;
}

void app_rmaker_user_transport_pc_cleanup(void **persist_curl)
{
    if (!persist_curl || !*persist_curl) {
        return;
    }
    CURL *curl = (CURL *)(*persist_curl);
    curl_easy_cleanup(curl);
    *persist_curl = NULL;
}
