/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <string>
#include <vector>

#include "brookesia/hal_interface/device.hpp"
#include "brookesia/hal_interface/interfaces/expansion/module_manager.hpp"

namespace esp_brookesia::hal::expansion {

/** @brief HAL device publishing the generic expansion module manager. */
class ExpansionDevice: public Device {
public:
    static constexpr const char *DEVICE_NAME = "Expansion";

    ExpansionDevice(const ExpansionDevice &) = delete;
    ExpansionDevice &operator=(const ExpansionDevice &) = delete;
    ExpansionDevice(ExpansionDevice &&) = delete;
    ExpansionDevice &operator=(ExpansionDevice &&) = delete;

    static std::string get_module_manager_iface_name(size_t id = 0)
    {
        return ModuleManagerIface::get_default_instance_name(id);
    }

    static ExpansionDevice &get_instance()
    {
        static ExpansionDevice instance;
        return instance;
    }

private:
    ExpansionDevice()
        : Device(std::string(DEVICE_NAME))
    {
    }
    ~ExpansionDevice() = default;

    bool probe() override;
    std::vector<InterfaceSpec> get_interface_specs() const override;
    bool on_init() override;
    void on_deinit() override;
};

} // namespace esp_brookesia::hal::expansion
