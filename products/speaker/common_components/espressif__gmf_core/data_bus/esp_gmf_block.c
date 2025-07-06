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
#include "esp_gmf_block.h"
#include "esp_log.h"
#include "esp_gmf_oal_mem.h"

static const char *TAG = "ESP_GMF_BLOCK";

/**
 * @brief  Structure representing a block buffer
 */
typedef struct {
    SemaphoreHandle_t  can_read;            /*!< Semaphore for blocking read operations */
    SemaphoreHandle_t  can_write;           /*!< Semaphore for blocking write operations */
    SemaphoreHandle_t  lock;                /*!< Semaphore for thread-safe access */
    uint32_t           b_cnt;               /*!< Number of blocks */
    uint32_t           b_size;              /*!< Size of each block */
    uint32_t           total_size;          /*!< Total size of the buffer */
    uint8_t           *buf;                 /*!< Pointer to the start of the buffer */
    uint8_t           *buf_end;             /*!< Pointer to the end of the buffer */
    uint8_t           *p_rd;                /*!< Pointer to the read position */
    uint8_t           *p_wr;                /*!< Pointer to the write position */
    uint8_t           *p_wr_end;            /*!< Pointer to the write position */
    uint32_t           fill_size;           /*!< Number of bytes filled in the buffer */
    uint8_t            _set_done      : 1;  /*!< Flag indicating done is set */
    uint8_t            _is_write_done : 1;  /*!< Flag indicating if writing is done */
    uint8_t            _is_abort      : 1;  /*!< Flag indicating if an abort signal has been received */
} esp_gmf_block_t;

static inline void _block_handle_free(esp_gmf_block_handle_t handle)
{
    esp_gmf_block_t *hd = (esp_gmf_block_t *)handle;
    if (hd->buf) {
        esp_gmf_oal_free(hd->buf);
    }
    if (hd->can_read) {
        vSemaphoreDelete(hd->can_read);
    }
    if (hd->can_write) {
        vSemaphoreDelete(hd->can_write);
    }
    if (hd->lock) {
        esp_gmf_oal_mutex_destroy(hd->lock);
    }
    esp_gmf_oal_free(hd);
}

static inline uint32_t get_fill_size(esp_gmf_block_t *hd)
{
    uint32_t fill_len = 0;
    if (hd->p_wr > hd->p_rd) {
        fill_len = hd->p_wr - hd->p_rd;
    } else if (hd->p_wr < hd->p_rd) {
        if (hd->p_wr_end == hd->p_rd) {
            fill_len = hd->p_wr - hd->buf;
        } else {
            fill_len = hd->p_wr_end - hd->p_rd;
        }
    } else {
        // p_wr == p_rd
        if (hd->p_wr_end == hd->buf) {
            fill_len = hd->p_wr - hd->p_rd;
        } else {
            fill_len = hd->p_wr_end - hd->p_wr;
        }
    }
    ESP_LOGV(TAG, "F:%p,%p,%p,%ld", hd->p_rd, hd->p_wr, hd->p_wr_end, fill_len);
    return fill_len;
}

static inline uint32_t get_empty_size(esp_gmf_block_t *hd)
{
    uint32_t emtpy_len = 0;
    if (hd->p_wr > hd->p_rd) {
        emtpy_len = hd->buf_end - hd->p_wr;
    } else if (hd->p_wr < hd->p_rd) {
        if (hd->p_wr_end == hd->p_rd) {
            emtpy_len = hd->buf_end - hd->p_wr;
        } else {
            emtpy_len = hd->p_rd - hd->p_wr;
        }
    } else {
        // p_wr == p_rd
        if (hd->p_wr_end == hd->buf) {
            emtpy_len = hd->buf_end - hd->p_wr;
        } else {
            emtpy_len = hd->p_rd - hd->p_wr;
        }
    }
    ESP_LOGV(TAG, "E:%p,%p,%p,%ld", hd->p_rd, hd->p_wr, hd->p_wr_end, emtpy_len);
    return emtpy_len;
}

