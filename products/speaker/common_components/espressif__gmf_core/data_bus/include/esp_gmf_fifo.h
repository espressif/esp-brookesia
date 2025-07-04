/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_gmf_data_bus.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  The GMF FIFO Buffer is an interface designed for passing buffer addresses without generating any copies.
 *         It maintains a list whose maximum number of items is specified when creating the FIFO handle. The FIFO buffer
 *         is not allocated during initialization; instead, it is allocated when `esp_gmf_fifo_acquire_write` is called.
 *         This interface provides blocking operations, meaning that `esp_gmf_fifo_acquire_read` and `esp_gmf_fifo_acquire_write`
 *         will block until a buffer becomes available or the timeout occurs. The functions `esp_gmf_fifo_release_read` and
 *         `esp_gmf_fifo_release_write` must be used in pairs and cannot be invoked recursively. Additionally, the GMF FIFO
 *         buffer is thread-safe, ensuring safe concurrent access in multi-threaded environments.
 */

/**
 * @brief  Handle for the GMF FIFO
 */
typedef void *esp_gmf_fifo_handle_t;

/**
 * @brief  Create a FIFO buffer for data blocks
 *
 * @param[in]   block_cnt   Number of blocks in the FIFO
 * @param[in]   block_size  Size of each block in bytes
 * @param[out]  handle      Pointer to the FIFO handle to be created
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success
 *       - ESP_GMF_ERR_MEMORY_LACK  Memory allocation failed
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid arguments
 */
esp_gmf_err_t esp_gmf_fifo_create(int block_cnt, int block_size, esp_gmf_fifo_handle_t *handle);

/**
 * @brief  Set alignment for FIFO buffer
 *
 * @note  Should call this API before `esp_gmf_fifo_acquire_write` to get aligned buffer
 *
 * @param[in]   handle  FIFO handle
 * @param[in]   align   Alignment for FIFO buffer to get (power of 2, 0=default)
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid arguments
 */
esp_gmf_err_t esp_gmf_fifo_set_align(esp_gmf_fifo_handle_t handle, uint8_t align);

/**
 * @brief  Destroy the FIFO buffer and release resources
 *
 * @param[in]  handle  FIFO handle to be destroyed
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid handle
 */
esp_gmf_err_t esp_gmf_fifo_destroy(esp_gmf_fifo_handle_t handle);

/**
 * @brief  Acquire a filled block from the FIFO, if there is insufficient block and block_ticks is greater than 0,
 *         the function will block until enough block becomes available
 *
 * @note  1. The `wanted_size` parameter is not used because the acquired block comes from the filled blocks.
 *        2. The obtained buffer address is internal and should not be freed externally.
 *        3. The obtained blocks must be released by `esp_gmf_fifo_release_read` in pairs.
 *        4. The actual valid size is stored in `blk->valid_size`
 *
 * @param[in]   handle       FIFO handle
 * @param[out]  blk          Pointer to the data block structure to be filled
 * @param[in]   wanted_size  Desired size to read in bytes
 * @param[in]   block_ticks  Maximum ticks to wait if the block is not available
 *
 * @return
 *       - > 0                 The specific length of data being read
 *       - ESP_GMF_IO_OK       Operation succeeded
 *       - ESP_GMF_IO_FAIL     Invalid arguments, or wanted_size > total size
 *       - ESP_GMF_IO_TIMEOUT  Operation timed out
 */
esp_gmf_err_io_t esp_gmf_fifo_acquire_read(esp_gmf_fifo_handle_t handle, esp_gmf_data_bus_block_t *blk, uint32_t wanted_size, int block_ticks);

/**
 * @brief  Returned acquired block address to the specific handle
 *
 * @note  1. The returned block MUST be acquired from `esp_gmf_block_acquire_read` to ensure proper synchronization
 *        2. `esp_gmf_block_acquire_read` and `esp_gmf_block_release_read` must be called in pairs
 *        3. Calling this function once makes one block available for the write operation
 *
 * @param[in]  handle       FIFO handle
 * @param[in]  blk          Pointer to the data block structure to be released
 * @param[in]  block_ticks  Maximum ticks to wait if necessary
 *
 * @return
 *       - ESP_GMF_IO_OK    Operation succeeded
 *       - ESP_GMF_IO_FAIL  Invalid arguments, or the buffer does not belong to the provided handle
 */
