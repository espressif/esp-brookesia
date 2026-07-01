/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include <cstddef>
#if defined(ESP_PLATFORM)
#include "sdkconfig.h"
#endif
#ifdef BROOKESIA_LOG_TAG
#   undef BROOKESIA_LOG_TAG
#endif
#define BROOKESIA_LOG_TAG "TestApp"
#include "brookesia/lib_utils.hpp"
