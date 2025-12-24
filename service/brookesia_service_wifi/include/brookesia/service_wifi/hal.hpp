/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <bitset>
#include <cstdint>
#include <string>
#include "esp_wifi.h"
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/lib_utils/task_scheduler.hpp"
#include "brookesia/service_helper/wifi.hpp"

namespace esp_brookesia::service::wifi {

using GeneralAction = helper::Wifi::GeneralAction;
using GeneralEvent = helper::Wifi::GeneralEvent;
using ApSignalLevel = helper::Wifi::ApSignalLevel;
using ApInfo = helper::Wifi::ApInfo;

enum class GeneralStateFlagBit {
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
    GeneralStateFlagBit, Initing, Inited, Deiniting, Starting, Started, Stopping, Connecting,
    Connected, Disconnecting, Max
);

using GeneralStateFlags = std::bitset<BROOKESIA_DESCRIBE_ENUM_TO_NUM(GeneralStateFlagBit::Max)>;

struct ConnectApInfo {
    ConnectApInfo() = default;
    ConnectApInfo(const std::string &ssid, const std::string &password = "")
        : ssid(ssid)
        , password(password)
    {}

    bool operator==(const ConnectApInfo &other) const
    {
        return (ssid == other.ssid) && (password == other.password) && (is_connectable == other.is_connectable);
    }

    bool operator!=(const ConnectApInfo &other) const
    {
        return !(*this == other);
    }

    std::string ssid;
    std::string password;
    bool is_connectable = true;
};
BROOKESIA_DESCRIBE_STRUCT(ConnectApInfo, (), (ssid, password, is_connectable));

struct ScanParams {
    bool operator==(const ScanParams &other) const
    {
        return (ap_count == other.ap_count) && (interval_ms == other.interval_ms) && (timeout_ms == other.timeout_ms);
    }

    bool operator!=(const ScanParams &other) const
    {
        return !(*this == other);
    }

    size_t ap_count = 20;
    uint32_t interval_ms = 10000;
    uint32_t timeout_ms = 60000;
};
BROOKESIA_DESCRIBE_STRUCT(ScanParams, (), (ap_count, interval_ms, timeout_ms));

class Hal {
public:
    inline static constexpr const char *WIFI_EVENT_PROCESS_GROUP = "wifi_event";
    inline static constexpr const char *GENERAL_CALLBACK_GROUP = "general_callback";

    using Helper = helper::Wifi;
    using GeneralEventCallback = std::function<void(
                                     GeneralEvent event, const GeneralStateFlags &old_flags,
                                     const GeneralStateFlags &new_flags
                                 )>;
    using GeneralActionCallback = std::function<void(GeneralAction action)>;
    using ScanApRecordsUpdatedCallback = std::function<void(const boost::json::array &scan_ap_infos)>;

    Hal(std::shared_ptr<lib_utils::TaskScheduler> task_scheduler)
        : task_scheduler_(task_scheduler)
    {}
    ~Hal();

    bool init();
    void deinit();
    bool start();
    void stop();
    void reset();
    void reset_data();

    bool is_initialized()
    {
        return is_initialized_.load();
    }
    bool is_running()
    {
        return is_running_.load();
    }

    bool do_general_action(GeneralAction action);

    bool set_scan_params(const ScanParams &params);
    bool start_ap_scan();
    void stop_ap_scan();
    bool is_scanning()
    {
        return is_scanning_.load();
    }
    bool is_scan_task_running()
    {
        return (scan_ap_periodic_task != 0) &&
               (task_scheduler_->get_state(scan_ap_periodic_task) == lib_utils::TaskScheduler::TaskState::Running);
    }
    const ScanParams &get_scan_params()
    {
        boost::lock_guard lock(operation_mutex_);
        return scan_params_;
    }

    bool set_target_connect_ap_info(const ConnectApInfo &ap_info);
    const ConnectApInfo &get_target_connect_ap_info()
    {
        boost::lock_guard lock(operation_mutex_);
        return target_connect_ap_info_;
    }

