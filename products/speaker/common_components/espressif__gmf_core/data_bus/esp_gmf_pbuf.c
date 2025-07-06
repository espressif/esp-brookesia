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
#include "esp_gmf_oal_mutex.h"
#include "esp_gmf_pbuf.h"
#include "esp_log.h"
#include "esp_gmf_oal_mem.h"
#include "esp_gmf_err.h"

static const char *TAG = "ESP_GMF_PBUF";

#define THREAD_SAFE

#ifdef THREAD_SAFE
#define pbuf_lock(x)   esp_gmf_oal_mutex_lock(x)
#define pbuf_unlock(x) esp_gmf_oal_mutex_unlock(x)

#else
#define pbuf_lock(x)
#define pbuf_unlock(x)

#endif  /* THREAD_SAFE */

/**
 * @brief  Structure representing a list node in the point buffer
 */
typedef struct pbuf_list_ {
    struct pbuf_list_        *next;   /*!< Pointer to the next node in the list */
    esp_gmf_data_bus_block_t  block;  /*!< Data bus block associated with the node */
} pbuf_list_t;

/**
 * @brief  Structure representing a point buffer
 */
typedef struct _pbuf_ {
    pbuf_list_t *empty_head;          /*!< Pointer to the head of the list of empty blocks */
    pbuf_list_t *empty_tail;          /*!< Pointer to the tail of the list of empty blocks */
    pbuf_list_t *fill_head;           /*!< Pointer to the head of the list of filled blocks */
    pbuf_list_t *fill_tail;           /*!< Pointer to the tail of the list of filled blocks */
    void        *lock;                /*!< Pointer to the lock used for synchronization */
    int          buf_cnt;             /*!< Number of blocks in the buffer */
    uint32_t     capacity;            /*!< Total capacity of the buffer */
    uint8_t      _is_write_done : 1;  /*!< Flag indicating if writing is complete */
    uint8_t      _is_abort      : 1;  /*!< Flag indicating if an abort operation has been requested */
} esp_gmf_pbuf_t;

static inline void _pbuf_handle_free(esp_gmf_pbuf_handle_t handle)
{
    esp_gmf_pbuf_t *pbuf = (esp_gmf_pbuf_t *)handle;
    if (pbuf->lock) {
        esp_gmf_oal_mutex_destroy(pbuf->lock);
    }
    esp_gmf_oal_free(pbuf);
}

esp_gmf_err_t esp_gmf_pbuf_create(int capacity, esp_gmf_pbuf_handle_t *handle)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    if (capacity < 1) {
        ESP_LOGE(TAG, "The capacity[%d] is not support", capacity);
        return ESP_GMF_ERR_INVALID_ARG;
    }
    *handle = NULL;
    esp_gmf_pbuf_t *pbuf = esp_gmf_oal_calloc(1, sizeof(esp_gmf_pbuf_t));
    ESP_GMF_MEM_CHECK(TAG, pbuf, return ESP_GMF_ERR_MEMORY_LACK;);
    pbuf->capacity = capacity;
    int ret = ESP_GMF_ERR_OK;
    pbuf->lock = (SemaphoreHandle_t)esp_gmf_oal_mutex_create();
    ESP_GMF_MEM_CHECK(TAG, pbuf->lock, {ret = ESP_GMF_ERR_MEMORY_LACK; goto esp_gmf_pbuf_err;});
    pbuf->_is_write_done = 0;
    pbuf->_is_abort = 0;
    pbuf->buf_cnt = 0;
    *handle = pbuf;
    return ESP_GMF_ERR_OK;

esp_gmf_pbuf_err:
    _pbuf_handle_free(pbuf);
    return ret;
}

