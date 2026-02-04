/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Structure to hold HTTP response data
 */
typedef struct {
    char *body;
    int   body_len;
} http_response_t;

/**
 * @brief  Structure to represent an HTTP request header key-value pair
 */
typedef struct {
    const char *key;
    const char *value;
} http_req_header_t;

/**
 * @brief Create a JWT (JSON Web Token) for Coze authentication
 *
 * @param[in]  kid             Key ID string
 * @param[in]  payload         JWT payload string
 * @param[in]  privateKey      Private key string
 * @param[in]  privateKeySize  Size of the private key string
 *
 * @return
 *       - Pointer to the generated JWT string (must be freed by the caller)
 *       - NULL  On failure
 */
char *coze_jwt_create_handler(const char *kid, const char *payload, const uint8_t *privateKey, size_t privateKeySize);

/**
 * @brief  Send an HTTP POST request
 *
 * @param[in]   url       The URL to which the POST request is sent
 * @param[in]   header    HTTP request headers (terminated by a {NULL, NULL} entry)
 * @param[in]   body      POST request body
 * @param[out]  response  http_response_t to receive the response data
 *
 * @return
 *       - ESP_OK    On success
 *       - ESP_FAIL  On failure
 */
esp_err_t http_client_post(const char *url, http_req_header_t *header, char *body, http_response_t *response);

#ifdef __cplusplus
}
#endif
