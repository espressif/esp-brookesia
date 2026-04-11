/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include "brookesia/lib_utils/describe_helpers.hpp"

enum class DisplayScreen {
    Emote,
    Settings,
    Max,
};

enum class DisplayAction {
    ScrollLeft,
    ScrollRight,
    ScrollUp,
    ScrollDown,
    EdgeScrollLeft,
    EdgeScrollRight,
    EdgeScrollUp,
    EdgeScrollDown,
    Max,
};

BROOKESIA_DESCRIBE_ENUM(DisplayScreen, Emote, Settings, Max)
BROOKESIA_DESCRIBE_ENUM(
    DisplayAction, ScrollLeft, ScrollRight, ScrollUp, ScrollDown, EdgeScrollLeft, EdgeScrollRight, EdgeScrollUp,
    EdgeScrollDown, Max
)
