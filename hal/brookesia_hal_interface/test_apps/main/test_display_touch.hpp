/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include "brookesia/hal_interface/device.hpp"
#include "brookesia/hal_interface/interfaces/display/touch.hpp"

namespace esp_brookesia {

class TestDisplayTouchIface: public hal::display::TouchIface {
public:
    static constexpr const char *NAME = "TestDisplayTouch:Touch";

    TestDisplayTouchIface(hal::display::TouchIface::Info info) : hal::display::TouchIface(std::move(info)) {}
    ~TestDisplayTouchIface() = default;

    bool read_points(std::vector<hal::display::TouchIface::Point> &points) override;

    bool register_interrupt_handler(hal::display::TouchIface::InterruptHandler handler, void *ctx) override;

    bool get_driver_specific(hal::display::TouchIface::DriverSpecific &specific) override;
};

class TestDisplayTouchDevice: public hal::Device {
public:
    static constexpr const char *NAME = "TestDisplayTouch";

    TestDisplayTouchDevice() : hal::Device(std::string(NAME)) {}

    bool probe() override;
    std::vector<hal::InterfaceSpec> get_interface_specs() const override;
    bool on_init() override;
    void on_deinit() override;
};

} // namespace esp_brookesia
