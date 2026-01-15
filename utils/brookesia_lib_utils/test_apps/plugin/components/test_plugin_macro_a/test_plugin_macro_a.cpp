/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "brookesia/lib_utils/plugin.hpp"
#include "test_class.hpp"
#include "test_plugin_macro_a.hpp"

BROOKESIA_PLUGIN_REGISTER(
    IPlugin, PluginA, PLUGIN_MACRO_A_NAME, MACRO_A_VALUE
);
