/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_gmf_err.h"
#include "esp_gmf_obj.h"
#include "esp_gmf_info.h"
#include "esp_gmf_payload.h"
#include "esp_gmf_task.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Handle to a GMF I/O
 */
typedef void *esp_gmf_io_handle_t;

/**
 * @brief  Enumeration for the direction of a GMF I/O (none, reader, writer)
 */
typedef enum {
    ESP_GMF_IO_DIR_NONE   = 0,  /*!< No direction */
    ESP_GMF_IO_DIR_READER = 1,  /*!< Reader direction */
    ESP_GMF_IO_DIR_WRITER = 2,  /*!< Writer direction */
} esp_gmf_io_dir_t;

/**
 * @brief  Enumeration for the type of data handled by a GMF I/O (byte or block)
 */
typedef enum {
    ESP_GMF_IO_TYPE_BYTE  = 1,  /*!< Byte type */
    ESP_GMF_IO_TYPE_BLOCK = 2,  /*!< Block type */
} esp_gmf_io_type_t;

/**
 * @brief  Configuration structure for a GMF I/O
 */
typedef struct {
    esp_gmf_task_config_t  thread;  /*!< Task configuration */
} esp_gmf_io_cfg_t;

/**
 * @brief  Structure representing a GMF I/O
 */
typedef struct esp_gmf_io_ {
    struct esp_gmf_obj_ parent;                                     /*!< Parent object */
    esp_gmf_err_t (*open)(esp_gmf_io_handle_t obj);                 /*!< Open callback function */
    esp_gmf_err_t (*seek)(esp_gmf_io_handle_t obj, uint64_t data);  /*!< Seek callback function */
    esp_gmf_err_t (*close)(esp_gmf_io_handle_t obj);                /*!< Close callback function */

    /*!< Previous close callback function
     *   For some block IO instances, this function can be called before the `close` operation
     */
    esp_gmf_err_t (*prev_close)(esp_gmf_io_handle_t handle);

    /*!< Process callback function
     *   If the thread is valid, this function should be implemented to register the GMF task
     */
    esp_gmf_job_err_t (*process)(esp_gmf_io_handle_t handle, void *params);  /*!< Process function for GMF task calling if it's exist */

    esp_gmf_err_io_t (*acquire_read)(esp_gmf_io_handle_t handle, void *payload, uint32_t wanted_size, int block_ticks);   /*!< Acquire read callback function */
    esp_gmf_err_io_t (*release_read)(esp_gmf_io_handle_t handle, void *payload, int block_ticks);                         /*!< Release read callback function */
    esp_gmf_err_io_t (*acquire_write)(esp_gmf_io_handle_t handle, void *payload, uint32_t wanted_size, int block_ticks);  /*!< Acquire write callback function */
    esp_gmf_err_io_t (*release_write)(esp_gmf_io_handle_t handle, void *payload, int block_ticks);                        /*!< Release write callback function */

    esp_gmf_task_handle_t  task_hd;  /*!< Task handle */
    esp_gmf_io_dir_t       dir;      /*!< I/O direction */
    esp_gmf_io_type_t      type;     /*!< I/O type */
    esp_gmf_info_file_t    attr;     /*!< File attribute */
} esp_gmf_io_t;

/**
 * @brief  Initialize a I/O handle with the given configuration
 *         If the stack size is greater than 0, a GMF task is created with the process job registered
 *
 * @param[in]  handle  GMF I/O handle to initialize
 * @param[in]  cfg     Pointer to the configuration structure
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 *       - ESP_GMF_ERR_MEMORY_LACK  Insufficient memory to perform the job registration
 *       - ESP_GMF_ERR_FAIL         Failed to create thread
 */
esp_gmf_err_t esp_gmf_io_init(esp_gmf_io_handle_t handle, esp_gmf_io_cfg_t *cfg);

/**
 * @brief  Deinitialize a GMF I/O handle, freeing associated resources
 *
 * @param[in]  handle  GMF I/O handle to deinitialize
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_t esp_gmf_io_deinit(esp_gmf_io_handle_t handle);

/**
 * @brief  Open the specific I/O handle, run the thread if it is valid
 *
 * @param[in]  handle  GMF I/O handle to open
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_t esp_gmf_io_open(esp_gmf_io_handle_t handle);

/**
 * @brief  Seek to a specific byte position in the speoific I/O handle
 *         If the IO thread is invalid, only the IO's seek function is called.
 *         If the IO thread is valid, the seek procedure is as follows: first,
 *         perform a previous close, then pause the thread, seek to the specified position, reopen if necessary.
 *         Finally, resume the thread if it was paused; otherwise, re-register the process job
 *
 * @param[in]  handle         GMF I/O handle
 * @param[in]  seek_byte_pos  Byte position to seek to
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 *       - Others                   Indicating failure
 */
esp_gmf_err_t esp_gmf_io_seek(esp_gmf_io_handle_t handle, uint64_t seek_byte_pos);

/**
 * @brief  Close a GMF I/O handle
 *         If both prev_close and thread are valid, they will be called before executing the IO close operation
 *
 * @param[in]  handle  GMF I/O handle to close
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_t esp_gmf_io_close(esp_gmf_io_handle_t handle);

/**
 * @brief  Acquire read access to the specific I/O handle
 *
 * @param[in]   handle       GMF I/O handle
 * @param[out]  load         Pointer to store the acquired payload
 * @param[in]   wanted_size  Desired payload size
 * @param[in]   wait_ticks   Timeout value in ticks, in milliseconds
 *
 * @return
 *       - > 0                 The specific length of data being read
 *       - ESP_GMF_IO_OK       Operation successful
 *       - ESP_GMF_IO_FAIL     Operation failed or invalid arguments
 *       - ESP_GMF_IO_ABORT    Operation aborted
 *       - ESP_GMF_IO_TIMEOUT  Operation timed out
 */
