/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include "brookesia/hal_interface/device.hpp"
#include "brookesia/hal_interface/display/panel.hpp"

namespace esp_brookesia {

class TestDisplayPanelIface: public hal::DisplayPanelIface {
public:
    static constexpr const char *NAME = "TestDisplayPanel:Panel";

    TestDisplayPanelIface(hal::DisplayPanelIface::Info info) : hal::DisplayPanelIface(std::move(info)) {}
    ~TestDisplayPanelIface() = default;

    bool draw_bitmap(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, const uint8_t *data) override;

    bool get_driver_specific(hal::DisplayPanelIface::DriverSpecific &specific) override;
};

class TestDisplayPanelDevice: public hal::Device {
public:
    static constexpr const char *NAME = "TestDisplayPanel";

    TestDisplayPanelDevice() : hal::Device(std::string(NAME)) {}

    bool probe() override;
    bool on_init() override;
    void on_deinit() override;
};

} // namespace esp_brookesia
