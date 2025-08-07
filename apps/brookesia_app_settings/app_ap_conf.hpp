/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

/*
 * Wi-Fi provisioning helper for ESP-Brookesia.
 *
 * This module turns the ESP into AP+STA mode (SoftAP name: ESP-Brookesia-XXXX)
 * while exposing a small HTTP + DNS captive-portal server that hosts a single
 * page (wifi.html) allowing the user to choose a network.
 *
 * Public API:
 *  - esp_err_t start(CredentialsCallback cb = nullptr);
 *      Enable provisioning mode. `cb` will be stored and called on success.
 *  - esp_err_t stop();
 *      Disable provisioning mode and restore the previous Wi-Fi mode.
 *  - const char *get_ap_ssid();
 *      Get the SoftAP SSID created by start().
 *  - void register_callback(CredentialsCallback cb);
 *      Register / replace the callback.
 *
 * All methods are thread-safe and can be called from any context.
 */

#include <functional>
#include <string>
#include <vector>
#include <mutex>
#include "freertos/FreeRTOS.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_http_server.h"

namespace esp_brookesia::apps {

class ApProvision {
public:
    using CredentialsCallback = std::function<void(const std::string &ssid, const std::string &password)>;

    using StateChangeCallback = std::function<void(bool running)>;

    // Enter APSTA mode and launch captive portal.
    static esp_err_t start(
        const CredentialsCallback &cb = nullptr,
        const StateChangeCallback &sc_cb = nullptr,
        const std::vector<wifi_ap_record_t> &initial_aps = {}
    );

    // Stop captive portal and restore previous Wi-Fi mode. Safe to call even
    // if not running.
    static esp_err_t stop();

    // Returns SoftAP SSID or nullptr when not running.
    static const char *get_ap_ssid();

    // Register / replace the callback independently of start().
    static void register_callback(const CredentialsCallback &cb);

    // Update the list of scanned APs that will be exposed by the captive portal.
    // Can be called at any time by external Wi-Fi scan logic.
    static void update_ap_list(const std::vector<wifi_ap_record_t> &aps);

    /* HTTP handlers */
    static esp_err_t handle_scan(httpd_req_t *req);
    static esp_err_t handle_connect(httpd_req_t *req);
    static esp_err_t handle_root(httpd_req_t *req);
    static esp_err_t handle_status(httpd_req_t *req);
    static inline int _dns_sock = -1;

private:
    /* Internal state */
    static inline bool                 _running            = false;
    /* Connection status */
    enum class ConnectStatus { IDLE, CONNECTING, SUCCESS, FAILED };
    static inline ConnectStatus     _connect_status     = ConnectStatus::IDLE;
    static inline std::string       _connect_error_msg  = "";

    static inline wifi_mode_t          _previous_mode      = WIFI_MODE_NULL;
    static inline CredentialsCallback  _cb                 = nullptr;
    static inline StateChangeCallback  _sc_cb              = nullptr;
    static inline std::string          _target_ssid        = "";
    static inline std::string          _target_password    = "";
    static inline httpd_handle_t       _httpd              = nullptr;
    static inline TaskHandle_t         _dns_task_handle    = nullptr;
    static inline char                 _ap_ssid[33]        = {0};
    static inline esp_netif_t         *_ap_netif           = nullptr;
    static inline std::vector<wifi_ap_record_t> _initial_aps;
    static inline std::mutex           _initial_aps_mutex;


    static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data);
    static void ip_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data);

    static esp_err_t init_softap();
    static esp_err_t init_http_server();
    static void       deinit_http_server();
    static esp_err_t init_dns_server();
    static void       deinit_dns_server();
};

} // namespace esp_brookesia::apps
