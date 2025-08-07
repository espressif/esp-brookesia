/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef ESP_BROOKESIA_APP_SPEAKER_SETTINGS_SIMULATOR

#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_sleep.h"
#include "esp_sntp.h"

// #define TIMEZONE        "CET-1CEST,M3.5.0,M10.5.0/3"
#define TIMEZONE        "CST-8"
#define SERVER_NAME_0   "asia.pool.ntp.org"
#define SERVER_NAME_1   "ntp.aliyun.com"
#define SERVER_NAME_2   "ntp.tencent.com"
#define SERVER_NAME_3   "cn.pool.ntp.org"

#define MAX_FAIL            3
#define MAX_RETRY           20
#define RETRY_INTERVAL_MS   1000

static const char *TAG = "sntp";

static void obtain_time(void);
static void initialize_sntp(void);

#ifdef CONFIG_SNTP_TIME_SYNC_METHOD_CUSTOM
void sntp_sync_time(struct timeval *tv)
{
    settimeofday(tv, NULL);
    ESP_LOGI(TAG, "Time is synchronized from custom code");
    sntp_set_sync_status(SNTP_SYNC_STATUS_COMPLETED);
}
#endif

static void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Time sync callback triggered");

    time_t now = 0;
    struct tm timeinfo = {0};
    time(&now);
    localtime_r(&now, &timeinfo);
    ESP_LOGI(TAG, "time: hour()=%d, minute()=%d, second()=%d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
}

void app_sntp_init(void)
{
    static bool sntp_initialized = false;
    time_t now;
    struct tm timeinfo;

    if (sntp_initialized) {
        return;
    }

    time(&now);
    localtime_r(&now, &timeinfo);

    // Set timezone to China Standard Time
    setenv("TZ", TIMEZONE, 1);
    tzset();
    // Is time set? If not, tm_year will be (1970 - 1900).
    if (timeinfo.tm_year < (2016 - 1900)) {
        ESP_LOGI(TAG, "Time is not set yet. Connecting to WiFi and getting time over NTP.");
        obtain_time();
        // update 'now' variable with current time
        time(&now);
    }
#ifdef CONFIG_SNTP_TIME_SYNC_METHOD_SMOOTH
    else {
        // add 500 ms error to the current system time.
        // Only to demonstrate a work of adjusting method!
        {
            ESP_LOGI(TAG, "Add a error for test adjtime");
            struct timeval tv_now;
            gettimeofday(&tv_now, NULL);
            int64_t cpu_time = (int64_t)tv_now.tv_sec * 1000000L + (int64_t)tv_now.tv_usec;
            int64_t error_time = cpu_time + 500 * 1000L;
            struct timeval tv_error = {.tv_sec = error_time / 1000000L, .tv_usec = error_time % 1000000L};
            settimeofday(&tv_error, NULL);
        }

        ESP_LOGI(TAG, "Time was set, now just adjusting it. Use SMOOTH SYNC method.");
        obtain_time();
        // update 'now' variable with current time
        time(&now);
    }
#endif

    char strftime_buf[64];
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "The current date/time in Shanghai is: %s", strftime_buf);

    if (sntp_get_sync_mode() == SNTP_SYNC_MODE_SMOOTH) {
        struct timeval outdelta;
        while (sntp_get_sync_status() == SNTP_SYNC_STATUS_IN_PROGRESS) {
            adjtime(NULL, &outdelta);
            ESP_LOGI(TAG, "Waiting for adjusting time ... outdelta = %li sec: %li ms: %li us",
                     (long)outdelta.tv_sec,
                     outdelta.tv_usec / 1000,
                     outdelta.tv_usec % 1000);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }

    sntp_initialized = true;
}

static void obtain_time(void)
{
    initialize_sntp();

    int retry = 0;
    int fail = 0;

    time_t now = 0;
    struct tm timeinfo = { 0 };

    while (true) {
        while (retry < MAX_RETRY) {
            time(&now);
            localtime_r(&now, &timeinfo);

            if (timeinfo.tm_year > (2020 - 1900)) {
                ESP_LOGI(TAG, "Time synchronized successfully");
                break;
            }

            ESP_LOGI(TAG, "Time not synced yet... retry %d/%d", retry + 1, MAX_RETRY);
            ++retry;
            vTaskDelay(pdMS_TO_TICKS(RETRY_INTERVAL_MS));
        }

        if (retry == MAX_RETRY) {
            ESP_LOGW(TAG, "SNTP sync failed after %d retries, restart SNTP", MAX_RETRY);
            sntp_restart();
            retry = 0;
            fail++;
        } else {
            break;
        }

        if (fail == MAX_FAIL) {
            ESP_LOGE(TAG, "SNTP sync failed after %d retries, restart device", MAX_FAIL);
            esp_restart();
        }
    }
}

static void initialize_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, SERVER_NAME_0);
    esp_sntp_setservername(1, SERVER_NAME_1);
    esp_sntp_setservername(2, SERVER_NAME_2);
    esp_sntp_setservername(3, SERVER_NAME_3);
    esp_sntp_set_time_sync_notification_cb(time_sync_notification_cb);
#ifdef CONFIG_SNTP_TIME_SYNC_METHOD_SMOOTH
    esp_sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH);
#endif
    esp_sntp_init();
}

#endif