esp_gmf_err_t esp_gmf_block_create(int block_size, int block_cnt, esp_gmf_block_handle_t *handle)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    if ((block_cnt < 1)
        || (block_size < 1)) {
        ESP_LOGE(TAG, "Invalid parameters,cnt:%d, size:%d", block_cnt, block_size);
        return ESP_GMF_ERR_INVALID_ARG;
    }
    *handle = NULL;
    esp_gmf_block_t *blk = esp_gmf_oal_calloc(1, sizeof(esp_gmf_block_t));
    int ret = ESP_GMF_ERR_OK;
    ESP_GMF_MEM_CHECK(TAG, blk, return ESP_GMF_ERR_MEMORY_LACK;);
    blk->buf = (uint8_t *)esp_gmf_oal_calloc(block_size, block_cnt);
    ESP_GMF_MEM_CHECK(TAG, blk->buf, {ret = ESP_GMF_ERR_MEMORY_LACK; goto esp_gmf_blk_err;});
    blk->total_size = block_size * block_cnt;
    blk->b_cnt = block_cnt;
    blk->b_size = block_size;
    blk->buf_end = blk->buf + blk->total_size;
    blk->p_rd = blk->buf;
    blk->p_wr = blk->buf;
    blk->p_wr_end = blk->buf;

    blk->can_read = xSemaphoreCreateBinary();
    ESP_GMF_MEM_CHECK(TAG, blk->can_read, {ret = ESP_GMF_ERR_MEMORY_LACK; goto esp_gmf_blk_err;});
    blk->can_write = xSemaphoreCreateBinary();
    ESP_GMF_MEM_CHECK(TAG, blk->can_write, {ret = ESP_GMF_ERR_MEMORY_LACK; goto esp_gmf_blk_err;});
    blk->lock = (SemaphoreHandle_t)esp_gmf_oal_mutex_create();
    ESP_GMF_MEM_CHECK(TAG, blk->lock, {ret = ESP_GMF_ERR_MEMORY_LACK; goto esp_gmf_blk_err;});
    blk->fill_size = 0;
    blk->_is_write_done = 0;
    blk->_is_abort = 0;
    *handle = blk;
    ESP_LOGI(TAG, "The block buf:%p, end:%p", blk->buf, blk->buf_end);
    return ESP_GMF_ERR_OK;

esp_gmf_blk_err:
    _block_handle_free(handle);
    return ret;
}

