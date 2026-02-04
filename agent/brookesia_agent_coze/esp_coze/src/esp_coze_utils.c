/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_idf_version.h"
#include "esp_http_client.h"

#include <mbedtls/pk.h>
#include <mbedtls/error.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/md.h>

#include "esp_coze_utils.h"

static const char *TAG = "COZE_UTILS";

#define COZE_UTILS_FREE(x, fn) do {    \
    if (x) {                           \
        fn(x);                         \
        x = NULL;                      \
    }                                  \
} while (0)

#define BASE64_PAD         '='
#define BASE64DE_FIRST     '+'
#define BASE64DE_LAST      'z'

#define JWT_HEADER_FMT        "{\"alg\":\"RS256\",\"typ\":\"JWT\",\"kid\":\"%s\"}"
#define JWT_HEADER_MAX_LEN    128
#define JWT_PAYLOAD_MAX_LEN   512
#define JWT_SIGNATURE_MAX_LEN 1024
#define OBUF_SIZE             2048

#define DEFAULT_HTTP_BUFFER_SIZE (1024)
#define HTTP_FINISH_BIT          (1 << 0)
#define HTTP_TIMEOUT_MS          (10000)

/**
 * @brief HTTP client context for event handling
 */
typedef struct {
    http_response_t   *resp;
    EventGroupHandle_t eg;
} http_client_ctx_t;

static const char base64en[] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
    'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
    'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
    'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
    'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
    'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
    'w', 'x', 'y', 'z', '0', '1', '2', '3',
    '4', '5', '6', '7', '8', '9', '-', '_',
};


static int base64url_encode(const unsigned char *in, unsigned int inlen, char *out)
{
    unsigned int i, j;

    for (i = j = 0; i < inlen; i++) {
        int s = i % 3; /* from 6/gcd(6, 8) */

        switch (s) {
        case 0:
            out[j++] = base64en[(in[i] >> 2) & 0x3F];
            continue;
        case 1:
            out[j++] = base64en[((in[i - 1] & 0x3) << 4) + ((in[i] >> 4) & 0xF)];
            continue;
        case 2:
            out[j++] = base64en[((in[i - 1] & 0xF) << 2) + ((in[i] >> 6) & 0x3)];
            out[j++] = base64en[in[i] & 0x3F];
        }
    }

    /* Handle padding for incomplete groups */
    i -= 1;

    if ((i % 3) == 0) {
        out[j++] = base64en[(in[i] & 0x3) << 4];
        /* Note: Base64 URL encoding omits padding characters */
    } else if ((i % 3) == 1) {
        out[j++] = base64en[(in[i] & 0xF) << 2];
        /* Note: Base64 URL encoding omits padding characters */
    }

    out[j++] = 0; /* Null terminate */

    return 0;
}

static char *mbedtlsError(int errnum)
{
    static char buffer[200];
    mbedtls_strerror(errnum, buffer, sizeof(buffer));
    return buffer;
}

static esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    http_client_ctx_t *ctx = (http_client_ctx_t *)evt->user_data;
    static int output_len = 0;

    switch (evt->event_id) {
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
        ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s",
                 evt->header_key, evt->header_value);
        break;

    case HTTP_EVENT_ON_DATA:
        if (output_len + evt->data_len > ctx->resp->body_len) {
            ctx->resp->body = (char *)realloc(ctx->resp->body,
                                              DEFAULT_HTTP_BUFFER_SIZE + ctx->resp->body_len);
            ctx->resp->body_len += DEFAULT_HTTP_BUFFER_SIZE;
        }
        memcpy(ctx->resp->body + output_len, evt->data, evt->data_len);
        output_len += evt->data_len;
        break;

    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
        output_len = 0;
        xEventGroupSetBits(ctx->eg, HTTP_FINISH_BIT);
        break;

    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
        break;

    case HTTP_EVENT_REDIRECT:
        break;
    }
    return ESP_OK;
}

