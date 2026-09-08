/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <string>
#include <string_view>
#include <variant>
#include "unity.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "brookesia/hal_interface.hpp"
#include "brookesia/hal_adaptor.hpp"
#if defined(CONFIG_ESP_BOARD_ESP_MOSAICO_V1_0)
#include "esp_board_device.h"
#include "brookesia/hal_custom/display/device.hpp"
#endif

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
#ifndef BROOKESIA_HAL_ADAPTOR_POWER_BATTERY_IMPL_BQ27220
#   define BROOKESIA_HAL_ADAPTOR_POWER_BATTERY_IMPL_BQ27220  (0)
#endif
#ifndef BROOKESIA_HAL_ADAPTOR_ENABLE_VIDEO_DEVICE
#   define BROOKESIA_HAL_ADAPTOR_ENABLE_VIDEO_DEVICE  (0)
#endif
#ifndef BROOKESIA_HAL_ADAPTOR_ENABLE_BLE_DEVICE
#   define BROOKESIA_HAL_ADAPTOR_ENABLE_BLE_DEVICE  (0)
#endif
#ifndef BROOKESIA_HAL_ADAPTOR_VIDEO_ENABLE_CAMERA_IMPL
#   define BROOKESIA_HAL_ADAPTOR_VIDEO_ENABLE_CAMERA_IMPL  (0)
#endif
#ifndef BROOKESIA_HAL_ADAPTOR_VIDEO_ENABLE_ENCODER_IMPL
#   define BROOKESIA_HAL_ADAPTOR_VIDEO_ENABLE_ENCODER_IMPL  (0)
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

#if defined(CONFIG_ESP_BOARD_ESP_MOSAICO_V1_0) && \
    BROOKESIA_HAL_ADAPTOR_VIDEO_ENABLE_CAMERA_IMPL && BROOKESIA_HAL_ADAPTOR_VIDEO_ENABLE_ENCODER_IMPL
constexpr const char *MOSAICO_CAMERA_DEVICE_PATH = "/dev/video2";
constexpr uint16_t MOSAICO_CAMERA_WIDTH = 640;
constexpr uint16_t MOSAICO_CAMERA_HEIGHT = 480;
constexpr uint8_t MOSAICO_CAMERA_FPS = 7;
constexpr size_t MOSAICO_CAMERA_FRAME_SIZE =
    static_cast<size_t>(MOSAICO_CAMERA_WIDTH) * MOSAICO_CAMERA_HEIGHT * sizeof(uint16_t);
constexpr size_t MOSAICO_CAMERA_HEAP_TOLERANCE = 4096;

hal::video::EncoderConfig make_mosaico_camera_config()
{
    return {
        .sinks = {{
                .format = hal::video::EncoderSinkFormat::RGB565,
                .width = MOSAICO_CAMERA_WIDTH,
                .height = MOSAICO_CAMERA_HEIGHT,
                .fps = MOSAICO_CAMERA_FPS,
            }
        },
        .enable_stream_mode = false,
        .source = hal::video::EncoderSourceConfig{
            .fixed_format = hal::video::EncoderSinkFormat::RGB565,
            .fixed_width = MOSAICO_CAMERA_WIDTH,
            .fixed_height = MOSAICO_CAMERA_HEIGHT,
            .v4l2_buffer_count = 2,
        },
    };
}

bool run_mosaico_camera_cycle(
    hal::video::EncoderIface &encoder, std::string &error_message, size_t &frame_size
)
{
    error_message.clear();
    bool frame_valid = false;
    const auto config = make_mosaico_camera_config();
    auto callback = [&](size_t sink_index, const hal::video::EncoderSinkInfo & sink_info,
    const uint8_t *data, size_t size) {
        frame_size = size;
        frame_valid = (sink_index == 0) &&
                      (sink_info.format == hal::video::EncoderSinkFormat::RGB565) &&
                      (sink_info.width == MOSAICO_CAMERA_WIDTH) &&
                      (sink_info.height == MOSAICO_CAMERA_HEIGHT) &&
                      (data != nullptr) && (size == MOSAICO_CAMERA_FRAME_SIZE);
    };

    if (!encoder.open(config, {}, &error_message)) {
        return false;
    }
    if (!encoder.start(&error_message)) {
        encoder.close();
        return false;
    }
    if (!encoder.fetch_frame(0, callback, &error_message)) {
        encoder.close();
        return false;
    }
    if (!frame_valid) {
        error_message = "Unexpected first RGB565 frame metadata or size: " + std::to_string(frame_size);
        encoder.close();
        return false;
    }
    if (!encoder.stop(&error_message)) {
        encoder.close();
        return false;
    }

    // Verify that frame-mode stop really tears down the lower capture pipeline
    // and that the same opened encoder can start cleanly again.
    frame_valid = false;
    frame_size = 0;
    if (!encoder.start(&error_message) ||
            !encoder.fetch_frame(0, callback, &error_message) ||
            !encoder.stop(&error_message)) {
        encoder.close();
        return false;
    }
    encoder.close();
    if (!frame_valid) {
        error_message = "Unexpected restarted RGB565 frame metadata or size: " + std::to_string(frame_size);
        return false;
    }
    if (encoder.is_opened() || encoder.is_started()) {
        error_message = "Encoder remained open or started after close";
        return false;
    }
    return true;
}

