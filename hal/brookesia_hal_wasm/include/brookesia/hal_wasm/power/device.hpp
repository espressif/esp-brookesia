/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "brookesia/hal_interface/device.hpp"

namespace esp_brookesia::hal {

class PowerWasmDevice: public Device {
public:
    static constexpr const char *DEVICE_NAME = "PowerWasm";

    PowerWasmDevice(const PowerWasmDevice &) = delete;
    PowerWasmDevice &operator=(const PowerWasmDevice &) = delete;
    PowerWasmDevice(PowerWasmDevice &&) = delete;
    PowerWasmDevice &operator=(PowerWasmDevice &&) = delete;

    static PowerWasmDevice &get_instance()
    {
        static PowerWasmDevice instance;
        return instance;
    }

private:
    PowerWasmDevice()
        : Device(std::string(DEVICE_NAME))
    {
    }
    ~PowerWasmDevice() = default;

    bool probe() override;
    bool on_init() override;
    void on_deinit() override;
};

} // namespace esp_brookesia::hal
