/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <memory>
#include "brookesia/hal_interface/device.hpp"

namespace esp_brookesia::hal {

class SntpClientLinuxIface;
class HttpClientLinuxIface;

class NetworkLinuxDevice: public Device {
public:
    static constexpr const char *DEVICE_NAME = "NetworkLinux";
    static constexpr const char *SNTP_CLIENT_IFACE_NAME = "NetworkLinux:SntpClient";
    static constexpr const char *HTTP_CLIENT_IFACE_NAME = "NetworkLinux:HttpClient";
    static constexpr const char *CONNECTIVITY_IFACE_NAME = "Network:Connectivity:NetworkLinux";

    NetworkLinuxDevice(const NetworkLinuxDevice &) = delete;
    NetworkLinuxDevice &operator=(const NetworkLinuxDevice &) = delete;
    NetworkLinuxDevice(NetworkLinuxDevice &&) = delete;
    NetworkLinuxDevice &operator=(NetworkLinuxDevice &&) = delete;

    static NetworkLinuxDevice &get_instance()
    {
        static NetworkLinuxDevice instance;
        return instance;
    }

private:
    NetworkLinuxDevice()
        : Device(std::string(DEVICE_NAME))
    {
    }
    ~NetworkLinuxDevice() = default;

    bool probe() override;
    std::vector<InterfaceSpec> get_interface_specs() const override;
    bool on_init() override;
    void on_deinit() override;

    std::shared_ptr<SntpClientLinuxIface> sntp_client_iface_;
    std::shared_ptr<HttpClientLinuxIface> http_client_iface_;
    std::shared_ptr<Interface> connectivity_iface_;
};

} // namespace esp_brookesia::hal
