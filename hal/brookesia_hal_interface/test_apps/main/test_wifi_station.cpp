/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <memory>

#include "unity.h"
#include "common.hpp"
#include "brookesia/lib_utils/task_scheduler.hpp"
#include "test_wifi_common.hpp"

using namespace esp_brookesia;

TEST_CASE("wifi::StationIface: acquire station interface", "[hal][device]")
{

    auto iface_handle = hal::acquire_first_interface<hal::wifi::StationIface>();
    auto name = std::string(iface_handle.instance_name());
    auto iface = iface_handle.get();
    TEST_ASSERT_EQUAL_STRING(TestWifiBackend::STATION_NAME, name.c_str());
    TEST_ASSERT_NOT_NULL(iface.get());
}

TEST_CASE("network::ConnectivityIface: acquire Wi-Fi connectivity interface", "[hal][device]")
{
    auto iface_handle = hal::acquire_first_interface<hal::network::ConnectivityIface>();
    auto name = std::string(iface_handle.instance_name());
    auto iface = iface_handle.get();
    TEST_ASSERT_EQUAL_STRING(TestWifiBackend::CONNECTIVITY_NAME, name.c_str());
    TEST_ASSERT_NOT_NULL(iface.get());
    TEST_ASSERT_FALSE(iface->is_network_ready());
    TEST_ASSERT_EQUAL(hal::network::ConnectivityState::Offline, iface->get_status().state());
}

