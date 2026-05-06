/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include "brookesia/hal_interface/device.hpp"
#include "brookesia/hal_interface/interfaces/display/backlight.hpp"

namespace esp_brookesia {

class TestDisplayBacklightIface: public hal::DisplayBacklightIface {
public:
    static constexpr const char *NAME = "TestDisplayBacklight:Backlight";

    explicit TestDisplayBacklightIface(bool light_on_off_supported)
        : hal::DisplayBacklightIface()
        , light_on_off_supported_(light_on_off_supported)
    {
    }
    ~TestDisplayBacklightIface() = default;

    bool set_brightness(uint8_t percent) override;
    bool get_brightness(uint8_t &percent) override;
    bool is_light_on_off_supported() override
    {
        return light_on_off_supported_;
    }
    bool set_light_on_off(bool on) override;
    bool is_light_on() const override
    {
        return is_light_on_;
    }

    uint8_t get_last_brightness() const
    {
        return brightness_;
    }

private:
    bool light_on_off_supported_ = false;
    uint8_t brightness_ = 0;
    bool is_light_on_ = false;
};

class TestDisplayBacklightDevice: public hal::Device {
public:
    static constexpr const char *NAME = "TestDisplayBacklight";

    TestDisplayBacklightDevice()
        : hal::Device(std::string(NAME))
    {
    }

    bool probe() override;
    bool on_init() override;
    void on_deinit() override;

    static void set_light_on_off_supported(bool light_on_off_supported);
};

} // namespace esp_brookesia
