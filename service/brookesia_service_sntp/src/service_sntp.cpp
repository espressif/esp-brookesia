/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <time.h>
#include <sys/time.h>
#include "lwip/ip_addr.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_sntp.h"
#include "esp_netif_sntp.h"
#include "esp_netif.h"
#include "brookesia/service_sntp/macro_configs.h"
#if !BROOKESIA_SERVICE_SNTP_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/lib_utils/plugin.hpp"
#include "brookesia/lib_utils/function_guard.hpp"
#include "brookesia/service_helper/nvs.hpp"
#include "brookesia/service_sntp/service_sntp.hpp"

namespace esp_brookesia::service {

using NVSHelper = helper::NVS;

constexpr uint32_t NVS_SAVE_DATA_TIMEOUT_MS = 20;
constexpr uint32_t NVS_ERASE_DATA_TIMEOUT_MS = 20;

bool SNTP::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGI(
        "Version: %1%.%2%.%3%", BROOKESIA_SERVICE_SNTP_VER_MAJOR, BROOKESIA_SERVICE_SNTP_VER_MINOR,
        BROOKESIA_SERVICE_SNTP_VER_PATCH
    );

#if LWIP_DHCP_GET_NTP_SRV
    BROOKESIA_CHECK_FALSE_RETURN(do_sntp_init(), false, "Failed to initialize SNTP");
#endif

    return true;
}

void SNTP::on_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    do_sntp_deinit();

    // reset time synced state when deiniting
    is_time_synced_ = false;
}

bool SNTP::on_start()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    scheduler_ = get_task_scheduler();
    BROOKESIA_CHECK_NULL_RETURN(scheduler_, false, "Failed to get task scheduler");

    update_timezone();

    // Load data from NVS
    try_load_data();

    // Only start SNTP if time is not synced
    if (!is_time_synced()) {
        BROOKESIA_CHECK_FALSE_RETURN(sntp_start(), false, "Failed to start SNTP");
    }

    return true;
}

void SNTP::on_stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    sntp_stop();
    scheduler_.reset();
}

std::expected<void, std::string> SNTP::function_set_servers(const boost::json::array &servers)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    std::vector<std::string> servers_list;
    if (!BROOKESIA_DESCRIBE_FROM_JSON(servers, servers_list)) {
        return std::unexpected("Failed to parse servers list");
    }

    if (servers_list.size() > SNTP_MAX_SERVERS) {
        return std::unexpected(
                   (boost::format("The number of servers (%1%) is greater than the maximum number of servers "
                                  "(%2%). Please reduce the number of servers or increase 'CONFIG_LWIP_SNTP_MAX_SERVERS'.")
                    % servers_list.size() % SNTP_MAX_SERVERS).str());
    }

    set_data<DataType::Servers>(std::move(servers_list));

    try_save_data(DataType::Servers);

    return {};
}

std::expected<void, std::string> SNTP::function_set_timezone(const std::string &timezone)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (timezone.empty()) {
        return std::unexpected("Timezone is empty");
    }

    set_data<DataType::Timezone>(std::move(timezone));
    update_timezone();

    try_save_data(DataType::Timezone);

    return {};
}

std::expected<void, std::string> SNTP::function_start()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!sntp_start()) {
        return std::unexpected("Failed to start SNTP");
    }

    return {};
}

std::expected<void, std::string> SNTP::function_stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    sntp_stop();

    return {};
}

std::expected<boost::json::array, std::string> SNTP::function_get_servers()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    return BROOKESIA_DESCRIBE_TO_JSON(data_servers_).as_array();
}

std::expected<std::string, std::string> SNTP::function_get_timezone()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    return data_timezone_;
}

std::expected<bool, std::string> SNTP::function_is_time_synced()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    return is_time_synced_;
}

std::expected<void, std::string> SNTP::function_reset_data()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    reset_data();

    try_erase_data();

    return {};
}

