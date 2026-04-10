/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include "brookesia/lib_utils/state_base.hpp"
#include "common.hpp"

class ScreenEmote : public esp_brookesia::lib_utils::StateBase {
public:
    ScreenEmote();
    ~ScreenEmote();

    bool on_enter(const std::string &from_state, const std::string &action) override;
    bool on_exit(const std::string &to_state, const std::string &action) override;
};
