/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <memory>
#include "brookesia/hal_interface/device.hpp"

namespace esp_brookesia::hal {

class WifiLinuxBackend;

class WifiLinuxDevice: public Device {
public:
    static constexpr const char *DEVICE_NAME = "WiFiLinux";
    static constexpr const char *BASIC_IFACE_NAME = "WiFiLinux:Basic";
    static constexpr const char *STATION_IFACE_NAME = "WiFiLinux:Station";
    static constexpr const char *SOFTAP_IFACE_NAME = "WiFiLinux:SoftAP";
    static constexpr const char *CONNECTIVITY_IFACE_NAME = "Network:Connectivity:WiFiLinux";

    WifiLinuxDevice(const WifiLinuxDevice &) = delete;
    WifiLinuxDevice &operator=(const WifiLinuxDevice &) = delete;
    WifiLinuxDevice(WifiLinuxDevice &&) = delete;
    WifiLinuxDevice &operator=(WifiLinuxDevice &&) = delete;

    static WifiLinuxDevice &get_instance()
    {
        static WifiLinuxDevice instance;
        return instance;
    }

private:
    WifiLinuxDevice()
        : Device(std::string(DEVICE_NAME))
    {
    }
    ~WifiLinuxDevice() = default;

    bool probe() override;
    std::vector<InterfaceSpec> get_interface_specs() const override;
    bool on_init() override;
    void on_deinit() override;

    std::shared_ptr<WifiLinuxBackend> backend_;
};

} // namespace esp_brookesia::hal
