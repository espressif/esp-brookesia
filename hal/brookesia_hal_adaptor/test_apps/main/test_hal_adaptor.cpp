/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <sys/stat.h>
#include <array>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <utility>
#include "unity.h"
#include "brookesia/lib_utils.hpp"
#include "brookesia/hal_interface.hpp"
#include "brookesia/hal_adaptor.hpp"

#ifndef BROOKESIA_HAL_ADAPTOR_GENERAL_ENABLE_BOARD_INFO_IMPL
#   define BROOKESIA_HAL_ADAPTOR_GENERAL_ENABLE_BOARD_INFO_IMPL  (0)
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
#ifndef BROOKESIA_HAL_ADAPTOR_STORAGE_ENABLE_GENERAL_FS_IMPL
#   define BROOKESIA_HAL_ADAPTOR_STORAGE_ENABLE_GENERAL_FS_IMPL  (0)
#endif
#ifndef BROOKESIA_HAL_ADAPTOR_STORAGE_GENERAL_FS_ENABLE_SPIFFS
#   define BROOKESIA_HAL_ADAPTOR_STORAGE_GENERAL_FS_ENABLE_SPIFFS  (0)
#endif
#ifndef BROOKESIA_HAL_ADAPTOR_STORAGE_GENERAL_FS_ENABLE_SDCARD
#   define BROOKESIA_HAL_ADAPTOR_STORAGE_GENERAL_FS_ENABLE_SDCARD  (0)
#endif
#ifndef BROOKESIA_HAL_ADAPTOR_POWER_ENABLE_BATTERY
#   define BROOKESIA_HAL_ADAPTOR_POWER_ENABLE_BATTERY  (0)
#endif

using namespace esp_brookesia;

