/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>

#include "esp_event.h"
#include "esp_gsp.h"
#include "esp_wifi.h"
#include "wifi_probe_hooks.hpp"

namespace esp_brookesia::gsp_app::wifi_probe_hooks {

namespace {

constexpr size_t FAULT_COUNT = 5;
constexpr size_t STATUS_COUNT = 4;
constexpr size_t UNICODE_AP_COUNT = 3;
constexpr uint32_t WIFI_STATUS_KEY = 0x03BA13BFU; // FNV-1a key of the generated "wifi_status" binding.
constexpr std::array<uint32_t, UNICODE_AP_COUNT> SSID_KEYS = {
    0x3D75BD29U, 0x3C75BB96U, 0x3B75BA03U,
};
constexpr std::array<uint32_t, 5> ROW_SSID_KEYS = {
    0x3D75BD29U, 0x3C75BB96U, 0x3B75BA03U, 0x3A75B870U, 0x4175C375U,
};
constexpr uint32_t CONNECTION_STATUS_KEY = 0x435B5D3CU; // FNV-1a key of "connection_status".
constexpr std::array<const char *, UNICODE_AP_COUNT> UNICODE_SSIDS = {
    "中文热点_嗨嗯", "繁體網路測試", "Café_Δίκτυο_Сеть",
};

std::atomic<Fault> armed_fault{Fault::None};
std::array<std::atomic<unsigned>, FAULT_COUNT> injections{};
std::array<std::atomic<unsigned>, STATUS_COUNT> statuses{};
std::array<std::atomic<unsigned>, UNICODE_AP_COUNT> unicode_rows{};
std::atomic<bool> cleanup_ok{true};
std::atomic<bool> unicode_records_pending{false};
std::mutex target_mutex;
std::string target_ssid;
std::atomic<int> target_row{-1};
std::atomic<unsigned> connected_targets{0};
unsigned wifi_status_updates = 0;
unsigned connection_status_updates = 0;
unsigned target_connecting_updates = 0;
std::string latest_wifi_status;
std::string latest_connection_status;

bool consume(Fault fault)
{
    auto expected = fault;
    if (!armed_fault.compare_exchange_strong(expected, Fault::None)) {
        return false;
    }
    injections[static_cast<size_t>(fault)].fetch_add(1);
    return true;
}

void fill_unicode_aps(uint16_t *number, wifi_ap_record_t *ap_records)
{
    const size_t count = std::min<size_t>(*number, UNICODE_AP_COUNT);
    for (size_t index = 0; index < count; ++index) {
        auto &ap = ap_records[index];
        ap = {};
        std::memcpy(ap.ssid, UNICODE_SSIDS[index], std::strlen(UNICODE_SSIDS[index]));
        ap.bssid[0] = 0x02;
        ap.bssid[5] = static_cast<uint8_t>(index + 1);
        ap.primary = static_cast<uint8_t>(1 + index * 5);
        ap.rssi = static_cast<int8_t>(-40 - index * 10);
        ap.authmode = index == 0 ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;
    }
    *number = static_cast<uint16_t>(count);
}

} // namespace

bool clear_driver_scan_list()
{
    if (esp_wifi_clear_ap_list() != ESP_OK) {
        cleanup_ok.store(false);
        return false;
    }
    return true;
}

bool arm(Fault fault)
{
    if (fault == Fault::None) {
        return false;
    }
    auto expected = Fault::None;
    return armed_fault.compare_exchange_strong(expected, fault);
}

void disarm()
{
    armed_fault.store(Fault::None);
    unicode_records_pending.store(false);
}

unsigned injection_count(Fault fault)
{
    return injections[static_cast<size_t>(fault)].load();
}

unsigned status_count(Status status)
{
    return statuses[static_cast<size_t>(status)].load();
}

unsigned unicode_row_count(size_t row)
{
    return row < unicode_rows.size() ? unicode_rows[row].load() : 0;
}

bool cleanup_succeeded()
{
    return cleanup_ok.load();
}

void track_target_ssid(std::string_view ssid)
{
    std::lock_guard lock(target_mutex);
    target_ssid.assign(ssid);
    target_row.store(-1);
    connected_targets.store(0);
    wifi_status_updates = 0;
    connection_status_updates = 0;
    target_connecting_updates = 0;
    latest_wifi_status.clear();
    latest_connection_status.clear();
}

void clear_target_ssid()
{
    track_target_ssid({});
}

void reset_target_row()
{
    target_row.store(-1);
}

int matching_target_row()
{
    return target_row.load();
}

unsigned connected_target_count()
{
    return connected_targets.load();
}

WifiDisplaySnapshot wifi_display_snapshot()
{
    std::lock_guard lock(target_mutex);
    return {
        .status_updates = wifi_status_updates,
        .connection_updates = connection_status_updates,
        .target_connecting_updates = target_connecting_updates,
        .wifi_off = latest_wifi_status == "Wi-Fi is off. Turn it on to discover networks.",
        .status_connecting = latest_wifi_status == "Connecting to network...",
        .connection_connecting = std::string_view(latest_connection_status).starts_with("Connecting: "),
    };
}

} // namespace esp_brookesia::gsp_app::wifi_probe_hooks

