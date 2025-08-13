/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_sleep.h"
#include "nvs_flash.h"
#include "esp_netif_sntp.h"
#include "lwip/ip_addr.h"
#include "esp_sntp.h"

#define TIMEZONE         "CST-8"
#define SNTP_SERVER_NAME "pool.ntp.org"
#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 48
#endif

#define RETRY_INTERVAL_MS 1000
#define RETRY_COUNT_MAX   60

static const char *TAG = "sntp";
static bool is_time_synced = false;

static bool obtain_time(void);

void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Notification of a time synchronization event");
}

bool app_sntp_init(void)
{
    if (is_time_synced) {
        return true;
    }

#if LWIP_DHCP_GET_NTP_SRV
    /**
     * NTP server address could be acquired via DHCP,
     * see following menuconfig options:
     * 'LWIP_DHCP_GET_NTP_SRV' - enable STNP over DHCP
     * 'LWIP_SNTP_DEBUG' - enable debugging messages
     *
     * NOTE: This call should be made BEFORE esp acquires IP address from DHCP,
     * otherwise NTP option would be rejected by default.
     */
    ESP_LOGI(TAG, "Initializing SNTP");
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(SNTP_SERVER_NAME);
    config.start = false;                       // start SNTP service explicitly (after connecting)
    config.server_from_dhcp = true;             // accept NTP offers from DHCP server, if any (need to enable *before* connecting)
    config.renew_servers_after_new_IP = true;   // let esp-netif update configured SNTP server(s) after receiving DHCP lease
    config.index_of_first_server = 1;           // updates from server num 1, leaving server 0 (from DHCP) intact
    // configure the event on which we renew servers
    config.ip_event_to_renew = IP_EVENT_STA_GOT_IP;
    config.sync_cb = time_sync_notification_cb; // only if we need the notification function
    esp_err_t ret = esp_netif_sntp_init(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SNTP(%s)", esp_err_to_name(ret));
        return false;
    }
#endif /* LWIP_DHCP_GET_NTP_SRV */

    return true;
}

bool app_sntp_start(void)
{
    if (is_time_synced) {
        return true;
    }

    if (!obtain_time()) {
        ESP_LOGE(TAG, "Failed to obtain time");
        return false;
    }

    time_t now;
    struct tm timeinfo;
    char strftime_buf[64] = {};
    setenv("TZ", TIMEZONE, 1);
    tzset();
    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "The current date/time in %s is: %s", TIMEZONE, strftime_buf);

    is_time_synced = true;

    return true;
}

static void print_servers(void)
{
    ESP_LOGI(TAG, "List of configured NTP servers:");

    for (uint8_t i = 0; i < SNTP_MAX_SERVERS; ++i) {
        if (esp_sntp_getservername(i)) {
            ESP_LOGI(TAG, "server %d: %s", i, esp_sntp_getservername(i));
        } else {
            // we have either IPv4 or IPv6 address, let's print it
            char buff[INET6_ADDRSTRLEN];
            ip_addr_t const *ip = esp_sntp_getserver(i);
            if (ipaddr_ntoa_r(ip, buff, INET6_ADDRSTRLEN) != NULL) {
                ESP_LOGI(TAG, "server %d: %s", i, buff);
            }
        }
    }
}

static bool obtain_time(void)
{
    esp_err_t ret = ESP_OK;

#if LWIP_DHCP_GET_NTP_SRV
    ESP_LOGI(TAG, "Starting SNTP");
    ret = esp_netif_sntp_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start SNTP(%s)", esp_err_to_name(ret));
        return false;
    }

#if LWIP_IPV6 && SNTP_MAX_SERVERS > 2
    /* This demonstrates using IPv6 address as an additional SNTP server
     * (statically assigned IPv6 address is also possible)
     */
    ip_addr_t ip6;
    if (ipaddr_aton("2a01:3f7::1", &ip6)) {    // ipv6 ntp source "ntp.netnod.se"
        esp_sntp_setserver(2, &ip6);
    }
#endif  /* LWIP_IPV6 */

#else
    ESP_LOGI(TAG, "Initializing and starting SNTP");
#if CONFIG_LWIP_SNTP_MAX_SERVERS > 1
    /* This demonstrates configuring more than one server
     */
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(2,
                               ESP_SNTP_SERVER_LIST(SNTP_SERVER_NAME, "pool.ntp.org" ) );
#else
    /*
     * This is the basic default config with one server and starting the service
     */
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(SNTP_SERVER_NAME);
#endif
    config.sync_cb = time_sync_notification_cb;     // Note: This is only needed if we want

    ret = esp_netif_sntp_init(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SNTP(%s)", esp_err_to_name(ret));
        return false;
    }
#endif

    print_servers();

    // wait for time to be set
    int retry = 0;
    while ((esp_netif_sntp_sync_wait(2000 / portTICK_PERIOD_MS) == ESP_ERR_TIMEOUT) && (++retry < RETRY_COUNT_MAX)) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, RETRY_COUNT_MAX);
    }

    return (retry < RETRY_COUNT_MAX);
}

bool app_sntp_is_time_synced(void)
{
    return is_time_synced;
}
