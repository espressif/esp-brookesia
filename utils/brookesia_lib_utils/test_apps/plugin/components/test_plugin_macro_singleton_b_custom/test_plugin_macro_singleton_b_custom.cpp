/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "brookesia/lib_utils/plugin.hpp"
#include "test_class.hpp"
#include "test_plugin_macro_singleton_b_custom.hpp"

BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    IPlugin, PluginSingletonB, PLUGIN_MACRO_SINGLETON_B_CUSTOM_NAME, PluginSingletonB::get_instance(),
    macro_singleton_b_custom_symbol
);
