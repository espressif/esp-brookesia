/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "brookesia/hal_interface/interfaces/general/board_info.hpp"

namespace esp_brookesia::hal {

/**
 * @brief Board-backed metadata HAL interface implemented through esp_board_manager.
 */
class BoardInfoImpl : public BoardInfoIface {
public:
    BoardInfoImpl();
    ~BoardInfoImpl() override = default;

    bool is_valid() const
    {
        return get_info().is_valid();
    }
};

} // namespace esp_brookesia::hal
