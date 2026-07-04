/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <bitset>
#include <cstdint>
#include <list>
#include <memory>
#include <span>
#include <string>
#include "brookesia/hal_adaptor/macro_configs.h"
#include "brookesia/hal_interface/interfaces/network/connectivity.hpp"
#include "brookesia/hal_interface/interfaces/wifi/basic.hpp"
#include "brookesia/hal_interface/interfaces/wifi/softap.hpp"
#include "brookesia/hal_interface/interfaces/wifi/station.hpp"
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/lib_utils/task_scheduler.hpp"
#include "softap.hpp"

namespace esp_brookesia::hal {

enum class WifiStateFlagBit {
    Idle,
    Initing,
    Inited,
    Deiniting,
    Starting,
    Started,
    Stopping,
    Connecting,
    Connected,
    Disconnecting,
    Max,
};

BROOKESIA_DESCRIBE_ENUM(
    WifiStateFlagBit, Idle, Initing, Inited, Deiniting, Starting, Started, Stopping, Connecting, Connected,
    Disconnecting, Max
);

using WifiStateFlags = std::bitset<BROOKESIA_DESCRIBE_ENUM_TO_NUM(WifiStateFlagBit::Max)>;

class WifiEspBackend {
public:
    friend class SoftAp;

    WifiEspBackend() = default;
    ~WifiEspBackend();

    bool configure(wifi::BasicIface::RuntimeContext runtime, wifi::BasicIface::Callbacks callbacks);
    bool configure(wifi::StationIface::Callbacks callbacks);
    bool configure(wifi::SoftApIface::Callbacks callbacks);
    void clear_basic_callbacks();
    void clear_station_callbacks();
    void clear_softap_callbacks();
    void clear_callbacks();

    bool init();
    void deinit();
    bool start();
    void stop();
    void reset_data();

    bool do_action(wifi::BasicAction action, bool is_force = false);
    bool is_action_running(wifi::BasicAction action);
    bool is_event_ready(wifi::BasicEvent event);

    bool do_station_action(wifi::StationAction action, bool is_force = false);
    bool is_station_action_running(wifi::StationAction action);
    bool is_station_event_ready(wifi::StationEvent event);

    /**
     * @brief Connect related
     */
    void mark_target_connect_ap_info_connectable(bool connectable);
    bool set_target_connect_ap_info(const wifi::ConnectApInfo &ap_info);
    bool set_last_connected_ap_info(const wifi::ConnectApInfo &ap_info);
    bool set_connected_ap_infos(const wifi::ConnectApInfoList &ap_infos);
    const wifi::ConnectApInfo &get_target_connect_ap_info() const
    {
        return target_connect_ap_info_;
    }
    const wifi::ConnectApInfo &get_last_connected_ap_info() const
    {
        return last_connected_ap_info_;
    }
    const wifi::ConnectApInfoList &get_connected_ap_infos() const
    {
        return connected_ap_info_list_;
    }

    /**
     * @brief Scan related
     */
    bool set_scan_params(const wifi::ScanParams &params);
    bool start_scan();
    void stop_scan();
    const wifi::ScanParams &get_scan_params() const
    {
        return scan_params_;
    }

    /**
     * @brief SoftAP related
     */
    bool start_soft_ap();
    void stop_soft_ap();
    bool set_soft_ap_params(const wifi::SoftApParams &params);
    const wifi::SoftApParams &get_soft_ap_params() const;
    bool start_soft_ap_provision();
    bool stop_soft_ap_provision();

    network::NetworkStatus get_network_status() const;

private:
    bool is_initialized()
    {
        return is_initialized_;
    }
    bool is_running()
    {
        return is_running_;
    }
    std::shared_ptr<lib_utils::TaskScheduler> get_task_scheduler()
    {
        return task_scheduler_;
    }
    lib_utils::TaskScheduler::Group get_task_group() const
    {
        return task_group_;
    }