TEST_CASE("wifi::StationIface: connect scan params and callbacks", "[hal][interface]")
{

    auto basic_iface_handle = hal::acquire_first_interface<hal::wifi::BasicIface>();
    auto basic_name = std::string(basic_iface_handle.instance_name());
    auto basic_iface = basic_iface_handle.get();
    auto station_iface_handle = hal::acquire_first_interface<hal::wifi::StationIface>();
    auto station_name = std::string(station_iface_handle.instance_name());
    auto station_iface = station_iface_handle.get();
    auto connectivity_iface_handle = hal::acquire_first_interface<hal::network::ConnectivityIface>();
    auto connectivity_iface = connectivity_iface_handle.get();
    TEST_ASSERT_NOT_NULL(basic_iface.get());
    TEST_ASSERT_NOT_NULL(station_iface.get());
    TEST_ASSERT_NOT_NULL(connectivity_iface.get());
    TEST_ASSERT_FALSE(basic_name.empty());
    TEST_ASSERT_FALSE(station_name.empty());

    auto scheduler = std::make_shared<lib_utils::TaskScheduler>();
    TEST_ASSERT_TRUE(basic_iface->configure(
    hal::wifi::BasicIface::RuntimeContext{
        .task_scheduler = scheduler,
        .task_group = "wifi-test",
    },
    {}
                     ));
    TEST_ASSERT_TRUE(basic_iface->init());
    TEST_ASSERT_TRUE(basic_iface->start());

    hal::wifi::StationAction last_action = hal::wifi::StationAction::Max;
    hal::wifi::StationEvent last_event = hal::wifi::StationEvent::Max;
    bool scan_state = false;
    size_t scan_ap_count = 0;
    hal::wifi::ConnectApInfo last_connected_ap{};
    hal::wifi::ConnectApInfoList connected_aps{};
    int error_count = 0;

    TEST_ASSERT_TRUE(station_iface->configure({
        .on_action = [&](hal::wifi::StationAction action)
        {
            last_action = action;
        },
        .on_event = [&](hal::wifi::StationEvent event, bool)
        {
            last_event = event;
        },
        .on_error = [&]()
        {
            error_count++;
        },
        .on_action_requested = nullptr,
        .on_scan_state_changed = [&](bool is_scanning)
        {
            scan_state = is_scanning;
        },
        .on_scan_ap_infos_updated = [&](std::span<const hal::wifi::ScanApInfo> ap_infos)
        {
            scan_ap_count = ap_infos.size();
        },
        .on_last_connected_ap_info_updated = [&](const hal::wifi::ConnectApInfo & ap_info)
        {
            last_connected_ap = ap_info;
        },
        .on_connected_ap_infos_updated = [&](const hal::wifi::ConnectApInfoList & ap_infos)
        {
            connected_aps = ap_infos;
        },
    }));

    hal::wifi::ConnectApInfo target_ap("TestSSID", "TestPassword");
    TEST_ASSERT_TRUE(station_iface->set_target_connect_ap_info(target_ap));
    TEST_ASSERT_EQUAL_STRING(target_ap.ssid.c_str(), station_iface->get_target_connect_ap_info().ssid.c_str());

    hal::wifi::ConnectApInfo last_ap("PrevSSID", "PrevPassword");
    TEST_ASSERT_TRUE(station_iface->set_last_connected_ap_info(last_ap));
    TEST_ASSERT_EQUAL_STRING(last_ap.ssid.c_str(), station_iface->get_last_connected_ap_info().ssid.c_str());

    hal::wifi::ConnectApInfoList ap_list = {
        hal::wifi::ConnectApInfo("AP1", "PW1"),
        hal::wifi::ConnectApInfo("AP2", "PW2"),
    };
    TEST_ASSERT_TRUE(station_iface->set_connected_ap_infos(ap_list));
    TEST_ASSERT_EQUAL(2, station_iface->get_connected_ap_infos().size());

    station_iface->mark_target_connect_ap_info_connectable(false);
    TEST_ASSERT_FALSE(station_iface->get_target_connect_ap_info().is_connectable);

    hal::wifi::ScanParams scan_params{
        .ap_count = 4,
        .interval_ms = 1000,
        .timeout_ms = 2000,
    };
    TEST_ASSERT_TRUE(station_iface->set_scan_params(scan_params));
    TEST_ASSERT_EQUAL(scan_params.ap_count, station_iface->get_scan_params().ap_count);
    TEST_ASSERT_EQUAL(scan_params.interval_ms, station_iface->get_scan_params().interval_ms);
    TEST_ASSERT_EQUAL(scan_params.timeout_ms, station_iface->get_scan_params().timeout_ms);

    TEST_ASSERT_TRUE(station_iface->start_scan());
    TEST_ASSERT_TRUE(scan_state);
    TEST_ASSERT_EQUAL(2, scan_ap_count);
    station_iface->stop_scan();
    TEST_ASSERT_FALSE(scan_state);

    TEST_ASSERT_TRUE(station_iface->do_action(hal::wifi::StationAction::Connect));
    TEST_ASSERT_EQUAL(hal::wifi::StationAction::Connect, last_action);
    TEST_ASSERT_EQUAL(hal::wifi::StationEvent::Connected, last_event);
    TEST_ASSERT_TRUE(station_iface->is_event_ready(hal::wifi::StationEvent::Connected));
    TEST_ASSERT_TRUE(connectivity_iface->is_network_ready());
    TEST_ASSERT_EQUAL(hal::network::ConnectivityState::LocalNetworkReady, connectivity_iface->get_status().state());
    TEST_ASSERT_EQUAL_STRING("TestSSID", last_connected_ap.ssid.c_str());
    TEST_ASSERT_EQUAL(3, connected_aps.size());

    TEST_ASSERT_TRUE(station_iface->do_action(hal::wifi::StationAction::Disconnect));
    TEST_ASSERT_EQUAL(hal::wifi::StationAction::Disconnect, last_action);
    TEST_ASSERT_EQUAL(hal::wifi::StationEvent::Disconnected, last_event);
    TEST_ASSERT_TRUE(station_iface->is_event_ready(hal::wifi::StationEvent::Disconnected));
    TEST_ASSERT_FALSE(connectivity_iface->is_network_ready());
    TEST_ASSERT_EQUAL(0, error_count);

    station_iface->clear_callbacks();
    basic_iface->deinit();
}
