/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdbool.h>
#include "sys/queue.h"
#include "esp_gmf_err.h"
#include "esp_gmf_payload.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define ESP_GMF_PORT_CHECK(log_tag, ret, ret_value, action, format, ...) {                              \
    if (unlikely(ret < ESP_GMF_IO_OK)) {                                                                \
        if (ret != ESP_GMF_IO_ABORT) {                                                                  \
            ESP_LOGE(log_tag, "%s(%d): " format, __FUNCTION__, __LINE__ __VA_OPT__(, ) __VA_ARGS__);    \
            ret_value = ESP_GMF_ERR_FAIL;                                                               \
        } else {                                                                                        \
            ret_value = ESP_GMF_ERR_OK;                                                                 \
        }                                                                                               \
        action;                                                                                         \
    }                                                                                                   \
}

#define ESP_GMF_PORT_ACQUIRE_IN_CHECK(TAG, ret, ret_value, action) ESP_GMF_PORT_CHECK(TAG, ret, ret_value, action, "Failed to acquire in, ret: %d", ret)

#define ESP_GMF_PORT_ACQUIRE_OUT_CHECK(TAG, ret, ret_value, action) ESP_GMF_PORT_CHECK(TAG, ret, ret_value, action, "Failed to acquire out, ret: %d", ret)

#define ESP_GMF_PORT_RELEASE_IN_CHECK(TAG, ret, ret_value, action) ESP_GMF_PORT_CHECK(TAG, ret, ret_value, action, "Failed to release in, ret: %d", ret)

#define ESP_GMF_PORT_RELEASE_OUT_CHECK(TAG, ret, ret_value, action) ESP_GMF_PORT_CHECK(TAG, ret, ret_value, action, "Failed to release out, ret: %d", ret)

/**
 * @brief  Definition the direction of a GMF port (input or output)
 */
#define ESP_GMF_PORT_DIR_IN   (0)  /*!< Input port */
#define ESP_GMF_PORT_DIR_OUT  (1)  /*!< Output port */

/**
 * @brief  Definition the bit mask for the type of data handled by a GMF port (byte or block)
 *         - A byte-type port transfers values by copying data byte by byte. Users must allocate memory,
 *           which means data transmission involves copying. This type of port allows convenient access to arbitrary byte lengths
 *         - A block-type port transfers data by passing memory addresses. The memory used by the user is provided by another source,
 *           so data transmission does not involve copying. However, accessing arbitrary byte lengths is less flexible and may require data concatenation
 */
#define ESP_GMF_PORT_TYPE_BYTE  (0x01)  /*!< Bit0 for the byte type of GMF port */
#define ESP_GMF_PORT_TYPE_BLOCK (0x02)  /*!< Bit1 for the block type of GMF port */

/**
 * @brief  Handle to a GMF port
 */
typedef struct esp_gmf_port_ *esp_gmf_port_handle_t;

/**
 * @brief  Function pointer type for acquiring data from a port
 */
typedef esp_gmf_err_io_t (*port_acquire)(void *handle, esp_gmf_payload_t *load, uint32_t wanted_size, int wait_ticks);

/**
 * @brief  Function pointer type for releasing data from a port
 */
typedef esp_gmf_err_io_t (*port_release)(void *handle, esp_gmf_payload_t *load, int wait_ticks);

/**
 * @brief  Function pointer type for freeing a port
 */
typedef void (*port_free)(void *p);

/**
 * @brief  Structure defining the I/O operations of a GMF port
 */
typedef struct {
    port_acquire  acquire;  /*!< Function pointer for acquiring data */
    port_release  release;  /*!< Function pointer for releasing data */
    port_free     del;      /*!< Function pointer for freeing the port */
} esp_gmf_port_io_ops_t;

/**
 * @brief  Structure defining the attributes of a GMF port
 */
