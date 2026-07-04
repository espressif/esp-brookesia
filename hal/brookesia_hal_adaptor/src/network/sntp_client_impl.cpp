/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <cstdlib>
#include <ctime>
#include <sys/time.h>
#include "esp_err.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_sntp.h"
#include "freertos/FreeRTOS.h"
#include "lwip/inet.h"
#include "lwip/ip_addr.h"
#include "brookesia/hal_adaptor/macro_configs.h"
#if !BROOKESIA_HAL_ADAPTOR_NETWORK_SNTP_CLIENT_IMPL_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "sntp_client_impl.hpp"

namespace esp_brookesia::hal {

namespace {

SntpClientAdaptorIface *s_sync_instance = nullptr;

constexpr const char *DEFAULT_STATIC_SERVER = "pool.ntp.org";
constexpr const char *DEFAULT_TIMEZONE = "CST-8";

} // namespace

SntpClientAdaptorIface::~SntpClientAdaptorIface()
{
    deinit();
}

bool SntpClientAdaptorIface::init(const Config &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_initialized_) {
        BROOKESIA_LOGD("SNTP already initialized, skip");
        return true;
    }

    if (!set_servers(config.servers)) {
        return false;
    }
    if (!set_timezone(config.timezone.empty() ? DEFAULT_TIMEZONE : config.timezone)) {
        return false;
    }
    use_dhcp_ = config.use_dhcp;

    esp_sntp_config_t sntp_config = {};
#if LWIP_DHCP_GET_NTP_SRV
    if (use_dhcp_) {
        BROOKESIA_LOGI("Initializing SNTP with DHCP NTP server support");
        esp_sntp_config_t dhcp_config = ESP_NETIF_SNTP_DEFAULT_CONFIG(servers_.front().c_str());
        dhcp_config.start = false;
        dhcp_config.server_from_dhcp = true;
        dhcp_config.renew_servers_after_new_IP = true;
        dhcp_config.index_of_first_server = 1;
        dhcp_config.ip_event_to_renew = IP_EVENT_STA_GOT_IP;
        dhcp_config.sync_cb = time_sync_notification_callback;
        sntp_config = dhcp_config;
    } else {
#endif
        BROOKESIA_LOGI("Initializing SNTP with static servers");
        esp_sntp_config_t static_config = ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(servers_.size(), {});
        for (std::size_t i = 0; (i < servers_.size()) && (i < SNTP_MAX_SERVERS); ++i) {
            static_config.servers[i] = servers_[i].c_str();
        }
        static_config.start = false;
        static_config.sync_cb = time_sync_notification_callback;
        sntp_config = static_config;
#if LWIP_DHCP_GET_NTP_SRV
    }
#endif

    s_sync_instance = this;
    auto ret = esp_netif_sntp_init(&sntp_config);
    if (ret != ESP_OK) {
        last_error_ = esp_err_to_name(ret);
        s_sync_instance = nullptr;
        BROOKESIA_LOGE("Failed to initialize SNTP: %1%", last_error_);
        return false;
    }

    is_time_synced_ = false;
    is_initialized_ = true;
    last_error_.clear();
    print_servers();

    return true;
}

void SntpClientAdaptorIface::deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_initialized_) {
        return;
    }

    esp_netif_sntp_deinit();
    if (s_sync_instance == this) {
        s_sync_instance = nullptr;
    }
    is_initialized_ = false;
    is_running_ = false;
    is_time_synced_ = false;
}

bool SntpClientAdaptorIface::start()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_initialized_) {
        last_error_ = "SNTP is not initialized";
        return false;
    }
    if (is_running_) {
        return true;
    }

    auto ret = esp_netif_sntp_start();
    if (ret != ESP_OK) {
        last_error_ = esp_err_to_name(ret);
        BROOKESIA_LOGE("Failed to start SNTP: %1%", last_error_);
        return false;
    }

    is_time_synced_ = false;
    is_running_ = true;
    last_error_.clear();
    print_servers();

    return true;
}

void SntpClientAdaptorIface::stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_running_) {
        return;
    }

    esp_netif_sntp_deinit();
    if (s_sync_instance == this) {
        s_sync_instance = nullptr;
    }
    is_initialized_ = false;
    is_running_ = false;
    is_time_synced_ = false;
}

bool SntpClientAdaptorIface::is_initialized() const
{
    return is_initialized_;
}

bool SntpClientAdaptorIface::is_running() const
{
    return is_running_;
}

bool SntpClientAdaptorIface::wait_time_sync(uint32_t timeout_ms)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_running_) {
        last_error_ = "SNTP is not running";
        return false;
    }

    const TickType_t timeout_ticks = pdMS_TO_TICKS(timeout_ms);
    auto ret = esp_netif_sntp_sync_wait(timeout_ticks);
    if (ret == ESP_OK) {
        is_time_synced_ = true;
        last_error_.clear();
        update_local_time();
        return true;
    }

    last_error_ = esp_err_to_name(ret);
    return false;
}

bool SntpClientAdaptorIface::is_time_synced() const
{
    return is_time_synced_.load();
}

bool SntpClientAdaptorIface::set_servers(const std::vector<std::string> &servers)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (servers.size() > SNTP_MAX_SERVERS) {
        last_error_ = "Too many SNTP servers";
        return false;
    }

    servers_ = servers;
    if (servers_.empty()) {
        servers_.push_back(DEFAULT_STATIC_SERVER);
    }
    last_error_.clear();

    return true;
}

std::vector<std::string> SntpClientAdaptorIface::get_servers() const
{
    return servers_;
}

std::size_t SntpClientAdaptorIface::get_max_servers() const
{
    return SNTP_MAX_SERVERS;
}

bool SntpClientAdaptorIface::set_timezone(const std::string &timezone)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (timezone.empty()) {
        last_error_ = "Timezone is empty";
        return false;
    }

    timezone_ = timezone;
    setenv("TZ", timezone_.c_str(), 1);
    tzset();
    update_local_time();
    last_error_.clear();

    return true;
}

std::string SntpClientAdaptorIface::get_timezone() const
{
    return timezone_;
}

void SntpClientAdaptorIface::time_sync_notification_callback(struct timeval *tv)
{
    (void)tv;

    if (s_sync_instance != nullptr) {
        s_sync_instance->is_time_synced_ = true;
    }
}

void SntpClientAdaptorIface::print_servers()
{
    BROOKESIA_LOGI("List of configured NTP servers:");

    for (uint8_t i = 0; i < SNTP_MAX_SERVERS; ++i) {
        if (esp_sntp_getservername(i) != nullptr) {
            BROOKESIA_LOGI("server %d: %s", i, esp_sntp_getservername(i));
        } else {
            char buff[INET6_ADDRSTRLEN] = {};
            ip_addr_t const *ip = esp_sntp_getserver(i);
            if ((ip != nullptr) && (ipaddr_ntoa_r(ip, buff, INET6_ADDRSTRLEN) != nullptr)) {
                BROOKESIA_LOGI("server %d: %s", i, buff);
            }
        }
    }
}

void SntpClientAdaptorIface::update_local_time()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    time_t now = 0;
    struct tm timeinfo = {};
    char strftime_buf[64] = {};
    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    BROOKESIA_LOGI("The current date/time in %s is: %s", timezone_.c_str(), strftime_buf);
}

} // namespace esp_brookesia::hal
