/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include "sdkconfig.h"

class Emote {
public:
    static Emote &get_instance()
    {
        static Emote instance;
        return instance;
    }

    bool init(int core_id = CONFIG_BROOKESIA_HAL_ADAPTOR_DISPLAY_LCD_PANEL_INIT_THREAD_CORE_ID);

private:
    Emote() = default;
    ~Emote() = default;

    Emote(const Emote &) = delete;
    Emote(Emote &&) = delete;
    Emote &operator=(const Emote &) = delete;
    Emote &operator=(Emote &&) = delete;
};
