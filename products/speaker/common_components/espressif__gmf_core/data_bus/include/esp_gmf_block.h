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
 * @brief  GMF block buffer is an interface for passing buffer addresses without generating any copies.
 *         It first allocates a memory block of size block_size * block_cnt. Each time the Acquire API is called,
 *         it returns the starting address of each block, and each Acquire size recommendation is divisible by the total cache size.
 *         Acquire size also supports any value within the total size, and the recommended Acquire size that is divisible
 *         by the total has the best performance. After esp_gmf_block_acquire_write and esp_gmf_block_release_write,
 *         esp_gmf_block_acquire_read can obtain the written data.
 *
 *         These interfaces are thread-safe, and esp_gmf_block_acquire_write and esp_gmf_block_acquire_read also have blocking functionality.
 */

/**
 * @brief  Represents a handle to a block buffer
 */
typedef void *esp_gmf_block_handle_t;

/**
 * @brief  Create a block buffer with total size = block_size * block_cnt
 *
 * @param[in]   block_size  Size of each block
 * @param[in]   block_cnt   Number of blocks
 * @param[out]  handle      Pointer to store the handle to the created block buffer
 *
 * @return
 *       - ESP_GMF_ERR_OK           Operation successful
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument provided
 *       - ESP_GMF_ERR_MEMORY_LACK  Memory allocation failed
 */
esp_gmf_err_t esp_gmf_block_create(int block_size, int block_cnt, esp_gmf_block_handle_t *handle);

/**
 * @brief  Cleanup and free all memory created by esp_gmf_block_create
 *
 * @param[in]  handle  The block handle
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_t esp_gmf_block_destroy(esp_gmf_block_handle_t handle);

/**
 * @brief  Retrieve the address of a valid data buffer from the internal buffer for reading, without performing any copying.
 *         If there is insufficient data and block_ticks is greater than 0, the function will block until enough data becomes available.
 *
 * @note  1. It's recommended to set `wanted_size` either equal to `block_size` or to ensure
 *           that the total size divided by `wanted_size` results in no remainder. This ensures expected
 *           behavior, particularly when reaching the end of the buffer, where the return value may not
 *           match the expected value due to the buffer's inability to wrap from the tail to the head.
 *        2. The obtained buffer address is internal and should not be freed externally
 *        3. The `wanted_size` can't be greater than block_size * block_cnt
 *        4. The actual valid size is stored in `blk->valid_size`
 *
 * @param[in]   handle       Handle to the block buffer
 * @param[out]  blk          Pointer to the data bus block structure to store the acquired data
 * @param[in]   wanted_size  Desired size of data to be acquired
 * @param[in]   block_ticks  Ticks to wait for the operation
 *
 * @return
 *       - > 0                 The specific length of data being read
 *       - ESP_GMF_IO_OK       Operation succeeded
 *       - ESP_GMF_IO_FAIL     Invalid arguments, or wanted_size > total size
 *       - ESP_GMF_IO_TIMEOUT  Operation timed out
 */
esp_gmf_err_io_t esp_gmf_block_acquire_read(esp_gmf_block_handle_t handle, esp_gmf_data_bus_block_t *blk, uint32_t wanted_size, int block_ticks);

/**
 * @brief  Returned acquired buffer address to the specific handle
 *
 * @note  1. The returned buffer MUST be acquired from `esp_gmf_block_acquire_read` to ensure proper synchronization
 *        2. `esp_gmf_block_acquire_read` and `esp_gmf_block_release_read` must be called in pairs, and consecutive acquisition
 *           followed by consecutive release is not allowed
 *        3. If the buffer address reaches the end of the internal buffer, the read pointer will be reset to the beginning of the internal buffer
 *
 * @param[in]  handle       Handle to the block buffer
 * @param[in]  blk          Pointer to the data bus block structure containing the data to be released
 * @param[in]  block_ticks  Ticks to wait for the operation
 *
 * @return
 *       - ESP_GMF_IO_OK    Operation succeeded
 *       - ESP_GMF_IO_FAIL  Invalid arguments, or the buffer does not belong to the provided handle
 */
