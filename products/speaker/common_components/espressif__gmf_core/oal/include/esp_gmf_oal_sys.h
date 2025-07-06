/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_gmf_err.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#if defined(__GNUC__) && (__GNUC__ >= 7)
#define FALL_THROUGH __attribute__((fallthrough))
#else
#define FALL_THROUGH ((void)0)
#endif  /* defined(__GNUC__) && (__GNUC__ >= 7) */

/**
 * @brief  Get system ticks based on the given millisecond value
 *
 * @param[in]  ms  Time in milliseconds
 *
 * @return
 *       - The  corresponding system ticks
 */
int esp_gmf_oal_sys_get_tick_by_time_ms(int ms);

/**
 * @brief  Retrieve the current system time in milliseconds
 *
 * @return
 *       - The  system time in milliseconds
 */
int64_t esp_gmf_oal_sys_get_time_ms(void);

/**
 * @brief  Print CPU usage statistics of tasks over a specified time period
 *
 *         This function measures and prints the CPU usage of tasks over a given
 *         period (in milliseconds) by calling `uxTaskGetSystemState()` twice,
 *         with a delay in between. The CPU usage is then calculated based on the
 *         difference in task run times before and after the delay.
 *
 * @note
 *        - If tasks are added or removed during the delay, their statistics will not be included in the final report
 *        - To minimize inaccuracies caused by delays, this function should be called from a high-priority task
 *        - In dual-core mode, each core will account for up to 50% of the total runtime
 *
 * @param[in]  elapsed_time_ms  Time period for the CPU usage measurement in milliseconds
 * @param[in]  markdown         Whether print use markdown format
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_MEMORY_LACK  No memory for operation
 *       - ESP_GMF_ERR_NOT_ENOUGH   More memory is needed for uxTaskGetSystemState
 *       - ESP_GMF_ERR_FAIL         On failure
 */
esp_gmf_err_t esp_gmf_oal_sys_get_real_time_stats(int elapsed_time_ms, bool markdown);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
