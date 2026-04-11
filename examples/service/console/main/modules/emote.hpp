/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

class Emote {
public:
    static Emote &get_instance()
    {
        static Emote instance;
        return instance;
    }

    bool init(int core_id = 0);

private:
    Emote() = default;
    ~Emote() = default;

    Emote(const Emote &) = delete;
    Emote(Emote &&) = delete;
    Emote &operator=(const Emote &) = delete;
    Emote &operator=(Emote &&) = delete;
};