esp_gmf_err_t esp_gmf_pbuf_destroy(esp_gmf_pbuf_handle_t handle)
{
    ESP_GMF_MEM_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG;);
    esp_gmf_pbuf_t *pbuf = (esp_gmf_pbuf_t *)handle;
    pbuf_lock(pbuf->lock);
    while (pbuf->empty_head) {
        pbuf_list_t *tmp = pbuf->empty_head->next;
        if (pbuf->empty_head->block.buf) {
            esp_gmf_oal_free(pbuf->empty_head->block.buf);
        }
        esp_gmf_oal_free(pbuf->empty_head);
        pbuf->empty_head = tmp;
    }
    while (pbuf->fill_head) {
        pbuf_list_t *tmp = pbuf->fill_head->next;
        if (pbuf->fill_head->block.buf) {
            esp_gmf_oal_free(pbuf->fill_head->block.buf);
        }
        esp_gmf_oal_free(pbuf->fill_head);
        pbuf->fill_head = tmp;
    }
    pbuf_unlock(pbuf->lock);
    _pbuf_handle_free(pbuf);
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_io_t esp_gmf_pbuf_acquire_read(esp_gmf_pbuf_handle_t handle, esp_gmf_data_bus_block_t *blk, uint32_t wanted_size, int block_ticks)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_IO_FAIL);
    ESP_GMF_NULL_CHECK(TAG, blk, return ESP_GMF_IO_FAIL);
    esp_gmf_pbuf_t *pbuf = (esp_gmf_pbuf_t *)handle;
    if (pbuf->_is_write_done) {
        return ESP_GMF_IO_OK;
    }
    if (pbuf->fill_head == NULL) {
        ESP_LOGD(TAG, "ACQ_RD, fill head is empty, p:%p", pbuf);
        return ESP_GMF_IO_FAIL;
    }
    pbuf_lock(pbuf->lock);

    blk->buf = pbuf->fill_head->block.buf;
    blk->buf_length = pbuf->fill_head->block.buf_length;
    blk->valid_size = pbuf->fill_head->block.valid_size;
    blk->is_last = pbuf->fill_head->block.is_last;

    pbuf_unlock(pbuf->lock);
    ESP_LOGD(TAG, "ACQ_RD, p:%p, h:%p b:%p, l:%d, vld:%d,last:%d, c:%d", pbuf,
             pbuf->fill_head, blk->buf, blk->buf_length, blk->valid_size, blk->is_last, pbuf->buf_cnt);
    return ESP_GMF_IO_OK;
}

esp_gmf_err_io_t esp_gmf_pbuf_release_read(esp_gmf_pbuf_handle_t handle, esp_gmf_data_bus_block_t *blk, int block_ticks)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_IO_FAIL);
    ESP_GMF_NULL_CHECK(TAG, blk, return ESP_GMF_IO_FAIL);
    esp_gmf_pbuf_t *pbuf = (esp_gmf_pbuf_t *)handle;
    pbuf_lock(pbuf->lock);
    if (pbuf->fill_head->block.buf == blk->buf) {
        pbuf->fill_head->block.buf_length = blk->buf_length;
        pbuf->fill_head->block.valid_size = 0;
        pbuf->fill_head->block.is_last = 0;
        pbuf_list_t *tmp = pbuf->fill_head->next;
        pbuf_list_t *empty = pbuf->fill_head;
        empty->next = NULL;
        pbuf->fill_head = tmp;

        if (pbuf->empty_head == NULL) {
            pbuf->empty_tail = empty;
            pbuf->empty_head = empty;
        } else {
            pbuf->empty_tail->next = empty;
            pbuf->empty_tail = empty;
        }
        blk->buf = NULL;
        blk->is_last = 0;
        blk->valid_size = 0;
        pbuf->buf_cnt--;
    } else {
        ESP_LOGE(TAG, "RLS_RD, the buffer is not belong to filled buffer, p:%p, buf:%p", pbuf, blk->buf);
        pbuf_unlock(pbuf->lock);
        return ESP_GMF_IO_FAIL;
    }
    ESP_LOGD(TAG, "RLS_RD, p:%p, head:%p, e:%p, b:%p, l:%d, vld:%d, c:%d", pbuf, pbuf->fill_head, pbuf->empty_head, blk->buf, blk->buf_length, blk->valid_size, pbuf->buf_cnt);
    pbuf_unlock(pbuf->lock);
    return ESP_GMF_IO_OK;
}