esp_gmf_err_t esp_gmf_block_destroy(esp_gmf_block_handle_t handle)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    _block_handle_free(handle);
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_io_t esp_gmf_block_acquire_read(esp_gmf_block_handle_t handle, esp_gmf_data_bus_block_t *blk, uint32_t wanted_size, int block_ticks)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_IO_FAIL);
    ESP_GMF_NULL_CHECK(TAG, blk, return ESP_GMF_IO_FAIL);
    esp_gmf_block_t *hd = (esp_gmf_block_t *)handle;
    if (wanted_size > hd->total_size || wanted_size == 0) {
        ESP_LOGE(TAG, "ACQ_R, out of range, total:%ld, wanted:%ld", hd->total_size, wanted_size);
        return ESP_GMF_IO_FAIL;
    }
    ESP_LOGD(TAG, "ACQ_R+, rd:%p, wr:%p, wr_e:%p, f:%ld, done:%d, wanted:%ld", hd->p_rd, hd->p_wr, hd->p_wr_end, hd->fill_size, hd->_is_write_done, wanted_size);
    esp_gmf_oal_mutex_lock(hd->lock);
    if (hd->fill_size == 0 && hd->_is_write_done) {
        esp_gmf_oal_mutex_unlock(hd->lock);
        blk->is_last = 1;
        blk->valid_size = 0;
        ESP_LOGW(TAG, "Done set on read, h:%p, rd:%p, wr:%p, wr_e:%p", hd, hd->p_rd, hd->p_wr, hd->p_wr_end);
        return ESP_GMF_IO_OK;
    }
    blk->is_last = 0;
    while (get_fill_size(hd) < wanted_size) {
        if (hd->buf < hd->p_wr_end) {
            if (hd->p_wr_end != hd->p_rd) {
                wanted_size = hd->p_wr_end - hd->p_rd;
                ESP_LOGV(TAG, "Read tail data is not enough for wanted:%ld, h:%p, r:%p, w:%p, we:%p", wanted_size, hd, hd->p_rd, hd->p_wr, hd->p_wr_end);
                break;
            } else {
                hd->p_wr_end = hd->buf;
                hd->p_rd = hd->buf;
                ESP_LOGV(TAG, "Read tail data is not enough for read, wanted:%ld, h:%p, r:%p, w:%p, we:%p", wanted_size, hd, hd->p_rd, hd->p_wr, hd->p_wr_end);
                if (get_fill_size(hd) >= wanted_size) {
                    break;
                }
            }
        }
        if (hd->_is_write_done) {
            wanted_size = get_fill_size(hd);
            blk->is_last = 1;
            ESP_LOGI(TAG, "Done on read, wanted:%ld, h:%p, r:%p, w:%p, we:%p", wanted_size, hd, hd->p_rd, hd->p_wr, hd->p_wr_end);
            break;
        }
        esp_gmf_oal_mutex_unlock(hd->lock);
        if (hd->_is_abort) {
            return ESP_GMF_IO_ABORT;
        }
        ESP_LOGV(TAG, "R-T:%p, %p, %p, wanted:%ld, fill:%ld", hd->p_rd, hd->p_wr, hd->p_wr_end, wanted_size, get_fill_size(hd));
        if (xSemaphoreTake(hd->can_read, block_ticks) != pdPASS) {
            ESP_LOGE(TAG, "Read timeout");
            return ESP_GMF_IO_TIMEOUT;
        }
        if (hd->_is_abort) {
            return ESP_GMF_IO_ABORT;
        }
        esp_gmf_oal_mutex_lock(hd->lock);
    }
    if (hd->_is_abort) {
        esp_gmf_oal_mutex_unlock(hd->lock);
        return ESP_GMF_IO_ABORT;
    }
    uint32_t act_sz = wanted_size;
    if ((hd->p_rd + act_sz) > hd->buf_end) {
        act_sz = hd->buf_end - hd->p_rd;
        blk->valid_size = act_sz;
        blk->buf = hd->p_rd;
        blk->buf_length = act_sz;
    } else {
        blk->valid_size = wanted_size;
        blk->buf = hd->p_rd;
        blk->buf_length = wanted_size;
    }
    esp_gmf_oal_mutex_unlock(hd->lock);
    return ESP_GMF_IO_OK;
}

esp_gmf_err_io_t esp_gmf_block_release_read(esp_gmf_block_handle_t handle, esp_gmf_data_bus_block_t *blk, int block_ticks)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_IO_FAIL);
    ESP_GMF_NULL_CHECK(TAG, blk, return ESP_GMF_IO_FAIL);
    esp_gmf_block_t *hd = (esp_gmf_block_t *)handle;
    if ((hd->p_rd + blk->valid_size) > hd->buf_end) {
        ESP_LOGE(TAG, "The block pointer is invalid, rd:%p, end:%p", hd->p_rd, hd->buf_end);
        return ESP_GMF_IO_FAIL;
    }
    esp_gmf_oal_mutex_lock(hd->lock);
    hd->p_rd += blk->valid_size;
    hd->fill_size -= blk->valid_size;
    ESP_LOGD(TAG, "ACQ_R-, rd:%p, wr:%p, wr_e:%p, f:%ld, done:%d, vld:%d", hd->p_rd, hd->p_wr, hd->p_wr_end, hd->fill_size, hd->_is_write_done, blk->valid_size);
    if ((hd->p_rd == hd->buf_end) || (hd->p_rd == hd->p_wr_end)) {
        hd->p_rd = hd->buf;
        hd->p_wr_end = hd->buf;
    }
    esp_gmf_oal_mutex_unlock(hd->lock);
    xSemaphoreGive(hd->can_write);

    return ESP_GMF_IO_OK;
}

