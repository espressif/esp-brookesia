/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_gmf_oal_mem.h"
#include "esp_gmf_oal_mutex.h"
#include "esp_gmf_err.h"
#include "esp_gmf_fifo.h"

static const char *TAG = "ESP_GMF_FIFO";

#define GMF_FIFO_DEFAULT_ALIGNMENT (16)

typedef struct esp_esp_gmf_fifo_node {
    struct esp_esp_gmf_fifo_node *next;
    void                         *buffer;
    size_t                        buf_length;
    size_t                        valid_size;
    bool                          is_done;
} esp_gmf_fifo_node_t;

/**
 * @brief  Structure representing a FIFO (First-In-First-Out) buffer
 */
typedef struct {
    uint32_t             node_cnt;            /*!< Maximum number of buffer nodes in the FIFO */
    uint32_t             capacity;            /*!< Current number of buffers in the FIFO */
    SemaphoreHandle_t    can_read;            /*!< Semaphore for indicating available data for reading. It blocks reading operations when no data is available */
    SemaphoreHandle_t    can_write;           /*!< Semaphore for indicating available space for writing. It blocks writing operations when the FIFO is full */
    SemaphoreHandle_t    lock;                /*!< Semaphore for controlling exclusive access to the FIFO to ensure thread safety */
    esp_gmf_fifo_node_t *empty_head;          /*!< Pointer to the head of the list of empty buffer nodes that can be used for writing */
    esp_gmf_fifo_node_t *fill_head;           /*!< Pointer to the head of the list of filled buffer nodes that can be read */
    uint8_t              _is_write_done : 1;  /*!< Flag indicating if all writing operations to the FIFO have been completed. Set to 1 when writing is finished */
    uint8_t              _is_abort      : 1;  /*!< Flag indicating if an abort operation has been requested. Set to 1 to signal that FIFO operations should be aborted */
    uint8_t              align;               /*!< Alignment for the request buffer */
} esp_gmf_fifo_t;

static inline esp_gmf_fifo_node_t *esp_gmf_fifo_node_create(void)
{
    esp_gmf_fifo_node_t *node = (esp_gmf_fifo_node_t *)esp_gmf_oal_calloc(1, sizeof(esp_gmf_fifo_node_t));
    if (!node) {
        return NULL;
    }
    return node;
}

static inline esp_gmf_fifo_node_t *esp_gmf_fifo_node_with_buf_create(size_t buf_size, uint8_t align)
{
    esp_gmf_fifo_node_t *node = esp_gmf_fifo_node_create();
    if (!node) {
        return NULL;
    }
    if (buf_size > 0) {
        node->buffer = esp_gmf_oal_malloc_align(align, buf_size);
        if (!node->buffer) {
            esp_gmf_oal_free(node);
            return NULL;
        }
        memset(node->buffer, 0, buf_size);
        node->buf_length = buf_size;
    }
    node->is_done = false;
    return node;
}

static inline int esp_gmf_fifo_node_get_cnt(esp_gmf_fifo_node_t *node)
{
    int cnt = 0;
    while (node) {
        cnt++;
        node = node->next;
    }
    return cnt;
}

static inline void esp_gmf_fifo_node_destroy(esp_gmf_fifo_node_t *node)
{
    if (!node) {
        return;
    }
    if (node->buffer) {
        esp_gmf_oal_free(node->buffer);
        node->buffer = NULL;
    }
    esp_gmf_oal_free(node);
}

static inline void _gmf_fifo_handle_free(esp_gmf_fifo_handle_t handle)
{
    esp_gmf_fifo_t *fifo = (esp_gmf_fifo_t *)handle;
    while (fifo->empty_head) {
        esp_gmf_fifo_node_t *node = fifo->empty_head;
        fifo->empty_head = fifo->empty_head->next;
        esp_gmf_fifo_node_destroy(node);
    }

    while (fifo->fill_head) {
        esp_gmf_fifo_node_t *node = fifo->fill_head;
        fifo->fill_head = fifo->fill_head->next;
        esp_gmf_fifo_node_destroy(node);
    }

    if (fifo->can_read) {
        vSemaphoreDelete(fifo->can_read);
    }
    if (fifo->can_write) {
        vSemaphoreDelete(fifo->can_write);
    }
    if (fifo->lock) {
        esp_gmf_oal_mutex_destroy(fifo->lock);
    }
    esp_gmf_oal_free(fifo);
}