namespace {

struct DeviceConfig {
    const char *name;
    bool enabled;
};

constexpr std::array<DeviceConfig, 5> DEVICE_CONFIGS = {{
        { hal::GeneralDevice::DEVICE_NAME, BROOKESIA_HAL_ADAPTOR_ENABLE_GENERAL_DEVICE },
        { hal::AudioDevice::DEVICE_NAME, BROOKESIA_HAL_ADAPTOR_ENABLE_AUDIO_DEVICE },
        { hal::DisplayDevice::DEVICE_NAME, BROOKESIA_HAL_ADAPTOR_ENABLE_DISPLAY_DEVICE },
        { hal::StorageDevice::DEVICE_NAME, BROOKESIA_HAL_ADAPTOR_ENABLE_STORAGE_DEVICE },
        { hal::PowerDevice::DEVICE_NAME, BROOKESIA_HAL_ADAPTOR_ENABLE_POWER_DEVICE },
    }
};

void deinit_enabled_devices()
{
    for (const auto &config : DEVICE_CONFIGS) {
        if (config.enabled) {
            hal::deinit_device(config.name);
        }
    }
}

void reset_and_init_device(const char *device_name, bool enabled, const char *disabled_message)
{
    hal::deinit_device(device_name);
    if (!enabled) {
        TEST_IGNORE_MESSAGE(disabled_message);
    }

    const auto initialized = hal::init_device(device_name);
    const auto message = (boost::format("Failed to init enabled HAL device: %1%") % device_name).str();
    TEST_ASSERT_TRUE_MESSAGE(initialized, message.c_str());
}

template<typename T>
std::map<std::string, std::shared_ptr<T>> get_interfaces_or_fail(const char *message)
{
    auto interfaces = hal::get_interfaces<T>();
    TEST_ASSERT_FALSE_MESSAGE(interfaces.empty(), message);
    return interfaces;
}

bool path_exists(const char *path)
{
    struct stat st = {};
    return (path != nullptr) && (stat(path, &st) == 0);
}

bool verify_file_system_access(const char *mount_point)
{
    if (mount_point == nullptr || mount_point[0] == '\0') {
        return false;
    }

    std::string file_path = mount_point;
    if (file_path.back() != '/') {
        file_path += '/';
    }
    file_path += "smoke.txt";

    constexpr char payload[] = "esp-brookesia-storage-smoke";
    std::array<char, sizeof(payload)> readback = {};

    FILE *fp = std::fopen(file_path.c_str(), "wb");
    if (fp == nullptr) {
        BROOKESIA_LOGW("Failed to create test file under %1%", mount_point);
        return false;
    }

    const auto written = std::fwrite(payload, 1, sizeof(payload) - 1, fp);
    std::fclose(fp);
    if (written != sizeof(payload) - 1) {
        std::remove(file_path.c_str());
        return false;
    }

    if (!path_exists(file_path.c_str())) {
        std::remove(file_path.c_str());
        return false;
    }

    fp = std::fopen(file_path.c_str(), "rb");
    if (fp == nullptr) {
        std::remove(file_path.c_str());
        return false;
    }

    const auto read = std::fread(readback.data(), 1, sizeof(payload) - 1, fp);
    std::fclose(fp);
    std::remove(file_path.c_str());

    return (read == sizeof(payload) - 1) && (std::memcmp(readback.data(), payload, sizeof(payload) - 1) == 0);
}

template<typename T>
void assert_common_interface_properties(
    const std::map<std::string, std::shared_ptr<T>> &interfaces, const char *device_name = nullptr
)
{
    for (const auto &[iface_name, iface] : interfaces) {
        TEST_ASSERT_NOT_NULL(iface.get());
        const auto type_name = std::string(T::NAME);
        TEST_ASSERT_EQUAL_STRING(type_name.c_str(), std::string(iface->get_name()).c_str());

        if (device_name != nullptr) {
            auto device = hal::get_device_by_device_name(device_name);
            TEST_ASSERT_NOT_NULL(device.get());
            TEST_ASSERT_NOT_NULL(device->get_interface<T>(iface_name).get());
        }
    }
}

std::string describe_interface(const std::shared_ptr<hal::Interface> &iface)
{
    if (auto player = std::dynamic_pointer_cast<hal::AudioCodecPlayerIface>(iface)) {
        return (boost::format("{pa_on_off_supported=%1%, pa_on=%2%}") %
                player->is_pa_on_off_supported() % player->is_pa_on()).str();
    }
    if (auto board = std::dynamic_pointer_cast<hal::BoardInfoIface>(iface)) {
        return BROOKESIA_DESCRIBE_TO_STR(board->get_info());
    }
    if (auto battery = std::dynamic_pointer_cast<hal::PowerBatteryIface>(iface)) {
        return BROOKESIA_DESCRIBE_TO_STR(battery->get_info());
    }
    if (auto recorder = std::dynamic_pointer_cast<hal::AudioCodecRecorderIface>(iface)) {
        return BROOKESIA_DESCRIBE_TO_STR(recorder->get_info());
    }
    if (auto backlight = std::dynamic_pointer_cast<hal::DisplayBacklightIface>(iface)) {
        uint8_t brightness = 0;
        auto has_brightness = backlight->get_brightness(brightness);
        return (boost::format("{light_on_off_supported=%1%, brightness=%2%, brightness_valid=%3%, light_on=%4%}") %
                backlight->is_light_on_off_supported() % static_cast<int>(brightness) % has_brightness %
                backlight->is_light_on()).str();
    }
    if (auto panel = std::dynamic_pointer_cast<hal::DisplayPanelIface>(iface)) {
        return BROOKESIA_DESCRIBE_TO_STR(panel->get_info());
    }
    if (auto touch = std::dynamic_pointer_cast<hal::DisplayTouchIface>(iface)) {
        return BROOKESIA_DESCRIBE_TO_STR(touch->get_info());
    }
    if (auto storage = std::dynamic_pointer_cast<hal::StorageFsIface>(iface)) {
        return BROOKESIA_DESCRIBE_TO_STR(storage->get_all_info());
    }

    return "Unsupported interface type";
}

void log_registered_devices()
{
    const auto devices = hal::get_all_devices();
    BROOKESIA_LOGI("================ HAL Devices (%1%) ================", devices.size());
    for (const auto &[plugin_name, device] : devices) {
        TEST_ASSERT_NOT_NULL(device.get());
        BROOKESIA_LOGI("[Device] plugin=%1%, name=%2%", plugin_name, device->get_name());
    }
}

void log_registered_interfaces()
{
    const auto interfaces = hal::get_all_interfaces();
    BROOKESIA_LOGI("================ HAL Interfaces (%1%) ================", interfaces.size());
    for (const auto &[iface_name, iface] : interfaces) {
        TEST_ASSERT_NOT_NULL(iface.get());
        BROOKESIA_LOGI(
            "[Interface] registry=%1%, type=%2%, info=%3%",
            iface_name, iface->get_name(), describe_interface(iface)
        );
    }
}

} // namespace

