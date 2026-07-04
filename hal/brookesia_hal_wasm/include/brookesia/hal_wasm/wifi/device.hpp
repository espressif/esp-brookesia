/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <memory>
#include "brookesia/hal_interface/device.hpp"

namespace esp_brookesia::hal {

class WifiWasmBackend;

class WifiWasmDevice: public Device {
public:
    static constexpr const char *DEVICE_NAME = "WiFiWasm";
    static constexpr const char *BASIC_IFACE_NAME = "WiFiWasm:Basic";
    static constexpr const char *STATION_IFACE_NAME = "WiFiWasm:Station";
    static constexpr const char *SOFTAP_IFACE_NAME = "WiFiWasm:SoftAP";
    static constexpr const char *CONNECTIVITY_IFACE_NAME = "Network:Connectivity:WiFiWasm";

    WifiWasmDevice(const WifiWasmDevice &) = delete;
    WifiWasmDevice &operator=(const WifiWasmDevice &) = delete;
    WifiWasmDevice(WifiWasmDevice &&) = delete;
    WifiWasmDevice &operator=(WifiWasmDevice &&) = delete;

    static WifiWasmDevice &get_instance()
    {
        static WifiWasmDevice instance;
        return instance;
    }

private:
    WifiWasmDevice()
        : Device(std::string(DEVICE_NAME))
    {
    }
    ~WifiWasmDevice() = default;

    bool probe() override;
    std::vector<InterfaceSpec> get_interface_specs() const override;
    bool on_init() override;
    void on_deinit() override;

    std::shared_ptr<WifiWasmBackend> backend_;
};

} // namespace esp_brookesia::hal