esp_gmf_err_t esp_gmf_fifo_create(int block_cnt, int block_size, esp_gmf_fifo_handle_t *handle)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    if (block_cnt < 1) {
        return ESP_GMF_ERR_INVALID_ARG;
    }
    *handle = NULL;
    esp_gmf_fifo_t *fifo = esp_gmf_oal_calloc(1, sizeof(esp_gmf_fifo_t));
    ESP_GMF_MEM_CHECK(TAG, fifo, return ESP_GMF_ERR_MEMORY_LACK;);
    fifo->can_read = xSemaphoreCreateBinary();
    ESP_GMF_MEM_CHECK(TAG, fifo->can_read, goto esp_gmf_fifo_err;);
    fifo->can_write = xSemaphoreCreateBinary();
    ESP_GMF_MEM_CHECK(TAG, fifo->can_write, goto esp_gmf_fifo_err;);
    fifo->lock = (SemaphoreHandle_t)esp_gmf_oal_mutex_create();
    ESP_GMF_MEM_CHECK(TAG, fifo->lock, goto esp_gmf_fifo_err;);

    fifo->capacity = block_cnt;
    fifo->node_cnt = 0;
    fifo->_is_write_done = 0;
    fifo->align = GMF_FIFO_DEFAULT_ALIGNMENT;
    *handle = fifo;
    return ESP_GMF_ERR_OK;

esp_gmf_fifo_err:
    _gmf_fifo_handle_free(fifo);
    return ESP_GMF_ERR_FAIL;
}

esp_gmf_err_t esp_gmf_fifo_set_align(esp_gmf_fifo_handle_t handle, uint8_t align)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    esp_gmf_fifo_t *fifo = (esp_gmf_fifo_t *)handle;
    fifo->align = (align == 0) ? GMF_FIFO_DEFAULT_ALIGNMENT : align;
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_fifo_destroy(esp_gmf_fifo_handle_t handle)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    _gmf_fifo_handle_free(handle);
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_io_t esp_gmf_fifo_acquire_read(esp_gmf_fifo_handle_t handle, esp_gmf_data_bus_block_t *blk, uint32_t wanted_size, int block_ticks)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_IO_FAIL);
    ESP_GMF_NULL_CHECK(TAG, blk, return ESP_GMF_IO_FAIL);
    esp_gmf_fifo_t *fifo = (esp_gmf_fifo_t *)handle;
    ESP_LOGD(TAG, "RD_ACQ+, hd:%p, wanted:%ld, ticks:%d", handle, wanted_size, block_ticks);
    if (fifo->fill_head == NULL) {
        while (fifo->fill_head == NULL) {
            if (xSemaphoreTake(fifo->can_read, block_ticks) != pdTRUE) {
                ESP_LOGE(TAG, "FIFO acquire read timeout");
                return ESP_GMF_IO_TIMEOUT;
            }
            if (fifo->_is_abort) {
                return ESP_GMF_IO_ABORT;
            }
        }
    }
    esp_gmf_fifo_node_t *node = fifo->fill_head;
    blk->buf = node->buffer;
    blk->buf_length = node->buf_length;
    blk->valid_size = node->valid_size;
    blk->is_last = node->is_done;
    ESP_LOGD(TAG, "RD_ACQ-, hd:%p, b:%p, l:%d, valid:%d, n:%ld, e:%d, f:%d", handle, blk->buf, blk->buf_length, blk->valid_size, fifo->node_cnt,
             esp_gmf_fifo_node_get_cnt(fifo->empty_head), esp_gmf_fifo_node_get_cnt(fifo->fill_head));
    return ESP_GMF_IO_OK;
}

