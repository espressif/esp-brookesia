/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include <cstddef>
#include "sdkconfig.h"
#ifdef BROOKESIA_LOG_TAG
#   undef BROOKESIA_LOG_TAG
#endif
#define BROOKESIA_LOG_TAG "TestApp"
#include "brookesia/lib_utils.hpp"