    bool do_init();
    bool do_deinit();
    bool do_start();
    bool do_stop();
    bool do_connect(const wifi::ConnectApInfo &ap_info);
    bool do_disconnect();

    bool do_scan_start(const void *params = nullptr);
    bool do_scan_stop();
    bool is_scanning()
    {
        return is_scanning_;
    }
    bool is_scan_task_running()
    {
        return (scan_periodic_task_ != 0) || (scan_delayed_start_task_ != 0) ||
               (scan_ap_timeout_task_ != 0);
    }

    bool do_general_action(wifi::GeneralAction action, bool is_force = false);
    void trigger_general_event(wifi::GeneralEvent event);
    bool is_general_action_running(wifi::GeneralAction action);
    bool is_general_event_ready(wifi::GeneralEvent event);

    wifi::GeneralEvent get_general_action_target_event(wifi::GeneralAction action);
    bool is_general_event_unexpected(wifi::GeneralEvent event);
    bool is_general_event_conflicting(wifi::GeneralEvent event);
    wifi::GeneralAction get_running_general_action();
    wifi::GeneralAction get_general_action_from_target_event(wifi::GeneralEvent event);

    WifiStateFlagBit get_general_action_state_flag_bit(wifi::GeneralAction action);
    std::pair<WifiStateFlagBit, bool> get_general_event_state_flag_bit(wifi::GeneralEvent event);
    void update_general_action_state_bit(wifi::GeneralAction action, bool enable);
    void update_general_event_state_bit(wifi::GeneralEvent event, bool enable);

    void notify_general_action(wifi::GeneralAction action);
    void notify_general_event(wifi::GeneralEvent event, bool is_unexpected);
    void notify_error_state(wifi::GeneralAction action);
    bool request_station_action(wifi::StationAction action);
    void notify_scan_state_changed(bool is_scanning);
    void notify_scan_ap_infos_updated(std::span<const wifi::ScanApInfo> ap_infos);
    void notify_last_connected_ap_info_updated(const wifi::ConnectApInfo &ap_info);
    void notify_connected_ap_infos_updated();
    void notify_softap_event(wifi::SoftApEvent event);
    void notify_softap_params_updated(const wifi::SoftApParams &params);

    bool on_wifi_event(uint8_t event);
    bool on_ip_event(uint8_t event);

    bool update_scan_ap_infos();

    bool is_initialized_ = false;
    bool is_running_ = false;
    bool is_unexpected_event_processing_ = false;

    WifiStateFlags state_flags_;

    std::shared_ptr<lib_utils::TaskScheduler> task_scheduler_;
    lib_utils::TaskScheduler::Group task_group_;
    wifi::BasicIface::Callbacks basic_callbacks_;
    wifi::StationIface::Callbacks station_callbacks_;
    wifi::SoftApIface::Callbacks softap_callbacks_;

#if CONFIG_ESP_HOSTED_ENABLED
    // Hosted doesn't support to multiple create netif, so use a static variable to store the netif
    inline static void *station_netif_ = nullptr;
#else
    void *station_netif_ = nullptr;
#endif
    void *wifi_event_handler_instance_ = nullptr;
    void *ip_event_handler_instance_ = nullptr;
    bool station_link_up_ = false;
    bool station_ip_ready_ = false;
    uint64_t connected_since_ms_ = 0;
    wifi::ConnectApInfo target_connect_ap_info_{};
    wifi::ConnectApInfo last_connected_ap_info_{};
    wifi::ConnectApInfoList connected_ap_info_list_;

    bool is_scanning_ = false;
    bool scan_one_shot_running_ = false;
    wifi::ScanParams scan_params_{};
    lib_utils::TaskScheduler::TaskId scan_periodic_task_ = 0;
    lib_utils::TaskScheduler::TaskId scan_delayed_start_task_ = 0;
    lib_utils::TaskScheduler::TaskId scan_ap_timeout_task_ = 0;

    wifi::SoftApParams softap_params_cache_{};
    std::unique_ptr<SoftAp> softap_;
};

} // namespace esp_brookesia::hal