TEST_CASE("HAL adaptor: enumerate all available interfaces and infos", "[hal][adaptor][enumerate]")
{
    deinit_enabled_devices();
    for (const auto &config : DEVICE_CONFIGS) {
        if (!config.enabled) {
            BROOKESIA_LOGI("Skip disabled HAL device: %1%", config.name);
            continue;
        }

        const auto initialized = hal::init_device(config.name);
        const auto message = (boost::format("Failed to init enabled HAL device: %1%") % config.name).str();
        TEST_ASSERT_TRUE_MESSAGE(initialized, message.c_str());
    }

    const auto devices = hal::get_all_devices();
    const auto interfaces = hal::get_all_interfaces();

    log_registered_devices();
    log_registered_interfaces();

    TEST_ASSERT_TRUE_MESSAGE(!devices.empty(), "No HAL devices were registered");
    TEST_ASSERT_TRUE_MESSAGE(!interfaces.empty(), "No HAL interfaces were registered on this board");

    deinit_enabled_devices();
}

TEST_CASE("HAL adaptor: enabled devices init smoke test", "[hal][adaptor][device]")
{
    deinit_enabled_devices();
    for (const auto &config : DEVICE_CONFIGS) {
        if (!config.enabled) {
            BROOKESIA_LOGI("Skip disabled HAL device: %1%", config.name);
            continue;
        }

        const auto initialized = hal::init_device(config.name);
        const auto message = (boost::format("Failed to init enabled HAL device: %1%") % config.name).str();
        TEST_ASSERT_TRUE_MESSAGE(initialized, message.c_str());
        hal::deinit_device(config.name);
    }
}

TEST_CASE("HAL adaptor: BoardInfoIface smoke test", "[hal][adaptor][board][info]")
{
    reset_and_init_device(
        hal::GeneralDevice::DEVICE_NAME,
        BROOKESIA_HAL_ADAPTOR_ENABLE_GENERAL_DEVICE && BROOKESIA_HAL_ADAPTOR_GENERAL_ENABLE_BOARD_INFO_IMPL,
        "BoardInfoIface is disabled by Kconfig"
    );

    auto interfaces = get_interfaces_or_fail<hal::BoardInfoIface>("No BoardInfoIface found after GeneralDevice init");
    assert_common_interface_properties(interfaces, hal::GeneralDevice::DEVICE_NAME);

    for (const auto &[iface_name, iface] : interfaces) {
        const auto &info = iface->get_info();
        BROOKESIA_LOGI("Testing board info interface %1%, info=%2%", iface_name, info);

        TEST_ASSERT_TRUE(info.is_valid());
        TEST_ASSERT_FALSE(info.name.empty() && info.chip.empty());
    }

    hal::deinit_device(hal::GeneralDevice::DEVICE_NAME);
}

TEST_CASE("HAL adaptor: PowerBatteryIface smoke test", "[hal][adaptor][battery][monitor]")
{
    reset_and_init_device(
        hal::PowerDevice::DEVICE_NAME,
        BROOKESIA_HAL_ADAPTOR_ENABLE_POWER_DEVICE && BROOKESIA_HAL_ADAPTOR_POWER_ENABLE_BATTERY,
        "PowerBatteryIface is disabled by Kconfig"
    );

    auto interfaces = get_interfaces_or_fail<hal::PowerBatteryIface>(
                          "No PowerBatteryIface found after PowerDevice init"
                      );
    assert_common_interface_properties(interfaces, hal::PowerDevice::DEVICE_NAME);

    for (const auto &[iface_name, iface] : interfaces) {
        const auto &info = iface->get_info();
        BROOKESIA_LOGI("Testing battery monitor interface %1%, info=%2%", iface_name, BROOKESIA_DESCRIBE_TO_STR(info));

        TEST_ASSERT_FALSE(info.name.empty());
        TEST_ASSERT_FALSE(info.abilities.empty());
        TEST_ASSERT_TRUE(info.has_ability(hal::PowerBatteryIface::Ability::Voltage));

        hal::PowerBatteryIface::State state;
        TEST_ASSERT_TRUE(iface->get_state(state));
        BROOKESIA_LOGI("Battery state: %1%", BROOKESIA_DESCRIBE_TO_STR(state));

        if (state.voltage_mv.has_value()) {
            TEST_ASSERT_GREATER_OR_EQUAL_UINT32(0, state.voltage_mv.value());
#if defined(BROOKESIA_HAL_ADAPTOR_POWER_BATTERY_CRITICAL_VOLTAGE_MV)
            if (state.voltage_mv.value() <= BROOKESIA_HAL_ADAPTOR_POWER_BATTERY_CRITICAL_VOLTAGE_MV) {
                TEST_ASSERT_TRUE(state.is_critical);
                TEST_ASSERT_TRUE(state.is_low);
            }
#endif
        }
        if (state.percentage.has_value()) {
            TEST_ASSERT_LESS_OR_EQUAL_UINT8(100, state.percentage.value());
        }

        hal::PowerBatteryIface::ChargeConfig config;
        if (info.has_ability(hal::PowerBatteryIface::Ability::ChargeConfig)) {
            TEST_ASSERT_TRUE(iface->get_charge_config(config));
            BROOKESIA_LOGI("Battery charge config: %1%", BROOKESIA_DESCRIBE_TO_STR(config));
        } else {
            TEST_ASSERT_FALSE(iface->get_charge_config(config));
            TEST_ASSERT_FALSE(iface->set_charge_config(config));
            TEST_ASSERT_FALSE(iface->set_charging_enabled(false));
        }
    }

    hal::deinit_device(hal::PowerDevice::DEVICE_NAME);
}

