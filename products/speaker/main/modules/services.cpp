/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "esp_brookesia.hpp"
#ifdef ESP_UTILS_LOG_TAG
#   undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "Services"
#include "esp_lib_utils.h"
#include "services.hpp"

using namespace esp_brookesia::services;

bool services_init()
{
    ESP_UTILS_LOG_TRACE_GUARD();

    /* Startup NVS Service */
    ESP_UTILS_CHECK_FALSE_RETURN(StorageNVS::requestInstance().begin(), false, "Failed to begin storage NVS");

    return true;
}
