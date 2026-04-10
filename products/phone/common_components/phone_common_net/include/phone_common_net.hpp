/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Default timezone, optional Wi-Fi STA + SNTP when CONFIG_RAINMAKER_APP_SUPPORT is set.
 * Call before bsp_display_start / any HTTP (e.g. RainMaker). No-op when RainMaker support is off.
 */
void phone_common_net_boot(void);

#ifdef __cplusplus
}
#endif
