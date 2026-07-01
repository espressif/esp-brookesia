/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <memory>
#include "brookesia/hal_interface/device.hpp"

namespace esp_brookesia::hal {

class BoardInfoWasmImpl;

class SystemWasmDevice: public Device {
public:
    static constexpr const char *DEVICE_NAME = "SystemWasm";
    static constexpr const char *BOARD_INFO_IFACE_NAME = "SystemWasm:BoardInfo";

    SystemWasmDevice(const SystemWasmDevice &) = delete;
    SystemWasmDevice &operator=(const SystemWasmDevice &) = delete;
    SystemWasmDevice(SystemWasmDevice &&) = delete;
    SystemWasmDevice &operator=(SystemWasmDevice &&) = delete;

    static SystemWasmDevice &get_instance()
    {
        static SystemWasmDevice instance;
        return instance;
    }

private:
    SystemWasmDevice()
        : Device(std::string(DEVICE_NAME))
    {
    }
    ~SystemWasmDevice() = default;

    bool probe() override;
    std::vector<InterfaceSpec> get_interface_specs() const override;
    bool on_init() override;
    void on_deinit() override;

    std::shared_ptr<BoardInfoWasmImpl> board_info_iface_;
};

} // namespace esp_brookesia::hal