bool SNTP::sntp_start()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if ((wait_for_network_task_ != 0) &&
            scheduler_->get_state(wait_for_network_task_) == lib_utils::TaskScheduler::TaskState::Running) {
        BROOKESIA_LOGD("Wait for network task already running, skip");
        return true;
    }

    // Reset time synced state when starting SNTP
    is_time_synced_ = false;

    if (is_sntp_running()) {
        BROOKESIA_LOGD("SNTP is running, stopping it first");
        sntp_stop();
    }

    auto wait_for_network_task = [this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        if (!is_network_connected()) {
            BROOKESIA_LOGD("Network is not connected, wait for network...");
            // Keep waiting for network connection...
            return true;
        }

        BROOKESIA_LOGI("Network is connected, starting SNTP...");
        BROOKESIA_CHECK_FALSE_RETURN(do_sntp_start(), false, "Failed to start SNTP");

        lib_utils::FunctionGuard reset_sntp_running_guard([this]() {
            BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
            is_sntp_running_ = false;
        });

        auto sync_start_time = boost::chrono::steady_clock::now();
        auto sync_time_task = [this, sync_start_time]() {
            BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

            auto is_timeout = boost::chrono::steady_clock::now() - sync_start_time >
                              boost::chrono::milliseconds(BROOKESIA_SERVICE_SNTP_SYNC_TIME_TIMEOUT_MS);
            if (esp_netif_sntp_sync_wait(0) == ESP_ERR_TIMEOUT && !is_timeout) {
                BROOKESIA_LOGD("Sync time timeout, retrying...");
                return true;
            }

            if (is_timeout) {
                BROOKESIA_LOGW(
                    "Sync time timeout, stop sync time task and retry to start SNTP after %1%ms",
                    BROOKESIA_SERVICE_SNTP_SYNC_TIME_RETRY_DELAY_MS
                );

                auto retry_task = [this]() {
                    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
                    BROOKESIA_CHECK_FALSE_EXIT(sntp_start(), "Failed to start SNTP");
                };
                auto result = scheduler_->post_delayed(
                                  std::move(retry_task), BROOKESIA_SERVICE_SNTP_SYNC_TIME_RETRY_DELAY_MS
                              );
                BROOKESIA_CHECK_FALSE_RETURN(result, false, "Failed to post retry task");

                return false;
            }

            BROOKESIA_LOGI("Sync time successful");

            update_local_time();
            is_time_synced_ = true;

            // Stop the sync time task
            return false;
        };
        auto result = scheduler_->post_periodic(
                          std::move(sync_time_task), BROOKESIA_SERVICE_SNTP_SYNC_TIME_INTERVAL_MS, &sync_time_task_
                      );
        BROOKESIA_CHECK_FALSE_RETURN(result, false, "Failed to post sync time task");

        reset_sntp_running_guard.release();

        // Stop the wait for network task
        return false;
    };
    auto result = scheduler_->post_periodic(
                      std::move(wait_for_network_task), BROOKESIA_SERVICE_SNTP_WAIT_FOR_NETWORK_INTERVAL_MS, &wait_for_network_task_
                  );
    BROOKESIA_CHECK_FALSE_RETURN(result, false, "Failed to post wait for network task");

    return true;
}

void SNTP::sntp_stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (wait_for_network_task_ != 0) {
        scheduler_->cancel(wait_for_network_task_);
        wait_for_network_task_ = 0;
    }
    if (sync_time_task_ != 0) {
        scheduler_->cancel(sync_time_task_);
        sync_time_task_ = 0;
    }

    do_sntp_deinit();
}

void SNTP::reset_data()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    data_servers_ = {DEFAULT_NTP_SERVER.data()};
    data_timezone_ = DEFAULT_TIMEZONE.data();
    is_time_synced_ = false;
}

bool SNTP::do_sntp_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_sntp_initialized()) {
        BROOKESIA_LOGD("SNTP already initialized, skip");
        return true;
    }

    esp_sntp_config_t sntp_config = {};
