/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#ifndef _AUDIO_SYS_H_
#define _AUDIO_SYS_H_

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ___STR___(x) #x
#define STR_AUDIO(x) ___STR___(x)

#define AUDIO_UNUSED(x) (void)(x)

#if defined(__GNUC__) && (__GNUC__ >= 7)
#define FALL_THROUGH __attribute__ ((fallthrough))
#else
#define FALL_THROUGH ((void)0)
#endif /* __GNUC__ >= 7 */

/**
 * @brief       Get system ticks by given millisecond
 *
 * @param[in]   ms millisecond
 *
 * @return
 *     - tick
 */
int audio_sys_get_tick_by_time_ms(int ms);

/**
 * @brief   Get system time with millisecond
 *
 * @return
 *     -  time with millisecond
 */
int64_t audio_sys_get_time_ms(void);

/**
 * @brief   Function to print the CPU usage of tasks over a given AUDIO_SYS_TASKS_ELAPSED_TIME_MS.
 *
 * This function will measure and print the CPU usage of tasks over a specified
 * number of ticks (i.e. real time stats). This is implemented by simply calling
 * uxTaskGetSystemState() twice separated by a delay, then calculating the
 * differences of task run times before and after the delay.
 *
 * @note    If any tasks are added or removed during the delay, the stats of
 *          those tasks will not be printed.
 * @note    This function should be called from a high priority task to minimize
 *          inaccuracies with delays.
 * @note    When running in dual core mode, each core will correspond to 50% of
 *          the run time.
 *
 * @return
 *  - ESP_OK
 *  - ESP_FAIL
 */
esp_err_t audio_sys_get_real_time_stats(void);

#ifdef __cplusplus
}
#endif

#endif
