/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <cstring>
#include <string>
#include <string_view>
#include <variant>
#include "unity.h"
#include "brookesia/hal_interface.hpp"
#include "brookesia/hal_adaptor.hpp"

#ifndef BROOKESIA_HAL_ADAPTOR_ENABLE_SYSTEM_DEVICE
#   define BROOKESIA_HAL_ADAPTOR_ENABLE_SYSTEM_DEVICE  (0)
#endif
#ifndef BROOKESIA_HAL_ADAPTOR_ENABLE_NETWORK_DEVICE
#   define BROOKESIA_HAL_ADAPTOR_ENABLE_NETWORK_DEVICE  (0)
#endif
#ifndef BROOKESIA_HAL_ADAPTOR_SYSTEM_ENABLE_BOARD_INFO_IMPL
#   define BROOKESIA_HAL_ADAPTOR_SYSTEM_ENABLE_BOARD_INFO_IMPL  (0)
#endif
#ifndef BROOKESIA_HAL_ADAPTOR_NETWORK_ENABLE_SNTP_CLIENT_IMPL
#   define BROOKESIA_HAL_ADAPTOR_NETWORK_ENABLE_SNTP_CLIENT_IMPL  (0)
#endif
#ifndef BROOKESIA_HAL_ADAPTOR_NETWORK_ENABLE_HTTP_CLIENT_IMPL
#   define BROOKESIA_HAL_ADAPTOR_NETWORK_ENABLE_HTTP_CLIENT_IMPL  (0)
#endif
#ifndef BROOKESIA_HAL_ADAPTOR_SYSTEM_ENABLE_OTA_UPDATER_IMPL
#   define BROOKESIA_HAL_ADAPTOR_SYSTEM_ENABLE_OTA_UPDATER_IMPL  (0)
#endif
#ifndef BROOKESIA_HAL_ADAPTOR_AUDIO_ENABLE_CODEC_PLAYER_IMPL
#   define BROOKESIA_HAL_ADAPTOR_AUDIO_ENABLE_CODEC_PLAYER_IMPL  (0)
#endif
#ifndef BROOKESIA_HAL_ADAPTOR_AUDIO_ENABLE_CODEC_RECORDER_IMPL
#   define BROOKESIA_HAL_ADAPTOR_AUDIO_ENABLE_CODEC_RECORDER_IMPL  (0)
#endif
#ifndef BROOKESIA_HAL_ADAPTOR_DISPLAY_ENABLE_LEDC_BACKLIGHT_IMPL
#   define BROOKESIA_HAL_ADAPTOR_DISPLAY_ENABLE_LEDC_BACKLIGHT_IMPL  (0)
#endif
#ifndef BROOKESIA_HAL_ADAPTOR_DISPLAY_ENABLE_LCD_PANEL_IMPL
#   define BROOKESIA_HAL_ADAPTOR_DISPLAY_ENABLE_LCD_PANEL_IMPL  (0)
#endif
#ifndef BROOKESIA_HAL_ADAPTOR_DISPLAY_ENABLE_LCD_TOUCH_IMPL
#   define BROOKESIA_HAL_ADAPTOR_DISPLAY_ENABLE_LCD_TOUCH_IMPL  (0)
#endif
#ifndef BROOKESIA_HAL_ADAPTOR_STORAGE_ENABLE_FILE_SYSTEM_IMPL
#   define BROOKESIA_HAL_ADAPTOR_STORAGE_ENABLE_FILE_SYSTEM_IMPL  (0)
#endif
#ifndef BROOKESIA_HAL_ADAPTOR_STORAGE_ENABLE_KEY_VALUE_IMPL
#   define BROOKESIA_HAL_ADAPTOR_STORAGE_ENABLE_KEY_VALUE_IMPL  (0)
#endif
#ifndef BROOKESIA_HAL_ADAPTOR_POWER_ENABLE_BATTERY
#   define BROOKESIA_HAL_ADAPTOR_POWER_ENABLE_BATTERY  (0)
#endif
#ifndef BROOKESIA_HAL_ADAPTOR_ENABLE_VIDEO_DEVICE
#   define BROOKESIA_HAL_ADAPTOR_ENABLE_VIDEO_DEVICE  (0)
#endif
#ifndef BROOKESIA_HAL_ADAPTOR_VIDEO_ENABLE_CAMERA_IMPL
#   define BROOKESIA_HAL_ADAPTOR_VIDEO_ENABLE_CAMERA_IMPL  (0)
#endif

