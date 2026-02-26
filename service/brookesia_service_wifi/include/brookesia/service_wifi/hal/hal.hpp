/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <bitset>
#include <cstdint>
#include <string>
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/lib_utils/task_scheduler.hpp"
#include "brookesia/service_helper/wifi.hpp"
#include "type.hpp"
#include "softap.hpp"

namespace esp_brookesia::service::wifi {

/**
 * @brief State flag bits for tracking WiFi general state
 *
 * These bits are used to track both transient action states (Initing, Deiniting, etc.)
 * and stable state markers (Inited, Started, Connected).
 * The Error bit indicates that an error occurred during a transient state.
 */
enum class GeneralStateFlagBit {
    Initing,        // WiFi is initializing
    Inited,         // WiFi is initialized (stable marker)
    Deiniting,      // WiFi is deinitializing
    Starting,       // WiFi is starting
    Started,        // WiFi is started (stable marker)
    Stopping,       // WiFi is stopping
    Connecting,     // WiFi is connecting
    Connected,      // WiFi is connected (stable marker)
    Disconnecting,  // WiFi is disconnecting
    Error,          // Error occurred during transient state
    Max,
};
BROOKESIA_DESCRIBE_ENUM(
    GeneralStateFlagBit, Initing, Inited, Deiniting, Starting, Started, Stopping, Connecting,
    Connected, Disconnecting, Error, Max
);

using GeneralStateFlags = std::bitset<BROOKESIA_DESCRIBE_ENUM_TO_NUM(GeneralStateFlagBit::Max)>;

class Hal {
public:
    friend class SoftAp;

    Hal() = default;
    ~Hal();

    bool init();
    void deinit();
    bool start();
    void stop();
    void reset_data();

    /**
     * @brief General action & event related
     */
    bool do_general_action(GeneralAction action, bool is_force = false);
    void trigger_general_event(GeneralEvent event);
    bool is_general_action_running(GeneralAction action);
    bool is_general_event_ready(GeneralEvent event);
    bool is_general_state_error()
    {
        return state_flags_.test(BROOKESIA_DESCRIBE_ENUM_TO_NUM(GeneralStateFlagBit::Error));
    }
    void update_general_state_error(bool enable);

    /**
     * @brief Connect related
     */
    void mark_target_connect_ap_info_connectable(bool connectable);
    bool set_target_connect_ap_info(const ConnectApInfo &ap_info);
    bool set_last_connected_ap_info(const ConnectApInfo &ap_info);
    bool set_connected_ap_infos(const std::list<ConnectApInfo> &ap_infos);
    const ConnectApInfo &get_target_connect_ap_info()
    {
        return target_connect_ap_info_;
    }
    const ConnectApInfo &get_last_connected_ap_info()
    {
        return last_connected_ap_info_;
    }
    const std::list<ConnectApInfo> &get_connected_ap_infos()
    {
        return connected_ap_info_list_;
    }

    /**
     * @brief Scan related
     */
    bool set_scan_params(const ScanParams &params);
    bool start_scan();
    void stop_scan();
    const ScanParams &get_scan_params()
    {
        return scan_params_;
    }

    /**
     * @brief SoftAP related
     */
    bool start_softap()
    {
        return softap_->start_softap();
    }
    void stop_softap()
    {
        softap_->stop_softap();
    }
    bool set_softap_params(const SoftApParams &params)
    {
        return softap_->set_softap_params(params);
    }
    const SoftApParams &get_softap_params()
    {
        return softap_->get_softap_params();
    }
    bool start_softap_provision()
    {
        return softap_->start_softap_provision();
    }
    bool stop_softap_provision()
    {
        return softap_->stop_softap_provision();
    }

private:
    using Helper = helper::Wifi;

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

    bool do_init();
    bool do_deinit();
    bool do_start();
    bool do_stop();
    bool do_connect(const ConnectApInfo &ap_info);
    bool do_disconnect();

    bool do_scan_start(const void *params = nullptr);
    bool do_scan_stop();
    bool is_scanning()
    {
        return is_scanning_;
    }
    bool is_scan_task_running()
    {
        return (scan_periodic_task_ != 0);
    }

    void set_disable_auto_connect(bool disable);
    bool is_disable_auto_connect()
    {
        return disable_auto_connect_;
    }

    GeneralEvent get_general_action_target_event(GeneralAction action);
    bool is_general_event_unexpected(GeneralEvent event);
    bool is_general_event_conflicting(GeneralEvent event);
    GeneralAction get_running_general_action();
    GeneralAction get_general_action_from_target_event(GeneralEvent event);

    GeneralStateFlagBit get_general_action_state_flag_bit(GeneralAction action);
    std::pair<GeneralStateFlagBit, bool> get_general_event_state_flag_bit(GeneralEvent event);
    void update_general_action_state_bit(GeneralAction action, bool enable);
    void update_general_event_state_bit(GeneralEvent event, bool enable);

    bool process_general_event_started();
    bool process_general_event_connected();
    bool process_general_event_unexpected_disconnected();

    bool on_wifi_event(uint8_t event);
    bool on_ip_event(uint8_t event);

    bool update_scan_ap_infos();

    void add_connected_ap_info(const ConnectApInfo &ap_info);
    void remove_connected_ap_info_by_ssid(const std::string &ssid);
    bool has_connected_ap_info(const ConnectApInfo &ap_info)
    {
        return std::any_of(connected_ap_info_list_.begin(), connected_ap_info_list_.end(),
        [&ap_info](const auto & info) {
            return info == ap_info;
        });
    }
    void clear_connected_ap_infos();
    bool get_connectable_ap_info_by_ssid(const std::string &ssid, ConnectApInfo &ap_info);
    bool get_last_connectable_ap_info(ConnectApInfo &ap_info);
    const ConnectApInfo &get_connecting_ap_info()
    {
        return connecting_ap_info_;
    }
    void set_connecting_ap_info(const ConnectApInfo &ap_info);
    bool increase_connect_retries();
    void reset_connect_retries();
    uint8_t get_connect_retries()
    {
        return connect_retries_;
    }
    uint8_t get_connect_retries_max()
    {
        return BROOKESIA_SERVICE_WIFI_HAL_CONNECT_RETRIES_MAX;
    }
    void stop_connect_task();

    bool is_initialized_ = false;
    bool is_running_ = false;
    bool is_unexpected_event_processing_ = false;
    bool disable_auto_connect_ = false;

    GeneralStateFlags state_flags_;

    std::shared_ptr<lib_utils::TaskScheduler> task_scheduler_;

#if CONFIG_ESP_HOSTED_ENABLED
    // Hosted doesn't support to multiple create netif, so use a static variable to store the netif
    inline static void *sta_netif_ = nullptr;
#else
    void *sta_netif_ = nullptr;
#endif
    void *wifi_event_handler_instance_ = nullptr;
    void *ip_event_handler_instance_ = nullptr;
    ConnectApInfo target_connect_ap_info_{};
    ConnectApInfo connecting_ap_info_{};
    ConnectApInfo last_connected_ap_info_{};
    std::list<ConnectApInfo> connected_ap_info_list_;
    lib_utils::TaskScheduler::TaskId wifi_started_connect_task_ = 0;
    uint8_t connect_retries_ = 0;

    bool is_scanning_ = false;
    ScanParams scan_params_{};
    lib_utils::TaskScheduler::TaskId scan_periodic_task_ = 0;
    lib_utils::TaskScheduler::TaskId scan_ap_timeout_task_ = 0;

    std::unique_ptr<SoftAp> softap_;
};

} // namespace esp_brookesia::service::wifi
