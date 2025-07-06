/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Allocates and initializes a new mutex object for synchronization
 *
 * @return
 *       - Pointer  to the newly created mutex on success
 *       - NULL     if the mutex creation fails
 */
void *esp_gmf_oal_mutex_create(void);

/**
 * @brief  Destroy a mutex
 *
 * @param[in]  mutex  Pointer to the mutex to destroy
 *
 * @return
 *       - 0         on success
 *       - Negative  value if an error occurs
 */
int esp_gmf_oal_mutex_destroy(void *mutex);

/**
 * @brief  Acquires a lock on the specified mutex, blocking if necessary until the lock becomes available
 *
 * @param[in]  mutex  Pointer to the mutex to lock
 *
 * @return
 *       - 0         on success
 *       - Negative  value if an error occurs
 */
int esp_gmf_oal_mutex_lock(void *mutex);

/**
 * @brief  Releases the lock held on the specified mutex, allowing other threads to acquire the lock
 *
 * @param[in]  mutex  Pointer to the mutex to unlock
 *
 * @return
 *       - 0         on success
 *       - Negative  value if an error occurs
 */
int esp_gmf_oal_mutex_unlock(void *mutex);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
