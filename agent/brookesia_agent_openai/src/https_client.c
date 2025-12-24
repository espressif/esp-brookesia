/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <sys/param.h>
#include <string.h>
#include "esp_log.h"
#include "https_client.h"
#include "esp_tls.h"
#include <sdkconfig.h>
#ifdef CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
#include "esp_crt_bundle.h"
#endif
#include "esp_http_client.h"

static const char *TAG = "HTTPS_CLIENT";

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    static int output_len;
    char *body_data = evt->user_data;
    switch (evt->event_id) {
    default:
        break;
    case HTTP_EVENT_ERROR:
        ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key,
                 evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        if (!esp_http_client_is_chunked_response(evt->client)) {
            int content_len = esp_http_client_get_content_length(evt->client);
            if (content_len > 0) {
                if (output_len < content_len) {
                    memcpy(body_data + output_len, evt->data, evt->data_len);
                    output_len += evt->data_len;
                    printf("body_data: %s\n", body_data);
                }
            }
        }
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
        output_len = 0;
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
        int mbedtls_err = 0;
        esp_err_t err = esp_tls_get_and_clear_last_error((esp_tls_error_handle_t)evt->data,
                        &mbedtls_err, NULL);
        if (err != 0) {
            ESP_LOGD(TAG, "Last esp error code: 0x%x", err);
            ESP_LOGD(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
        }
        break;
    case HTTP_EVENT_REDIRECT:
        esp_http_client_set_redirection(evt->client);
        break;
    }
    return ESP_OK;
}


int https_send_request(const char *url, const char *api_key, const char *method, char *data, char *body)
{

    esp_http_client_config_t config = {
        .url = url,
        .event_handler = _http_event_handler,
#ifdef CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
        .crt_bundle_attach = esp_crt_bundle_attach,
#endif
        .user_data = body,
        .timeout_ms = 10000, // Change default timeout to be 10s
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Fail to init client");
        return -1;
    }
    // POST
    int err = 0;
    esp_http_client_set_url(client, url);
    if (strcmp(method, "POST") == 0) {
        esp_http_client_set_method(client, HTTP_METHOD_POST);
    } else if (strcmp(method, "DELETE") == 0) {
        esp_http_client_set_method(client, HTTP_METHOD_DELETE);
    } else if (strcmp(method, "PATCH") == 0) {
        esp_http_client_set_method(client, HTTP_METHOD_PATCH);
    } else {
        err = -1;
        goto _exit;
    }


    char auth[256];
    snprintf(auth, sizeof(auth), "Bearer %s", api_key);
    esp_http_client_set_header(client, "Content-Type", "application/sdp");
    esp_http_client_set_header(client, "Authorization", auth);

    esp_http_client_set_post_field(client, data, strlen(data));

    err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %lld",
                 esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    }
_exit:
    esp_http_client_cleanup(client);
    return err;
}

int https_post(const char *url, const char *api_key, char *data, char *body)
{
    return https_send_request(url, api_key, "POST", data, body);
}
