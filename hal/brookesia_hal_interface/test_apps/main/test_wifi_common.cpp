/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <utility>

#include "unity.h"
#include "test_wifi_common.hpp"

using namespace esp_brookesia;

namespace {

hal::wifi::ScanApInfoList make_scan_ap_infos()
{
    return hal::wifi::ScanApInfoList{
        hal::wifi::ScanApInfo{
            .ssid = "TestAP-1",
            .is_locked = true,
            .rssi = -42,
            .signal_level = hal::wifi::ScanApSignalLevel::LEVEL_4,
            .channel = 1,
        },
        hal::wifi::ScanApInfo{
            .ssid = "TestAP-2",
            .is_locked = false,
            .rssi = -72,
            .signal_level = hal::wifi::ScanApSignalLevel::LEVEL_1,
            .channel = 6,
        },
    };
}

} // namespace

namespace esp_brookesia {

bool TestWifiBackend::configure_basic(hal::wifi::BasicIface::RuntimeContext runtime, hal::wifi::BasicIface::Callbacks callbacks)
{
    if (!runtime.task_scheduler) {
        return false;
    }

    runtime_ = std::move(runtime);
    basic_callbacks_ = std::move(callbacks);
    basic_configured_ = true;
    return true;
}

bool TestWifiBackend::configure_station(hal::wifi::StationIface::Callbacks callbacks)
{
    station_callbacks_ = std::move(callbacks);
    return true;
}

bool TestWifiBackend::configure_softap(hal::wifi::SoftApIface::Callbacks callbacks)
{
    softap_callbacks_ = std::move(callbacks);
    return true;
}

void TestWifiBackend::clear_basic_callbacks()
{
    basic_callbacks_ = {};
}

void TestWifiBackend::clear_station_callbacks()
{
    station_callbacks_ = {};
}

void TestWifiBackend::clear_softap_callbacks()
{
    softap_callbacks_ = {};
}

void TestWifiBackend::clear_all_callbacks()
{
    clear_basic_callbacks();
    clear_station_callbacks();
    clear_softap_callbacks();
}

bool TestWifiBackend::init()
{
    initialized_ = true;
    return true;
}

void TestWifiBackend::deinit()
{
    reset_data();
    initialized_ = false;
    started_ = false;
    basic_configured_ = false;
    running_basic_action_ = hal::wifi::BasicAction::Max;
    running_station_action_ = hal::wifi::StationAction::Max;
    runtime_ = {};
    clear_all_callbacks();
}

bool TestWifiBackend::start()
{
    if (!initialized_ || !basic_configured_) {
        return false;
    }

    started_ = true;
    return true;
}

void TestWifiBackend::stop()
{
    started_ = false;
    connected_ = false;
    scanning_ = false;
    softap_started_ = false;
    provision_started_ = false;
}

void TestWifiBackend::reset_data()
{
    target_ap_ = {};
    last_ap_ = {};
    connected_aps_.clear();
    scan_params_ = {};
    scan_ap_infos_.clear();
    softap_params_ = {};
    connected_ = false;
    scanning_ = false;
    softap_started_ = false;
    provision_started_ = false;
}

bool TestWifiBackend::do_action(hal::wifi::BasicAction action, bool)
{
    running_basic_action_ = action;
    if (basic_callbacks_.on_action) {
        basic_callbacks_.on_action(action);
    }

    switch (action) {
    case hal::wifi::BasicAction::Init:
        initialized_ = true;
        if (basic_callbacks_.on_event) {
            basic_callbacks_.on_event(hal::wifi::BasicEvent::Inited, false);
        }
        break;
    case hal::wifi::BasicAction::Deinit:
        stop();
        initialized_ = false;
        if (basic_callbacks_.on_event) {
            basic_callbacks_.on_event(hal::wifi::BasicEvent::Deinited, false);
        }
        break;
    case hal::wifi::BasicAction::Start:
        if (!initialized_) {
            if (basic_callbacks_.on_error) {
                basic_callbacks_.on_error();
            }
            running_basic_action_ = hal::wifi::BasicAction::Max;
            return false;
        }
        started_ = true;
        if (basic_callbacks_.on_event) {
            basic_callbacks_.on_event(hal::wifi::BasicEvent::Started, false);
        }
        break;
    case hal::wifi::BasicAction::Stop:
        stop();
        if (basic_callbacks_.on_event) {
            basic_callbacks_.on_event(hal::wifi::BasicEvent::Stopped, false);
        }
        break;
    default:
        if (basic_callbacks_.on_error) {
            basic_callbacks_.on_error();
        }
        running_basic_action_ = hal::wifi::BasicAction::Max;
        return false;
    }

    running_basic_action_ = hal::wifi::BasicAction::Max;
    return true;
}

bool TestWifiBackend::is_action_running(hal::wifi::BasicAction action)
{
    return running_basic_action_ == action;
}

bool TestWifiBackend::is_event_ready(hal::wifi::BasicEvent event)
{
    switch (event) {
    case hal::wifi::BasicEvent::Inited:
        return initialized_;
    case hal::wifi::BasicEvent::Deinited:
        return !initialized_;
    case hal::wifi::BasicEvent::Started:
        return started_;
    case hal::wifi::BasicEvent::Stopped:
        return !started_;
    default:
        return false;
    }
}

bool TestWifiBackend::do_station_action(hal::wifi::StationAction action, bool)
{
    running_station_action_ = action;
    if (station_callbacks_.on_action) {
        station_callbacks_.on_action(action);
    }

    switch (action) {
    case hal::wifi::StationAction::Connect:
        if (!target_ap_.is_valid()) {
            if (station_callbacks_.on_error) {
                station_callbacks_.on_error();
            }
            running_station_action_ = hal::wifi::StationAction::Max;
            return false;
        }
        connected_ = true;
        last_ap_ = target_ap_;
        if (station_callbacks_.on_last_connected_ap_info_updated) {
            station_callbacks_.on_last_connected_ap_info_updated(last_ap_);
        }
        connected_aps_.remove_if([this](const hal::wifi::ConnectApInfo & info) {
            return info.ssid == last_ap_.ssid;
        });
        connected_aps_.push_back(last_ap_);
        if (station_callbacks_.on_connected_ap_infos_updated) {
            station_callbacks_.on_connected_ap_infos_updated(connected_aps_);
        }
        if (station_callbacks_.on_event) {
            station_callbacks_.on_event(hal::wifi::StationEvent::Connected, false);
        }
        break;
    case hal::wifi::StationAction::Disconnect:
        connected_ = false;
        if (station_callbacks_.on_event) {
            station_callbacks_.on_event(hal::wifi::StationEvent::Disconnected, false);
        }
        break;
    default:
        if (station_callbacks_.on_error) {
            station_callbacks_.on_error();
        }
        running_station_action_ = hal::wifi::StationAction::Max;
        return false;
    }

    running_station_action_ = hal::wifi::StationAction::Max;
    return true;
}

bool TestWifiBackend::is_station_action_running(hal::wifi::StationAction action)
{
    return running_station_action_ == action;
}

bool TestWifiBackend::is_station_event_ready(hal::wifi::StationEvent event)
{
    switch (event) {
    case hal::wifi::StationEvent::Connected:
        return connected_;
    case hal::wifi::StationEvent::Disconnected:
        return !connected_;
    default:
        return false;
    }
}

void TestWifiBackend::mark_target_connect_ap_info_connectable(bool connectable)
{
    target_ap_.is_connectable = connectable;
}

bool TestWifiBackend::set_target_connect_ap_info(const hal::wifi::ConnectApInfo &ap_info)
{
    target_ap_ = ap_info;
    return true;
}

bool TestWifiBackend::set_last_connected_ap_info(const hal::wifi::ConnectApInfo &ap_info)
{
    last_ap_ = ap_info;
    return true;
}

bool TestWifiBackend::set_connected_ap_infos(const hal::wifi::ConnectApInfoList &ap_infos)
{
    connected_aps_ = ap_infos;
    return true;
}

const hal::wifi::ConnectApInfo &TestWifiBackend::get_target_connect_ap_info() const
{
    return target_ap_;
}

const hal::wifi::ConnectApInfo &TestWifiBackend::get_last_connected_ap_info() const
{
    return last_ap_;
}

const hal::wifi::ConnectApInfoList &TestWifiBackend::get_connected_ap_infos() const
{
    return connected_aps_;
}

bool TestWifiBackend::set_scan_params(const hal::wifi::ScanParams &params)
{
    scan_params_ = params;
    return true;
}

const hal::wifi::ScanParams &TestWifiBackend::get_scan_params() const
{
    return scan_params_;
}

bool TestWifiBackend::start_scan()
{
    scanning_ = true;
    scan_ap_infos_ = make_scan_ap_infos();
    if (station_callbacks_.on_scan_state_changed) {
        station_callbacks_.on_scan_state_changed(true);
    }
    if (station_callbacks_.on_scan_ap_infos_updated) {
        station_callbacks_.on_scan_ap_infos_updated(std::span<const hal::wifi::ScanApInfo>(scan_ap_infos_));
    }
    return true;
}

void TestWifiBackend::stop_scan()
{
    scanning_ = false;
    if (station_callbacks_.on_scan_state_changed) {
        station_callbacks_.on_scan_state_changed(false);
    }
}

bool TestWifiBackend::set_soft_ap_params(const hal::wifi::SoftApParams &params)
{
    softap_params_ = params;
    if (softap_callbacks_.on_params_updated) {
        softap_callbacks_.on_params_updated(softap_params_);
    }
    return true;
}

const hal::wifi::SoftApParams &TestWifiBackend::get_soft_ap_params() const
{
    return softap_params_;
}

bool TestWifiBackend::start_soft_ap()
{
    softap_started_ = true;
    if (softap_callbacks_.on_event) {
        softap_callbacks_.on_event(hal::wifi::SoftApEvent::Started);
    }
    return true;
}

void TestWifiBackend::stop_soft_ap()
{
    softap_started_ = false;
    provision_started_ = false;
    if (softap_callbacks_.on_event) {
        softap_callbacks_.on_event(hal::wifi::SoftApEvent::Stopped);
    }
}

bool TestWifiBackend::start_soft_ap_provision()
{
    softap_started_ = true;
    provision_started_ = true;
    if (softap_callbacks_.on_station_action_requested) {
        softap_callbacks_.on_station_action_requested(hal::wifi::StationAction::Disconnect);
    }
    if (softap_callbacks_.on_event) {
        softap_callbacks_.on_event(hal::wifi::SoftApEvent::Started);
    }
    return true;
}

bool TestWifiBackend::stop_soft_ap_provision()
{
    provision_started_ = false;
    softap_started_ = false;
    if (softap_callbacks_.on_event) {
        softap_callbacks_.on_event(hal::wifi::SoftApEvent::Stopped);
    }
    return true;
}

hal::network::NetworkStatus TestWifiBackend::get_network_status() const
{
    hal::network::NetworkStatus status = {
        .interface_type = hal::network::InterfaceType::WifiStation,
        .link_state = connected_ ? hal::network::LinkState::Up : hal::network::LinkState::Down,
        .ip_state = connected_ ? hal::network::IpState::Ready : hal::network::IpState::None,
        .reachability = connected_ ? hal::network::Reachability::LocalOnly : hal::network::Reachability::Unreachable,
    };
    if (connected_) {
        status.ip_info = hal::network::IpInfo{
            .ip = "192.168.10.2",
            .netmask = "255.255.255.0",
            .gateway = "192.168.10.1",
            .dns = "192.168.10.1",
        };
        status.signal_dbm = -42;
        status.connected_duration_ms = 0;
    }
    return status;
}

bool TestWifiDevice::probe()
{
    return true;
}

std::vector<hal::InterfaceSpec> TestWifiDevice::get_interface_specs() const
{
    return {
        {hal::wifi::BasicIface::NAME, TestWifiBackend::BASIC_NAME},
        {hal::wifi::StationIface::NAME, TestWifiBackend::STATION_NAME},
        {hal::wifi::SoftApIface::NAME, TestWifiBackend::SOFTAP_NAME},
        {hal::network::ConnectivityIface::NAME, TestWifiBackend::CONNECTIVITY_NAME},
    };
}

bool TestWifiBasicIface::configure(RuntimeContext runtime, Callbacks callbacks)
{
    return backend_->configure_basic(std::move(runtime), std::move(callbacks));
}

void TestWifiBasicIface::clear_callbacks()
{
    backend_->clear_basic_callbacks();
}

bool TestWifiBasicIface::init()
{
    return backend_->init();
}

void TestWifiBasicIface::deinit()
{
    backend_->deinit();
}

bool TestWifiBasicIface::start()
{
    return backend_->start();
}

void TestWifiBasicIface::stop()
{
    backend_->stop();
}

void TestWifiBasicIface::reset_data()
{
    backend_->reset_data();
}

bool TestWifiBasicIface::do_action(hal::wifi::BasicAction action, bool is_force)
{
    return backend_->do_action(action, is_force);
}

bool TestWifiBasicIface::is_action_running(hal::wifi::BasicAction action)
{
    return backend_->is_action_running(action);
}

bool TestWifiBasicIface::is_event_ready(hal::wifi::BasicEvent event)
{
    return backend_->is_event_ready(event);
}

bool TestWifiStationIface::configure(Callbacks callbacks)
{
    return backend_->configure_station(std::move(callbacks));
}

void TestWifiStationIface::clear_callbacks()
{
    backend_->clear_station_callbacks();
}

bool TestWifiStationIface::do_action(hal::wifi::StationAction action, bool is_force)
{
    return backend_->do_station_action(action, is_force);
}

bool TestWifiStationIface::is_action_running(hal::wifi::StationAction action)
{
    return backend_->is_station_action_running(action);
}

bool TestWifiStationIface::is_event_ready(hal::wifi::StationEvent event)
{
    return backend_->is_station_event_ready(event);
}

void TestWifiStationIface::mark_target_connect_ap_info_connectable(bool connectable)
{
    backend_->mark_target_connect_ap_info_connectable(connectable);
}

bool TestWifiStationIface::set_target_connect_ap_info(const hal::wifi::ConnectApInfo &ap_info)
{
    return backend_->set_target_connect_ap_info(ap_info);
}

bool TestWifiStationIface::set_last_connected_ap_info(const hal::wifi::ConnectApInfo &ap_info)
{
    return backend_->set_last_connected_ap_info(ap_info);
}

bool TestWifiStationIface::set_connected_ap_infos(const hal::wifi::ConnectApInfoList &ap_infos)
{
    return backend_->set_connected_ap_infos(ap_infos);
}

const hal::wifi::ConnectApInfo &TestWifiStationIface::get_target_connect_ap_info() const
{
    return backend_->get_target_connect_ap_info();
}

const hal::wifi::ConnectApInfo &TestWifiStationIface::get_last_connected_ap_info() const
{
    return backend_->get_last_connected_ap_info();
}

const hal::wifi::ConnectApInfoList &TestWifiStationIface::get_connected_ap_infos() const
{
    return backend_->get_connected_ap_infos();
}

bool TestWifiStationIface::set_scan_params(const hal::wifi::ScanParams &params)
{
    return backend_->set_scan_params(params);
}

const hal::wifi::ScanParams &TestWifiStationIface::get_scan_params() const
{
    return backend_->get_scan_params();
}

bool TestWifiStationIface::start_scan()
{
    return backend_->start_scan();
}

void TestWifiStationIface::stop_scan()
{
    backend_->stop_scan();
}

bool TestWifiSoftApIface::configure(Callbacks callbacks)
{
    return backend_->configure_softap(std::move(callbacks));
}

void TestWifiSoftApIface::clear_callbacks()
{
    backend_->clear_softap_callbacks();
}

bool TestWifiSoftApIface::set_params(const hal::wifi::SoftApParams &params)
{
    return backend_->set_soft_ap_params(params);
}

const hal::wifi::SoftApParams &TestWifiSoftApIface::get_params() const
{
    return backend_->get_soft_ap_params();
}

bool TestWifiSoftApIface::start()
{
    return backend_->start_soft_ap();
}

void TestWifiSoftApIface::stop()
{
    backend_->stop_soft_ap();
}

bool TestWifiSoftApIface::start_provision()
{
    return backend_->start_soft_ap_provision();
}

bool TestWifiSoftApIface::stop_provision()
{
    return backend_->stop_soft_ap_provision();
}

hal::network::NetworkStatus TestWifiConnectivityIface::get_status() const
{
    return backend_->get_network_status();
}

bool TestWifiDevice::on_init()
{
    backend_ = std::make_shared<TestWifiBackend>();
    interfaces_.emplace(TestWifiBackend::BASIC_NAME, std::make_shared<TestWifiBasicIface>(backend_));
    interfaces_.emplace(TestWifiBackend::STATION_NAME, std::make_shared<TestWifiStationIface>(backend_));
    interfaces_.emplace(TestWifiBackend::SOFTAP_NAME, std::make_shared<TestWifiSoftApIface>(backend_));
    interfaces_.emplace(TestWifiBackend::CONNECTIVITY_NAME, std::make_shared<TestWifiConnectivityIface>(backend_));
    return true;
}

void TestWifiDevice::on_deinit()
{
    interfaces_.clear();
    backend_.reset();
}

} // namespace esp_brookesia

BROOKESIA_PLUGIN_REGISTER(hal::Device, TestWifiDevice, std::string(TestWifiDevice::NAME));