esp_gmf_err_io_t esp_gmf_block_acquire_write(esp_gmf_block_handle_t handle, esp_gmf_data_bus_block_t *blk, uint32_t wanted_size, int block_ticks)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_IO_FAIL);
    ESP_GMF_NULL_CHECK(TAG, blk, return ESP_GMF_IO_FAIL);
    esp_gmf_block_t *hd = (esp_gmf_block_t *)handle;
    if ((wanted_size > hd->total_size) || wanted_size == 0) {
        ESP_LOGE(TAG, "ACQ_WR, out of range, total:%ld, wanted:%ld", hd->total_size, wanted_size);
        return ESP_GMF_IO_FAIL;
    }
    esp_gmf_oal_mutex_lock(hd->lock);
    uint32_t emtpy_size = get_empty_size(hd);
    ESP_LOGD(TAG, "ACQ_W+, f:%ld, emt:%ld, rd:%p, wr:%p, wr_e:%p, done:%d, wanted:%ld", hd->fill_size, emtpy_size, hd->p_rd, hd->p_wr, hd->p_wr_end, hd->_is_write_done, wanted_size);
    if ((emtpy_size == 0) && hd->_is_write_done) {
        ESP_LOGW(TAG, "Done set on write, h:%p, rd:%p, wr:%p, wr_e:%p", hd, hd->p_rd, hd->p_wr, hd->p_wr_end);
        blk->is_last = 1;
        blk->valid_size = 0;
        esp_gmf_oal_mutex_unlock(hd->lock);
        return ESP_GMF_IO_OK;
    }
    blk->is_last = 0;
    while ((emtpy_size = get_empty_size(hd)) < wanted_size) {
        if (hd->p_wr >= hd->p_rd) {
            if (((hd->buf_end - hd->p_wr) < wanted_size)
                && ((hd->p_rd - hd->buf) >= wanted_size)
                && (hd->buf == hd->p_wr_end)) {
                ESP_LOGV(TAG, "Move WR to head, r:%p, w:%p, e:%p, empt:%ld, wanted:%ld", hd->p_rd, hd->p_wr, hd->p_wr_end, emtpy_size, wanted_size);
                hd->p_wr_end = hd->p_wr;
                hd->p_wr = hd->buf;
                break;
            }
        }
        if (hd->_is_write_done) {
            wanted_size = hd->fill_size;
            blk->is_last = 1;
            ESP_LOGI(TAG, "Done on write");
            break;
        }
        esp_gmf_oal_mutex_unlock(hd->lock);
        if (hd->_is_abort) {
            return ESP_GMF_IO_ABORT;
        }
        ESP_LOGV(TAG, "W-T:%p,%p,%p,%ld, empt:%ld\r\n", hd->p_rd, hd->p_wr, hd->p_wr_end, wanted_size, get_empty_size(hd));
        if (xSemaphoreTake(hd->can_write, block_ticks) != pdPASS) {
            ESP_LOGE(TAG, "Write timeout");
            return ESP_GMF_IO_TIMEOUT;
        }
        if (hd->_is_abort) {
            return ESP_GMF_IO_ABORT;
        }
        esp_gmf_oal_mutex_lock(hd->lock);
    }
    if (hd->_is_abort) {
        esp_gmf_oal_mutex_unlock(hd->lock);
        return ESP_GMF_IO_ABORT;
    }
    blk->buf_length = wanted_size;
    blk->buf = hd->p_wr;
    blk->valid_size = 0;

    esp_gmf_oal_mutex_unlock(hd->lock);
    return ESP_GMF_IO_OK;
}

