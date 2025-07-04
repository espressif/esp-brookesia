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

/**
 * @brief  Handle for the data bus interface
 */
typedef void *esp_gmf_db_handle_t;

/**
 * @brief  Type of data bus
 */
typedef enum {
    DATA_BUS_TYPE_BYTE  = 1,  /*!< Data bus type for byte-oriented data */
    DATA_BUS_TYPE_BLOCK = 2,  /*!< Data bus type for block-oriented data */
} esp_gmf_data_bus_type_t;

/**
 * @brief  Structure representing a block of data for block-oriented data buses
 */
typedef struct {
    uint8_t *buf;         /*!< Pointer to the buffer */
    size_t   buf_length;  /*!< Length of the buffer */
    size_t   valid_size;  /*!< Valid data size in the buffer */
    bool     is_last;     /*!< Flag indicating if the buffer is the last */
} esp_gmf_data_bus_block_t;

/**
 * @brief  Structure representing the operations of a data bus
 */
struct data_bus_op_t {

    esp_gmf_err_t (*deinit)(esp_gmf_db_handle_t handle);                                                     /*!< Deinitialize the data bus */
    esp_gmf_err_t (*read)(esp_gmf_db_handle_t handle, uint8_t *buffer, int buf_len, int block_ticks);        /*!< Read data from the data bus */
    esp_gmf_err_t (*write)(esp_gmf_db_handle_t handle, uint8_t *buffer, int written_size, int block_ticks);  /*!< Write data to the data bus */

    esp_gmf_err_io_t (*acquire_read)(esp_gmf_db_handle_t handle, esp_gmf_data_bus_block_t *blk, uint32_t wanted_size, int block_ticks);  /*!< Acquire a block of data for reading */
    esp_gmf_err_io_t (*release_read)(esp_gmf_db_handle_t handle, esp_gmf_data_bus_block_t *blk, int block_ticks);                        /*!< Release a block of data after reading */

    esp_gmf_err_io_t (*acquire_write)(esp_gmf_db_handle_t handle, esp_gmf_data_bus_block_t *blk, uint32_t wanted_size, int block_ticks);  /*!< Acquire a block of data for writing */
    esp_gmf_err_io_t (*release_write)(esp_gmf_db_handle_t handle, esp_gmf_data_bus_block_t *blk, int block_ticks);                        /*!< Release a block of data after writing */

    esp_gmf_err_t (*done_write)(esp_gmf_db_handle_t handle);                               /*!< Signal that writing to the data bus is done */
    esp_gmf_err_t (*reset_done_write)(esp_gmf_db_handle_t handle);                         /*!< Reset the "done writing" signal */
    esp_gmf_err_t (*reset)(esp_gmf_db_handle_t handle);                                    /*!< Reset the data bus */
    esp_gmf_err_t (*abort)(esp_gmf_db_handle_t handle);                                    /*!< Abort ongoing operations */
    esp_gmf_err_t (*get_total_size)(esp_gmf_db_handle_t handle, uint32_t *size);           /*!< Get the total size of the data bus */
    esp_gmf_err_t (*get_filled_size)(esp_gmf_db_handle_t handle, uint32_t *filled_size);   /*!< Get the filled size of the data bus */
    esp_gmf_err_t (*get_available)(esp_gmf_db_handle_t handle, uint32_t *available_size);  /*!< Get the available size of the data bus */
};

/**
 * @brief  Structure representing a data bus
 */
typedef struct {
    struct data_bus_op_t     op;            /*!< Operations of the data bus */
    void                    *child;         /*!< Pointer to the child */
    void                    *writer;        /*!< Pointer to the writer who call the write associated functions */
    void                    *reader;        /*!< Pointer to the reader who call the read associated functions*/
    const char              *name;          /*!< Name of the data bus */
    esp_gmf_data_bus_type_t  type;          /*!< Type of the data bus */
    int                      max_item_num;  /*!< Maximum number of items */
    int                      max_size;      /*!< Maximum size */
} esp_gmf_data_bus_t;

/**
 * @brief  Configuration for the database
 */