esp_gmf_err_io_t esp_gmf_fifo_release_read(esp_gmf_fifo_handle_t handle, esp_gmf_data_bus_block_t *blk, int block_ticks);

/**
 * @brief  Acquire freed block to the desired size within a specific handle
 *         If the number of the list is reach the maximum and block_ticks is greater than 0, the function will block
 *         until enough space becomes available.
 *
 * @note  1. `esp_gmf_fifo_acquire_write` and `esp_gmf_fifo_release_write` must be called in pairs
 *        2. The obtained buffer address is internal and should not be freed externally
 *        3. The `wanted_size` parameter is used to specify the size of the block that can be written
 *        4. If the freed block is not ready and the number of items in the list is below the maximum limit, a new node and buffer will be allocated for the list
 *        5. If the obtained buffer is smaller than the `wanted_size`, the buffer will be reallocated to match the requested size
 *
 * @param[in]   handle       FIFO handle
 * @param[out]  blk          Pointer to the data block structure to be filled
 * @param[in]   wanted_size  Desired size to write in bytes
 * @param[in]   block_ticks  Maximum ticks to wait if the block is not available
 *
 * @return
 *       - > 0                 The specific length of space can be write
 *       - ESP_GMF_IO_OK       Operation succeeded, or it's done to write
 *       - ESP_GMF_IO_FAIL     Invalid arguments, or no filled data
 *       - ESP_GMF_IO_TIMEOUT  Operation timed out
 */
esp_gmf_err_io_t esp_gmf_fifo_acquire_write(esp_gmf_fifo_handle_t handle, esp_gmf_data_bus_block_t *blk, uint32_t wanted_size, int block_ticks);

/**
 * @brief  Returned a previously acquired write block
 *
 * @note  1. The returned buffer MUST be acquired from `esp_gmf_fifo_acquire_write` to ensure proper synchronization
 *        2. `esp_gmf_fifo_acquire_write` and `esp_gmf_fifo_release_write` must be called in pairs
 *        3. Calling this function once makes one block available for read operations, and the count of freed blocks will be decremented by one
 *
 * @param[in]  handle       FIFO handle
 * @param[in]  blk          Pointer to the data block structure to be released
 * @param[in]  block_ticks  Maximum ticks to wait if necessary
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_io_t esp_gmf_fifo_release_write(esp_gmf_fifo_handle_t handle, esp_gmf_data_bus_block_t *blk, int block_ticks);

/**
 * @brief  Indicate that writing to the FIFO is complete
 *
 * @param[in]  handle  FIFO handle
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid handle
 */
esp_gmf_err_t esp_gmf_fifo_done_write(esp_gmf_fifo_handle_t handle);

/**
 * @brief  Abort all operations on the FIFO
 *
 * @param[in]  handle  FIFO handle
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid handle
 */
esp_gmf_err_t esp_gmf_fifo_abort(esp_gmf_fifo_handle_t handle);

/**
 * @brief  Reset the FIFO to its initial state
 *
 * @param[in]  handle  FIFO handle
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid handle
 */
esp_gmf_err_t esp_gmf_fifo_reset(esp_gmf_fifo_handle_t handle);

/**
 * @brief  Get the amount of free space in the FIFO
 *
 * @param[in]   handle     FIFO handle
 * @param[out]  free_size  Pointer to store the free size in bytes
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid arguments
 */
esp_gmf_err_t esp_gmf_fifo_get_free_size(esp_gmf_fifo_handle_t handle, uint32_t *free_size);

/**
 * @brief  Get the amount of filled space in the FIFO
 *
 * @param[in]   handle       FIFO handle
 * @param[out]  filled_size  Pointer to store the filled size in bytes
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid arguments
 */
esp_gmf_err_t esp_gmf_fifo_get_filled_size(esp_gmf_fifo_handle_t handle, uint32_t *filled_size);

/**
 * @brief  Get the total capacity of the FIFO
 *
 * @param[in]   handle      FIFO handle
 * @param[out]  total_size  Pointer to store the total size in bytes
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid arguments
 */
esp_gmf_err_t esp_gmf_fifo_get_total_size(esp_gmf_fifo_handle_t handle, uint32_t *total_size);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