bool is_board_device_deinitialized(const char *device_name)
{
    void *device_handle = nullptr;
    return (esp_board_device_get_handle(device_name, &device_handle) != ESP_OK) &&
           (device_handle == nullptr);
}

bool is_board_device_initialized(const char *device_name)
{
    void *device_handle = nullptr;
    return (esp_board_device_get_handle(device_name, &device_handle) == ESP_OK) &&
           (device_handle != nullptr);
}

size_t memory_loss(size_t before, size_t after)
{
    return (before > after) ? (before - after) : 0;
}
#endif

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
#if BROOKESIA_HAL_ADAPTOR_ENABLE_BLE_DEVICE
    TEST_ASSERT_TRUE(has_interface_info(
                         infos, hal::BleDevice::DEVICE_NAME, hal::bluetooth::ble::PeripheralIface::NAME,
                         hal::BleDevice::PERIPHERAL_IFACE_NAME
                     ));
#   if CONFIG_BT_CONTROLLER_DISABLED
    // Hosted capability probing must be cached. A second discovery must not run a
    // controller init/deinit pair that could interfere with Remote Wi-Fi or BLE acquire.
    const auto repeated_infos = hal::get_device_infos();
    TEST_ASSERT_TRUE(has_interface_info(
                         repeated_infos, hal::BleDevice::DEVICE_NAME, hal::bluetooth::ble::PeripheralIface::NAME,
                         hal::BleDevice::PERIPHERAL_IFACE_NAME
                     ));
#   endif
#endif
}

