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
 * @brief  GMF ringbuffer represents a ring buffer. The buffer size is determined by
 *         multiplying the block size with the number of blocks.Read and write operations
 *         occur on a byte-by-byte basis and do not offer direct access to the internal
 *         buffer address. Its access is thread-safe. The 'esp_gmf_rb_acquire_read' and
 *         'esp_gmf_rb_release_write' perform read and write operations as copies, respectively.
 *         `esp_gmf_rb_release_read` and `esp_gmf_rb_acquire_write` have no practical significance.
 *
 *         The `esp_gmf_rb_release_write` and `esp_gmf_rb_acquire_read` have blocking functionality.
 */

/**
 * @brief  Handle to the ring buffer
 */
typedef void *esp_gmf_rb_handle_t;

/**
 * @brief  Create a ring buffer with total size = block_size * n_blocks
 *
 * @param[in]   block_size  Size of each block
 * @param[in]   n_blocks    Number of blocks
 * @param[out]  handle      Pointer to store the handle to the created ringbufer buffer
 *
 * @return
 *       - ESP_GMF_ERR_OK           Operation successful
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument provided
 */
esp_gmf_err_t esp_gmf_rb_create(int block_size, int n_blocks, esp_gmf_rb_handle_t *handle);

/**
 * @brief  Cleanup and free all memory allocated for the ring buffer
 *
 * @param[in]  handle  The Ringbuffer handle
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid arguments
 */
esp_gmf_err_t esp_gmf_rb_destroy(esp_gmf_rb_handle_t handle);

/**
 * @brief  Reset the ring buffer, clearing all values to the initial state
 *
 * @param[in]  handle  The Ringbuffer handle
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid arguments
 */
esp_gmf_err_t esp_gmf_rb_reset(esp_gmf_rb_handle_t handle);

/**
 * @brief  Copy valid data from ring buffer to the given buffer with specific handle
 *         When the ring buffer handle cannot provide enough size, it will block for the duration specified by block_ticks.
 *         The actual valid size is stored in `blk->valid_size`
 *
 * @param[in]   handle         The Ringbuffer handle
 * @param[out]  blk            Pointer to the data block structure to be filled
 * @param[in]   wanted_size    Desired size to read
 * @param[in]   ticks_to_wait  Maximum duration to wait for the operation to complete
 *
 * @return
 *       - > 0                 The specific length of data being read
 *       - ESP_GMF_IO_OK       Operation succeeded
 *       - ESP_GMF_IO_FAIL     Invalid arguments
 *       - ESP_GMF_IO_TIMEOUT  Operation timed out
 *       - ESP_GMF_IO_ABORT    Operation aborted
 */
esp_gmf_err_io_t esp_gmf_rb_acquire_read(esp_gmf_rb_handle_t handle, esp_gmf_data_bus_block_t *blk, uint32_t wanted_size, int ticks_to_wait);

/**
 * @brief  Release the read operation
 *
 * @note  It's do nothing due to read acquire is a copy operation
 *
 * @param[in]  handle       The Ringbuffer handle
 * @param[in]  blk          Pointer to the data block structure to release
 * @param[in]  block_ticks  Maximum duration to wait for the operation to complete
 *
 * @return
 *       - ESP_GMF_IO_OK  Operation succeeded
 */
esp_gmf_err_io_t esp_gmf_rb_release_read(esp_gmf_rb_handle_t handle, esp_gmf_data_bus_block_t *blk, int block_ticks);

/**
 * @brief  Acquire space for write
 *
 * @note  It's do nothing due to write acquire is a copy operation
 *
 * @param[in]   handle         The Ringbuffer handle
 * @param[out]  blk            Pointer to the data block structure to be filled
 * @param[in]   wanted_size    Desired size to write
 * @param[in]   ticks_to_wait  Maximum duration to wait for the operation to complete
 *
 * @return
 *       - > 0                 The specific length of space can be write
 *       - ESP_GMF_IO_OK       Operation succeeded
 *       - ESP_GMF_IO_FAIL     Invalid arguments
 *       - ESP_GMF_IO_TIMEOUT  Operation timed out
 *       - ESP_GMF_IO_ABORT    Operation aborted
 */
esp_gmf_err_io_t esp_gmf_rb_acquire_write(esp_gmf_rb_handle_t handle, esp_gmf_data_bus_block_t *blk, uint32_t wanted_size, int ticks_to_wait);

/**
 * @brief  Copy the given buffer to the ring buffer with specific handle
 *         When the ring buffer handle cannot accommodate the given buffer size, it will block for the duration specified by block_ticks.
 *
 * @param[in]  handle       The Ringbuffer handle
 * @param[in]  blk          Pointer to the data block structure to release
 * @param[in]  block_ticks  Maximum duration to wait for the operation to complete
 *
 * @return
 *       - ESP_GMF_IO_OK       Operation succeeded
 *       - ESP_GMF_IO_FAIL     Invalid arguments
 *       - ESP_GMF_IO_TIMEOUT  Operation timed out
 *       - ESP_GMF_IO_ABORT    Operation aborted
 */
esp_gmf_err_io_t esp_gmf_rb_release_write(esp_gmf_rb_handle_t handle, esp_gmf_data_bus_block_t *blk, int block_ticks);

/**
 * @brief  Abort any pending operations on the ring buffer
 *
 * @param[in]  handle  The Ringbuffer handle
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid arguments
 */
esp_gmf_err_t esp_gmf_rb_abort(esp_gmf_rb_handle_t handle);

/**
 * @brief  Set the status of writing to the ring buffer as done
 *
 * @param[in]  handle  The Ringbuffer handle
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid arguments
 */
esp_gmf_err_t esp_gmf_rb_done_write(esp_gmf_rb_handle_t handle);

/**
 * @brief  Reset the status of writing to the ring buffer as not done
 *
 * @param[in]  handle  The Ringbuffer handle
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid arguments
 */
esp_gmf_err_t esp_gmf_rb_reset_done_write(esp_gmf_rb_handle_t handle);

/**
 * @brief  Unblock the reader of the ring buffer
 *
 * @param[in]  handle  The Ringbuffer handle
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid arguments
 */
esp_gmf_err_t esp_gmf_rb_unblock_reader(esp_gmf_rb_handle_t handle);

/**
 * @brief  Get the number of bytes available for reading from the ring buffer
 *
 * @param[in]   handle          The Ringbuffer handle
 * @param[out]  available_size  Pointer to store the available size
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid arguments
 */
esp_gmf_err_t esp_gmf_rb_bytes_available(esp_gmf_rb_handle_t handle, uint32_t *available_size);

/**
 * @brief  Get the number of bytes filled in the ring buffer
 *
 * @param[in]   handle       The Ringbuffer handle
 * @param[out]  filled_size  Pointer to store the filled size
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid arguments
 */
esp_gmf_err_t esp_gmf_rb_bytes_filled(esp_gmf_rb_handle_t handle, uint32_t *filled_size);

/**
 * @brief  Get the total size of the ring buffer
 *
 * @param[in]   handle      The Ringbuffer handle
 * @param[out]  valid_size  Pointer to store the total size
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid arguments
 */
esp_gmf_err_t esp_gmf_rb_get_size(esp_gmf_rb_handle_t handle, uint32_t *valid_size);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