typedef struct {
    uint8_t  buf_addr_aligned;  /*!< Byte-align the address of the buffer */
    uint8_t  buf_size_aligned;  /*!< Byte-align the length of the buffer */
    uint8_t  dir;               /*!< Port direction */
    uint8_t  type;              /*!< Port type */
} esp_gmf_port_attr_t;

/**
 * @brief  Structure representing a GMF port
 *         The usage of the port in linked elements is as follows
 *
 *          +---------+     +---------------+    +----------+
 *          | In Port +-----> First Element +----> Out Port |
 *          +---------+     +-------+-------+    +----------+
 *                                  |
 *                                  v
 *          +---------+     +-------+-------+    +----------+
 *          | In Port +-----> More Element  +----> Out Port |
 *          +---------+     +-------+-------+    +----------+
 *                                  |
 *                                  v
 *          +---------+     +-------+-------+    +----------+
 *          | In Port +-----> Last Element  +----> Out Port |
 *          +---------+     +---------------+    +----------+
 */
typedef struct esp_gmf_port_ {
    struct esp_gmf_port_  *next;           /*!< Pointer to the next port */
    void                  *writer;         /*!< Acquire out functions caller with the port */
    void                  *reader;         /*!< Acquire in functions caller with the port */
    esp_gmf_port_io_ops_t  ops;            /*!< I/O operations of the port */
    esp_gmf_port_attr_t    attr;           /*!< Port attributes */
    int                    data_length;    /*!< Data length of the payload */
    void                  *ctx;            /*!< User context for the port */
    int                    wait_ticks;     /*!< Timeout for port operations */
    esp_gmf_payload_t     *payload;        /*!< Payload pointer to be set */
    uint8_t                is_shared : 1;  /*!< Payload is shared to the next element port or not, 1 for shared (default), 0 for dedicated */
    esp_gmf_payload_t     *self_payload;   /*!< Self payload of the port */
    struct esp_gmf_port_  *ref_port;       /*!< Pointer to the reference port */
    int8_t                 ref_count;      /*!< Reference count indicating the number of active references */
} esp_gmf_port_t;

/**
 * @brief  Structure defining the configuration of a GMF port
 */
typedef struct esp_gmf_port_config_ {
    uint8_t                dir;          /*!< Direction of the port */
    uint8_t                type;         /*!< Type of data handled by the port */
    esp_gmf_port_io_ops_t  ops;          /*!< I/O operations of the port */
    void                  *ctx;          /*!< User context associated with the port */
    int                    data_length;  /*!< Data length of the port */
    int                    wait_ticks;   /*!< Timeout for port operations */
} esp_gmf_port_config_t;

/**
 * @brief  Initialize a GMF port with the given configuration
 *
 * @param[in]   cfg         Pointer to the configuration structure
 * @param[out]  out_result  Pointer to store the result handle after initialization
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 *       - ESP_GMF_ERR_MEMORY_LACK  Memory allocate failed
 */
esp_gmf_err_t esp_gmf_port_init(esp_gmf_port_config_t *cfg, esp_gmf_port_handle_t *out_result);

/**
 * @brief  Deinitialize a GMF port
 *
 * @param[in]  handle  The handle of the port to be deinitialized
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_t esp_gmf_port_deinit(esp_gmf_port_handle_t handle);

/**
 * @brief  Set the self payload for the specific port
 *
 * @param[in]  handle  The handle of the port
 * @param[in]  load    Pointer to the payload structure
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_t esp_gmf_port_set_payload(esp_gmf_port_handle_t handle, esp_gmf_payload_t *load);

/**
 * @brief  Clean the payload done status of a GMF port
 *
 * @param[in]  handle  The handle of the port
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_t esp_gmf_port_clean_payload_done(esp_gmf_port_handle_t handle);

/**
 * @brief  Enables or disables shared payload for the specified port
 *
 * @param[in]  handle  The port handle to enable or disable payload sharing on
 * @param[in]  enable  Set to true to enable payload sharing, or false to disable it
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument provided
 */
