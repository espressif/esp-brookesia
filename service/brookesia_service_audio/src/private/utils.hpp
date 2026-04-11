/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "brookesia/service_audio/macro_configs.h"
#undef BROOKESIA_LOG_TAG
#define BROOKESIA_LOG_TAG BROOKESIA_SERVICE_AUDIO_LOG_TAG
#include "brookesia/lib_utils/log.hpp"
#include "brookesia/lib_utils/check.hpp"

#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32P4
#   define AV_PROCESSOR_AFE_USE_CUSTOM 0
#else
#   define AV_PROCESSOR_AFE_USE_CUSTOM 1
#endif
