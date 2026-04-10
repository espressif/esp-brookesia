/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include "brookesia/hal_interface/device.hpp"
#include "brookesia/hal_interface/display/touch.hpp"

namespace esp_brookesia {

class TestDisplayTouchIface: public hal::DisplayTouchIface {
public:
    static constexpr const char *NAME = "TestDisplayTouch:Touch";

    TestDisplayTouchIface(hal::DisplayTouchIface::Info info) : hal::DisplayTouchIface(std::move(info)) {}
    ~TestDisplayTouchIface() = default;

    bool read_points(std::vector<hal::DisplayTouchIface::Point> &points) override;

    bool register_interrupt_handler(hal::DisplayTouchIface::InterruptHandler handler) override;

    bool get_driver_specific(hal::DisplayTouchIface::DriverSpecific &specific) override;
};

class TestDisplayTouchDevice: public hal::Device {
public:
    static constexpr const char *NAME = "TestDisplayTouch";

    TestDisplayTouchDevice() : hal::Device(std::string(NAME)) {}

    bool probe() override;
    bool on_init() override;
    void on_deinit() override;
};

} // namespace esp_brookesia
