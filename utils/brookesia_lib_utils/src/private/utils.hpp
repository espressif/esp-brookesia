/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "brookesia/lib_utils/macro_configs.h"

#if !BROOKESIA_UTILS_ENABLE_DEBUG_LOG || defined(UTILS_DISABLE_DEBUG_LOG)
#   undef BROOKESIA_LOGD_IMPL_FUNC
#   undef BROOKESIA_LOGT_IMPL_FUNC
#   define BROOKESIA_LOGD_IMPL_FUNC(...) {}
#   define BROOKESIA_LOGT_IMPL_FUNC(...) {}
#endif
#undef BROOKESIA_LOG_TAG
#define BROOKESIA_LOG_TAG BROOKESIA_UTILS_LOG_TAG
#include "brookesia/lib_utils/log.hpp"
