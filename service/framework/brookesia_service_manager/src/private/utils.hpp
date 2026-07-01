/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "brookesia/service_manager/macro_configs.h"
#undef BROOKESIA_LOG_TAG
#define BROOKESIA_LOG_TAG BROOKESIA_SERVICE_MANAGER_LOG_TAG
#include "brookesia/lib_utils/log.hpp"
#include "brookesia/lib_utils/check.hpp"
#include "brookesia/lib_utils/function_guard.hpp"
#include "brookesia/lib_utils/thread_config.hpp"

namespace esp_brookesia::service {

std::string utils_generate_uuid();

} // namespace esp_brookesia::service
