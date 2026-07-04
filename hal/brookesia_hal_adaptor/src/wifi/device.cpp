/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/hal_adaptor/macro_configs.h"
#if !BROOKESIA_HAL_ADAPTOR_WIFI_DEVICE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/hal_adaptor/wifi/device.hpp"
#include "backend.hpp"

namespace esp_brookesia::hal {

namespace {

class WifiBasicAdaptorIface: public wifi::BasicIface {
public:
    explicit WifiBasicAdaptorIface(std::shared_ptr<WifiEspBackend> backend)
        : backend_(std::move(backend))
    {
    }

    bool configure(RuntimeContext runtime, Callbacks callbacks) override
    {
        return backend_->configure(std::move(runtime), std::move(callbacks));
    }

    void clear_callbacks() override
    {
        backend_->clear_basic_callbacks();
    }

    bool init() override
    {
        return backend_->init();
    }

    void deinit() override
    {
        backend_->deinit();
    }

    bool start() override
    {
        return backend_->start();
    }

    void stop() override
    {
        backend_->stop();
    }

    void reset_data() override
    {
        backend_->reset_data();
    }

    bool do_action(wifi::BasicAction action, bool is_force = false) override
    {
        return backend_->do_action(action, is_force);
    }

    bool is_action_running(wifi::BasicAction action) override
    {
        return backend_->is_action_running(action);
    }

    bool is_event_ready(wifi::BasicEvent event) override
    {
        return backend_->is_event_ready(event);
    }

private:
    std::shared_ptr<WifiEspBackend> backend_;
};

class WifiStationAdaptorIface: public wifi::StationIface {
public:
    explicit WifiStationAdaptorIface(std::shared_ptr<WifiEspBackend> backend)
        : backend_(std::move(backend))
    {
    }

    bool configure(Callbacks callbacks) override
    {
        return backend_->configure(std::move(callbacks));
    }

    void clear_callbacks() override
    {
        backend_->clear_station_callbacks();
    }

    bool do_action(wifi::StationAction action, bool is_force = false) override
    {
        return backend_->do_station_action(action, is_force);
    }

    bool is_action_running(wifi::StationAction action) override
    {
        return backend_->is_station_action_running(action);
    }

    bool is_event_ready(wifi::StationEvent event) override
    {
        return backend_->is_station_event_ready(event);
    }

    void mark_target_connect_ap_info_connectable(bool connectable) override
    {
        backend_->mark_target_connect_ap_info_connectable(connectable);
    }

    bool set_target_connect_ap_info(const wifi::ConnectApInfo &ap_info) override
    {
        return backend_->set_target_connect_ap_info(ap_info);
    }

    bool set_last_connected_ap_info(const wifi::ConnectApInfo &ap_info) override
    {
        return backend_->set_last_connected_ap_info(ap_info);
    }

    bool set_connected_ap_infos(const wifi::ConnectApInfoList &ap_infos) override
    {
        return backend_->set_connected_ap_infos(ap_infos);
    }

    const wifi::ConnectApInfo &get_target_connect_ap_info() const override
    {
        return backend_->get_target_connect_ap_info();
    }

    const wifi::ConnectApInfo &get_last_connected_ap_info() const override
    {
        return backend_->get_last_connected_ap_info();
    }

    const wifi::ConnectApInfoList &get_connected_ap_infos() const override
    {
        return backend_->get_connected_ap_infos();
    }

    bool set_scan_params(const wifi::ScanParams &params) override
    {
        return backend_->set_scan_params(params);
    }

    const wifi::ScanParams &get_scan_params() const override
    {
        return backend_->get_scan_params();
    }

    bool start_scan() override
    {
        return backend_->start_scan();
    }

    void stop_scan() override
    {
        backend_->stop_scan();
    }

private:
    std::shared_ptr<WifiEspBackend> backend_;
};

class WifiSoftApAdaptorIface: public wifi::SoftApIface {
public:
    explicit WifiSoftApAdaptorIface(std::shared_ptr<WifiEspBackend> backend)
        : backend_(std::move(backend))
    {
    }

    bool configure(Callbacks callbacks) override
    {
        return backend_->configure(std::move(callbacks));
    }

    void clear_callbacks() override
    {
        backend_->clear_softap_callbacks();
    }

    bool set_params(const wifi::SoftApParams &params) override
    {
        return backend_->set_soft_ap_params(params);
    }

    const wifi::SoftApParams &get_params() const override
    {
        return backend_->get_soft_ap_params();
    }

    bool start() override
    {
        return backend_->start_soft_ap();
    }

    void stop() override
    {
        backend_->stop_soft_ap();
    }

    bool start_provision() override
    {
        return backend_->start_soft_ap_provision();
    }

    bool stop_provision() override
    {
        return backend_->stop_soft_ap_provision();
    }

private:
    std::shared_ptr<WifiEspBackend> backend_;
};

class WifiConnectivityAdaptorIface: public network::ConnectivityIface {
public:
    explicit WifiConnectivityAdaptorIface(std::shared_ptr<WifiEspBackend> backend)
        : backend_(std::move(backend))
    {
    }

    network::NetworkStatus get_status() const override
    {
        return backend_->get_network_status();
    }

private:
    std::shared_ptr<WifiEspBackend> backend_;
};

} // namespace

bool WifiDevice::probe()
{
    return true;
}

std::vector<InterfaceSpec> WifiDevice::get_interface_specs() const
{
    return {
        {wifi::BasicIface::NAME, BASIC_IFACE_NAME},
        {wifi::StationIface::NAME, STATION_IFACE_NAME},
        {wifi::SoftApIface::NAME, SOFTAP_IFACE_NAME},
        {network::ConnectivityIface::NAME, CONNECTIVITY_IFACE_NAME},
    };
}

bool WifiDevice::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_EXCEPTION_RETURN(
        backend_ = std::make_shared<WifiEspBackend>(), false, "Failed to create Wi-Fi ESP backend"
    );

    interfaces_.emplace(BASIC_IFACE_NAME, std::make_shared<WifiBasicAdaptorIface>(backend_));
    interfaces_.emplace(STATION_IFACE_NAME, std::make_shared<WifiStationAdaptorIface>(backend_));
    interfaces_.emplace(SOFTAP_IFACE_NAME, std::make_shared<WifiSoftApAdaptorIface>(backend_));
    interfaces_.emplace(CONNECTIVITY_IFACE_NAME, std::make_shared<WifiConnectivityAdaptorIface>(backend_));

    return true;
}

void WifiDevice::on_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    interfaces_.erase(BASIC_IFACE_NAME);
    interfaces_.erase(STATION_IFACE_NAME);
    interfaces_.erase(SOFTAP_IFACE_NAME);
    interfaces_.erase(CONNECTIVITY_IFACE_NAME);
    backend_.reset();
}

#if BROOKESIA_HAL_ADAPTOR_ENABLE_WIFI_DEVICE
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    Device, WifiDevice, WifiDevice::DEVICE_NAME, WifiDevice::get_instance(),
    BROOKESIA_HAL_ADAPTOR_WIFI_DEVICE_PLUGIN_SYMBOL
);
#endif

} // namespace esp_brookesia::hal
