/**
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2025 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include <sys/param.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "https_client.h"
#include "esp_tls.h"
#include <sdkconfig.h>
#ifdef CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
#include "esp_crt_bundle.h"
#endif
#include "esp_http_client.h"

static const char *TAG = "HTTPS_CLIENT";

#define OPENAI_REALTIME_CALLS_URL "https://api.openai.com/v1/realtime/calls"
#define MULTIPART_BOUNDARY          "----ESPOpenAIBoundary7MA4YWxkTrZu0gW"

static char *build_realtime_multipart_body(const char *sdp, const char *session_json, size_t *out_len)
{
    const char *prefix =
        "--" MULTIPART_BOUNDARY "\r\n"
        "Content-Disposition: form-data; name=\"sdp\"\r\n"
        "Content-Type: application/sdp\r\n"
        "\r\n";
    const char *middle =
        "\r\n"
        "--" MULTIPART_BOUNDARY "\r\n"
        "Content-Disposition: form-data; name=\"session\"\r\n"
        "Content-Type: application/json\r\n"
        "\r\n";
    const char *suffix = "\r\n--" MULTIPART_BOUNDARY "--\r\n";

    size_t len = strlen(prefix) + strlen(sdp) + strlen(middle) + strlen(session_json) + strlen(suffix) + 1;
    char *body = (char *)malloc(len);
    if (body == NULL) {
        return NULL;
    }
    char *p = body;
    p += sprintf(p, "%s%s%s%s%s", prefix, sdp, middle, session_json, suffix);
    *out_len = p - body;
    return body;
}

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


int https_send_request(const char *url, const char *api_key,const char *method, char *data, char *body)
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

int https_post_realtime_call(const char *api_key, const char *sdp, const char *session_json, char *body)
{
    if (api_key == NULL || sdp == NULL || session_json == NULL || body == NULL) {
        return -1;
    }

    size_t multipart_len = 0;
    char *multipart = build_realtime_multipart_body(sdp, session_json, &multipart_len);
    if (multipart == NULL) {
        ESP_LOGE(TAG, "Failed to build multipart body");
        return -1;
    }

    esp_http_client_config_t config = {
        .url = OPENAI_REALTIME_CALLS_URL,
        .event_handler = _http_event_handler,
#ifdef CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
        .crt_bundle_attach = esp_crt_bundle_attach,
#endif
        .user_data = body,
        .timeout_ms = 30000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Fail to init client");
        free(multipart);
        return -1;
    }

    char auth[256];
    snprintf(auth, sizeof(auth), "Bearer %s", api_key);
    char content_type[128];
    snprintf(content_type, sizeof(content_type), "multipart/form-data; boundary=" MULTIPART_BOUNDARY);

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Authorization", auth);
    esp_http_client_set_header(client, "Content-Type", content_type);
    esp_http_client_set_post_field(client, multipart, multipart_len);

    memset(body, 0, 4096);
    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Realtime call Status = %d, content_length = %lld", status,
                 esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "Realtime call request failed: %s", esp_err_to_name(err));
        status = -1;
    }

    esp_http_client_cleanup(client);
    free(multipart);
    return status;
}
