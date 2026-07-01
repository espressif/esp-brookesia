/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <string>

namespace esp_brookesia::hal::nvs_flash_manager {

bool acquire();
void release();
std::string get_last_error();

} // namespace esp_brookesia::hal::nvs_flash_manager
