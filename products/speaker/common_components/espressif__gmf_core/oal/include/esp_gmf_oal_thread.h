/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_gmf_err.h"
#include "stdbool.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

typedef void *esp_gmf_oal_thread_t;

/**
 * @brief  This function creates a new thread, specifying its properties like name, priority, stack size, and the core to which it should be pinned
 *
 * @note
 *        - Please apply the ./idf_patches/idf_v3.3/4.x_freertos.patch first
 *        - Please enable support for external RAM and `Allow external memory as an argument to xTaskCreateStatic`
 *        to be able to use external memory for task stack, namely `CONFIG_SPIRAM_BOOT_INIT=y` and `CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY=y`
 *
 * @param[out]  p_handle      Pointer to the thread handle that will be created
 * @param[in]   name          The name of the thread
 * @param[in]   main_func     The main function that the thread will execute
 * @param[in]   arg           Argument to pass to the thread's main function
 * @param[in]   stack         The size of the stack (in bytes) to allocate for the thread
 * @param[in]   prio          The priority of the thread
 * @param[in]   stack_in_ext  If true, the stack will be allocated in external memory
 * @param[in]   core_id       The core to pin the thread to (-1 for no pinning)
 *
 * @return
 *       - ESP_GMF_ERR_OK    Operation successful
 *       - ESP_GMF_ERR_FAIL  Operation failed, usually due to insufficient memory for creating a new thread
 */
esp_gmf_err_t esp_gmf_oal_thread_create(esp_gmf_oal_thread_t *p_handle, const char *name, void (*main_func)(void *arg), void *arg,
                                        uint32_t stack, int prio, bool stack_in_ext, int core_id);

/**
 * @brief  Delete an existing GMF OAL thread
 *
 * @param[in]  p_handle  Pointer to the thread handle to be deleted
 *
 * @return
 *       - ESP_GMR_ERR_OK  Operation successful
 */
esp_gmf_err_t esp_gmf_oal_thread_delete(esp_gmf_oal_thread_t p_handle);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
