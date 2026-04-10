/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <atomic>
#include <vector>
#include "brookesia/service_manager/event/registry.hpp"
#include "brookesia/service_manager/service/manager.hpp"

class WifiProvisioning {
public:
    struct Config {
        std::shared_ptr<esp_brookesia::lib_utils::TaskScheduler> task_scheduler;
    };

    static WifiProvisioning &get_instance()
    {
        static WifiProvisioning instance;
        return instance;
    }

    bool init(const Config &config);

    bool is_initialized() const
    {
        return is_initialized_.load();
    }

    bool start();

private:
    WifiProvisioning() = default;
    ~WifiProvisioning() = default;
    WifiProvisioning(const WifiProvisioning &) = delete;
    WifiProvisioning &operator=(const WifiProvisioning &) = delete;

    // Start STA flow: subscribe → trigger Start → handle Connected/Disconnected events
    void start_sta_connect_flow();
    // Start SoftAP flow: subscribe → trigger SoftApProvisionStart → handle Connected event
    void start_softap_provision_flow();

    std::atomic<bool> is_initialized_{false};
    Config config_{};
    // Holds the active event subscription; reset() to unsubscribe
    std::shared_ptr<esp_brookesia::service::EventRegistry::SignalConnection> active_conn_;
    std::vector<esp_brookesia::service::ServiceBinding> service_bindings_;
};
