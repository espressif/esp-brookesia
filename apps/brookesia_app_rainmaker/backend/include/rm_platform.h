/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

/**
 * @file rm_platform.h
 * @brief Build target for RainMaker app: ESP32 firmware vs host (PC simulator / tests).
 *
 * - ESP-IDF builds: RM_HOST_BUILD is unset (default).
 * - Host PC CMake (see pc/CMakeLists.txt): pass -DRM_HOST_BUILD=1.
 */

#if defined(RM_HOST_BUILD) && RM_HOST_BUILD
#define RM_PLATFORM_IS_HOST 1
#else
#define RM_PLATFORM_IS_HOST 0
#endif