esp_gmf_err_io_t esp_gmf_block_release_write(esp_gmf_block_handle_t handle, esp_gmf_data_bus_block_t *blk, int block_ticks)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_IO_FAIL);
    ESP_GMF_NULL_CHECK(TAG, blk, return ESP_GMF_IO_FAIL);
    esp_gmf_block_t *hd = (esp_gmf_block_t *)handle;
    if ((hd->p_wr + blk->valid_size) > hd->buf_end) {
        ESP_LOGE(TAG, "The buffer is out of range, wr:%p, vld:%d, end:%p", hd->p_wr, blk->valid_size, hd->buf_end);
        return ESP_GMF_IO_FAIL;
    }
    esp_gmf_oal_mutex_lock(hd->lock);
    hd->p_wr += blk->valid_size;
    hd->fill_size += blk->valid_size;
    if (hd->p_wr == hd->buf_end) {
        hd->p_wr = hd->buf;
        hd->p_wr_end = hd->buf_end;
    }
    if (hd->_set_done) {
        hd->_is_write_done = hd->_set_done;
    }
    ESP_LOGD(TAG, "ACQ_W-, f:%ld, emt:%ld, rd:%p, wr:%p,wr_e:%p, done:%d, vld:%d", hd->fill_size, get_empty_size(hd), hd->p_rd, hd->p_wr, hd->p_wr_end, hd->_is_write_done, blk->valid_size);
    esp_gmf_oal_mutex_unlock(hd->lock);
    xSemaphoreGive(hd->can_read);
    return ESP_GMF_IO_OK;
}

esp_gmf_err_t esp_gmf_block_done_write(esp_gmf_block_handle_t handle)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    esp_gmf_block_t *hd = (esp_gmf_block_t *)handle;
    hd->_set_done = 1;
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_block_abort(esp_gmf_block_handle_t handle)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    esp_gmf_block_t *hd = (esp_gmf_block_t *)handle;
    hd->_is_abort = 1;
    xSemaphoreGive(hd->can_read);
    xSemaphoreGive(hd->can_write);
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_block_reset(esp_gmf_block_handle_t handle)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    esp_gmf_block_t *hd = (esp_gmf_block_t *)handle;
    esp_gmf_oal_mutex_lock(hd->lock);
    hd->p_rd = hd->buf;
    hd->p_wr = hd->buf;
    hd->p_wr_end = hd->buf;
    hd->fill_size = 0;
    hd->_set_done = 0;
    hd->_is_write_done = 0;
    hd->_is_abort = 0;
    ESP_LOGD(TAG, "esp_gmf_block_reset, %p", hd);
    esp_gmf_oal_mutex_unlock(hd->lock);
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_block_get_free_size(esp_gmf_block_handle_t handle, uint32_t *free_size)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    ESP_GMF_NULL_CHECK(TAG, free_size, return ESP_GMF_ERR_INVALID_ARG);
    esp_gmf_block_t *hd = (esp_gmf_block_t *)handle;
    esp_gmf_oal_mutex_lock(hd->lock);
    *free_size = hd->total_size - hd->fill_size;
    esp_gmf_oal_mutex_unlock(hd->lock);
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_block_get_filled_size(esp_gmf_block_handle_t handle, uint32_t *filled_size)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    ESP_GMF_NULL_CHECK(TAG, filled_size, return ESP_GMF_ERR_INVALID_ARG);
    esp_gmf_block_t *hd = (esp_gmf_block_t *)handle;
    esp_gmf_oal_mutex_lock(hd->lock);
    *filled_size = hd->fill_size;
    esp_gmf_oal_mutex_unlock(hd->lock);
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_block_get_total_size(esp_gmf_block_handle_t handle, uint32_t *total_size)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    ESP_GMF_NULL_CHECK(TAG, total_size, return ESP_GMF_ERR_INVALID_ARG);
    esp_gmf_block_t *hd = (esp_gmf_block_t *)handle;
    esp_gmf_oal_mutex_lock(hd->lock);
    *total_size = hd->total_size;
    esp_gmf_oal_mutex_unlock(hd->lock);
    return ESP_GMF_ERR_OK;
}
