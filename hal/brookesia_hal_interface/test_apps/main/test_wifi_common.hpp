/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include <list>
#include <memory>
#include <utility>
#include <vector>
#include "brookesia/hal_interface/device.hpp"
#include "brookesia/hal_interface/interfaces/network/connectivity.hpp"
#include "brookesia/hal_interface/interfaces/wifi/basic.hpp"
#include "brookesia/hal_interface/interfaces/wifi/softap.hpp"
#include "brookesia/hal_interface/interfaces/wifi/station.hpp"

namespace esp_brookesia {

class TestWifiBackend {
public:
    static constexpr const char *BASIC_NAME = "TestWiFi:Basic";
    static constexpr const char *STATION_NAME = "TestWiFi:Station";
    static constexpr const char *SOFTAP_NAME = "TestWiFi:SoftAP";
    static constexpr const char *CONNECTIVITY_NAME = "TestWiFi:Connectivity";

    bool configure_basic(hal::wifi::BasicIface::RuntimeContext runtime, hal::wifi::BasicIface::Callbacks callbacks);
    bool configure_station(hal::wifi::StationIface::Callbacks callbacks);
    bool configure_softap(hal::wifi::SoftApIface::Callbacks callbacks);
    void clear_basic_callbacks();
    void clear_station_callbacks();
    void clear_softap_callbacks();
    void clear_all_callbacks();

    bool init();
    void deinit();
    bool start();
    void stop();
    void reset_data();

    bool do_action(hal::wifi::BasicAction action, bool is_force = false);
    bool is_action_running(hal::wifi::BasicAction action);
    bool is_event_ready(hal::wifi::BasicEvent event);

    bool do_station_action(hal::wifi::StationAction action, bool is_force = false);
    bool is_station_action_running(hal::wifi::StationAction action);
    bool is_station_event_ready(hal::wifi::StationEvent event);

    void mark_target_connect_ap_info_connectable(bool connectable);
    bool set_target_connect_ap_info(const hal::wifi::ConnectApInfo &ap_info);
    bool set_last_connected_ap_info(const hal::wifi::ConnectApInfo &ap_info);
    bool set_connected_ap_infos(const hal::wifi::ConnectApInfoList &ap_infos);
    const hal::wifi::ConnectApInfo &get_target_connect_ap_info() const;
    const hal::wifi::ConnectApInfo &get_last_connected_ap_info() const;
    const hal::wifi::ConnectApInfoList &get_connected_ap_infos() const;

    bool set_scan_params(const hal::wifi::ScanParams &params);
    const hal::wifi::ScanParams &get_scan_params() const;
    bool start_scan();
    void stop_scan();

    bool set_soft_ap_params(const hal::wifi::SoftApParams &params);
    const hal::wifi::SoftApParams &get_soft_ap_params() const;
    bool start_soft_ap();
    void stop_soft_ap();
    bool start_soft_ap_provision();
    bool stop_soft_ap_provision();
    hal::network::NetworkStatus get_network_status() const;

private:
    bool initialized_ = false;
    bool started_ = false;
    bool connected_ = false;
    bool scanning_ = false;
    bool softap_started_ = false;
    bool provision_started_ = false;
    bool basic_configured_ = false;
    hal::wifi::BasicAction running_basic_action_ = hal::wifi::BasicAction::Max;
    hal::wifi::StationAction running_station_action_ = hal::wifi::StationAction::Max;
    hal::wifi::BasicIface::RuntimeContext runtime_{};
    hal::wifi::BasicIface::Callbacks basic_callbacks_{};
    hal::wifi::StationIface::Callbacks station_callbacks_{};
    hal::wifi::SoftApIface::Callbacks softap_callbacks_{};
    hal::wifi::ConnectApInfo target_ap_{};
    hal::wifi::ConnectApInfo last_ap_{};
    hal::wifi::ConnectApInfoList connected_aps_{};
    hal::wifi::ScanParams scan_params_{};
    hal::wifi::ScanApInfoList scan_ap_infos_{};
    hal::wifi::SoftApParams softap_params_{};
};

class TestWifiBasicIface: public hal::wifi::BasicIface {
public:
    explicit TestWifiBasicIface(std::shared_ptr<TestWifiBackend> backend)
        : backend_(std::move(backend))
    {
    }

    bool configure(RuntimeContext runtime, Callbacks callbacks) override;
    void clear_callbacks() override;

    bool init() override;
    void deinit() override;
    bool start() override;
    void stop() override;
    void reset_data() override;

    bool do_action(hal::wifi::BasicAction action, bool is_force = false) override;
    bool is_action_running(hal::wifi::BasicAction action) override;
    bool is_event_ready(hal::wifi::BasicEvent event) override;

private:
    std::shared_ptr<TestWifiBackend> backend_;
};

class TestWifiStationIface: public hal::wifi::StationIface {
public:
    explicit TestWifiStationIface(std::shared_ptr<TestWifiBackend> backend)
        : backend_(std::move(backend))
    {
    }

    bool configure(Callbacks callbacks) override;
    void clear_callbacks() override;

    bool do_action(hal::wifi::StationAction action, bool is_force = false) override;
    bool is_action_running(hal::wifi::StationAction action) override;
    bool is_event_ready(hal::wifi::StationEvent event) override;

    void mark_target_connect_ap_info_connectable(bool connectable) override;
    bool set_target_connect_ap_info(const hal::wifi::ConnectApInfo &ap_info) override;
    bool set_last_connected_ap_info(const hal::wifi::ConnectApInfo &ap_info) override;
    bool set_connected_ap_infos(const hal::wifi::ConnectApInfoList &ap_infos) override;
    const hal::wifi::ConnectApInfo &get_target_connect_ap_info() const override;
    const hal::wifi::ConnectApInfo &get_last_connected_ap_info() const override;
    const hal::wifi::ConnectApInfoList &get_connected_ap_infos() const override;

    bool set_scan_params(const hal::wifi::ScanParams &params) override;
    const hal::wifi::ScanParams &get_scan_params() const override;
    bool start_scan() override;
    void stop_scan() override;

private:
    std::shared_ptr<TestWifiBackend> backend_;
};

class TestWifiSoftApIface: public hal::wifi::SoftApIface {
public:
    explicit TestWifiSoftApIface(std::shared_ptr<TestWifiBackend> backend)
        : backend_(std::move(backend))
    {
    }

    bool configure(Callbacks callbacks) override;
    void clear_callbacks() override;

    bool set_params(const hal::wifi::SoftApParams &params) override;
    const hal::wifi::SoftApParams &get_params() const override;
    bool start() override;
    void stop() override;
    bool start_provision() override;
    bool stop_provision() override;

private:
    std::shared_ptr<TestWifiBackend> backend_;
};

class TestWifiConnectivityIface: public hal::network::ConnectivityIface {
public:
    explicit TestWifiConnectivityIface(std::shared_ptr<TestWifiBackend> backend)
        : backend_(std::move(backend))
    {
    }

    hal::network::NetworkStatus get_status() const override;

private:
    std::shared_ptr<TestWifiBackend> backend_;
};

class TestWifiDevice: public hal::Device {
public:
    static constexpr const char *NAME = "TestWiFi";

    TestWifiDevice()
        : hal::Device(std::string(NAME))
    {
    }

    bool probe() override;
    std::vector<hal::InterfaceSpec> get_interface_specs() const override;
    bool on_init() override;
    void on_deinit() override;

private:
    std::shared_ptr<TestWifiBackend> backend_;
};

} // namespace esp_brookesia