TEST_CASE("HAL adaptor: BLE peripheral lifecycle is restart-safe", "[hal][adaptor][ble]")
{
#if BROOKESIA_HAL_ADAPTOR_ENABLE_BLE_DEVICE
    constexpr const char *SERVICE_UUID = "7a5a0001-6c8d-4f5a-9c2e-3b9e0b2f4a10";
    constexpr const char *RX_UUID = "7a5a0002-6c8d-4f5a-9c2e-3b9e0b2f4a10";
    constexpr const char *TX_UUID = "7a5a0003-6c8d-4f5a-9c2e-3b9e0b2f4a10";

    auto peripheral = hal::acquire_interface<hal::bluetooth::ble::PeripheralIface>(hal::BleDevice::PERIPHERAL_IFACE_NAME);
    TEST_ASSERT_TRUE(static_cast<bool>(peripheral));

    const hal::bluetooth::ble::PeripheralConfig config = {
        .device_name = "Brookesia-BLE-Test",
        .preferred_mtu = 247,
        .advertised_service_uuids = {SERVICE_UUID},
        .services = {{
                .uuid = SERVICE_UUID,
                .characteristics = {
                    {.uuid = RX_UUID, .write = true},
                    {.uuid = TX_UUID, .notify = true},
                },
            }
        },
    };
    TEST_ASSERT_TRUE(peripheral->configure(config, {}));
    TEST_ASSERT_TRUE(peripheral->init());
    TEST_ASSERT_TRUE(peripheral->start());
    TEST_ASSERT_TRUE(peripheral->start_advertising());
    TEST_ASSERT_TRUE(peripheral->stop_advertising());
    TEST_ASSERT_TRUE(peripheral->stop());
    TEST_ASSERT_TRUE(peripheral->start());
    TEST_ASSERT_TRUE(peripheral->stop());
    TEST_ASSERT_TRUE(peripheral->deinit());
#else
    TEST_IGNORE_MESSAGE("BLE device disabled");
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

#if defined(CONFIG_ESP_BOARD_ESP_MOSAICO_V1_0)
    auto command_backlight = hal::acquire_interface<hal::display::BacklightIface>(
                                 hal::CustomDisplayDevice::DISPLAY_BACKLIGHT_IMPL_NAME
                             );
    TEST_ASSERT_TRUE(static_cast<bool>(command_backlight));
    TEST_ASSERT_TRUE(command_backlight->set_brightness(25));
    uint8_t brightness = 0;
    TEST_ASSERT_TRUE(command_backlight->get_brightness(brightness));
    TEST_ASSERT_EQUAL_UINT8(25, brightness);
    TEST_ASSERT_TRUE(command_backlight->set_brightness(100));
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
    TEST_ASSERT_FALSE(battery->get_info().name.empty());
    TEST_ASSERT_FALSE(battery->get_info().abilities.empty());
#   if BROOKESIA_HAL_ADAPTOR_POWER_BATTERY_IMPL_BQ27220
    TEST_ASSERT_EQUAL_size_t(1, battery->get_info().abilities.size());
    TEST_ASSERT_TRUE(battery->get_info().has_ability(hal::power::BatteryIface::Ability::Voltage));
    hal::power::BatteryIface::State battery_state;
    TEST_ASSERT_TRUE(battery->get_state(battery_state));
    if (battery_state.is_present) {
        TEST_ASSERT_TRUE(battery_state.voltage_mv.has_value());
        printf("BQ27220 battery voltage: %lu mV\n", static_cast<unsigned long>(*battery_state.voltage_mv));
    } else {
        TEST_ASSERT_FALSE(battery_state.voltage_mv.has_value());
        printf("BQ27220 battery is not present\n");
    }
#   endif
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

TEST_CASE("HAL adaptor: Mosaico expansion runtime outlives video", "[hal][adaptor][video][expansion][mosaico]")
{
#if defined(CONFIG_ESP_BOARD_ESP_MOSAICO_V1_0) && \
    BROOKESIA_HAL_ADAPTOR_VIDEO_ENABLE_CAMERA_IMPL && BROOKESIA_HAL_ADAPTOR_VIDEO_ENABLE_ENCODER_IMPL && \
    BROOKESIA_HAL_ADAPTOR_ENABLE_EXPANSION_MODULES
    auto encoder = hal::acquire_interface<hal::video::EncoderIface>(hal::VideoDevice::get_encoder_iface_name(0));
    TEST_ASSERT_TRUE(static_cast<bool>(encoder));

    std::string error_message;
    if (!encoder->open(make_mosaico_camera_config(), {}, &error_message)) {
        encoder->close();
        encoder.reset();
        TEST_IGNORE_MESSAGE("Insert a camera module into the Mosaico left slot and retry");
    }
    TEST_ASSERT_TRUE(is_board_device_initialized("camera"));
    TEST_ASSERT_TRUE(is_board_device_initialized("camera_slot_claim"));
    TEST_ASSERT_TRUE(is_board_device_initialized("expansion_runtime_pin"));
    TEST_ASSERT_TRUE(is_board_device_initialized("expansion_module_manager"));

    auto expansion = hal::acquire_first_interface<hal::expansion::ModuleManagerIface>();
    TEST_ASSERT_TRUE(static_cast<bool>(expansion));

    encoder->close();
    TEST_ASSERT_FALSE(encoder->is_opened());
    TEST_ASSERT_TRUE(is_board_device_deinitialized("camera"));
    TEST_ASSERT_TRUE(is_board_device_deinitialized("camera_slot_claim"));
    TEST_ASSERT_TRUE(is_board_device_initialized("expansion_runtime_pin"));
    TEST_ASSERT_TRUE(is_board_device_initialized("expansion_module_manager"));
    TEST_ASSERT_TRUE(hal::expansion::request_rescan());

    bool left_camera_ready = false;
    for (size_t attempt = 0; attempt < 5; attempt++) {
        vTaskDelay(pdMS_TO_TICKS(BROOKESIA_HAL_ADAPTOR_EXPANSION_SCAN_INTERVAL_MS));
        for (const auto &info : expansion->get_module_infos()) {
            if ((info.provider == "mosaico") && (info.slot == "left") &&
                    (info.type == "camera") && (info.state == hal::expansion::ModuleState::Ready)) {
                left_camera_ready = true;
                break;
            }
        }
        if (left_camera_ready) {
            break;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(left_camera_ready, "Mosaico scanner stopped after Video released its camera dependency");

    expansion.reset();
    TEST_ASSERT_TRUE(is_board_device_deinitialized("expansion_runtime_pin"));
    TEST_ASSERT_TRUE(is_board_device_deinitialized("expansion_module_manager"));
#else
    TEST_IGNORE_MESSAGE("Mosaico video encoder and expansion runtime are not enabled");
#endif
}

TEST_CASE("HAL adaptor: Mosaico camera frame and repeated lifecycle", "[hal][adaptor][video][mosaico]")
{
#if defined(CONFIG_ESP_BOARD_ESP_MOSAICO_V1_0) && \
    BROOKESIA_HAL_ADAPTOR_VIDEO_ENABLE_CAMERA_IMPL && BROOKESIA_HAL_ADAPTOR_VIDEO_ENABLE_ENCODER_IMPL
    memory_leak_threshold = static_cast<int>(MOSAICO_CAMERA_HEAP_TOLERANCE);
    auto camera = hal::acquire_interface<hal::video::CameraIface>(hal::VideoDevice::get_camera_iface_name(0));
    auto encoder = hal::acquire_interface<hal::video::EncoderIface>(hal::VideoDevice::get_encoder_iface_name(0));
    TEST_ASSERT_TRUE(static_cast<bool>(camera));
    TEST_ASSERT_TRUE(static_cast<bool>(encoder));

    auto device_infos = camera->get_device_infos();
    const auto camera_info = std::find_if(device_infos.begin(), device_infos.end(), [](const auto & info) {
        return info.device_path == MOSAICO_CAMERA_DEVICE_PATH;
    });
    if (camera_info == device_infos.end()) {
        device_infos.clear();
        device_infos.shrink_to_fit();
        camera.reset();
        encoder.reset();
        TEST_IGNORE_MESSAGE("Insert a camera module into the Mosaico left slot and retry");
    }
    TEST_ASSERT_TRUE(std::find(
                         camera_info->supported_formats.begin(), camera_info->supported_formats.end(),
                         hal::video::EncoderSinkFormat::RGB565
                     ) != camera_info->supported_formats.end());

    std::string error_message;
    size_t frame_size = 0;
    for (size_t i = 0; i < 3; i++) {
        TEST_ASSERT_TRUE_MESSAGE(
            run_mosaico_camera_cycle(*encoder, error_message, frame_size), error_message.c_str()
        );
    }

    vTaskDelay(pdMS_TO_TICKS(50));
    const size_t internal_before = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    const size_t external_before = heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    const UBaseType_t tasks_before = uxTaskGetNumberOfTasks();

    for (size_t i = 0; i < 25; i++) {
        TEST_ASSERT_TRUE_MESSAGE(
            run_mosaico_camera_cycle(*encoder, error_message, frame_size), error_message.c_str()
        );
    }
    vTaskDelay(pdMS_TO_TICKS(50));
    const size_t internal_middle = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    const size_t external_middle = heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    const UBaseType_t tasks_middle = uxTaskGetNumberOfTasks();

    for (size_t i = 0; i < 25; i++) {
        TEST_ASSERT_TRUE_MESSAGE(
            run_mosaico_camera_cycle(*encoder, error_message, frame_size), error_message.c_str()
        );
    }
    vTaskDelay(pdMS_TO_TICKS(50));
    const size_t internal_after = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    const size_t external_after = heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    const UBaseType_t tasks_after = uxTaskGetNumberOfTasks();

    printf(
        "Mosaico camera: frame=%u bytes, internal loss=%u/%u, external loss=%u/%u, tasks=%u/%u/%u\n",
        static_cast<unsigned>(frame_size),
        static_cast<unsigned>(memory_loss(internal_before, internal_middle)),
        static_cast<unsigned>(memory_loss(internal_middle, internal_after)),
        static_cast<unsigned>(memory_loss(external_before, external_middle)),
        static_cast<unsigned>(memory_loss(external_middle, external_after)),
        static_cast<unsigned>(tasks_before), static_cast<unsigned>(tasks_middle),
        static_cast<unsigned>(tasks_after)
    );
    TEST_ASSERT_EQUAL_size_t(MOSAICO_CAMERA_FRAME_SIZE, frame_size);
    TEST_ASSERT_TRUE(memory_loss(internal_before, internal_after) <= MOSAICO_CAMERA_HEAP_TOLERANCE);
    TEST_ASSERT_TRUE(memory_loss(internal_middle, internal_after) <= MOSAICO_CAMERA_HEAP_TOLERANCE);
    TEST_ASSERT_TRUE(memory_loss(external_before, external_after) <= MOSAICO_CAMERA_HEAP_TOLERANCE);
    TEST_ASSERT_TRUE(memory_loss(external_middle, external_after) <= MOSAICO_CAMERA_HEAP_TOLERANCE);
    TEST_ASSERT_EQUAL_UINT32(tasks_before, tasks_middle);
    TEST_ASSERT_EQUAL_UINT32(tasks_middle, tasks_after);

    TEST_ASSERT_TRUE(is_board_device_deinitialized("camera"));
    TEST_ASSERT_TRUE(is_board_device_deinitialized("camera_slot_claim"));
    TEST_ASSERT_TRUE(is_board_device_deinitialized("expansion_runtime_pin"));
    TEST_ASSERT_TRUE(is_board_device_deinitialized("expansion_module_manager"));
    TEST_ASSERT_EQUAL(ESP_OK, esp_board_device_show("camera"));
    TEST_ASSERT_EQUAL(ESP_OK, esp_board_device_show("camera_slot_claim"));
    TEST_ASSERT_EQUAL(ESP_OK, esp_board_device_show("expansion_runtime_pin"));
    TEST_ASSERT_EQUAL(ESP_OK, esp_board_device_show("expansion_module_manager"));
#else
    TEST_IGNORE_MESSAGE("Mosaico camera and encoder are not enabled");
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