TEST_CASE("HAL adaptor: AudioCodecPlayerIface smoke test", "[hal][adaptor][audio][player]")
{
    reset_and_init_device(
        hal::AudioDevice::DEVICE_NAME,
        BROOKESIA_HAL_ADAPTOR_ENABLE_AUDIO_DEVICE && BROOKESIA_HAL_ADAPTOR_AUDIO_ENABLE_CODEC_PLAYER_IMPL,
        "AudioCodecPlayerIface is disabled by Kconfig"
    );

    auto interfaces = get_interfaces_or_fail<hal::AudioCodecPlayerIface>(
                          "No AudioCodecPlayerIface found after AudioDevice init"
                      );
    assert_common_interface_properties(interfaces, hal::AudioDevice::DEVICE_NAME);

    for (const auto &[iface_name, iface] : interfaces) {
        BROOKESIA_LOGI(
            "Testing player interface %1%, pa_on_off_supported=%2%",
            iface_name, iface->is_pa_on_off_supported()
        );

        TEST_ASSERT_TRUE(iface->open(hal::AudioCodecPlayerIface::Config {
            .bits = 16,
            .channels = 1,
            .sample_rate = 16000,
        }));

        TEST_ASSERT_TRUE(iface->set_volume(0));
        TEST_ASSERT_TRUE(iface->set_volume(45));
        TEST_ASSERT_TRUE(iface->set_volume(100));

        TEST_ASSERT_TRUE(iface->set_volume(33));

        const std::array<uint8_t, 32> silence = {};
        TEST_ASSERT_TRUE(iface->write_data(silence.data(), silence.size()));

        if (iface->is_pa_on_off_supported()) {
            TEST_ASSERT_TRUE(iface->is_pa_on());
            TEST_ASSERT_TRUE(iface->set_pa_on_off(false));
            TEST_ASSERT_FALSE(iface->is_pa_on());
            TEST_ASSERT_TRUE(iface->set_pa_on_off(true));
            TEST_ASSERT_TRUE(iface->is_pa_on());
        } else {
            TEST_ASSERT_FALSE(iface->set_pa_on_off(false));
            TEST_ASSERT_TRUE(iface->is_pa_on());
        }

        iface->close();

        TEST_ASSERT_TRUE(iface->set_volume(12));
        TEST_ASSERT_TRUE(iface->open(hal::AudioCodecPlayerIface::Config {
            .bits = 16,
            .channels = 1,
            .sample_rate = 16000,
        }));
        TEST_ASSERT_TRUE(iface->write_data(silence.data(), silence.size()));
        iface->close();
    }

    hal::deinit_device(hal::AudioDevice::DEVICE_NAME);
}

