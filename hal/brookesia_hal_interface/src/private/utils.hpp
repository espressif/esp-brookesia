/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "brookesia/hal_interface/macro_configs.h"
#undef BROOKESIA_LOG_TAG
#define BROOKESIA_LOG_TAG BROOKESIA_HAL_INTERFACE_LOG_TAG
#include "brookesia/lib_utils/log.hpp"
#include "brookesia/lib_utils/check.hpp"
