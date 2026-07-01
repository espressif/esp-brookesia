/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <array>
#include <string_view>
#include "unity.h"
#include "common.hpp"
#include "test_audio_player.hpp"
#include "test_audio_recorder.hpp"
#include "test_power_battery.hpp"
#include "test_general_board_info.hpp"
#include "test_display_panel.hpp"
#include "test_display_touch.hpp"
#include "test_display_backlight.hpp"
#include "test_storage_fs.hpp"
#include "test_storage_kv.hpp"
#include "test_protocol_sntp.hpp"
#include "test_wifi_common.hpp"

using namespace esp_brookesia;

namespace {

struct ExpectedInterface {
    std::string_view device_name;
    std::string_view type_name;
    std::string_view instance_name;
};

constexpr std::array EXPECTED_INTERFACES = {
    ExpectedInterface{TestAudioPlayerDevice::NAME, hal::audio::CodecPlayerIface::NAME, TestAudioCodecPlayerIface::NAME},
    ExpectedInterface{
        TestAudioRecorderDevice::NAME, hal::audio::CodecRecorderIface::NAME, TestAudioCodecRecorderIface::NAME
    },
    ExpectedInterface{TestBatteryDevice::NAME, hal::power::BatteryIface::NAME, TestPowerBatteryIface::NAME},
    ExpectedInterface{TestBoardInfoDevice::NAME, hal::system::BoardInfoIface::NAME, TestGeneralBoardInfoIface::NAME},
    ExpectedInterface{TestDisplayPanelDevice::NAME, hal::display::PanelIface::NAME, TestDisplayPanelIface::NAME},
    ExpectedInterface{TestDisplayTouchDevice::NAME, hal::display::TouchIface::NAME, TestDisplayTouchIface::NAME},
    ExpectedInterface{
        TestDisplayBacklightDevice::NAME, hal::display::BacklightIface::NAME, TestDisplayBacklightIface::NAME
    },
    ExpectedInterface{TestStorageFsDevice::NAME, hal::storage::FileSystemIface::NAME, TestStorageFsIface::NAME},
    ExpectedInterface{TestStorageKvDevice::NAME, hal::storage::KeyValueIface::NAME, TestStorageKvIface::NAME},
    ExpectedInterface{TestGeneralSntpDevice::NAME, hal::network::SntpClientIface::NAME, TestSntpClientIface::NAME},
    ExpectedInterface{TestWifiDevice::NAME, hal::wifi::BasicIface::NAME, TestWifiBackend::BASIC_NAME},
    ExpectedInterface{TestWifiDevice::NAME, hal::wifi::StationIface::NAME, TestWifiBackend::STATION_NAME},
    ExpectedInterface{TestWifiDevice::NAME, hal::wifi::SoftApIface::NAME, TestWifiBackend::SOFTAP_NAME},
    ExpectedInterface{TestWifiDevice::NAME, hal::network::ConnectivityIface::NAME, TestWifiBackend::CONNECTIVITY_NAME},
};

bool has_device_info(const hal::DeviceInfoList &infos, std::string_view device_name)
{
    for (const auto &device_info : infos) {
        if (device_info.name == device_name) {
            return true;
        }
    }
    return false;
}

bool has_interface_info(
    const hal::DeviceInfoList &infos, std::string_view device_name,
    std::string_view type_name, std::string_view instance_name
)
{
    for (const auto &device_info : infos) {
        if (device_info.name != device_name) {
            continue;
        }
        for (const auto &interface_info : device_info.interfaces) {
            if ((interface_info.type_name == type_name) && (interface_info.instance_name == instance_name)) {
                return true;
            }
        }
    }
    return false;
}

} // namespace

TEST_CASE("DeviceInfo: lists probed devices and declared interfaces", "[hal][device]")
{
    const auto infos = hal::get_device_infos();
    TEST_ASSERT_GREATER_OR_EQUAL(EXPECTED_INTERFACES.size(), infos.size());

    for (const auto &expected : EXPECTED_INTERFACES) {
        TEST_ASSERT_TRUE_MESSAGE(has_device_info(infos, expected.device_name), expected.device_name.data());
        TEST_ASSERT_TRUE_MESSAGE(
            has_interface_info(infos, expected.device_name, expected.type_name, expected.instance_name),
            expected.instance_name.data()
        );
    }
}

TEST_CASE("Interface acquire: discovers interfaces before explicit provider startup", "[hal][interface]")
{
    for (const auto &expected : EXPECTED_INTERFACES) {
        TEST_ASSERT_TRUE_MESSAGE(hal::has_interface(expected.type_name), expected.type_name.data());
    }
    TEST_ASSERT_FALSE(hal::has_interface("MissingInterface"));

    auto missing = hal::acquire_interface<hal::system::BoardInfoIface>("missing:BoardInfo");
    TEST_ASSERT_FALSE(static_cast<bool>(missing));
}

TEST_CASE("Interface acquire: owns provider lifetime with move-only handles", "[hal][interface]")
{
    auto first = hal::acquire_first_interface<hal::system::BoardInfoIface>();
    TEST_ASSERT_TRUE(static_cast<bool>(first));
    TEST_ASSERT_EQUAL_STRING(TestGeneralBoardInfoIface::NAME, std::string(first.instance_name()).c_str());

    auto second = hal::acquire_interface<hal::system::BoardInfoIface>(TestGeneralBoardInfoIface::NAME);
    TEST_ASSERT_TRUE(static_cast<bool>(second));
    first.reset();

    auto iface = second.get();
    TEST_ASSERT_NOT_NULL(iface.get());
    TEST_ASSERT_TRUE(iface->get_info().is_valid());
    second.reset();

    auto reacquired = hal::acquire_first_interface<hal::system::BoardInfoIface>();
    TEST_ASSERT_TRUE(static_cast<bool>(reacquired));
}

TEST_CASE("Interface acquire: returns all matching instances", "[hal][interface]")
{
    auto basic_handles = hal::acquire_interfaces<hal::wifi::BasicIface>();
    auto station_handles = hal::acquire_interfaces<hal::wifi::StationIface>();
    auto softap_handles = hal::acquire_interfaces<hal::wifi::SoftApIface>();

    TEST_ASSERT_GREATER_OR_EQUAL(1, basic_handles.size());
    TEST_ASSERT_GREATER_OR_EQUAL(1, station_handles.size());
    TEST_ASSERT_GREATER_OR_EQUAL(1, softap_handles.size());
    TEST_ASSERT_EQUAL_STRING(TestWifiBackend::BASIC_NAME, std::string(basic_handles.front().instance_name()).c_str());
    TEST_ASSERT_EQUAL_STRING(TestWifiBackend::STATION_NAME, std::string(station_handles.front().instance_name()).c_str());
    TEST_ASSERT_EQUAL_STRING(TestWifiBackend::SOFTAP_NAME, std::string(softap_handles.front().instance_name()).c_str());
}
