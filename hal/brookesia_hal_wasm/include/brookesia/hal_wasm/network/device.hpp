/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <memory>
#include "brookesia/hal_interface/device.hpp"

namespace esp_brookesia::hal {

class NetworkConnectivityWasmImpl;
class NetworkHttpClientWasmImpl;

class NetworkWasmDevice: public Device {
public:
    static constexpr const char *DEVICE_NAME = "NetworkWasm";
    static constexpr const char *CONNECTIVITY_IFACE_NAME = "Network:Connectivity:NetworkWasm";
    static constexpr const char *HTTP_CLIENT_IFACE_NAME = "Network:HttpClient:NetworkWasm";

    NetworkWasmDevice(const NetworkWasmDevice &) = delete;
    NetworkWasmDevice &operator=(const NetworkWasmDevice &) = delete;
    NetworkWasmDevice(NetworkWasmDevice &&) = delete;
    NetworkWasmDevice &operator=(NetworkWasmDevice &&) = delete;

    static NetworkWasmDevice &get_instance()
    {
        static NetworkWasmDevice instance;
        return instance;
    }

private:
    NetworkWasmDevice()
        : Device(std::string(DEVICE_NAME))
    {
    }
    ~NetworkWasmDevice() = default;

    bool probe() override;
    std::vector<InterfaceSpec> get_interface_specs() const override;
    bool on_init() override;
    void on_deinit() override;

    std::shared_ptr<NetworkConnectivityWasmImpl> connectivity_iface_;
    std::shared_ptr<NetworkHttpClientWasmImpl> http_client_iface_;
};

} // namespace esp_brookesia::hal