extern int memory_leak_threshold;

using namespace esp_brookesia;

namespace {

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

TEST_CASE("HAL adaptor: device info exposes enabled provider declarations", "[hal][adaptor]")
{
    const auto infos = hal::get_device_infos();

#if BROOKESIA_HAL_ADAPTOR_ENABLE_SYSTEM_DEVICE
    TEST_ASSERT_TRUE(has_device_info(infos, hal::SystemDevice::DEVICE_NAME));
#endif
#if BROOKESIA_HAL_ADAPTOR_ENABLE_NETWORK_DEVICE
    TEST_ASSERT_TRUE(has_device_info(infos, hal::NetworkDevice::DEVICE_NAME));
#endif
#if BROOKESIA_HAL_ADAPTOR_SYSTEM_ENABLE_BOARD_INFO_IMPL
    TEST_ASSERT_TRUE(has_interface_info(
                         infos, hal::SystemDevice::DEVICE_NAME, hal::system::BoardInfoIface::NAME,
                         hal::SystemDevice::BOARD_INFO_IFACE_NAME
                     ));
#endif
#if BROOKESIA_HAL_ADAPTOR_NETWORK_ENABLE_SNTP_CLIENT_IMPL
    TEST_ASSERT_TRUE(has_interface_info(
                         infos, hal::NetworkDevice::DEVICE_NAME, hal::network::SntpClientIface::NAME,
                         hal::NetworkDevice::SNTP_CLIENT_IFACE_NAME
                     ));
#endif
#if BROOKESIA_HAL_ADAPTOR_NETWORK_ENABLE_HTTP_CLIENT_IMPL
    TEST_ASSERT_TRUE(has_interface_info(
                         infos, hal::NetworkDevice::DEVICE_NAME, hal::network::HttpClientIface::NAME,
                         hal::NetworkDevice::HTTP_CLIENT_IFACE_NAME
                     ));
#endif
#if BROOKESIA_HAL_ADAPTOR_SYSTEM_ENABLE_OTA_UPDATER_IMPL
    TEST_ASSERT_TRUE(has_interface_info(
                         infos, hal::SystemDevice::DEVICE_NAME, hal::system::OtaUpdaterIface::NAME,
                         hal::SystemDevice::OTA_UPDATER_IFACE_NAME
                     ));
#endif
#if BROOKESIA_HAL_ADAPTOR_ENABLE_WIFI_DEVICE
    TEST_ASSERT_TRUE(has_interface_info(
                         infos, hal::WifiDevice::DEVICE_NAME, hal::wifi::BasicIface::NAME, hal::WifiDevice::BASIC_IFACE_NAME
                     ));
    TEST_ASSERT_TRUE(has_interface_info(
                         infos, hal::WifiDevice::DEVICE_NAME, hal::wifi::StationIface::NAME, hal::WifiDevice::STATION_IFACE_NAME
                     ));
    TEST_ASSERT_TRUE(has_interface_info(
                         infos, hal::WifiDevice::DEVICE_NAME, hal::wifi::SoftApIface::NAME, hal::WifiDevice::SOFTAP_IFACE_NAME
                     ));
    TEST_ASSERT_TRUE(has_interface_info(
                         infos, hal::WifiDevice::DEVICE_NAME, hal::network::ConnectivityIface::NAME,
                         hal::WifiDevice::CONNECTIVITY_IFACE_NAME
                     ));
#endif
}

TEST_CASE("HAL adaptor: acquire system and network interfaces", "[hal][adaptor]")
{
#if BROOKESIA_HAL_ADAPTOR_SYSTEM_ENABLE_BOARD_INFO_IMPL
    auto board_info = hal::acquire_interface<hal::system::BoardInfoIface>(hal::SystemDevice::BOARD_INFO_IFACE_NAME);
    TEST_ASSERT_TRUE(static_cast<bool>(board_info));
    TEST_ASSERT_TRUE(board_info->get_info().is_valid());
#else
    TEST_IGNORE_MESSAGE("Board info implementation disabled");
#endif

#if BROOKESIA_HAL_ADAPTOR_NETWORK_ENABLE_SNTP_CLIENT_IMPL
    auto sntp = hal::acquire_interface<hal::network::SntpClientIface>(hal::NetworkDevice::SNTP_CLIENT_IFACE_NAME);
    TEST_ASSERT_TRUE(static_cast<bool>(sntp));
#endif

#if BROOKESIA_HAL_ADAPTOR_NETWORK_ENABLE_HTTP_CLIENT_IMPL
    auto http = hal::acquire_interface<hal::network::HttpClientIface>(hal::NetworkDevice::HTTP_CLIENT_IFACE_NAME);
    TEST_ASSERT_TRUE(static_cast<bool>(http));
    TEST_ASSERT_TRUE(static_cast<bool>(http->create_transaction()));
#endif

#if BROOKESIA_HAL_ADAPTOR_SYSTEM_ENABLE_OTA_UPDATER_IMPL
    auto system_ota =
        hal::acquire_interface<hal::system::OtaUpdaterIface>(hal::SystemDevice::OTA_UPDATER_IFACE_NAME);
    TEST_ASSERT_TRUE(static_cast<bool>(system_ota));
    TEST_ASSERT_FALSE(system_ota->get_running_firmware_version().empty());
#endif
}

TEST_CASE("HAL adaptor: acquire display interfaces", "[hal][adaptor]")
{
#if BROOKESIA_HAL_ADAPTOR_DISPLAY_ENABLE_LCD_PANEL_IMPL
    auto panel = hal::acquire_interface<hal::display::PanelIface>(hal::DisplayDevice::LCD_PANEL_IMPL_NAME);
    TEST_ASSERT_TRUE(static_cast<bool>(panel));
    TEST_ASSERT_GREATER_THAN_UINT32(0, panel->get_info().h_res);
    TEST_ASSERT_GREATER_THAN_UINT32(0, panel->get_info().v_res);
#endif

#if BROOKESIA_HAL_ADAPTOR_DISPLAY_ENABLE_LCD_TOUCH_IMPL
    auto touch = hal::acquire_interface<hal::display::TouchIface>(hal::DisplayDevice::LCD_TOUCH_IMPL_NAME);
    TEST_ASSERT_TRUE(static_cast<bool>(touch));
#endif

#if BROOKESIA_HAL_ADAPTOR_DISPLAY_ENABLE_LEDC_BACKLIGHT_IMPL
    auto backlight = hal::acquire_interface<hal::display::BacklightIface>(hal::DisplayDevice::LEDC_BACKLIGHT_IMPL_NAME);
    TEST_ASSERT_TRUE(static_cast<bool>(backlight));
#endif

#if !BROOKESIA_HAL_ADAPTOR_ENABLE_DISPLAY_DEVICE
    TEST_IGNORE_MESSAGE("Display device disabled");
#endif
}

TEST_CASE("HAL adaptor: acquire storage interfaces", "[hal][adaptor]")
{
#if BROOKESIA_HAL_ADAPTOR_STORAGE_ENABLE_FILE_SYSTEM_IMPL
    auto fs = hal::acquire_interface<hal::storage::FileSystemIface>(hal::StorageDevice::FILE_SYSTEM_IFACE_NAME);
    TEST_ASSERT_TRUE(static_cast<bool>(fs));
    TEST_ASSERT_FALSE(fs->get_all_info().empty());
#endif

#if BROOKESIA_HAL_ADAPTOR_STORAGE_ENABLE_KEY_VALUE_IMPL
    auto kv = hal::acquire_interface<hal::storage::KeyValueIface>(hal::StorageDevice::KEY_VALUE_IFACE_NAME);
    TEST_ASSERT_TRUE(static_cast<bool>(kv));
    TEST_ASSERT_TRUE(kv->init());
    hal::storage::KeyValueIface::KeyValueMap values = {{"adaptor-key", std::string("value")}};
    TEST_ASSERT_TRUE(kv->set("adaptor-test", values));
    hal::storage::KeyValueIface::KeyValueMap read_values;
    TEST_ASSERT_TRUE(kv->get("adaptor-test", {"adaptor-key"}, read_values));
    TEST_ASSERT_EQUAL_STRING("value", std::get<std::string>(read_values.at("adaptor-key")).c_str());
    TEST_ASSERT_TRUE(kv->erase("adaptor-test", {}));
    kv->deinit();
#endif

#if !BROOKESIA_HAL_ADAPTOR_ENABLE_STORAGE_DEVICE
    TEST_IGNORE_MESSAGE("Storage device disabled");
#endif
}

TEST_CASE("HAL adaptor: acquire audio and power interfaces", "[hal][adaptor]")
{
#if BROOKESIA_HAL_ADAPTOR_AUDIO_ENABLE_CODEC_PLAYER_IMPL
    auto player = hal::acquire_interface<hal::audio::CodecPlayerIface>(hal::AudioDevice::CODEC_PLAYER_IMPL_NAME);
    TEST_ASSERT_TRUE(static_cast<bool>(player));
#endif

#if BROOKESIA_HAL_ADAPTOR_AUDIO_ENABLE_CODEC_RECORDER_IMPL
    auto recorder = hal::acquire_interface<hal::audio::CodecRecorderIface>(hal::AudioDevice::CODEC_RECORDER_IMPL_NAME);
    TEST_ASSERT_TRUE(static_cast<bool>(recorder));
    TEST_ASSERT_GREATER_THAN_UINT8(0, recorder->get_info().bits);
    TEST_ASSERT_GREATER_THAN_UINT8(0, recorder->get_info().channels);
    TEST_ASSERT_GREATER_THAN_UINT32(0, recorder->get_info().sample_rate);
#endif

#if BROOKESIA_HAL_ADAPTOR_POWER_ENABLE_BATTERY
    auto battery = hal::acquire_interface<hal::power::BatteryIface>(hal::PowerDevice::BATTERY_IMPL_NAME);
    TEST_ASSERT_TRUE(static_cast<bool>(battery));
    TEST_ASSERT_TRUE(battery->get_info().is_valid());
#endif
}

TEST_CASE("HAL adaptor: acquire video interfaces", "[hal][adaptor]")
{
#if BROOKESIA_HAL_ADAPTOR_VIDEO_ENABLE_CAMERA_IMPL
    const auto infos = hal::get_device_infos();
    TEST_ASSERT_TRUE(has_interface_info(
                         infos, hal::VideoDevice::DEVICE_NAME, hal::video::CameraIface::NAME,
                         hal::VideoDevice::get_camera_iface_name(0)
                     ));

    auto camera = hal::acquire_interface<hal::video::CameraIface>(hal::VideoDevice::get_camera_iface_name(0));
    TEST_ASSERT_TRUE(static_cast<bool>(camera));
    auto device_infos = camera->get_device_infos();
    for (const auto &info : device_infos) {
        TEST_ASSERT_FALSE(info.device_path.empty());
    }
#else
    TEST_IGNORE_MESSAGE("Video camera implementation disabled");
#endif
}

TEST_CASE("HAL adaptor: acquire WiFi interfaces", "[hal][adaptor]")
{
#if BROOKESIA_HAL_ADAPTOR_ENABLE_WIFI_DEVICE
    auto basic = hal::acquire_interface<hal::wifi::BasicIface>(hal::WifiDevice::BASIC_IFACE_NAME);
    auto station = hal::acquire_interface<hal::wifi::StationIface>(hal::WifiDevice::STATION_IFACE_NAME);
    auto softap = hal::acquire_interface<hal::wifi::SoftApIface>(hal::WifiDevice::SOFTAP_IFACE_NAME);
    auto connectivity = hal::acquire_interface<hal::network::ConnectivityIface>(hal::WifiDevice::CONNECTIVITY_IFACE_NAME);
    TEST_ASSERT_TRUE(static_cast<bool>(basic));
    TEST_ASSERT_TRUE(static_cast<bool>(station));
    TEST_ASSERT_TRUE(static_cast<bool>(softap));
    TEST_ASSERT_TRUE(static_cast<bool>(connectivity));
#else
    TEST_IGNORE_MESSAGE("WiFi device disabled");
#endif
}