    const ConnectApInfo &get_connecting_ap_info()
    {
        boost::lock_guard lock(operation_mutex_);
        return connecting_ap_info_;
    }
    void set_last_connected_ap_info(const ConnectApInfo &ap_info);
    const ConnectApInfo &get_last_connected_ap_info()
    {
        boost::lock_guard lock(operation_mutex_);
        return last_connected_ap_info_;
    }

    void add_connected_ap_info(const ConnectApInfo &ap_info);
    void remove_connected_ap_info_by_ssid(const std::string &ssid);
    bool has_connected_ap_info(const ConnectApInfo &ap_info)
    {
        boost::lock_guard lock(operation_mutex_);
        return std::any_of(connected_ap_info_list_.begin(), connected_ap_info_list_.end(),
        [&ap_info](const auto & info) {
            return info == ap_info;
        });
    }
    void clear_connected_ap_infos();
    void get_connected_ap_infos(std::vector<ConnectApInfo> &infos)
    {
        boost::lock_guard lock(operation_mutex_);
        infos.clear();
        for (const auto &info : connected_ap_info_list_) {
            infos.push_back(info);
        }
    }
    bool get_connectable_ap_info_by_ssid(const std::string &ssid, ConnectApInfo &ap_info)
    {
        boost::lock_guard lock(operation_mutex_);
        for (const auto &info : connected_ap_info_list_) {
            if ((info.ssid == ssid) && info.is_connectable) {
                ap_info = info;
                return true;
            }
        }
        return false;
    }
    bool get_last_connectable_ap_info(ConnectApInfo &ap_info)
    {
        boost::lock_guard lock(operation_mutex_);
        for (auto it = connected_ap_info_list_.rbegin(); it != connected_ap_info_list_.rend(); ++it) {
            const auto &info = *it;
            if (info.is_connectable) {
                ap_info = info;
                return true;
            }
        }
        return false;
    }

    void register_general_event_callback(GeneralEventCallback callback)
    {
        boost::lock_guard lock(operation_mutex_);
        general_event_callback_ = std::move(callback);
    }
    void register_general_action_callback(GeneralActionCallback callback)
    {
        boost::lock_guard lock(operation_mutex_);
        general_action_callback_ = std::move(callback);
    }
    void register_scan_ap_infos_updated_callback(ScanApRecordsUpdatedCallback callback)
    {
        boost::lock_guard lock(operation_mutex_);
        scan_ap_infos_updated_callback_ = std::move(callback);
    }

