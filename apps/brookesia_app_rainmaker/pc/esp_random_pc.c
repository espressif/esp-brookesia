/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_random.h"
#include <stdlib.h>

uint32_t esp_random(void)
{
    return (uint32_t)rand();
}