TEST_CASE("HAL adaptor: AudioCodecRecorderIface smoke test", "[hal][adaptor][audio][recorder]")
{
    reset_and_init_device(
        hal::AudioDevice::DEVICE_NAME,
        BROOKESIA_HAL_ADAPTOR_ENABLE_AUDIO_DEVICE && BROOKESIA_HAL_ADAPTOR_AUDIO_ENABLE_CODEC_RECORDER_IMPL,
        "AudioCodecRecorderIface is disabled by Kconfig"
    );

    auto interfaces = get_interfaces_or_fail<hal::AudioCodecRecorderIface>(
                          "No AudioCodecRecorderIface found after AudioDevice init"
                      );
    assert_common_interface_properties(interfaces, hal::AudioDevice::DEVICE_NAME);

    for (const auto &[iface_name, iface] : interfaces) {
        const auto &info = iface->get_info();
        BROOKESIA_LOGI("Testing recorder interface %1%, info=%2%", iface_name, BROOKESIA_DESCRIBE_TO_STR(info));

        TEST_ASSERT_TRUE(info.bits > 0);
        TEST_ASSERT_TRUE(info.channels > 0);
        TEST_ASSERT_TRUE(info.sample_rate > 0);

        TEST_ASSERT_TRUE(iface->open());
        TEST_ASSERT_TRUE(iface->set_general_gain(info.general_gain));
        TEST_ASSERT_TRUE(iface->set_channel_gains(info.channel_gains));

        std::array<uint8_t, 64> buffer = {};
        TEST_ASSERT_TRUE(iface->read_data(buffer.data(), buffer.size()));

        iface->close();
    }

    hal::deinit_device(hal::AudioDevice::DEVICE_NAME);
}

TEST_CASE("HAL adaptor: DisplayBacklightIface smoke test", "[hal][adaptor][display][backlight]")
{
    reset_and_init_device(
        hal::DisplayDevice::DEVICE_NAME,
        BROOKESIA_HAL_ADAPTOR_ENABLE_DISPLAY_DEVICE && BROOKESIA_HAL_ADAPTOR_DISPLAY_ENABLE_LEDC_BACKLIGHT_IMPL,
        "DisplayBacklightIface is disabled by Kconfig"
    );

    auto interfaces = get_interfaces_or_fail<hal::DisplayBacklightIface>(
                          "No DisplayBacklightIface found after DisplayDevice init"
                      );
    assert_common_interface_properties(interfaces, hal::DisplayDevice::DEVICE_NAME);

    for (const auto &[iface_name, iface] : interfaces) {
        BROOKESIA_LOGI(
            "Testing backlight interface %1%, light_on_off_supported=%2%",
            iface_name, iface->is_light_on_off_supported()
        );

        TEST_ASSERT_TRUE(iface->set_brightness(0));
        TEST_ASSERT_TRUE(iface->set_brightness(42));
        TEST_ASSERT_TRUE(iface->set_brightness(100));

        uint8_t brightness = 0;
        TEST_ASSERT_TRUE(iface->get_brightness(brightness));
        TEST_ASSERT_EQUAL_UINT8(100, brightness);

        if (iface->is_light_on_off_supported()) {
            TEST_ASSERT_TRUE(iface->set_light_on_off(false));
            TEST_ASSERT_FALSE(iface->is_light_on());
            TEST_ASSERT_TRUE(iface->set_light_on_off(true));
            TEST_ASSERT_TRUE(iface->is_light_on());
        } else {
            TEST_ASSERT_FALSE(iface->set_light_on_off(false));
        }
    }

    hal::deinit_device(hal::DisplayDevice::DEVICE_NAME);
}

TEST_CASE("HAL adaptor: DisplayPanelIface smoke test", "[hal][adaptor][display][panel]")
{
    reset_and_init_device(
        hal::DisplayDevice::DEVICE_NAME,
        BROOKESIA_HAL_ADAPTOR_ENABLE_DISPLAY_DEVICE && BROOKESIA_HAL_ADAPTOR_DISPLAY_ENABLE_LCD_PANEL_IMPL,
        "DisplayPanelIface is disabled by Kconfig"
    );

    auto interfaces = get_interfaces_or_fail<hal::DisplayPanelIface>(
                          "No DisplayPanelIface found after DisplayDevice init"
                      );
    assert_common_interface_properties(interfaces, hal::DisplayDevice::DEVICE_NAME);

    for (const auto &[iface_name, iface] : interfaces) {
        const auto &info = iface->get_info();
        BROOKESIA_LOGI("Testing panel interface %1%, info=%2%", iface_name, BROOKESIA_DESCRIBE_TO_STR(info));

        TEST_ASSERT_TRUE(info.is_valid());
        TEST_ASSERT_TRUE(info.get_pixel_bits() > 0);

        hal::DisplayPanelIface::DriverSpecific specific = {};
        TEST_ASSERT_TRUE(iface->get_driver_specific(specific));
        TEST_ASSERT_NOT_NULL(specific.panel_handle);
        TEST_ASSERT_TRUE(specific.bus_type != hal::DisplayPanelIface::BusType::Max);

        std::vector<uint8_t> pixel_data(info.get_pixel_bits() / 8, 0);
        TEST_ASSERT_TRUE(iface->draw_bitmap(0, 0, 1, 1, pixel_data.data()));
    }

    hal::deinit_device(hal::DisplayDevice::DEVICE_NAME);
}