typedef struct {
    void                    *child;         /*!< Pointer to the child */
    const char              *name;          /*!< Name of the database */
    esp_gmf_data_bus_type_t  type;          /*!< Type of the data bus */
    int                      max_item_num;  /*!< Maximum number of items */
    int                      max_size;      /*!< Maximum size */
    bool                     safeguard;     /*!< Flag indicating if the safeguard is enabled */
} esp_gmf_db_config_t;

/**
 * @brief  Initialize data bus with the given configuration
 *
 * @param[in]   db_config  Pointer to the configuration structure
 * @param[out]  hd         Pointer to store the data bus handle after initialization
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid arguments
 *       - ESP_GMF_ERR_MEMORY_LACK  Memory allocation failed
 */
esp_gmf_err_t esp_gmf_db_init(esp_gmf_db_config_t *db_config, esp_gmf_db_handle_t *hd);

/**
 * @brief  Deinitialize data bus, freeing associated resources
 *
 * @param[in]  handle  data bus handle to deinitialize
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_t esp_gmf_db_deinit(esp_gmf_db_handle_t handle);

/**
 * @brief  Read data from data bus
 *
 * @param[in]   handle       data bus handle
 * @param[out]  buffer       Pointer to the buffer to store the read data
 * @param[in]   buf_len      Length of the buffer
 * @param[in]   block_ticks  Maximum time to wait for the read operation
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_t esp_gmf_db_read(esp_gmf_db_handle_t handle, void *buffer, int buf_len, int block_ticks);

/**
 * @brief  Write data to data bus
 *
 * @param[in]  handle       data bus handle
 * @param[in]  buffer       Pointer to the data buffer to be written
 * @param[in]  buf_len      Length of the data buffer
 * @param[in]  block_ticks  Maximum time to wait for the write operation
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_t esp_gmf_db_write(esp_gmf_db_handle_t handle, void *buffer, int buf_len, int block_ticks);

/**
 * @brief  Acquire expected data on data bus
 *         The actual valid size is stored in `blk->valid_size`
 *
 * @param[in]   handle       data bus handle
 * @param[out]  blk          Pointer to the data bus block structure
 * @param[in]   wanted_size  Size of data to acquire
 * @param[in]   block_ticks  Maximum time to wait for the acquire operation
 *
 * @return
 *       - > 0                 The specific length of data being read
 *       - ESP_GMF_IO_OK       Operation successful
 *       - ESP_GMF_IO_FAIL     Operation failed
 *       - ESP_GMF_IO_ABORT    Operation aborted
 *       - ESP_GMF_IO_TIMEOUT  Operation timed out
 */
esp_gmf_err_io_t esp_gmf_db_acquire_read(esp_gmf_db_handle_t handle, esp_gmf_data_bus_block_t *blk, uint32_t wanted_size, int block_ticks);

/**
 * @brief  Release a read operation on data bus
 *
 * @param[in]  handle       data bus handle
 * @param[in]  blk          Pointer to the data bus block structure
 * @param[in]  block_ticks  Maximum time to wait for the release operation
 *
 * @return
 *       - ESP_GMF_IO_OK       Operation successful
 *       - ESP_GMF_IO_FAIL     Operation failed
 *       - ESP_GMF_IO_ABORT    Operation aborted
 *       - ESP_GMF_IO_TIMEOUT  Operation timed out
 */
esp_gmf_err_io_t esp_gmf_db_release_read(esp_gmf_db_handle_t handle, esp_gmf_data_bus_block_t *blk, int block_ticks);

/**
 * @brief  Acquire space to write operation on data bus
 *
 * @param[in]   handle       data bus handle
 * @param[out]  blk          Pointer to the data bus block structure
 * @param[in]   wanted_size  Size of data to acquire
 * @param[in]   block_ticks  Maximum time to wait for the acquire operation
 *
 * @return
 *       - > 0                 The specific length of space can be write
 *       - ESP_GMF_IO_OK       Operation successful
 *       - ESP_GMF_IO_FAIL     Operation failed
 *       - ESP_GMF_IO_ABORT    Operation aborted
 *       - ESP_GMF_IO_TIMEOUT  Operation timed out
 */
