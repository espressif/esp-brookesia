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

using namespace esp_brookesia;

namespace {

struct KnownDeviceStatus {
    bool storage = false;
    bool audio = false;
    bool display = false;
};

void deinit_known_devices()
{
    hal::deinit_device(hal::DisplayDevice::DEVICE_NAME);
    hal::deinit_device(hal::AudioDevice::DEVICE_NAME);
    hal::deinit_device(hal::StorageDevice::DEVICE_NAME);
}

KnownDeviceStatus init_known_devices()
{
    KnownDeviceStatus status{};

    status.storage = hal::init_device(hal::StorageDevice::DEVICE_NAME);
    status.audio = hal::init_device(hal::AudioDevice::DEVICE_NAME);
    status.display = hal::init_device(hal::DisplayDevice::DEVICE_NAME);

    return status;
}

void reset_and_init_known_devices()
{
    deinit_known_devices();
    auto status = init_known_devices();

    BROOKESIA_LOGI(
        "Known device init status: storage=%1%, audio=%2%, display=%3%",
        status.storage, status.audio, status.display
    );
}

template<typename T>
std::map<std::string, std::shared_ptr<T>> get_interfaces_or_ignore(const char *message)
{
    auto interfaces = hal::get_interfaces<T>();
    if (interfaces.empty()) {
        deinit_known_devices();
        TEST_IGNORE_MESSAGE(message);
    }
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
    file_path += "hal_adaptor_storage_smoke.txt";

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
        TEST_ASSERT_EQUAL_STRING(T::NAME, std::string(iface->get_name()).c_str());

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
        return BROOKESIA_DESCRIBE_TO_STR(player->get_info());
    }
    if (auto recorder = std::dynamic_pointer_cast<hal::AudioCodecRecorderIface>(iface)) {
        return BROOKESIA_DESCRIBE_TO_STR(recorder->get_info());
    }
    if (auto backlight = std::dynamic_pointer_cast<hal::DisplayBacklightIface>(iface)) {
        return BROOKESIA_DESCRIBE_TO_STR(backlight->get_info());
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
    reset_and_init_known_devices();

    const auto devices = hal::get_all_devices();
    const auto interfaces = hal::get_all_interfaces();

    log_registered_devices();
    log_registered_interfaces();

    TEST_ASSERT_TRUE_MESSAGE(!devices.empty(), "No HAL devices were registered");
    TEST_ASSERT_TRUE_MESSAGE(!interfaces.empty(), "No HAL interfaces were registered on this board");

    deinit_known_devices();
}

TEST_CASE("HAL adaptor: AudioCodecPlayerIface smoke test", "[hal][adaptor][audio][player]")
{
    reset_and_init_known_devices();

    auto interfaces = get_interfaces_or_ignore<hal::AudioCodecPlayerIface>(
                          "No AudioCodecPlayerIface found on the current board"
                      );
    assert_common_interface_properties(interfaces, hal::AudioDevice::DEVICE_NAME);

    for (const auto &[iface_name, iface] : interfaces) {
        const auto &info = iface->get_info();
        BROOKESIA_LOGI("Testing player interface %1%, info=%2%", iface_name, BROOKESIA_DESCRIBE_TO_STR(info));

        TEST_ASSERT_TRUE(info.volume_min <= info.volume_default);
        TEST_ASSERT_TRUE(info.volume_default <= info.volume_max);
        TEST_ASSERT_TRUE(info.volume_max <= 100);

        TEST_ASSERT_TRUE(iface->open(hal::AudioCodecPlayerIface::Config {
            .bits = 16,
            .channels = 1,
            .sample_rate = 16000,
        }));

        TEST_ASSERT_TRUE(iface->set_volume(info.volume_default));

        uint8_t volume = 0;
        TEST_ASSERT_TRUE(iface->get_volume(volume));
        TEST_ASSERT_EQUAL_UINT8(info.volume_default, volume);

        TEST_ASSERT_TRUE(iface->mute());
        TEST_ASSERT_TRUE(iface->get_volume(volume));
        TEST_ASSERT_EQUAL_UINT8(0, volume);

        TEST_ASSERT_TRUE(iface->unmute());
        TEST_ASSERT_TRUE(iface->get_volume(volume));
        TEST_ASSERT_EQUAL_UINT8(info.volume_default, volume);

        const std::array<uint8_t, 32> silence = {};
        TEST_ASSERT_TRUE(iface->write_data(silence.data(), silence.size()));

        iface->close();
    }

    deinit_known_devices();
}

TEST_CASE("HAL adaptor: AudioCodecRecorderIface smoke test", "[hal][adaptor][audio][recorder]")
{
    reset_and_init_known_devices();

    auto interfaces = get_interfaces_or_ignore<hal::AudioCodecRecorderIface>(
                          "No AudioCodecRecorderIface found on the current board"
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

    deinit_known_devices();
}

TEST_CASE("HAL adaptor: DisplayBacklightIface smoke test", "[hal][adaptor][display][backlight]")
{
    reset_and_init_known_devices();

    auto interfaces = get_interfaces_or_ignore<hal::DisplayBacklightIface>(
                          "No DisplayBacklightIface found on the current board"
                      );
    assert_common_interface_properties(interfaces, hal::DisplayDevice::DEVICE_NAME);

    for (const auto &[iface_name, iface] : interfaces) {
        const auto &info = iface->get_info();
        BROOKESIA_LOGI("Testing backlight interface %1%, info=%2%", iface_name, BROOKESIA_DESCRIBE_TO_STR(info));

        TEST_ASSERT_TRUE(info.brightness_min <= info.brightness_default);
        TEST_ASSERT_TRUE(info.brightness_default <= info.brightness_max);
        TEST_ASSERT_TRUE(info.brightness_max <= 100);

        TEST_ASSERT_TRUE(iface->set_brightness(info.brightness_default));

        uint8_t brightness = 0;
        TEST_ASSERT_TRUE(iface->get_brightness(brightness));
        TEST_ASSERT_EQUAL_UINT8(info.brightness_default, brightness);

        TEST_ASSERT_TRUE(iface->turn_off());
        TEST_ASSERT_TRUE(iface->get_brightness(brightness));
        TEST_ASSERT_EQUAL_UINT8(0, brightness);

        TEST_ASSERT_TRUE(iface->turn_on());
        TEST_ASSERT_TRUE(iface->get_brightness(brightness));
        TEST_ASSERT_TRUE(brightness > 0);
    }

    deinit_known_devices();
}

TEST_CASE("HAL adaptor: DisplayPanelIface smoke test", "[hal][adaptor][display][panel]")
{
    reset_and_init_known_devices();

    auto interfaces = get_interfaces_or_ignore<hal::DisplayPanelIface>(
                          "No DisplayPanelIface found on the current board"
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

    deinit_known_devices();
}

TEST_CASE("HAL adaptor: DisplayTouchIface smoke test", "[hal][adaptor][display][touch]")
{
    reset_and_init_known_devices();

    auto interfaces = get_interfaces_or_ignore<hal::DisplayTouchIface>(
                          "No DisplayTouchIface found on the current board"
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

    deinit_known_devices();
}

TEST_CASE("HAL adaptor: StorageFsIface smoke test", "[hal][adaptor][storage][fs]")
{
    reset_and_init_known_devices();

    auto interfaces = get_interfaces_or_ignore<hal::StorageFsIface>(
                          "No StorageFsIface found on the current board"
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

    deinit_known_devices();
}
