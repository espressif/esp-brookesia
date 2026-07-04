/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <array>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "brookesia/hal_interface.hpp"
#include "brookesia/hal_linux.hpp"

using namespace esp_brookesia;

namespace {

template<typename T>
hal::InterfaceHandle<T> try_get_iface(const char *expected_name)
{
    auto iface = hal::acquire_interface<T>(expected_name);
    if (!iface) {
        std::cout << "[REAL-SMOKE] skip: " << expected_name << " is unavailable\n";
        return iface;
    }
    std::cout << "[REAL-SMOKE] acquired: " << iface.instance_name() << '\n';
    return iface;
}

void smoke_display()
{
    auto panel = try_get_iface<hal::display::PanelIface>(hal::DisplayLinuxDevice::PANEL_IFACE_NAME);
    auto touch = try_get_iface<hal::display::TouchIface>(hal::DisplayLinuxDevice::TOUCH_IFACE_NAME);
    auto backlight = try_get_iface<hal::display::BacklightIface>(hal::DisplayLinuxDevice::BACKLIGHT_IFACE_NAME);
    if (backlight) {
        backlight->set_light_on_off(true);
        backlight->set_brightness(80);
    }
    if (panel) {
        std::array<uint16_t, 4> pixels = {
            0xF800,
            0x07E0,
            0x001F,
            0xFFFF,
        };
        const bool ok = panel->draw_bitmap(0, 0, 2, 2, reinterpret_cast<const uint8_t *>(pixels.data()));
        std::cout << "[REAL-SMOKE] display draw: " << (ok ? "ok" : "fallback/failed") << '\n';
    }
    if (touch) {
        std::vector<hal::display::TouchIface::Point> points;
        touch->read_points(points);
        std::cout << "[REAL-SMOKE] touch points: " << points.size() << '\n';
    }
}

void smoke_power()
{
    auto battery = try_get_iface<hal::power::BatteryIface>(hal::PowerLinuxDevice::BATTERY_IFACE_NAME);
    if (!battery) {
        return;
    }
    hal::power::BatteryIface::State state;
    const bool ok = battery->get_state(state);
    std::cout << "[REAL-SMOKE] power state: " << (ok ? "ok" : "failed") <<
              ", present=" << state.is_present << '\n';
}

void smoke_connectivity(const char *instance_name, const char *label)
{
    auto connectivity = try_get_iface<hal::network::ConnectivityIface>(instance_name);
    if (!connectivity) {
        return;
    }
    const auto status = connectivity->get_status();
    std::cout << "[REAL-SMOKE] " << label << " connectivity local-ready=" << status.is_local_network_ready() <<
              ", internet-ready=" << status.is_internet_ready() << '\n';
}

void smoke_video()
{
    const std::string encoder_name = hal::VideoLinuxDevice::get_encoder_iface_name(0);
    auto encoder = try_get_iface<hal::video::EncoderIface>(encoder_name.c_str());
    if (!encoder) {
        return;
    }

    std::error_code error;
    if (!std::filesystem::exists("/dev/video0", error)) {
        std::cout << "[REAL-SMOKE] skip: /dev/video0 is unavailable\n";
        return;
    }

    std::string error_message;
    bool saw_frame = false;
    const auto callback = [&saw_frame](
                              size_t, const hal::video::EncoderSinkInfo &, const uint8_t *data, size_t size
    ) {
        saw_frame = (data != nullptr) && (size > 0);
    };
    const bool opened = encoder->open({
        .sinks = {
            {
                .format = hal::video::EncoderSinkFormat::MJPEG,
                .width = 320,
                .height = 240,
                .fps = 15,
            },
        },
        .enable_stream_mode = false,
        .source = hal::video::EncoderSourceConfig{
            .device_path = std::string("/dev/video0"),
        },
    }, callback, &error_message);
    if (!opened || !encoder->start(&error_message)) {
        std::cout << "[REAL-SMOKE] video start failed/fallback: " << error_message << '\n';
        encoder->close();
        return;
    }
    encoder->fetch_frame(0, callback, &error_message);
    encoder->stop(&error_message);
    encoder->close();
    std::cout << "[REAL-SMOKE] video frame: " << (saw_frame ? "ok" : "not captured") << '\n';
}

} // namespace

int main()
{
    try {
        smoke_display();
        smoke_power();
        smoke_connectivity(hal::NetworkLinuxDevice::CONNECTIVITY_IFACE_NAME, "network");
        smoke_connectivity(hal::WifiLinuxDevice::CONNECTIVITY_IFACE_NAME, "wifi");
        smoke_video();
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "[REAL-SMOKE] exception: " << e.what() << '\n';
        return 1;
    }
}