TEST_CASE("HAL adaptor: DisplayTouchIface smoke test", "[hal][adaptor][display][touch]")
{
    reset_and_init_device(
        hal::DisplayDevice::DEVICE_NAME,
        BROOKESIA_HAL_ADAPTOR_ENABLE_DISPLAY_DEVICE && BROOKESIA_HAL_ADAPTOR_DISPLAY_ENABLE_LCD_TOUCH_IMPL,
        "DisplayTouchIface is disabled by Kconfig"
    );

    auto interfaces = get_interfaces_or_fail<hal::DisplayTouchIface>(
                          "No DisplayTouchIface found after DisplayDevice init"
                      );
    assert_common_interface_properties(interfaces, hal::DisplayDevice::DEVICE_NAME);

    for (const auto &[iface_name, iface] : interfaces) {
        const auto &info = iface->get_info();
        BROOKESIA_LOGI("Testing touch interface %1%, info=%2%", iface_name, BROOKESIA_DESCRIBE_TO_STR(info));

        TEST_ASSERT_TRUE(info.is_valid());

        hal::DisplayTouchIface::DriverSpecific specific = {};
        TEST_ASSERT_TRUE(iface->get_driver_specific(specific));
        TEST_ASSERT_NOT_NULL(specific.touch_handle);

        std::vector<hal::DisplayTouchIface::Point> points;
        TEST_ASSERT_TRUE(iface->read_points(points));
        for (const auto &point : points) {
            TEST_ASSERT_TRUE(point.x >= 0);
            TEST_ASSERT_TRUE(point.y >= 0);
            TEST_ASSERT_TRUE(static_cast<uint16_t>(point.x) <= info.x_max);
            TEST_ASSERT_TRUE(static_cast<uint16_t>(point.y) <= info.y_max);
        }

        if (info.operation_mode == hal::DisplayTouchIface::OperationMode::Interrupt) {
            TEST_ASSERT_TRUE(iface->register_interrupt_handler([]() {}));
            TEST_ASSERT_TRUE(iface->register_interrupt_handler(nullptr));
        } else {
            TEST_ASSERT_FALSE(iface->register_interrupt_handler([]() {}));
        }
    }

    hal::deinit_device(hal::DisplayDevice::DEVICE_NAME);
}

TEST_CASE("HAL adaptor: StorageFsIface smoke test", "[hal][adaptor][storage][fs]")
{
    reset_and_init_device(
        hal::StorageDevice::DEVICE_NAME,
        BROOKESIA_HAL_ADAPTOR_ENABLE_STORAGE_DEVICE && BROOKESIA_HAL_ADAPTOR_STORAGE_ENABLE_GENERAL_FS_IMPL &&
        (BROOKESIA_HAL_ADAPTOR_STORAGE_GENERAL_FS_ENABLE_SPIFFS ||
         BROOKESIA_HAL_ADAPTOR_STORAGE_GENERAL_FS_ENABLE_SDCARD),
        "StorageFsIface is disabled by Kconfig"
    );

    auto interfaces = get_interfaces_or_fail<hal::StorageFsIface>(
                          "No StorageFsIface found after StorageDevice init"
                      );
    assert_common_interface_properties(interfaces, hal::StorageDevice::DEVICE_NAME);

    for (const auto &[iface_name, iface] : interfaces) {
        const auto &info_list = iface->get_all_info();
        BROOKESIA_LOGI("Testing storage interface %1%, count=%2%", iface_name, info_list.size());

        TEST_ASSERT_FALSE(info_list.empty());
        for (const auto &info : info_list) {
            BROOKESIA_LOGI("  fs_info=%1%", BROOKESIA_DESCRIBE_TO_STR(info));
            TEST_ASSERT_NOT_NULL(info.mount_point);
            TEST_ASSERT_TRUE(info.mount_point[0] != '\0');
            TEST_ASSERT_TRUE(verify_file_system_access(info.mount_point));
        }
    }

    hal::deinit_device(hal::StorageDevice::DEVICE_NAME);
}