esp_gmf_err_io_t esp_gmf_block_release_read(esp_gmf_block_handle_t handle, esp_gmf_data_bus_block_t *blk, int block_ticks);

/**
 * @brief  Acquire space corresponding to the desired size within a specific handle
 *         If there is insufficient space and block_ticks is greater than 0, the function will block until enough space becomes available.
 *
 * @note  1. `esp_gmf_block_acquire_write` and `esp_gmf_block_release_write` must be called in pairs, and consecutive acquisition
 *           followed by consecutive release is not allowed
 *        2. The obtained buffer address is internal and should not be freed externally
 *        3. The `wanted_size` can't be greater than block_size * block_cnt
 *
 * @param[in]   handle       Handle to the block buffer
 * @param[out]  blk          Pointer to the data bus block structure to store the acquired space
 * @param[in]   wanted_size  Desired size of space to be acquired
 * @param[in]   block_ticks  Ticks to wait for the operation
 *
 * @return
 *       - > 0                 The specific length of space can be write
 *       - ESP_GMF_IO_OK       Operation succeeded, or it's done to write
 *       - ESP_GMF_IO_FAIL     Invalid arguments, or no filled data
 *       - ESP_GMF_IO_TIMEOUT  Operation timed out
 */
esp_gmf_err_io_t esp_gmf_block_acquire_write(esp_gmf_block_handle_t handle, esp_gmf_data_bus_block_t *blk, uint32_t wanted_size, int block_ticks);

/**
 * @brief  Returned acquired buffer address to the specific handle
 *
 * @note  1. The returned buffer MUST be acquired from `esp_gmf_block_acquire_write` to ensure proper synchronization
 *        2. `esp_gmf_block_acquire_write` and `esp_gmf_block_release_write` must be called in pairs, and consecutive acquisition
 *            followed by consecutive release is not allowed
 *        3. If the buffer address reaches the end of the internal buffer, the write pointer will be reset to the beginning of the internal buffer
 *
 * @param[in]  handle       Handle to the block buffer
 * @param[in]  blk          Pointer to the data bus block structure containing the space to be released
 * @param[in]  block_ticks  Ticks to wait for the operation
 *
 * @return
 *       - ESP_GMF_IO_OK    Operation succeeded
 *       - ESP_GMF_IO_FAIL  Invalid arguments, or the buffer does not belong to the provided handle
 */
esp_gmf_err_io_t esp_gmf_block_release_write(esp_gmf_block_handle_t handle, esp_gmf_data_bus_block_t *blk, int block_ticks);

/**
 * @brief  Set status of writing to is done
 *
 * @note  This function just mark the done flag, which is actually used in esp_gmf_block_release_write to ensure that the
 *        last frame of data can be read. So it will be called in esp_gmf_block_release_write only.
 *
 * @param[in]  handle  The block handle
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_t esp_gmf_block_done_write(esp_gmf_block_handle_t handle);

/**
 * @brief  Abort waiting if reading or writing is blocking
 *
 * @param[in]  handle  The block handle
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_t esp_gmf_block_abort(esp_gmf_block_handle_t handle);

/**
 * @brief  Reset, clear all values as initial state
 *
 * @param[in]  handle  The block handle
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_t esp_gmf_block_reset(esp_gmf_block_handle_t handle);

/**
 * @brief  Get the free size of a GMF block
 *
 * @param[in]   handle     The handle to the GMF block.
 * @param[out]  free_size  Pointer to a variable where the free size will be stored.
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_t esp_gmf_block_get_free_size(esp_gmf_block_handle_t handle, uint32_t *free_size);

/**
 * @brief  Get the total size of a GMF block
 *
 * @param[in]   handle      The handle to the GMF block
 * @param[out]  total_size  Pointer to a variable where the total size of the block will be stored
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_t esp_gmf_block_get_total_size(esp_gmf_block_handle_t handle, uint32_t *total_size);

/**
 * @brief  Get the filled size of a GMF block
 *
 * @param[in]   handle       The handle to the GMF block
 * @param[out]  filled_size  Pointer to a variable where the filled size will be stored
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_t esp_gmf_block_get_filled_size(esp_gmf_block_handle_t handle, uint32_t *filled_size);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