esp_gmf_err_io_t esp_gmf_pbuf_acquire_write(esp_gmf_pbuf_handle_t handle, esp_gmf_data_bus_block_t *blk, uint32_t wanted_size, int block_ticks)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_IO_FAIL);
    ESP_GMF_NULL_CHECK(TAG, blk, return ESP_GMF_IO_FAIL);
    esp_gmf_pbuf_t *pbuf = (esp_gmf_pbuf_t *)handle;
    if (wanted_size == 0) {
        ESP_LOGE(TAG, "ACQ_WR, the wanted size is not correct, p:%p, size:%ld", pbuf, wanted_size);
        return ESP_GMF_IO_FAIL;
    }
    if (pbuf->_is_write_done) {
        return ESP_GMF_IO_OK;
    }
    pbuf_lock(pbuf->lock);
    if (pbuf->empty_head == NULL) {
        if (pbuf->capacity < pbuf->buf_cnt) {
            ESP_LOGE(TAG, "ACQ_WR, the buffer size is out of range, %p, cnt:%d, cap:%ld", pbuf, pbuf->buf_cnt, pbuf->capacity);
            pbuf_unlock(pbuf->lock);
            return ESP_GMF_IO_FAIL;
        }
        pbuf_list_t *new_pbuf = (pbuf_list_t *)esp_gmf_oal_calloc(1, sizeof(pbuf_list_t));
        ESP_GMF_MEM_CHECK(TAG, new_pbuf, { pbuf_unlock(pbuf->lock); return ESP_GMF_IO_FAIL;});
        new_pbuf->block.buf = (uint8_t *)esp_gmf_oal_calloc(1, wanted_size);
        ESP_GMF_MEM_CHECK(TAG, new_pbuf->block.buf, { pbuf_unlock(pbuf->lock); esp_gmf_oal_free(new_pbuf); return ESP_GMF_IO_FAIL;});
        new_pbuf->block.buf_length = wanted_size;
        new_pbuf->block.valid_size = wanted_size;
        new_pbuf->block.is_last = 0;
        pbuf->empty_head = new_pbuf;
        pbuf->empty_tail = new_pbuf;
        ESP_LOGD(TAG, "ACQ_WR, w:%ld, new buf:%p, l:%d, self:%p, next:%p", wanted_size, new_pbuf->block.buf,
                 new_pbuf->block.buf_length, new_pbuf, new_pbuf->next);
    }
    if (pbuf->empty_head->block.buf_length < wanted_size) {
        if (pbuf->empty_head->block.buf) {
            esp_gmf_oal_free(pbuf->empty_head->block.buf);
        }
        pbuf->empty_head->block.buf = esp_gmf_oal_calloc(1, wanted_size);
        ESP_GMF_MEM_CHECK(TAG, pbuf->empty_head->block.buf, { pbuf_unlock(pbuf->lock); return ESP_GMF_IO_FAIL;});
        pbuf->empty_head->block.buf_length = wanted_size;
        pbuf->empty_head->block.valid_size = wanted_size;
    }
    blk->buf = pbuf->empty_head->block.buf;
    blk->buf_length = pbuf->empty_head->block.buf_length;
    blk->valid_size = pbuf->empty_head->block.valid_size;
    if (wanted_size <= blk->buf_length) {
        blk->valid_size = wanted_size;
    }
    pbuf->buf_cnt++;
    ESP_LOGD(TAG, "ACQ_WR, w:%ld, b:%p, l:%d, vld:%d self:%p, nxt:%p, c:%d", wanted_size, blk->buf,
             blk->buf_length, blk->valid_size, pbuf->empty_head, pbuf->empty_head->next, pbuf->buf_cnt);
    pbuf_unlock(pbuf->lock);
    return ESP_GMF_IO_OK;
}