extern "C" esp_err_t __real_esp_wifi_scan_get_ap_num(uint16_t *number);
extern "C" esp_err_t __real_esp_wifi_scan_get_ap_records(uint16_t *number, wifi_ap_record_t *ap_records);
extern "C" esp_err_t __real_esp_event_post(esp_event_base_t event_base, int32_t event_id,
        const void *event_data, size_t event_data_size, TickType_t ticks_to_wait);
extern "C" esp_gsp_err_t __real_esp_gsp_component_set_text(esp_gsp_handle_t gsp,
        gsp_component_key_t key, const char *value);

extern "C" esp_err_t __wrap_esp_wifi_scan_get_ap_num(uint16_t *number)
{
    using namespace esp_brookesia::gsp_app::wifi_probe_hooks;
    const auto result = __real_esp_wifi_scan_get_ap_num(number);
    if (result != ESP_OK) {
        return result;
    }
    if (consume(Fault::Empty)) {
        clear_driver_scan_list();
        *number = 0;
    } else if (consume(Fault::CountFailure)) {
        clear_driver_scan_list();
        return ESP_FAIL;
    } else if (consume(Fault::UnicodeAps)) {
        clear_driver_scan_list();
        unicode_records_pending.store(true);
        *number = UNICODE_AP_COUNT;
    }
    return ESP_OK;
}

extern "C" esp_err_t __wrap_esp_wifi_scan_get_ap_records(uint16_t *number, wifi_ap_record_t *ap_records)
{
    using namespace esp_brookesia::gsp_app::wifi_probe_hooks;
    if (unicode_records_pending.exchange(false)) {
        fill_unicode_aps(number, ap_records);
        return ESP_OK;
    }
    return __real_esp_wifi_scan_get_ap_records(number, ap_records);
}

extern "C" esp_err_t __wrap_esp_event_post(esp_event_base_t event_base, int32_t event_id,
        const void *event_data, size_t event_data_size, TickType_t ticks_to_wait)
{
    using namespace esp_brookesia::gsp_app::wifi_probe_hooks;
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE) {
        wifi_event_sta_scan_done_t scan_done{};
        const bool has_payload = event_data && event_data_size >= sizeof(scan_done);
        if (has_payload) {
            scan_done = *static_cast<const wifi_event_sta_scan_done_t *>(event_data);
        }
        if ((!has_payload || scan_done.status == 0) && consume(Fault::ScanFailure)) {
            scan_done.status = 1;
            return __real_esp_event_post(event_base, event_id, &scan_done, sizeof(scan_done), ticks_to_wait);
        }
    }
    return __real_esp_event_post(event_base, event_id, event_data, event_data_size, ticks_to_wait);
}

extern "C" esp_gsp_err_t __wrap_esp_gsp_component_set_text(esp_gsp_handle_t gsp,
        gsp_component_key_t key, const char *value)
{
    using namespace esp_brookesia::gsp_app::wifi_probe_hooks;
    const auto result = __real_esp_gsp_component_set_text(gsp, key, value);
    if (result != ESP_GSP_OK || !value) {
        return result;
    }
    if (key == WIFI_STATUS_KEY) {
        constexpr std::array<const char *, STATUS_COUNT> STATUS_TEXTS = {
            "Scanning for networks...", "No networks found. Tap Refresh to try again.",
            "Scan failed. Tap Refresh to retry.", "Nearby networks",
        };
        for (size_t index = 0; index < STATUS_TEXTS.size(); ++index) {
            if (std::strcmp(value, STATUS_TEXTS[index]) == 0) {
                statuses[index].fetch_add(1);
                break;
            }
        }
    }
    for (size_t index = 0; index < SSID_KEYS.size(); ++index) {
        if (key == SSID_KEYS[index] && std::strcmp(value, UNICODE_SSIDS[index]) == 0) {
            unicode_rows[index].fetch_add(1);
        }
    }
    std::lock_guard lock(target_mutex);
    if (key == WIFI_STATUS_KEY) {
        latest_wifi_status = value;
        ++wifi_status_updates;
    } else if (key == CONNECTION_STATUS_KEY) {
        latest_connection_status = value;
        ++connection_status_updates;
        if (!target_ssid.empty() && std::string_view(value).starts_with("Connecting: ") &&
                std::string_view(value).substr(sizeof("Connecting: ") - 1) == target_ssid) {
            ++target_connecting_updates;
        }
    }
    if (target_ssid.empty()) {
        return result;
    }
    for (size_t index = 0; index < ROW_SSID_KEYS.size(); ++index) {
        if (key == ROW_SSID_KEYS[index]) {
            if (target_ssid == value) {
                target_row.store(static_cast<int>(index));
            } else if (target_row.load() == static_cast<int>(index)) {
                target_row.store(-1);
            }
            break;
        }
    }
    if (key == CONNECTION_STATUS_KEY &&
            std::string_view(value).starts_with("Connected: ") &&
            std::string_view(value).substr(sizeof("Connected: ") - 1) == target_ssid) {
        connected_targets.fetch_add(1);
    }
    return result;
}
