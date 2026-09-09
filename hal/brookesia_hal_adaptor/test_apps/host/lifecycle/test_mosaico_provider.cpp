/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */

// Keep the actual board provider and its private class in one translation unit.
#include "module_provider.cpp"

std::shared_ptr<esp_brookesia::hal::expansion::ModuleProvider> make_mosaico_provider_for_test()
{
    return std::make_shared<esp_brookesia::hal::expansion::MosaicoModuleProvider>();
}
