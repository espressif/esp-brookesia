/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "brookesia/lib_utils/plugin.hpp"
#include "test_class.hpp"

BROOKESIA_PLUGIN_REGISTER(
    IPlugin, PluginA, "macro_unused"
);
