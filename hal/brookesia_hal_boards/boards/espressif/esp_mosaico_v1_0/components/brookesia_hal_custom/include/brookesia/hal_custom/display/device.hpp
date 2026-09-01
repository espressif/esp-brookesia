/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <string>
#include "brookesia/hal_interface/device.hpp"

namespace esp_brookesia::hal {

class CustomDisplayDevice : public Device {
public:
    static constexpr const char *DEVICE_NAME = "CustomDisplay";
    static constexpr const char *DISPLAY_BACKLIGHT_IMPL_NAME = "CustomDisplay:Backlight";

    CustomDisplayDevice(const CustomDisplayDevice &) = delete;
    CustomDisplayDevice &operator=(const CustomDisplayDevice &) = delete;
    CustomDisplayDevice(CustomDisplayDevice &&) = delete;
    CustomDisplayDevice &operator=(CustomDisplayDevice &&) = delete;

    static CustomDisplayDevice &get_instance()
    {
        static CustomDisplayDevice instance;
        return instance;
    }

private:
    CustomDisplayDevice()
        : Device(std::string(DEVICE_NAME))
    {
    }
    ~CustomDisplayDevice() = default;

    bool probe() override;
    std::vector<InterfaceSpec> get_interface_specs() const override;
    bool on_init() override;
    void on_deinit() override;

    bool init_backlight();
    void deinit_backlight();
};

} // namespace esp_brookesia::hal
