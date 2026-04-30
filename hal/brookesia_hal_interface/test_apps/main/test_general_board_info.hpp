/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include "sdkconfig.h"
#include "brookesia/hal_interface/interfaces/general/board_info.hpp"
#include "brookesia/hal_interface/device.hpp"

namespace esp_brookesia {

class TestGeneralBoardInfoIface: public hal::BoardInfoIface {
public:
    static constexpr const char *NAME = "TestGeneralBoardInfo:BoardInfo";

    TestGeneralBoardInfoIface()
        : BoardInfoIface(hal::BoardInfoIface::Info {
        .name = "ESP-Test-Board",
        .chip = CONFIG_IDF_TARGET,
        .version = "v1.2",
        .description = "Test board for HAL interface coverage",
        .manufacturer = "Espressif",
    })
    {
    }
    ~TestGeneralBoardInfoIface() = default;
};

class TestBoardInfoDevice: public hal::Device {
public:
    static constexpr const char *NAME = "TestGeneralBoardInfo";

    TestBoardInfoDevice()
        : hal::Device(std::string(NAME))
    {
    }

    bool probe() override;
    bool on_init() override;
    void on_deinit() override;
};

} // namespace esp_brookesia