esp_gmf_err_t esp_gmf_port_enable_payload_share(esp_gmf_port_handle_t handle, bool enable);

/**
 * @brief  Reset the port payload and variable of self payload
 *
 * @param[in]  handle  The handle of the port
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_t esp_gmf_port_reset(esp_gmf_port_handle_t handle);

/**
 * @brief  Set the wait ticks for the specific port
 *
 * @param[in]  handle         The handle of the port
 * @param[in]  wait_ticks_ms  Number of milliseconds to wait
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_t esp_gmf_port_set_wait_ticks(esp_gmf_port_handle_t handle, int wait_ticks_ms);

/**
 * @brief  Set the esp_gmf_port_acquire_in and esp_gmf_port_release_in caller for the specific port
 *
 * @param[in]  handle  The handle of the port
 * @param[in]  reader  Pointer to the reader
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_t esp_gmf_port_set_reader(esp_gmf_port_handle_t handle, void *reader);

/**
 * @brief  Set the esp_gmf_port_acquire_out and esp_gmf_port_acquire_out caller for the specific port
 *
 * @param[in]  handle  The handle of the port
 * @param[in]  writer  Pointer to the writer
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_t esp_gmf_port_set_writer(esp_gmf_port_handle_t handle, void *writer);

/**
 * @brief  Add a GMF port to the end of the list
 *
 * @param[in]  head     The head of the port list
 * @param[in]  io_inst  The port instance to be added
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_t esp_gmf_port_add_last(esp_gmf_port_handle_t head, esp_gmf_port_handle_t io_inst);

/**
 * @brief  Delete a GMF port from the list
 *
 * @param[in,out]  head     A pointer to the head of the port list
 * @param[in]      io_inst  The port instance to be deleted
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_t esp_gmf_port_del_at(esp_gmf_port_handle_t *head, esp_gmf_port_handle_t io_inst);

/**
 * @brief  Acquire the expected valid data into the specified payload, regardless of whether the payload is NULL or not
 *         If writer of port is valid, the payload from the previous element stored on the port `payload` pointer is fetched
 *         If the `*load` pointer is NULL, a new payload will be allocated before calling the acquire operation
 *         The actual valid size is stored in `load->valid_size`
 *
 * @param[in]      handle       The handle of the port
 * @param[in,out]  load         Pointer to store the acquired payload
 * @param[in]      wanted_size  Size of the payload to be acquired
 * @param[in]      wait_ticks   Number of ticks to wait, in milliseconds
 *
 * @return
 *       - ESP_GMF_IO_OK       Operation successful
 *       - ESP_GMF_IO_FAIL     Operation failed or invalid arguments
 *       - ESP_GMF_IO_ABORT    Operation aborted
 *       - ESP_GMF_IO_TIMEOUT  Operation timed out
 */
esp_gmf_err_io_t esp_gmf_port_acquire_in(esp_gmf_port_handle_t handle, esp_gmf_payload_t **load, uint32_t wanted_size, int wait_ticks);

/**
 * @brief  Call the release operation or clean the payload pointer of the port
 *
 * @param[in]  handle      The handle of the port
 * @param[in]  load        Pointer to the payload to be released
 * @param[in]  wait_ticks  Number of ticks to wait
 *
 * @return
 *       - ESP_GMF_IO_OK       Operation successful
 *       - ESP_GMF_IO_FAIL     Operation failed or invalid arguments
 *       - ESP_GMF_IO_ABORT    Operation aborted
 *       - ESP_GMF_IO_TIMEOUT  Operation timed out
 */
esp_gmf_err_io_t esp_gmf_port_release_in(esp_gmf_port_handle_t handle, esp_gmf_payload_t *load, int wait_ticks);