esp_err_t http_client_post(const char *url, http_req_header_t *header,
                           char *body, http_response_t *response)
{
    esp_err_t err = ESP_OK;
    esp_http_client_handle_t client = NULL;
    http_response_t rsp_data = {0};
    http_client_ctx_t ctx = {0};
    esp_http_client_config_t config = {0};

    /* Initialize response buffer */
    rsp_data.body = (char *)calloc(1, DEFAULT_HTTP_BUFFER_SIZE);
    if (!rsp_data.body) {
        ESP_LOGE(TAG, "Failed to allocate response buffer");
        return ESP_ERR_NO_MEM;
    }
    rsp_data.body_len = DEFAULT_HTTP_BUFFER_SIZE;
    ctx.resp = &rsp_data;
    ctx.eg = xEventGroupCreate();
    if (!ctx.eg) {
        ESP_LOGE(TAG, "Failed to create event group");
        free(rsp_data.body);
        return ESP_ERR_NO_MEM;
    }

    config.buffer_size_tx = 2048;
    config.url = url;
    config.query = "esp";
    config.event_handler = _http_event_handler;
    config.user_data = &ctx;
    client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        goto cleanup;
    }
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    if (header) {
        for (int i = 0; header[i].key && header[i].value; i++) {
            ESP_LOGD(TAG, "Setting header: %s: %s",
                     header[i].key, header[i].value);
            esp_http_client_set_header(client, header[i].key, header[i].value);
        }
    }
    if (body) {
        esp_http_client_set_post_field(client, body, strlen(body));
    }
    err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
        goto cleanup;
    }

    EventBits_t bits = xEventGroupWaitBits(ctx.eg, HTTP_FINISH_BIT,
                                           pdTRUE, pdFALSE,
                                           pdMS_TO_TICKS(HTTP_TIMEOUT_MS));
    if (!(bits & HTTP_FINISH_BIT)) {
        ESP_LOGE(TAG, "HTTP request timeout");
        err = ESP_ERR_TIMEOUT;
        goto cleanup;
    }

    *response = rsp_data;

    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    vEventGroupDelete(ctx.eg);
    return ESP_OK;

cleanup:
    if (client) {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
    }
    vEventGroupDelete(ctx.eg);
    free(rsp_data.body);
    return err;
}

