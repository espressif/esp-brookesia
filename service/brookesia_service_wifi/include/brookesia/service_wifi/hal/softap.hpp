/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <bitset>
#include <string>
#include <memory>
#include "boost/thread.hpp"
#include "type.hpp"

namespace esp_brookesia::service::wifi {

enum class SoftApStateFlagBit {
    Setuping,       // SoftAP is setuping
    Setuped,        // SoftAP is setuped (stable marker)
    Starting,       // SoftAP is starting
    Started,        // SoftAP is started (stable marker)
    Stopping,       // SoftAP is stopping
    Max,
};
BROOKESIA_DESCRIBE_ENUM(SoftApStateFlagBit, Setuping, Setuped, Starting, Started, Stopping, Max);

using SoftApStateFlags = std::bitset<BROOKESIA_DESCRIBE_ENUM_TO_NUM(SoftApStateFlagBit::Max)>;

class SoftAp {
public:
    enum class ProvisionConnectStatus {
        Idle,
        Connecting,
        Success,
        Failed
    };

    SoftAp(void *hal)
        : hal_(hal)
    {}
    ~SoftAp() = default;

    SoftAp(const SoftAp &) = delete;
    SoftAp(SoftAp &&) = delete;
    SoftAp &operator=(const SoftAp &) = delete;
    SoftAp &operator=(SoftAp &&) = delete;

    /**
     * @brief SoftAP related
     */
    bool start_softap();
    void stop_softap();
    bool set_softap_params(const SoftApParams &params);
    const SoftApParams &get_softap_params()
    {
        return softap_params_;
    }

    bool start_softap_provision();
    bool stop_softap_provision();
    bool is_softap_provision_running() const
    {
        return is_softap_provision_running_;
    }

    bool on_hal_scan_ap_infos_updated(const std::vector<ApInfo> &ap_infos);
    bool on_hal_softap_wifi_event(uint8_t event);
    bool on_hal_softap_ip_event(uint8_t event);

    void reset_data();

    void set_provision_connect_status(ProvisionConnectStatus status);
    void set_provision_connect_error_msg(const std::string &msg);
    void set_provision_dns_sock(int sock);
    ProvisionConnectStatus get_provision_connect_status() const
    {
        return provision_connect_status_;
    }
    const std::string &get_provision_connect_error_msg() const
    {
        return provision_connect_error_msg_;
    }
    void trigger_provision_connect(const std::string &ssid, const std::string &password);

    static std::string get_ap_default_ssid();
    static uint8_t get_ap_best_channel(const std::vector<ApInfo> &ap_infos);

private:
    bool do_softap_start(const SoftApParams *params = nullptr);
    void do_softap_stop();

    bool start_softap_setup_task();
    void stop_softap_setup_task();
    bool start_softap_event_task(SoftApEvent event);
    void stop_softap_event_task();
    bool is_softap_setup_task_running()
    {
        return (softap_setup_task_ != 0);
    }
    bool is_softap_setup_ready();
    void set_softap_target_event(SoftApEvent event);
    SoftApEvent get_softap_target_event()
    {
        return softap_target_event_;
    }
    bool has_softap_state_flag_bit(SoftApStateFlagBit flag_bit)
    {
        return softap_state_flags_.test(BROOKESIA_DESCRIBE_ENUM_TO_NUM(flag_bit));
    }
    void update_softap_state_flag_bit(SoftApStateFlagBit flag_bit, bool enable);
    void clear_softap_state_flags();

    void cleanup_provision_servers();
    bool init_provision_http_server();
    void deinit_provision_http_server();
    bool init_provision_dns_server();
    void deinit_provision_dns_server();

    void *hal_ = nullptr;
    void *softap_netif_ = nullptr;
    SoftApParams softap_params_{};
    SoftApEvent softap_target_event_ = SoftApEvent::Max;
    SoftApStateFlags softap_state_flags_;
    lib_utils::TaskScheduler::TaskId softap_setup_task_ = 0;
    lib_utils::TaskScheduler::TaskId softap_event_task_ = 0;

    // Provision related
    bool is_softap_provision_running_ = false;
    ProvisionConnectStatus provision_connect_status_ = ProvisionConnectStatus::Idle;
    std::string provision_connect_error_msg_;
    void *provision_httpd_ = nullptr;
    std::unique_ptr<boost::thread> provision_dns_thread_;
    int provision_dns_sock_ = -1;

    std::vector<ApInfo> provision_scan_ap_infos_;
    bool provision_scan_completed_ = false;
    boost::mutex provision_scan_mutex_;
    boost::condition_variable provision_scan_cv_;
    lib_utils::TaskScheduler::TaskId provision_scan_periodic_task_ = 0;
};

BROOKESIA_DESCRIBE_ENUM(SoftAp::ProvisionConnectStatus, Idle, Connecting, Success, Failed);

} // namespace esp_brookesia::service::wifi
