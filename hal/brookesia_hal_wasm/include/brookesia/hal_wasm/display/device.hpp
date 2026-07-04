/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include "brookesia/hal_interface/device.hpp"

namespace esp_brookesia::hal {

class DisplayPanelWasmImpl;
class DisplayTouchWasmImpl;
class DisplayBacklightWasmImpl;
class DisplayWasmBackend;

class DisplayWasmDevice: public Device {
public:
    struct Config {
        uint16_t width_px = 800;
        uint16_t height_px = 480;
        std::string window_title = "ESP-Brookesia Web Simulator";
    };

    static constexpr const char *DEVICE_NAME = "DisplayWasm";
    static constexpr const char *PANEL_IFACE_NAME = "DisplayWasm:Panel";
    static constexpr const char *TOUCH_IFACE_NAME = "DisplayWasm:Touch";
    static constexpr const char *BACKLIGHT_IFACE_NAME = "DisplayWasm:Backlight";

    DisplayWasmDevice(const DisplayWasmDevice &) = delete;
    DisplayWasmDevice &operator=(const DisplayWasmDevice &) = delete;
    DisplayWasmDevice(DisplayWasmDevice &&) = delete;
    DisplayWasmDevice &operator=(DisplayWasmDevice &&) = delete;

    static DisplayWasmDevice &get_instance()
    {
        static DisplayWasmDevice instance;
        return instance;
    }

    bool configure(Config config);
    Config get_config() const;
    bool is_quit_requested() const;
    void poll_events();

private:
    DisplayWasmDevice()
        : Device(std::string(DEVICE_NAME))
    {
    }
    ~DisplayWasmDevice() = default;

    bool probe() override;
    std::vector<InterfaceSpec> get_interface_specs() const override;
    bool on_init() override;
    void on_deinit() override;

    std::shared_ptr<DisplayPanelWasmImpl> panel_iface_;
    std::shared_ptr<DisplayTouchWasmImpl> touch_iface_;
    std::shared_ptr<DisplayBacklightWasmImpl> backlight_iface_;
    std::shared_ptr<DisplayWasmBackend> backend_;
    mutable std::mutex config_mutex_;
    Config config_{};
    bool initialized_ = false;
};

} // namespace esp_brookesia::hal
