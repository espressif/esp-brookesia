/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file device.hpp
 * @brief Singleton HAL device that publishes board metadata from the board manager.
 */
#pragma once

#include <string>
#include "brookesia/hal_interface/device.hpp"

namespace esp_brookesia::hal {

/**
 * @brief Board-backed metadata device: publishes a board information HAL interface.
 *
 * Obtained via get_instance(). Not copyable or movable.
 */
class GeneralDevice : public Device {
public:
    /** @brief Logical device name passed to the base @ref Device constructor. */
    static constexpr const char *DEVICE_NAME = "General";
    /** @brief Registry key for the board information HAL interface (`"General:BoardInfo"`). */
    static constexpr const char *BOARD_INFO_IMPL_NAME = "General:BoardInfo";

    GeneralDevice(const GeneralDevice &) = delete;
    GeneralDevice &operator=(const GeneralDevice &) = delete;
    GeneralDevice(GeneralDevice &&) = delete;
    GeneralDevice &operator=(GeneralDevice &&) = delete;

    /**
     * @brief Returns the process-wide singleton board device.
     *
     * @return Reference to the unique @ref GeneralDevice instance.
     */
    static GeneralDevice &get_instance()
    {
        static GeneralDevice instance;
        return instance;
    }

private:
    GeneralDevice()
        : Device(std::string(DEVICE_NAME))
    {
    }
    ~GeneralDevice() = default;

    bool probe() override;
    bool on_init() override;
    void on_deinit() override;

    bool init_board_info();
    void deinit_board_info();
};

} // namespace esp_brookesia::hal
