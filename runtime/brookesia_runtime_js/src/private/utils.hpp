/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "brookesia/runtime_js/macro_configs.h"
#undef BROOKESIA_LOG_TAG
#define BROOKESIA_LOG_TAG BROOKESIA_RUNTIME_JS_LOG_TAG
#include "brookesia/lib_utils/log.hpp"
#include "brookesia/lib_utils/check.hpp"
