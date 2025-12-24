/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Do post https request
 *
 * @note  This API will internally call `https_send_request`
 *
 * @param[in]  url        HTTPS URL to post
 * @param[in]  headers    HTTP headers, headers are array of "Type: Info", last one need set to NULL
 * @param[in]  data       Content data to be sent
 * @param[in]  header_cb  Header callback
 * @param[in]  body       Body callback
 * @param[in]  ctx        User context
 *
 * @return
 *       - 0       On success
 *       - Others  Fail to do https request
 */
int https_post(const char *url, const char *api_key, char *data, char *body);

#ifdef __cplusplus
}
#endif
