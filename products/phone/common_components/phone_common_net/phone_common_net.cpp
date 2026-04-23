/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "phone_common_net.hpp"

#include "sdkconfig.h"

#if CONFIG_RAINMAKER_APP_SUPPORT

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"

#include <string.h>

#include <cstdlib>
#include <ctime>

static const char *TAG = "phone_net";

static EventGroupHandle_t s_wifi_event_group = nullptr;
static constexpr int WIFI_CONNECTED_BIT = BIT0;
static constexpr int WIFI_FAIL_BIT = BIT1;
static int s_wifi_retry_num = 0;

static void wifi_event_handler(void * /*arg*/, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        return;
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_wifi_retry_num < CONFIG_PHONE_AUTO_WIFI_MAX_RETRY) {
            esp_wifi_connect();
            s_wifi_retry_num++;
            ESP_LOGW(TAG, "Wi-Fi disconnected, retry %d/%d", s_wifi_retry_num, CONFIG_PHONE_AUTO_WIFI_MAX_RETRY);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            ESP_LOGE(TAG, "Wi-Fi connect failed (max retry reached)");
        }
        return;
    }

    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        auto *event = (ip_event_got_ip_t *)event_data;
        s_wifi_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        ESP_LOGI(TAG, "Wi-Fi got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

static void sntp_time_sync_notification_cb(struct timeval *tv)
{
    setenv("TZ", CONFIG_PHONE_TIMEZONE, 1);
    tzset();
    ESP_LOGI(TAG, "SNTP time synchronized (TZ=%s): %s", CONFIG_PHONE_TIMEZONE, ctime(&tv->tv_sec));
}

static void wifi_auto_connect_on_boot(void)
{
#if !CONFIG_PHONE_AUTO_WIFI_ENABLE
    return;
#else
    if (strlen(CONFIG_PHONE_AUTO_WIFI_SSID) == 0) {
        ESP_LOGW(TAG, "Auto Wi-Fi enabled but SSID is empty; skip");
        return;
    }

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    } else {
        ESP_ERROR_CHECK(ret);
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    s_wifi_event_group = xEventGroupCreate();
    if (s_wifi_event_group == nullptr) {
        ESP_LOGE(TAG, "Create Wi-Fi event group failed");
        return;
    }

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, nullptr));

    wifi_config_t wifi_config = {};
    strlcpy((char *)wifi_config.sta.ssid, CONFIG_PHONE_AUTO_WIFI_SSID, sizeof(wifi_config.sta.ssid));
    strlcpy((char *)wifi_config.sta.password, CONFIG_PHONE_AUTO_WIFI_PASSWORD, sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Auto Wi-Fi connecting to SSID: %s", CONFIG_PHONE_AUTO_WIFI_SSID);
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to Wi-Fi");
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Failed to connect to Wi-Fi");
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
    ESP_LOGI(TAG, "Auto Wi-Fi connected to SSID: %s", CONFIG_PHONE_AUTO_WIFI_SSID);

    if (bits & WIFI_CONNECTED_BIT) {
        esp_sntp_config_t sntp_config = ESP_NETIF_SNTP_DEFAULT_CONFIG(CONFIG_PHONE_SNTP_SERVER);
        sntp_config.sync_cb = sntp_time_sync_notification_cb;
        esp_err_t err = esp_netif_sntp_init(&sntp_config);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "SNTP started, server: %s", CONFIG_PHONE_SNTP_SERVER);
        } else {
            ESP_LOGE(TAG, "SNTP init failed: %s", esp_err_to_name(err));
        }
    }
#endif
}

#endif /* CONFIG_RAINMAKER_APP_SUPPORT */

extern "C" void phone_common_net_boot(void)
{
#if CONFIG_RAINMAKER_APP_SUPPORT
    setenv("TZ", CONFIG_PHONE_TIMEZONE, 1);
    tzset();
    wifi_auto_connect_on_boot();
#endif
}
