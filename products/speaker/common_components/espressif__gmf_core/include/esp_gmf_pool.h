/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_gmf_audio_element.h"
#include "esp_gmf_io.h"
#include "esp_gmf_pipeline.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

typedef struct esp_gmf_pool *esp_gmf_pool_handle_t;

/**
 * @brief  Initialize a GMF pool
 *
 * @param[out]  handle  Pointer to store the handle of the initialized GMF pool
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 *       - ESP_GMF_ERR_MEMORY_LACK  Memory allocation failed
 */
esp_gmf_err_t esp_gmf_pool_init(esp_gmf_pool_handle_t *handle);

/**
 * @brief  Deinitialize a GMF pool, freeing registered elements and I/O instances
 *
 * @param[in]  handle  GMF pool handle to deinitialize
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_t esp_gmf_pool_deinit(esp_gmf_pool_handle_t handle);

/**
 * @brief  Register a GMF element to specific pool
 *
 * @param[in]  handle  GMF pool handle
 * @param[in]  el      GMF element handle to register
 * @param[in]  tag     Tag associated with the element
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid arguments
 *       - ESP_GMF_ERR_MEMORY_LACK  Memory allocation failed
 */
esp_gmf_err_t esp_gmf_pool_register_element(esp_gmf_pool_handle_t handle, esp_gmf_element_handle_t el, const char *tag);

/**
 * @brief  Register a GMF I/O instance with a GMF pool
 *
 * @param[in]  handle  GMF pool handle
 * @param[in]  port    GMF I/O handle to register
 * @param[in]  tag     Tag associated with the I/O port
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid arguments
 *       - ESP_GMF_ERR_MEMORY_LACK  Memory allocation failed
 */
esp_gmf_err_t esp_gmf_pool_register_io(esp_gmf_pool_handle_t handle, esp_gmf_io_handle_t port, const char *tag);

/**
 * @brief  Create a new GMF pipeline from the specific pool
 *         It checks if the element is already registered. If so, it duplicates them, links them together
 *         Similarly to the element, it checks the I/O ports, duplicates them, and then adds input and output ports to the pipeline
 *         If either `in_name` or `out_name` is NULL, the corresponding port will be ignored
 *
 * @param[in]   handle          GMF pool handle
 * @param[in]   in_name         Name of the input element
 * @param[in]   el_name         Array of names for intermediate elements
 * @param[in]   num_of_el_name  Number of elements in the el_name array
 * @param[in]   out_name        Name of the output element
 * @param[out]  pipeline        Pointer to store the created GMF pipeline handle
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_MEMORY_LACK  Memory allocation failed
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument or the number of names is incorrect
 *       - ESP_GMF_ERR_NOT_SUPPORT  Not support port type
 *       - ESP_GMF_ERR_NOT_FOUND    Not found the specific instance
 *       - ESP_GMF_ERR_FAIL         Others failed
 */
esp_gmf_err_t esp_gmf_pool_new_pipeline(esp_gmf_pool_handle_t handle, const char *in_name,
                                        const char *el_name[], int num_of_el_name,
                                        const char *out_name,
                                        esp_gmf_pipeline_handle_t *pipeline);

/**
 * @brief  Create a new I/O instance from the GMF pool by given name
 *
 * @param[in]   handle  GMF pool handle
 * @param[in]   name    Name of the I/O handle
 * @param[in]   dir     Direction of the I/O (reader or writer)
 * @param[out]  new_io  Pointer to the new I/O handle to be created
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid arguments
 *       - ESP_GMF_ERR_MEMORY_LACK  Memory allocation failed
 *       - ESP_GMF_ERR_NOT_FOUND    Not found the specific I/O instance
 */
esp_gmf_err_t esp_gmf_pool_new_io(esp_gmf_pool_handle_t handle, const char *name, esp_gmf_io_dir_t dir, esp_gmf_io_handle_t *new_io);

/**
 * @brief  Create a new element from the GMF pool by given name
 *
 * @param[in]   handle   GMF pool handle
 * @param[in]   el_name  Name of the element
 * @param[out]  new_el   Pointer to the new element handle to be created
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid arguments
 *       - ESP_GMF_ERR_MEMORY_LACK  Memory allocation failed
 *       - ESP_GMF_ERR_NOT_FOUND    Not found the specific element instance
 */
esp_gmf_err_t esp_gmf_pool_new_element(esp_gmf_pool_handle_t handle, const char *el_name, esp_gmf_element_handle_t *new_el);

/**
 * @brief  Print information about elements and IOs registered in a GMF pool
 *
 * @param[in]  handle  GMF pool handle
 * @param[in]  line    Line of log
 * @param[in]  func    Function name of log
 */
void esp_gmf_pool_show_lists(esp_gmf_pool_handle_t handle, int line, const char *func);

#define ESP_GMF_POOL_SHOW_ITEMS(x) esp_gmf_pool_show_lists(x, __LINE__, __FUNCTION__)

#ifdef __cplusplus
}
#endif  /* __cplusplus */