char *coze_jwt_create_handler(const char *kid, const char *payload,
                              const uint8_t *privateKey, size_t privateKeySize)
{
    int r = 0;
    size_t signature_bytes_length = 0, key_length = privateKeySize;
    unsigned char hash[32] = {0};

    /* JWT components */
    char base64Header[JWT_HEADER_MAX_LEN] = {0};
    char header[JWT_HEADER_MAX_LEN] = {0};
    char base64Payload[JWT_PAYLOAD_MAX_LEN] = {0};
    char *headerAndPayload = NULL;
    char *base64Signature = NULL;
    char *jwt = NULL;

    /* mbedTLS contexts */
    mbedtls_pk_context pk;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;

    /* Working buffers */
    unsigned char *signature_bytes = NULL;
    unsigned char *key = NULL;

    /* Initialize mbedTLS contexts */
    mbedtls_pk_init(&pk);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);

    /* Step 1: Create and encode JWT header */
    snprintf(header, sizeof(header), JWT_HEADER_FMT, kid);
    base64url_encode((const unsigned char *)header, strlen(header), base64Header);

    /* Step 2: Encode JWT payload */
    base64url_encode((const unsigned char *)payload, strlen(payload), base64Payload);

    /* Step 3: Combine header and payload */
    size_t headerAndPayloadLen = strlen(base64Header) + 1 + strlen(base64Payload) + 1;
    headerAndPayload = (char *)calloc(1, headerAndPayloadLen);
    if (!headerAndPayload) {
        ESP_LOGE(TAG, "Failed to allocate memory for headerAndPayload");
        goto cleanup;
    }
    int headerAndPayload_len = snprintf(headerAndPayload, headerAndPayloadLen,
                                        "%s.%s", base64Header, base64Payload);

    /* Step 4: Allocate working buffers */
    signature_bytes = (unsigned char *)calloc(1, JWT_SIGNATURE_MAX_LEN);
    key = (unsigned char *)calloc(1, key_length + 2); // Extra space for NUL terminator

    if (!signature_bytes || !key) {
        ESP_LOGE(TAG, "Failed to allocate working buffers");
        goto cleanup;
    }

    /* Step 5: Prepare private key */
    memcpy(key, privateKey, key_length);
    if (key[key_length - 1] != '\0') {
        key[key_length] = '\0';
        key_length += 1;
    }

    /* Step 6: Initialize random number generator */
    r = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                              (const unsigned char *)"l8w8jwt_mbedtls_pers.!#@", 24);
    if (r != 0) {
        ESP_LOGE(TAG, "mbedtls_ctr_drbg_seed failed: %d (-0x%x): %s",
                 r, -r, mbedtlsError(r));
        goto cleanup;
    }

    /* Step 7: Parse private key */
    r = mbedtls_pk_parse_key(&pk, key, key_length, NULL, 0,
                             mbedtls_ctr_drbg_random, &ctr_drbg);
    if (r != 0) {
        ESP_LOGE(TAG, "mbedtls_pk_parse_key failed: %d (-0x%x): %s",
                 r, -r, mbedtlsError(r));
        goto cleanup;
    }

    /* Step 8: Validate key type and length */
    if (!mbedtls_pk_can_do(&pk, MBEDTLS_PK_RSA) &&
            !mbedtls_pk_can_do(&pk, MBEDTLS_PK_RSA_ALT)) {
        ESP_LOGE(TAG, "Key is not RSA type");
        goto cleanup;
    }

    if (mbedtls_pk_get_bitlen(&pk) < 2048) {
        ESP_LOGE(TAG, "RSA key too short (<2048 bits)");
        goto cleanup;
    }

    /* Step 9: Get SHA256 message digest info */
    const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (!md_info) {
        ESP_LOGE(TAG, "Failed to get MD info for SHA256");
        goto cleanup;
    }

    /* Step 10: Create hash of header.payload */
    r = mbedtls_md(md_info, (const unsigned char *)headerAndPayload,
                   headerAndPayload_len, hash);
    if (r != 0) {
        ESP_LOGE(TAG, "mbedtls_md failed: %d (-0x%x): %s",
                 r, -r, mbedtlsError(r));
        goto cleanup;
    }

    /* Step 11: Sign the hash */
    r = mbedtls_pk_sign(&pk, MBEDTLS_MD_SHA256, hash, 0, signature_bytes,
                        JWT_SIGNATURE_MAX_LEN, &signature_bytes_length,
                        mbedtls_ctr_drbg_random, &ctr_drbg);
    if (r != 0 || signature_bytes_length == 0) {
        ESP_LOGE(TAG, "mbedtls_pk_sign failed: %d (-0x%x): %s",
                 r, -r, mbedtlsError(r));
        goto cleanup;
    }

    /* Step 12: Encode signature to Base64 URL */
    base64Signature = (char *)calloc(1, JWT_SIGNATURE_MAX_LEN);
    if (!base64Signature) {
        ESP_LOGE(TAG, "Failed to allocate memory for base64Signature");
        goto cleanup;
    }
    base64url_encode(signature_bytes, signature_bytes_length, base64Signature);

    /* Step 13: Combine all parts to create final JWT */
    size_t jwtLen = strlen(headerAndPayload) + 1 + strlen(base64Signature) + 1;
    jwt = (char *)calloc(1, jwtLen);
    if (!jwt) {
        ESP_LOGE(TAG, "Failed to allocate memory for JWT");
        goto cleanup;
    }
    snprintf(jwt, jwtLen, "%s.%s", headerAndPayload, base64Signature);

    /* Success - clean up and return JWT */
    COZE_UTILS_FREE(base64Signature, free);
    COZE_UTILS_FREE(headerAndPayload, free);
    COZE_UTILS_FREE(signature_bytes, free);
    COZE_UTILS_FREE(key, free);
    mbedtls_pk_free(&pk);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    return jwt;

cleanup:
    /* Clean up on error */
    COZE_UTILS_FREE(base64Signature, free);
    COZE_UTILS_FREE(headerAndPayload, free);
    COZE_UTILS_FREE(signature_bytes, free);
    COZE_UTILS_FREE(key, free);
    mbedtls_pk_free(&pk);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    return NULL;
}