esp_gmf_err_io_t esp_gmf_pbuf_release_write(esp_gmf_pbuf_handle_t handle, esp_gmf_data_bus_block_t *blk, int block_ticks)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_IO_FAIL);
    ESP_GMF_NULL_CHECK(TAG, blk, return ESP_GMF_IO_FAIL);
    esp_gmf_pbuf_t *pbuf = (esp_gmf_pbuf_t *)handle;

    pbuf_lock(pbuf->lock);
    if (pbuf->empty_head->block.buf == blk->buf) {
        pbuf->empty_head->block.buf_length = blk->buf_length;
        pbuf->empty_head->block.valid_size = blk->valid_size;
        pbuf->empty_head->block.is_last = blk->is_last;
        pbuf_list_t *tmp = pbuf->empty_head->next;
        pbuf_list_t *filled = pbuf->empty_head;
        filled->next = NULL;
        pbuf->empty_head = tmp;

        if (pbuf->fill_head == NULL) {
            pbuf->fill_tail = filled;
            pbuf->fill_head = filled;
        } else {
            pbuf->fill_tail->next = filled;
            pbuf->fill_tail = filled;
        }
        blk->buf = NULL;
        blk->is_last = 0;
        blk->valid_size = 0;
    } else {
        ESP_LOGE(TAG, "RLS_WR, buffer is not belong to empty header, p:%p, buf:%p", pbuf, blk->buf);
        pbuf_unlock(pbuf->lock);
        return ESP_GMF_IO_FAIL;
    }
    ESP_LOGD(TAG, "RLS_WR, p:%p, h:%p, b:%p, l:%d, vld:%d,last:%d, c:%d", pbuf, pbuf->empty_head, blk->buf,
             blk->buf_length, blk->valid_size, blk->is_last, pbuf->buf_cnt);
    pbuf_unlock(pbuf->lock);
    return ESP_GMF_IO_OK;
}

esp_gmf_err_t esp_gmf_pbuf_done_write(esp_gmf_pbuf_handle_t handle)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    esp_gmf_pbuf_t *pbuf = (esp_gmf_pbuf_t *)handle;
    pbuf->_is_write_done = 1;
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_pbuf_abort(esp_gmf_pbuf_handle_t handle)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    esp_gmf_pbuf_t *pbuf = (esp_gmf_pbuf_t *)handle;
    pbuf->_is_abort = 1;
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_pbuf_reset(esp_gmf_pbuf_handle_t handle)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    esp_gmf_pbuf_t *pbuf = (esp_gmf_pbuf_t *)handle;
    pbuf_lock(pbuf->lock);
    pbuf->_is_write_done = 0;
    pbuf->_is_abort = 0;
    pbuf_unlock(pbuf->lock);
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_pbuf_get_free_size(esp_gmf_pbuf_handle_t handle, uint32_t *free_size)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    ESP_GMF_NULL_CHECK(TAG, free_size, return ESP_GMF_ERR_INVALID_ARG);
    esp_gmf_pbuf_t *pbuf = (esp_gmf_pbuf_t *)handle;
    pbuf_lock(pbuf->lock);
    pbuf_list_t *tmp = pbuf->empty_head;
    int sz = 0;
    while (tmp) {
        sz += tmp->block.buf_length;
        tmp = tmp->next;
    }
    *free_size = sz;
    pbuf_unlock(pbuf->lock);
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_pbuf_get_filled_size(esp_gmf_pbuf_handle_t handle, uint32_t *filled_size)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    ESP_GMF_NULL_CHECK(TAG, filled_size, return ESP_GMF_ERR_INVALID_ARG);
    esp_gmf_pbuf_t *pbuf = (esp_gmf_pbuf_t *)handle;
    pbuf_lock(pbuf->lock);
    pbuf_list_t *tmp = pbuf->fill_head;
    int sz = 0;
    while (tmp) {
        sz += tmp->block.valid_size;
        tmp = tmp->next;
    }
    *filled_size = sz;
    pbuf_unlock(pbuf->lock);
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_pbuf_get_total_size(esp_gmf_pbuf_handle_t handle, uint32_t *total_size)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    ESP_GMF_NULL_CHECK(TAG, total_size, return ESP_GMF_ERR_INVALID_ARG);
    esp_gmf_pbuf_t *pbuf = (esp_gmf_pbuf_t *)handle;
    pbuf_lock(pbuf->lock);
    pbuf_list_t *tmp = pbuf->fill_head;
    int sz = 0;
    while (tmp) {
        sz += tmp->block.buf_length;
        tmp = tmp->next;
    }
    tmp = pbuf->empty_head;
    while (tmp) {
        sz += tmp->block.buf_length;
        tmp = tmp->next;
    }
    *total_size = sz;
    pbuf_unlock(pbuf->lock);
    return ESP_GMF_ERR_OK;
}
