/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include "sdkconfig.h"

#ifdef BROOKESIA_LOG_TAG
#   undef BROOKESIA_LOG_TAG
#endif
#define BROOKESIA_LOG_TAG "HalInterfaceTest"

#include "brookesia/hal_interface.hpp"
#include "brookesia/lib_utils/log.hpp"
