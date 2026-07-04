/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <memory>
#include "brookesia/hal_interface/device.hpp"

namespace esp_brookesia::hal {

class PowerBatteryLinuxImpl;

class PowerLinuxDevice: public Device {
public:
    static constexpr const char *DEVICE_NAME = "PowerLinux";
    static constexpr const char *BATTERY_IFACE_NAME = "PowerLinux:Battery";

    PowerLinuxDevice(const PowerLinuxDevice &) = delete;
    PowerLinuxDevice &operator=(const PowerLinuxDevice &) = delete;
    PowerLinuxDevice(PowerLinuxDevice &&) = delete;
    PowerLinuxDevice &operator=(PowerLinuxDevice &&) = delete;

    static PowerLinuxDevice &get_instance()
    {
        static PowerLinuxDevice instance;
        return instance;
    }

private:
    PowerLinuxDevice()
        : Device(std::string(DEVICE_NAME))
    {
    }
    ~PowerLinuxDevice() = default;

    bool probe() override;
    std::vector<InterfaceSpec> get_interface_specs() const override;
    bool on_init() override;
    void on_deinit() override;

    std::shared_ptr<PowerBatteryLinuxImpl> battery_iface_;
};

} // namespace esp_brookesia::hal