#if LWIP_DHCP_GET_NTP_SRV
    if (!is_network_connected()) {
        BROOKESIA_LOGI("Initializing SNTP via DHCP");
        /**
         * NTP server address could be acquired via DHCP,
         * see following menuconfig options:
         * 'LWIP_DHCP_GET_NTP_SRV' - enable STNP over DHCP
         * 'LWIP_SNTP_DEBUG' - enable debugging messages
         *
         * NOTE: This call should be made BEFORE esp acquires IP address from DHCP,
         * otherwise NTP option would be rejected by default.
         */
        esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(DEFAULT_NTP_SERVER.data());
        config.server_from_dhcp = true;             // accept NTP offers from DHCP server, if any (need to enable *before* connecting)
        config.renew_servers_after_new_IP = true;   // let esp-netif update configured SNTP server(s) after receiving DHCP lease
        config.index_of_first_server = 1;           // updates from server num 1, leaving server 0 (from DHCP) intact
        // configure the event on which we renew servers
        config.ip_event_to_renew = IP_EVENT_STA_GOT_IP;
        sntp_config = config;
    } else {
        BROOKESIA_LOGW(
            "Network is connected, NTP server address could not be acquired via DHCP, using static configuration"
        );
#endif /* LWIP_DHCP_GET_NTP_SRV */
        BROOKESIA_LOGI("Initializing SNTP via static configuration");
        esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(data_servers_.size(), nullptr);
        size_t size = data_servers_.size();
        for (size_t i = 0; (i < size) && (i < SNTP_MAX_SERVERS); i++) {
            config.servers[i] = data_servers_[i].c_str();
        }
        sntp_config = config;
#if LWIP_DHCP_GET_NTP_SRV
    }
#endif /* LWIP_DHCP_GET_NTP_SRV */

    sntp_config.start = false;                       // start SNTP service explicitly (after connecting)
    BROOKESIA_CHECK_ESP_ERR_RETURN(esp_netif_sntp_init(&sntp_config), false, "Failed to initialize SNTP");

    is_sntp_initialized_ = true;

    return true;
}

void SNTP::do_sntp_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_sntp_initialized()) {
        BROOKESIA_LOGD("SNTP not initialized, skip");
        return;
    }

    esp_netif_sntp_deinit();

    is_sntp_initialized_ = false;
    is_sntp_running_ = false;

    BROOKESIA_LOGI("SNTP deinitialized");
}

bool SNTP::do_sntp_start()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_sntp_running()) {
        BROOKESIA_LOGD("SNTP already started, skip");
        return true;
    }

    if (!is_sntp_initialized()) {
        BROOKESIA_LOGD("SNTP not initialized, initializing...");
        BROOKESIA_CHECK_FALSE_RETURN(do_sntp_init(), false, "Failed to initialize SNTP");
    }

    BROOKESIA_CHECK_ESP_ERR_RETURN(esp_netif_sntp_start(), false, "Failed to start SNTP");

    BROOKESIA_LOGI("SNTP started");

#if LWIP_DHCP_GET_NTP_SRV && defined(LWIP_IPV6) && SNTP_MAX_SERVERS > 2
    ip_addr_t ip6;
    if (ipaddr_aton("2a01:3f7::1", &ip6)) {    // ipv6 ntp source "ntp.netnod.se"
        esp_sntp_setserver(2, &ip6);
    }
#endif

    print_sntp_servers();

    is_sntp_running_ = true;

    return true;
}

bool SNTP::is_network_connected()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    // Get the default STA network interface
    esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (sta_netif == nullptr) {
        BROOKESIA_LOGD("STA netif not found");
        return false;
    }

    // Check if the network interface is up
    if (!esp_netif_is_netif_up(sta_netif)) {
        BROOKESIA_LOGD("STA netif is not up");
        return false;
    }

    // Check if we have a valid IP address
    esp_netif_ip_info_t ip_info;
    esp_err_t ret = esp_netif_get_ip_info(sta_netif, &ip_info);
    if (ret != ESP_OK) {
        BROOKESIA_LOGD("Failed to get IP info: %s", esp_err_to_name(ret));
        return false;
    }

    // Check if IP address is valid (not 0.0.0.0)
    if (ip_info.ip.addr == 0) {
        BROOKESIA_LOGD("IP address is 0.0.0.0");
        return false;
    }

    BROOKESIA_LOGD("Network is connected, IP: %d.%d.%d.%d", IP2STR(&ip_info.ip));

    return true;
}

void SNTP::print_sntp_servers()
{
    BROOKESIA_LOGI("List of configured NTP servers:");

    for (uint8_t i = 0; i < SNTP_MAX_SERVERS; ++i) {
        if (esp_sntp_getservername(i)) {
            BROOKESIA_LOGI("server %d: %s", i, esp_sntp_getservername(i));
        } else {
            // we have either IPv4 or IPv6 address, let's print it
            char buff[INET6_ADDRSTRLEN];
            ip_addr_t const *ip = esp_sntp_getserver(i);
            if (ipaddr_ntoa_r(ip, buff, INET6_ADDRSTRLEN) != NULL) {
                BROOKESIA_LOGI("server %d: %s", i, buff);
            }
        }
    }
}

