/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * Minimal PC-side HTTPS GET to RainMaker public endpoint (same base URL as firmware).
 * Use this to verify TLS + libcurl on the host while developing UI; device build uses
 * esp_http_client via app_rmaker_user_transport_esp.c instead.
 */

#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>

static size_t write_cb(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    size_t total = size * nmemb;
    (void)userdata;
    fwrite(ptr, 1, total, stdout);
    return total;
}

int main(void)
{
    CURL *curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "curl_easy_init failed\n");
        return 1;
    }

    curl_easy_setopt(curl, CURLOPT_URL, "https://api.rainmaker.espressif.com/apiversions");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    CURLcode res = curl_easy_perform(curl);
    long code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    curl_easy_cleanup(curl);

    printf("\nHTTP status: %ld (curl result %d)\n", code, (int)res);
    return (res == CURLE_OK && code == 200) ? 0 : 2;
}
