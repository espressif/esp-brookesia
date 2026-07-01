/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <random>
#include "utils.hpp"

namespace esp_brookesia::service {

std::string utils_generate_uuid()
{
    // Use static random number generator to avoid creating large objects on the stack for each call
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static std::uniform_int_distribution<> dis2(8, 11);

    // Hexadecimal character table
    static const char hex_chars[] = "0123456789abcdef";

    // UUID format: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx (36 characters + '\0')
    char uuid[37];
    int pos = 0;

    // Generate 8 characters
    for (int i = 0; i < 8; i++) {
        uuid[pos++] = hex_chars[dis(gen)];
    }
    uuid[pos++] = '-';

    // Generate 4 characters
    for (int i = 0; i < 4; i++) {
        uuid[pos++] = hex_chars[dis(gen)];
    }
    uuid[pos++] = '-';

    // Generate '4' + 3 characters
    uuid[pos++] = '4';
    for (int i = 0; i < 3; i++) {
        uuid[pos++] = hex_chars[dis(gen)];
    }
    uuid[pos++] = '-';

    // Generate y (8-b) + 3 characters
    uuid[pos++] = hex_chars[dis2(gen)];
    for (int i = 0; i < 3; i++) {
        uuid[pos++] = hex_chars[dis(gen)];
    }
    uuid[pos++] = '-';

    // Generate 12 characters
    for (int i = 0; i < 12; i++) {
        uuid[pos++] = hex_chars[dis(gen)];
    }
    uuid[pos] = '\0';

    return std::string(uuid);
}

} // namespace esp_brookesia::service