esp_gmf_err_io_t esp_gmf_fifo_release_read(esp_gmf_fifo_handle_t handle, esp_gmf_data_bus_block_t *blk, int block_ticks)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_IO_FAIL);
    ESP_GMF_NULL_CHECK(TAG, blk, return ESP_GMF_IO_FAIL);
    esp_gmf_fifo_t *fifo = (esp_gmf_fifo_t *)handle;

    ESP_LOGD(TAG, "RD_RLS+, hd:%p, b:%p, l:%d", handle, blk->buf, blk->buf_length);
    esp_gmf_oal_mutex_lock(fifo->lock);
    esp_gmf_fifo_node_t *node = fifo->fill_head;
    fifo->fill_head = node->next;
    if (node->buffer != blk->buf) {
        ESP_LOGE(TAG, "Release read error, buffer not match");
        esp_gmf_oal_mutex_unlock(fifo->lock);
        return ESP_GMF_IO_FAIL;
    }

    node->is_done = false;
    node->valid_size = 0;
    node->next = NULL;
    if (fifo->empty_head == NULL) {
        fifo->empty_head = node;
    } else {
        esp_gmf_fifo_node_t *tmp = fifo->empty_head;
        while (tmp->next != NULL) {
            tmp = tmp->next;
        }
        tmp->next = node;
    }
    xSemaphoreGive(fifo->can_write);
    esp_gmf_oal_mutex_unlock(fifo->lock);
    ESP_LOGD(TAG, "RD_RLS-, hd:%p, b:%p, l:%d, n:%ld, e:%d, f:%d", handle, blk->buf, blk->buf_length, fifo->node_cnt,
             esp_gmf_fifo_node_get_cnt(fifo->empty_head), esp_gmf_fifo_node_get_cnt(fifo->fill_head));
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_io_t esp_gmf_fifo_acquire_write(esp_gmf_fifo_handle_t handle, esp_gmf_data_bus_block_t *blk, uint32_t wanted_size, int block_ticks)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_IO_FAIL);
    ESP_GMF_NULL_CHECK(TAG, blk, return ESP_GMF_IO_FAIL);
    esp_gmf_fifo_t *fifo = (esp_gmf_fifo_t *)handle;

    ESP_LOGD(TAG, "WR_ACQ+, hd:%p, wanted:%ld, ticks:%d", handle, wanted_size, block_ticks);
    esp_gmf_fifo_node_t *node = NULL;
    esp_gmf_oal_mutex_lock(fifo->lock);
    if (fifo->empty_head == NULL) {
        if (fifo->node_cnt < fifo->capacity) {
            node = esp_gmf_fifo_node_with_buf_create(wanted_size, fifo->align);
            ESP_GMF_NULL_CHECK(TAG, node, {esp_gmf_oal_mutex_unlock(fifo->lock); return ESP_GMF_ERR_MEMORY_LACK;});
            fifo->empty_head = node;
            fifo->node_cnt++;
            ESP_LOGD(TAG, "New an empty node:%p, addr:%p, n:%ld, e:%d, f:%d", node, node->buffer, fifo->node_cnt,
                     esp_gmf_fifo_node_get_cnt(fifo->empty_head), esp_gmf_fifo_node_get_cnt(fifo->fill_head));
        } else {
            esp_gmf_oal_mutex_unlock(fifo->lock);
            while (fifo->empty_head == NULL) {
                if (xSemaphoreTake(fifo->can_write, block_ticks) != pdTRUE) {
                    return ESP_GMF_IO_FAIL;
                }
                if (fifo->_is_abort) {
                    return ESP_GMF_IO_ABORT;
                }
            }
            esp_gmf_oal_mutex_lock(fifo->lock);
        }
    }
    node = fifo->empty_head;
    if (node->buf_length < wanted_size) {
        esp_gmf_oal_free(node->buffer);
        node->buffer = esp_gmf_oal_malloc(wanted_size);
        ESP_GMF_NULL_CHECK(TAG, node->buffer, {esp_gmf_oal_mutex_unlock(fifo->lock); return ESP_GMF_ERR_MEMORY_LACK;});
        node->buf_length = wanted_size;
    }

    blk->buf = node->buffer;
    blk->buf_length = node->buf_length;
    blk->is_last = node->is_done;
    esp_gmf_oal_mutex_unlock(fifo->lock);
    ESP_LOGD(TAG, "WR_ACQ-, hd:%p, b:%p, l:%d, n:%ld, e:%d, f:%d", handle, blk->buf, blk->buf_length, fifo->node_cnt,
             esp_gmf_fifo_node_get_cnt(fifo->empty_head), esp_gmf_fifo_node_get_cnt(fifo->fill_head));
    return ESP_GMF_IO_OK;
}