void SNTP::update_timezone()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    setenv("TZ", data_timezone_.c_str(), 1);
    tzset();
    update_local_time();

    BROOKESIA_LOGI("Timezone updated to %s", data_timezone_.c_str());
}

void SNTP::update_local_time()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    time_t now;
    struct tm timeinfo;
    char strftime_buf[64] = {};
    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    BROOKESIA_LOGI("The current date/time in %s is: %s", data_timezone_.c_str(), strftime_buf);
}

void SNTP::try_load_data()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_data_loaded_) {
        BROOKESIA_LOGD("Data is already loaded, skip");
        return;
    }

    if (!NVSHelper::is_available()) {
        BROOKESIA_LOGD("NVS is not available, skip");
        return;
    }

    auto binding = service::ServiceManager::get_instance().bind(NVSHelper::get_name().data());
    BROOKESIA_CHECK_FALSE_EXIT(binding.is_valid(), "Failed to bind NVS service");

    auto nvs_namespace = get_attributes().name;

    {
        auto key = BROOKESIA_DESCRIBE_TO_STR(DataType::Timezone);
        auto result = NVSHelper::get_key_value<std::string>(nvs_namespace, key);
        if (!result) {
            BROOKESIA_LOGD("Failed to load '%1%' from NVS: %2%", key, result.error());
        } else {
            set_data<DataType::Timezone>(std::move(result.value()));
            update_timezone();
            BROOKESIA_LOGD("Loaded '%1%' from NVS", key);
        }
    }

    {
        auto key = BROOKESIA_DESCRIBE_TO_STR(DataType::Servers);
        auto result = NVSHelper::get_key_value<std::vector<std::string>>(nvs_namespace, key);
        if (!result) {
            BROOKESIA_LOGD("Failed to load '%1%' from NVS: %2%", key, result.error());
        } else {
            set_data<DataType::Servers>(std::move(result.value()));
        }
    }

    is_data_loaded_ = true;

    BROOKESIA_LOGI("Loaded all data from NVS");
}

void SNTP::try_save_data(DataType type)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto key = BROOKESIA_DESCRIBE_TO_STR(type);
    BROOKESIA_LOGD("Params: type(%1%)", key);

    if (!NVSHelper::is_available()) {
        BROOKESIA_LOGD("NVS is not available, skip");
        return;
    }

    auto nvs_namespace = get_attributes().name;

    auto save_function = [this, &nvs_namespace, &key](const auto & data_value) {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        auto result = NVSHelper::save_key_value(nvs_namespace, key, data_value, NVS_SAVE_DATA_TIMEOUT_MS);
        if (!result) {
            BROOKESIA_LOGE("Failed to save '%1%' to NVS: %2%", key, result.error());
        } else {
            BROOKESIA_LOGI("Saved '%1%' to NVS", key);
        }
    };

    if (type == DataType::Timezone) {
        save_function(get_data<DataType::Timezone>());
    } else if (type == DataType::Servers) {
        save_function(get_data<DataType::Servers>());
    } else {
        BROOKESIA_LOGE("Invalid data type for saving to NVS: %1%", key);
    }
}

void SNTP::try_erase_data()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!NVSHelper::is_available()) {
        BROOKESIA_LOGD("NVS is not available, skip");
        return;
    }

    auto nvs_namespace = get_attributes().name;
    auto result = NVSHelper::erase_keys(nvs_namespace, {}, NVS_ERASE_DATA_TIMEOUT_MS);
    if (!result) {
        BROOKESIA_LOGE("Failed to erase NVS namespace '%1%' data: %2%", nvs_namespace, result.error());
    } else {
        BROOKESIA_LOGI("Erased NVS namespace '%1%' data", nvs_namespace);
    }
}

#if BROOKESIA_SERVICE_SNTP_ENABLE_AUTO_REGISTER
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    ServiceBase, SNTP, SNTP::get_instance().get_attributes().name, SNTP::get_instance(),
    BROOKESIA_SERVICE_SNTP_PLUGIN_SYMBOL
);
#endif

} // namespace esp_brookesia::service
