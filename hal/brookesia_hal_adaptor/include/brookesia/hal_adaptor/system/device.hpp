/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file device.hpp
 * @brief Singleton HAL device that publishes system interfaces.
 */
#pragma once

#include <string>
#include "brookesia/hal_interface/device.hpp"

namespace esp_brookesia::hal {

class SystemDevice: public Device {
public:
    static constexpr const char *DEVICE_NAME = "System";
    static constexpr const char *BOARD_INFO_IFACE_NAME = "System:BoardInfo";
    static constexpr const char *OTA_UPDATER_IFACE_NAME = "System:OtaUpdater";

    SystemDevice(const SystemDevice &) = delete;
    SystemDevice &operator=(const SystemDevice &) = delete;
    SystemDevice(SystemDevice &&) = delete;
    SystemDevice &operator=(SystemDevice &&) = delete;

    static SystemDevice &get_instance()
    {
        static SystemDevice instance;
        return instance;
    }

private:
    SystemDevice()
        : Device(std::string(DEVICE_NAME))
    {
    }
    ~SystemDevice() = default;

    bool probe() override;
    std::vector<InterfaceSpec> get_interface_specs() const override;
    bool on_init() override;
    void on_deinit() override;

    bool init_board_info();
    void deinit_board_info();
    bool init_ota_updater();
    void deinit_ota_updater();
};

} // namespace esp_brookesia::hal
