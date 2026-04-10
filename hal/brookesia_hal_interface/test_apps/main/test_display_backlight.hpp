/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include "brookesia/hal_interface/device.hpp"
#include "brookesia/hal_interface/display/backlight.hpp"

namespace esp_brookesia {

class TestDisplayBacklightIface: public hal::DisplayBacklightIface {
public:
    static constexpr const char *NAME = "TestDisplayBacklight:Backlight";

    TestDisplayBacklightIface(hal::DisplayBacklightIface::Info info) : hal::DisplayBacklightIface(std::move(info)) {}
    ~TestDisplayBacklightIface() = default;

    bool set_brightness(uint8_t percent) override;
    bool get_brightness(uint8_t &percent) override;
    bool turn_on() override;
    bool turn_off() override;

private:
    uint8_t brightness_ = 0;
};

class TestDisplayBacklightDevice: public hal::Device {
public:
    static constexpr const char *NAME = "TestDisplayBacklight";

    TestDisplayBacklightDevice() : hal::Device(std::string(NAME)) {}

    bool probe() override;
    bool on_init() override;
    void on_deinit() override;
};

} // namespace esp_brookesia
