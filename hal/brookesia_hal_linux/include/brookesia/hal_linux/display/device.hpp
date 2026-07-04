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

class DisplayPanelLinuxImpl;
class DisplayTouchLinuxImpl;
class DisplayBacklightLinuxImpl;
class DisplayLinuxBackend;

class DisplayLinuxDevice: public Device {
public:
    struct Config {
        uint16_t width_px = 800;
        uint16_t height_px = 480;
        std::string window_title = "ESP-Brookesia HAL Linux Display";
        std::string render_driver = "software";
    };

    static constexpr const char *DEVICE_NAME = "DisplayLinux";
    static constexpr const char *PANEL_IFACE_NAME = "DisplayLinux:Panel";
    static constexpr const char *TOUCH_IFACE_NAME = "DisplayLinux:Touch";
    static constexpr const char *BACKLIGHT_IFACE_NAME = "DisplayLinux:Backlight";

    DisplayLinuxDevice(const DisplayLinuxDevice &) = delete;
    DisplayLinuxDevice &operator=(const DisplayLinuxDevice &) = delete;
    DisplayLinuxDevice(DisplayLinuxDevice &&) = delete;
    DisplayLinuxDevice &operator=(DisplayLinuxDevice &&) = delete;

    static DisplayLinuxDevice &get_instance()
    {
        static DisplayLinuxDevice instance;
        return instance;
    }

    bool configure(Config config);
    Config get_config() const;
    bool is_quit_requested() const;

private:
    DisplayLinuxDevice()
        : Device(std::string(DEVICE_NAME))
    {
    }
    ~DisplayLinuxDevice() = default;

    bool probe() override;
    std::vector<InterfaceSpec> get_interface_specs() const override;
    bool on_init() override;
    void on_deinit() override;

    std::shared_ptr<DisplayPanelLinuxImpl> panel_iface_;
    std::shared_ptr<DisplayTouchLinuxImpl> touch_iface_;
    std::shared_ptr<DisplayBacklightLinuxImpl> backlight_iface_;
    std::shared_ptr<DisplayLinuxBackend> backend_;
    mutable std::mutex config_mutex_;
    Config config_{};
    bool initialized_ = false;
};

} // namespace esp_brookesia::hal