esp_gmf_err_io_t esp_gmf_db_acquire_write(esp_gmf_db_handle_t handle, esp_gmf_data_bus_block_t *blk, uint32_t wanted_size, int block_ticks);

/**
 * @brief  Release a write operation on data bus
 *
 * @param[in]  handle       data bus handle
 * @param[in]  blk          Pointer to the data bus block structure
 * @param[in]  block_ticks  Maximum time to wait for the release operation
 *
 * @return
 *       - ESP_GMF_IO_OK       Operation successful
 *       - ESP_GMF_IO_FAIL     Operation failed
 *       - ESP_GMF_IO_ABORT    Operation aborted
 *       - ESP_GMF_IO_TIMEOUT  Operation timed out
 */
esp_gmf_err_io_t esp_gmf_db_release_write(esp_gmf_db_handle_t handle, esp_gmf_data_bus_block_t *blk, int block_ticks);

/**
 * @brief  Mark a write operation as done on data bus
 *
 * @param[in]  handle  data bus handle
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_t esp_gmf_db_done_write(esp_gmf_db_handle_t handle);

/**
 * @brief  Reset the status of a completed write operation on data bus
 *
 * @param[in]  handle  data bus handle
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_t esp_gmf_db_reset_done_write(esp_gmf_db_handle_t handle);

/**
 * @brief  Reset data bus, clearing any pending operations and resetting status
 *
 * @param[in]  handle  data bus handle
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_t esp_gmf_db_reset(esp_gmf_db_handle_t handle);

/**
 * @brief  Abort exit the data bus
 *
 * @param[in]  handle  data bus handle
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_t esp_gmf_db_abort(esp_gmf_db_handle_t handle);

/**
 * @brief  Get the total size of data bus buffer
 *
 * @param[in]   handle     data bus handle
 * @param[out]  buff_size  Pointer to store the total buffer size
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_t esp_gmf_db_get_total_size(esp_gmf_db_handle_t handle, uint32_t *buff_size);

/**
 * @brief  Get the filled size of data bus buffer
 *
 * @param[in]   handle       data bus handle
 * @param[out]  filled_size  Pointer to store the filled buffer size
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_t esp_gmf_db_get_filled_size(esp_gmf_db_handle_t handle, uint32_t *filled_size);

/**
 * @brief  Get the available size in data bus buffer
 *
 * @param[in]   handle          data bus handle
 * @param[out]  available_size  Pointer to store the available buffer size
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_t esp_gmf_db_get_available(esp_gmf_db_handle_t handle, uint32_t *available_size);

/**
 * @brief  Set the writer holder for data bus
 *
 * @param[in]  handle  data bus handle
 * @param[in]  holder  Pointer to the writer holder
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_t esp_gmf_db_set_writer(esp_gmf_db_handle_t handle, void *holder);

/**
 * @brief  Get the writer holder from data bus
 *
 * @param[in]   handle  data bus handle
 * @param[out]  holder  Pointer to store the writer holder
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_t esp_gmf_db_get_writer(esp_gmf_db_handle_t handle, void **holder);

/**
 * @brief  Set the reader holder for data bus
 *
 * @param[in]  handle  data bus handle
 * @param[in]  holder  Pointer to the reader holder
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_t esp_gmf_db_set_reader(esp_gmf_db_handle_t handle, void *holder);

/**
 * @brief  Get the reader holder from data bus
 *
 * @param[in]   handle  data bus handle
 * @param[out]  holder  Pointer to store the reader holder
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_t esp_gmf_db_get_reader(esp_gmf_db_handle_t handle, void **holder);

/**
 * @brief  Get the data bus type of data bus
 *
 * @param[in]   handle   data bus handle
 * @param[out]  db_type  Pointer to store the data bus type
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_t esp_gmf_db_get_type(esp_gmf_db_handle_t handle, esp_gmf_data_bus_type_t *db_type);

/**
 * @brief  Get the name of data bus
 *
 * @param[in]  handle  data bus handle
 *
 * @return
 *       - Pointer  to the name of the data bus
 */
const char *esp_gmf_db_get_name(esp_gmf_db_handle_t handle);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