esp_gmf_err_io_t esp_gmf_fifo_release_write(esp_gmf_fifo_handle_t handle, esp_gmf_data_bus_block_t *blk, int block_ticks)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_IO_FAIL);
    ESP_GMF_NULL_CHECK(TAG, blk, return ESP_GMF_IO_FAIL);
    esp_gmf_fifo_t *fifo = (esp_gmf_fifo_t *)handle;
    ESP_LOGD(TAG, "WR_RLS+, hd:%p, b:%p, l:%d, valid:%d, %d, %d", handle, blk->buf, blk->buf_length, blk->valid_size, esp_gmf_fifo_node_get_cnt(fifo->empty_head),
             esp_gmf_fifo_node_get_cnt(fifo->fill_head));
    esp_gmf_oal_mutex_lock(fifo->lock);
    esp_gmf_fifo_node_t *node = fifo->empty_head;
    if (node == NULL) {
        ESP_LOGE(TAG, "%s,%d, empty_head is NULL", __func__, __LINE__);
        esp_gmf_oal_mutex_unlock(fifo->lock);
        return ESP_GMF_IO_FAIL;
    }
    fifo->empty_head = node->next;

    node->next = NULL;
    if (fifo->fill_head == NULL) {
        fifo->fill_head = node;
    } else {
        esp_gmf_fifo_node_t *tmp = fifo->fill_head;
        while (tmp->next != NULL) {
            tmp = tmp->next;
        }
        tmp->next = node;
    }
    node->buffer = blk->buf;
    node->buf_length = blk->buf_length;
    node->valid_size = blk->valid_size;
    node->is_done = blk->is_last;

    xSemaphoreGive(fifo->can_read);
    esp_gmf_oal_mutex_unlock(fifo->lock);
    ESP_LOGD(TAG, "WR_RLS-, hd:%p, b:%p, l:%d, valid:%d, n:%ld, e:%d, f:%d", handle, blk->buf, blk->buf_length, blk->valid_size,
             fifo->node_cnt, esp_gmf_fifo_node_get_cnt(fifo->empty_head), esp_gmf_fifo_node_get_cnt(fifo->fill_head));
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_fifo_done_write(esp_gmf_fifo_handle_t handle)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    esp_gmf_fifo_t *fifo = (esp_gmf_fifo_t *)handle;
    fifo->_is_write_done = 1;
    xSemaphoreGive(fifo->can_read);
    xSemaphoreGive(fifo->can_write);
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_fifo_abort(esp_gmf_fifo_handle_t handle)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    esp_gmf_fifo_t *fifo = (esp_gmf_fifo_t *)handle;
    fifo->_is_abort = 1;
    xSemaphoreGive(fifo->can_read);
    xSemaphoreGive(fifo->can_write);
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_fifo_reset(esp_gmf_fifo_handle_t handle)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    esp_gmf_fifo_t *fifo = (esp_gmf_fifo_t *)handle;
    esp_gmf_fifo_node_t *node = fifo->fill_head;
    while (node) {
        node->is_done = 0;
        node->valid_size = 0;
        node = node->next;
    }
    node = fifo->empty_head;
    while (node) {
        node->is_done = 0;
        node->valid_size = 0;
        node = node->next;
    }
    fifo->_is_write_done = 0;
    fifo->_is_abort = 0;
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_fifo_get_free_size(esp_gmf_fifo_handle_t handle, uint32_t *free_size)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    ESP_GMF_NULL_CHECK(TAG, free_size, return ESP_GMF_ERR_INVALID_ARG);
    esp_gmf_fifo_t *fifo = (esp_gmf_fifo_t *)handle;
    *free_size = 0;
    esp_gmf_fifo_node_t *node = fifo->empty_head;
    while (node) {
        *free_size += node->buf_length;
        node = node->next;
    }
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_fifo_get_total_size(esp_gmf_fifo_handle_t handle, uint32_t *total_size)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    ESP_GMF_NULL_CHECK(TAG, total_size, return ESP_GMF_ERR_INVALID_ARG);
    esp_gmf_fifo_t *fifo = (esp_gmf_fifo_t *)handle;
    *total_size = 0;
    esp_gmf_fifo_node_t *node = fifo->empty_head;
    while (node) {
        *total_size += node->buf_length;
        node = node->next;
    }
    node = fifo->fill_head;
    while (node) {
        *total_size += node->buf_length;
        node = node->next;
    }
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_fifo_get_filled_size(esp_gmf_fifo_handle_t handle, uint32_t *filled_size)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    ESP_GMF_NULL_CHECK(TAG, filled_size, return ESP_GMF_ERR_INVALID_ARG);
    esp_gmf_fifo_t *fifo = (esp_gmf_fifo_t *)handle;
    *filled_size = 0;
    esp_gmf_fifo_node_t *node = fifo->fill_head;
    while (node) {
        *filled_size += node->buf_length;
        node = node->next;
    }
    return ESP_GMF_ERR_OK;
}