    GeneralEvent get_general_action_target_event(GeneralAction action)
    {
        switch (action) {
        case GeneralAction::Init:
            return GeneralEvent::Inited;
        case GeneralAction::Deinit:
            return GeneralEvent::Deinited;
        case GeneralAction::Start:
            return GeneralEvent::Started;
        case GeneralAction::Stop:
            return GeneralEvent::Stopped;
        case GeneralAction::Connect:
            return GeneralEvent::Connected;
        case GeneralAction::Disconnect:
            return GeneralEvent::Disconnected;
        default:
            return GeneralEvent::Max;
        }
    }
    GeneralStateFlagBit get_general_action_state_flag_bit(GeneralAction action)
    {
        switch (action) {
        case GeneralAction::Init:
            return GeneralStateFlagBit::Initing;
        case GeneralAction::Deinit:
            return GeneralStateFlagBit::Deiniting;
        case GeneralAction::Start:
            return GeneralStateFlagBit::Starting;
        case GeneralAction::Stop:
            return GeneralStateFlagBit::Stopping;
        case GeneralAction::Connect:
            return GeneralStateFlagBit::Connecting;
        case GeneralAction::Disconnect:
            return GeneralStateFlagBit::Disconnecting;
        default:
            return GeneralStateFlagBit::Max;
        }
    }
    GeneralStateFlagBit get_general_event_state_flag_bit(GeneralEvent event)
    {
        switch (event) {
        case GeneralEvent::Inited:
            return GeneralStateFlagBit::Inited;
        case GeneralEvent::Deinited:
            return GeneralStateFlagBit::Inited;
        case GeneralEvent::Started:
            return GeneralStateFlagBit::Started;
        case GeneralEvent::Stopped:
            return GeneralStateFlagBit::Started;
        case GeneralEvent::Connected:
            return GeneralStateFlagBit::Connected;
        case GeneralEvent::Disconnected:
            return GeneralStateFlagBit::Connected;
        default:
            return GeneralStateFlagBit::Max;
        }
    }
    bool is_general_action_running(GeneralAction action)
    {
        boost::lock_guard lock(state_mutex_);
        GeneralStateFlagBit flag_bit = GeneralStateFlagBit::Max;
        switch (action) {
        case GeneralAction::Init:
            flag_bit = GeneralStateFlagBit::Initing;
            break;
        case GeneralAction::Deinit:
            flag_bit = GeneralStateFlagBit::Deiniting;
            break;
        case GeneralAction::Start:
            flag_bit = GeneralStateFlagBit::Starting;
            break;
        case GeneralAction::Stop:
            flag_bit = GeneralStateFlagBit::Stopping;
            break;
        case GeneralAction::Connect:
            flag_bit = GeneralStateFlagBit::Connecting;
            break;
        case GeneralAction::Disconnect:
            flag_bit = GeneralStateFlagBit::Disconnecting;
            break;
        default:
            return false;
        }
        return state_flags_.test(BROOKESIA_DESCRIBE_ENUM_TO_NUM(flag_bit));
    }
    bool is_general_event_ready(GeneralEvent event)
    {
        boost::lock_guard lock(state_mutex_);
        return is_general_event_ready_internal(event);
    }
    bool is_general_event_changed(
        GeneralEvent event, const GeneralStateFlags &old_flags, const GeneralStateFlags &new_flags
    );

private:
    void stop_internal();
    void deinit_internal();
    void reset_internal();
    void reset_data_internal();

    bool do_init();
    bool do_deinit();
    bool do_start();
    bool do_stop();
    bool do_connect();
    bool do_disconnect();

    bool do_scan_start();
    bool do_scan_stop();

    uint32_t get_general_event_wait_timeout_ms(GeneralEvent event);
    bool is_general_event_ready_internal(GeneralEvent event);

    void trigger_general_event(GeneralEvent event);
    bool wait_for_general_event(GeneralEvent event, uint32_t timeout_ms);

    bool process_wifi_event(wifi_event_t event, void *data);
    bool process_ip_event(ip_event_t event, void *data);
    static void on_wifi_ip_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data);

    bool post_scan_interval_task();
    bool update_scan_ap_infos();

    std::atomic<bool> is_initialized_{false};
    std::atomic<bool> is_running_{false};

    boost::mutex state_mutex_;
    boost::condition_variable_any state_condition_variable_;
    GeneralStateFlags state_flags_;

    std::shared_ptr<lib_utils::TaskScheduler> task_scheduler_;

    GeneralEventCallback general_event_callback_;
    GeneralActionCallback general_action_callback_;
    ScanApRecordsUpdatedCallback scan_ap_infos_updated_callback_;

    boost::mutex operation_mutex_;

    esp_netif_t *sta_netif_ = nullptr;
    esp_event_handler_instance_t wifi_event_handler_instance_ = nullptr;
    esp_event_handler_instance_t ip_event_handler_instance_ = nullptr;
    wifi_config_t target_wifi_config_ = {};
    ConnectApInfo target_connect_ap_info_;
    ConnectApInfo connecting_ap_info_;
    ConnectApInfo last_connected_ap_info_;
    std::list<ConnectApInfo> connected_ap_info_list_;

    std::atomic<bool> is_scanning_{false};
    ScanParams scan_params_;
    boost::json::array scan_ap_infos_;
    lib_utils::TaskScheduler::TaskId scan_ap_periodic_task = 0;
    lib_utils::TaskScheduler::TaskId scan_ap_timeout_task = 0;
};

} // namespace esp_brookesia::service::wifi