esp_gmf_err_io_t esp_gmf_io_acquire_read(esp_gmf_io_handle_t handle, esp_gmf_payload_t *load, uint32_t wanted_size, int wait_ticks);

/**
 * @brief  Release read access to the specific I/O handle
 *
 * @param[in]  handle      GMF I/O handle
 * @param[in]  load        Pointer to the payload to release
 * @param[in]  wait_ticks  Timeout value in ticks
 *
 * @return
 *       - ESP_GMF_IO_OK       Operation successful
 *       - ESP_GMF_IO_FAIL     Operation failed or invalid arguments
 *       - ESP_GMF_IO_ABORT    Operation aborted
 *       - ESP_GMF_IO_TIMEOUT  Operation timed out
 */
esp_gmf_err_io_t esp_gmf_io_release_read(esp_gmf_io_handle_t handle, esp_gmf_payload_t *load, int wait_ticks);

/**
 * @brief  Acquire write access to the specific I/O handle
 *
 * @param[in]   handle       GMF I/O handle
 * @param[out]  load         Pointer to store the acquired payload
 * @param[in]   wanted_size  Desired payload size
 * @param[in]   wait_ticks   Timeout value in ticks, in milliseconds
 *
 * @return
 *       - > 0                 The specific length of space can be write
 *       - ESP_GMF_IO_OK       Operation successful
 *       - ESP_GMF_IO_FAIL     Operation failed or invalid arguments
 *       - ESP_GMF_IO_ABORT    Operation aborted
 *       - ESP_GMF_IO_TIMEOUT  Operation timed out
 */
esp_gmf_err_io_t esp_gmf_io_acquire_write(esp_gmf_io_handle_t handle, esp_gmf_payload_t *load, uint32_t wanted_size, int wait_ticks);

/**
 * @brief  Release write access to the specific I/O handle
 *
 * @param[in]  handle      GMF I/O handle
 * @param[in]  load        Pointer to the payload to release
 * @param[in]  wait_ticks  Timeout value in ticks
 *
 * @return
 *       - ESP_GMF_IO_OK       Operation successful
 *       - ESP_GMF_IO_FAIL     Operation failed or invalid arguments
 *       - ESP_GMF_IO_ABORT    Operation aborted
 *       - ESP_GMF_IO_TIMEOUT  Operation timed out
 */
esp_gmf_err_io_t esp_gmf_io_release_write(esp_gmf_io_handle_t handle, esp_gmf_payload_t *load, int wait_ticks);

/**
 * @brief  Set information for the specific I/O handle
 *
 * @param[in]  handle  GMF I/O handle
 * @param[in]  info    Pointer to the file information structure
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_t esp_gmf_io_set_info(esp_gmf_io_handle_t handle, esp_gmf_info_file_t *info);

/**
 * @brief  Get information from the specific I/O handle
 *
 * @param[in]   handle  GMF I/O handle
 * @param[out]  info    Pointer to store the file information
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_t esp_gmf_io_get_info(esp_gmf_io_handle_t handle, esp_gmf_info_file_t *info);

/**
 * @brief  Set the URI for the specific I/O handle
 *
 * @param[in]  handle  GMF I/O handle
 * @param[in]  uri     Pointer to the URI string
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 *       - ESP_GMF_ERR_MEMORY_LACK  Memory allocation failed
 */
esp_gmf_err_t esp_gmf_io_set_uri(esp_gmf_io_handle_t handle, const char *uri);

/**
 * @brief  Get the URI from the specific I/O handle
 *
 * @param[in]   handle  GMF I/O handle
 * @param[out]  uri     Pointer to store the URI string
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_t esp_gmf_io_get_uri(esp_gmf_io_handle_t handle, char **uri);

/**
 * @brief  Set the byte position for the specific I/O handle
 *
 * @param[in]  handle    GMF I/O handle
 * @param[in]  byte_pos  Byte position to set
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_t esp_gmf_io_set_pos(esp_gmf_io_handle_t handle, uint64_t byte_pos);

/**
 * @brief  Update the byte position for the specific I/O handle
 *
 * @param[in]  handle    GMF I/O handle
 * @param[in]  byte_pos  Byte position to update
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_t esp_gmf_io_update_pos(esp_gmf_io_handle_t handle, uint64_t byte_pos);

/**
 * @brief  Get the current byte position from the specific I/O handle
 *
 * @param[in]   handle    GMF I/O handle
 * @param[out]  byte_pos  Pointer to store the current byte position
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_t esp_gmf_io_get_pos(esp_gmf_io_handle_t handle, uint64_t *byte_pos);

/**
 * @brief  Set the total size for the specific I/O handle
 *
 * @param[in]  handle      GMF I/O handle
 * @param[in]  total_size  Total size to set
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_t esp_gmf_io_set_size(esp_gmf_io_handle_t handle, uint64_t total_size);

/**
 * @brief  Get the total size from the specific I/O handle
 *
 * @param[in]   handle      GMF I/O handle
 * @param[out]  total_size  Pointer to store the total size
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_t esp_gmf_io_get_size(esp_gmf_io_handle_t handle, uint64_t *total_size);

/**
 * @brief  Get the I/O type of the specific I/O handle
 *
 * @param[in]   handle  GMF I/O handle
 * @param[out]  type    Pointer to store the I/O type
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_t esp_gmf_io_get_type(esp_gmf_io_handle_t handle, esp_gmf_io_type_t *type);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
