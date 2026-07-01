/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file device.hpp
 * @brief Singleton HAL device that publishes network protocol interfaces.
 */
#pragma once

#include <string>
#include "brookesia/hal_interface/device.hpp"

namespace esp_brookesia::hal {

class NetworkDevice: public Device {
public:
    static constexpr const char *DEVICE_NAME = "Network";
    static constexpr const char *SNTP_CLIENT_IFACE_NAME = "Network:SntpClient";
    static constexpr const char *HTTP_CLIENT_IFACE_NAME = "Network:HttpClient";

    NetworkDevice(const NetworkDevice &) = delete;
    NetworkDevice &operator=(const NetworkDevice &) = delete;
    NetworkDevice(NetworkDevice &&) = delete;
    NetworkDevice &operator=(NetworkDevice &&) = delete;

    static NetworkDevice &get_instance()
    {
        static NetworkDevice instance;
        return instance;
    }

private:
    NetworkDevice()
        : Device(std::string(DEVICE_NAME))
    {
    }
    ~NetworkDevice() = default;

    bool probe() override;
    std::vector<InterfaceSpec> get_interface_specs() const override;
    bool on_init() override;
    void on_deinit() override;

    bool init_sntp_client();
    void deinit_sntp_client();
    bool init_http_client();
    void deinit_http_client();
};

} // namespace esp_brookesia::hal