/**
 * @brief  Acquire the buffer of the expected size into the specified payload,
 *         If the reader of the port is valid, store the provided or allocated payload to the input port of the next element
 *         If reader pointer is NULL, prepare a payload if `*load` is invalid before calling the acquire operation
 *         The actual valid size is stored in `load->valid_size`
 *
 * @param[in]      handle       The handle of the port
 * @param[in,out]  load         Pointer to store the acquired payload
 * @param[in]      wanted_size  Size of the payload to be acquired
 * @param[in]      wait_ticks   Number of ticks to wait, in milliseconds
 *
 * @return
 *       - ESP_GMF_IO_OK       Operation successful
 *       - ESP_GMF_IO_FAIL     Operation failed or invalid arguments
 *       - ESP_GMF_IO_ABORT    Operation aborted
 *       - ESP_GMF_IO_TIMEOUT  Operation timed out
 */
esp_gmf_err_io_t esp_gmf_port_acquire_out(esp_gmf_port_handle_t handle, esp_gmf_payload_t **load, uint32_t wanted_size, int wait_ticks);

/**
 * @brief  Call the release operation or clean the payload pointer of the port
 *
 * @param[in]  handle      The handle of the port
 * @param[in]  load        Pointer to the payload to be released
 * @param[in]  wait_ticks  Number of ticks to wait
 *
 * @return
 *       - ESP_GMF_IO_OK       Operation successful
 *       - ESP_GMF_IO_FAIL     Operation failed or invalid arguments
 *       - ESP_GMF_IO_ABORT    Operation aborted
 *       - ESP_GMF_IO_TIMEOUT  Operation timed out
 */
esp_gmf_err_io_t esp_gmf_port_release_out(esp_gmf_port_handle_t handle, esp_gmf_payload_t *load, int wait_ticks);

static inline void *NEW_ESP_GMF_PORT(uint8_t dir, uint8_t type, void *acq, void *release,
                                     void *del, void *ctx, int length, int ticks_ms)
{
    esp_gmf_port_config_t port_config = {
        .dir = (uint8_t)dir,
        .type = (uint8_t)type,
        .ops = {
            .acquire = (port_acquire)acq,
            .release = (port_release)release,
            .del = (port_free)del,
        },
        .ctx = ctx,
        .data_length = length,
        .wait_ticks = ticks_ms,
    };
    esp_gmf_port_handle_t new_port = NULL;
    esp_gmf_port_init(&port_config, &new_port);
    return new_port;
}

static inline void *NEW_ESP_GMF_PORT_IN_BYTE(void *acq, void *release, void *del, void *ctx, int length, int ticks_ms)
{
    return NEW_ESP_GMF_PORT(ESP_GMF_PORT_DIR_IN, ESP_GMF_PORT_TYPE_BYTE, acq, release, del, ctx, length, ticks_ms);
}
static inline void *NEW_ESP_GMF_PORT_OUT_BYTE(void *acq, void *release, void *del, void *ctx, int length, int ticks_ms)
{
    return NEW_ESP_GMF_PORT(ESP_GMF_PORT_DIR_OUT, ESP_GMF_PORT_TYPE_BYTE, acq, release, del, ctx, length, ticks_ms);
}

static inline void *NEW_ESP_GMF_PORT_IN_BLOCK(void *acq, void *release, void *del, void *ctx, int length, int ticks_ms)
{
    return NEW_ESP_GMF_PORT(ESP_GMF_PORT_DIR_IN, ESP_GMF_PORT_TYPE_BLOCK, acq, release, del, ctx, length, ticks_ms);
}
static inline void *NEW_ESP_GMF_PORT_OUT_BLOCK(void *acq, void *release, void *del, void *ctx, int length, int ticks_ms)
{
    return NEW_ESP_GMF_PORT(ESP_GMF_PORT_DIR_OUT, ESP_GMF_PORT_TYPE_BLOCK, acq, release, del, ctx, length, ticks_ms);
}

#ifdef __cplusplus
}
#endif  /* __cplusplus */
