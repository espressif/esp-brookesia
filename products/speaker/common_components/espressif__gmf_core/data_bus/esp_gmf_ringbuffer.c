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
#include "esp_gmf_ringbuffer.h"
#include "esp_log.h"
#include "esp_gmf_oal_mem.h"

static const char *TAG = "ESP_GMF_RB";

/**
 * @brief  Structure representing a ring buffer
 */
struct esp_gmf_ringbuffer {
    char *p_o;                            /*!< Original pointer */
    char *volatile p_r;                   /*!< Read pointer */
    char *volatile p_w;                   /*!< Write pointer */
    volatile uint32_t  fill_cnt;           /*!< Number of filled size */
    uint32_t           size;               /*!< Buffer size */
    SemaphoreHandle_t  can_read;           /*!< Semaphore to control reading */
    SemaphoreHandle_t  can_write;          /*!< Semaphore to control writing */
    SemaphoreHandle_t  lock;               /*!< Semaphore to protect critical sections */
    uint8_t            abort_read    : 1;  /*!< Flag to indicate read abort */
    uint8_t            abort_write   : 1;  /*!< Flag to indicate write abort */
    uint8_t            is_done_write : 1;  /*!< Flag to signal completion of writing */
};

esp_gmf_err_t esp_gmf_rb_create(int block_size, int n_blocks, esp_gmf_rb_handle_t *handle)
{
    struct esp_gmf_ringbuffer *rb = NULL;
    *handle = NULL;
    bool _success = (
                        (rb            = esp_gmf_oal_malloc(sizeof(struct esp_gmf_ringbuffer))) &&
                        (rb->p_o       = esp_gmf_oal_calloc(n_blocks, block_size)) &&
                        (rb->can_read  = xSemaphoreCreateBinary()) &&
                        (rb->lock      = xSemaphoreCreateMutex()) &&
                        (rb->can_write = xSemaphoreCreateBinary())
                    );

    ESP_GMF_MEM_CHECK(TAG, _success, goto _esp_gmf_rb_init_failed);
    rb->p_r = rb->p_w = rb->p_o;
    rb->fill_cnt = 0;
    rb->size = block_size * n_blocks;
    rb->is_done_write = 0;
    rb->abort_read = 0;
    rb->abort_write = 0;
    *handle = rb;
    return ESP_GMF_ERR_OK;
_esp_gmf_rb_init_failed:
    esp_gmf_rb_destroy(rb);
    return ESP_GMF_ERR_FAIL;
}

esp_gmf_err_t esp_gmf_rb_destroy(esp_gmf_rb_handle_t handle)
{
    struct esp_gmf_ringbuffer *rb = (struct esp_gmf_ringbuffer *)handle;
    if (rb == NULL) {
        return ESP_GMF_ERR_INVALID_ARG;
    }
    if (rb->p_o) {
        esp_gmf_oal_free(rb->p_o);
        rb->p_o = NULL;
    }
    if (rb->can_read) {
        vSemaphoreDelete(rb->can_read);
        rb->can_read = NULL;
    }
    if (rb->can_write) {
        vSemaphoreDelete(rb->can_write);
        rb->can_write = NULL;
    }
    if (rb->lock) {
        vSemaphoreDelete(rb->lock);
        rb->lock = NULL;
    }
    esp_gmf_oal_free(rb);
    rb = NULL;
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_rb_reset(esp_gmf_rb_handle_t handle)
{
    struct esp_gmf_ringbuffer *rb = (struct esp_gmf_ringbuffer *)handle;
    if (rb == NULL) {
        return ESP_GMF_ERR_INVALID_ARG;
    }
    rb->p_r = rb->p_w = rb->p_o;
    rb->fill_cnt = 0;
    rb->is_done_write = 0;
    rb->abort_read = 0;
    rb->abort_write = 0;
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_io_t esp_gmf_rb_acquire_read(esp_gmf_rb_handle_t handle, esp_gmf_data_bus_block_t *blk, uint32_t wanted_size, int ticks_to_wait)
{
    struct esp_gmf_ringbuffer *rb = (struct esp_gmf_ringbuffer *)handle;
    int read_size = 0;
    int total_read_size = 0;
    int ret_val = ESP_GMF_IO_OK;

    if (rb == NULL || blk == NULL) {
        ESP_LOGE(TAG, "Invalid parameters on acquire read, rb:%p, blk:%p", rb, blk);
        return ESP_GMF_IO_FAIL;
    }
    uint32_t buf_len = wanted_size;
    uint8_t *buf = blk->buf;
    ESP_LOGV(TAG, "ACQ_RD+:%p, b:%p, l:%d, s:%ld", rb, buf, blk->buf_length, wanted_size);
    while (buf_len) {
        // take buffer lock
        if (xSemaphoreTake(rb->lock, portMAX_DELAY) != pdTRUE) {
            ret_val = ESP_GMF_IO_TIMEOUT;
            goto read_err;
        }

        if (rb->fill_cnt < buf_len) {
            read_size = rb->fill_cnt;
            /**
             * When non-multiple of 4(word size) bytes are written to I2S, there is noise.
             * Below is the kind of workaround to read only in multiple of 4. Avoids noise when rb is read in small chunks.
             * Note that, when we have buf_len bytes available in rb, we still read those irrespective of if it's multiple of 4.
             */
            read_size = read_size & 0xfffffffc;
            if ((read_size == 0) && rb->is_done_write) {
                read_size = rb->fill_cnt;
            }
        } else {
            read_size = buf_len;
        }
        if (read_size == 0) {
            // no data to read, release thread block to allow other threads to write data
            if (rb->is_done_write) {
                blk->is_last = 1;
                xSemaphoreGive(rb->lock);
                goto read_err;
            }
            if (rb->abort_read) {
                ret_val = ESP_GMF_IO_ABORT;
                xSemaphoreGive(rb->lock);
                goto read_err;
            }

            xSemaphoreGive(rb->lock);
            xSemaphoreGive(rb->can_write);
            // wait till some data available to read
            if (xSemaphoreTake(rb->can_read, ticks_to_wait) != pdTRUE) {
                ret_val = ESP_GMF_IO_TIMEOUT;
                goto read_err;
            }
            continue;
        }

        if ((rb->p_r + read_size) > (rb->p_o + rb->size)) {
            int rlen1 = rb->p_o + rb->size - rb->p_r;
            int rlen2 = read_size - rlen1;
            if (buf) {
                memcpy(buf, rb->p_r, rlen1);
                memcpy(buf + rlen1, rb->p_o, rlen2);
            }
            rb->p_r = rb->p_o + rlen2;
        } else {
            if (buf) {
                memcpy(buf, rb->p_r, read_size);
            }
            rb->p_r = rb->p_r + read_size;
        }

        buf_len -= read_size;
        rb->fill_cnt -= read_size;
        total_read_size += read_size;
        buf += read_size;
        xSemaphoreGive(rb->lock);
        if (buf_len == 0) {
            break;
        }
    }
read_err:
    if (total_read_size > 0) {
        xSemaphoreGive(rb->can_write);
    }
    ESP_LOGV(TAG, "ACQ_RD-:%p, ret:%d", rb, ret_val);
    blk->valid_size = total_read_size;
    return ret_val;
}

esp_gmf_err_io_t esp_gmf_rb_release_read(esp_gmf_rb_handle_t handle, esp_gmf_data_bus_block_t *blk, int block_ticks)
{
    // Do nothing
    return ESP_GMF_IO_OK;
}

esp_gmf_err_io_t esp_gmf_rb_acquire_write(esp_gmf_rb_handle_t handle, esp_gmf_data_bus_block_t *blk, uint32_t wanted_size, int ticks_to_wait)
{
    return wanted_size;
}

esp_gmf_err_io_t esp_gmf_rb_release_write(esp_gmf_rb_handle_t handle, esp_gmf_data_bus_block_t *blk, int block_ticks)
{
    struct esp_gmf_ringbuffer *rb = (struct esp_gmf_ringbuffer *)handle;
    uint32_t write_size;
    int total_write_size = 0;
    esp_gmf_err_io_t ret_val = ESP_GMF_IO_OK;
    if (rb == NULL || blk == NULL) {
        ESP_LOGE(TAG, "Invalid parameters on release write, rb:%p, blk:%p", rb, blk);
        return ESP_GMF_IO_FAIL;
    }
    uint8_t *buf = blk->buf;
    int buf_len = blk->valid_size;
    esp_gmf_rb_bytes_available(rb, &write_size);
    ESP_LOGV(TAG, "RLS_WR+:%p, blk:%p, vld_sz:%d, wanted:%ld, time:%d", rb, blk, blk->valid_size, write_size, block_ticks);
    while (buf_len) {
        // take buffer lock
        if (xSemaphoreTake(rb->lock, portMAX_DELAY) != pdTRUE) {
            ret_val = ESP_GMF_IO_TIMEOUT;
            goto write_err;
        }
        esp_gmf_rb_bytes_available(rb, &write_size);
        if (buf_len < write_size) {
            write_size = buf_len;
        }
        if (write_size == 0) {
            // no space to write, release thread block to allow other to read data
            if (rb->is_done_write) {
                xSemaphoreGive(rb->lock);
                ESP_LOGD(TAG, "WR:%p, done\r\n", rb);
                goto write_err;
            }
            if (rb->abort_write) {
                ret_val = ESP_GMF_IO_ABORT;
                xSemaphoreGive(rb->lock);
                ESP_LOGD(TAG, "WR:%p, abort\r\n", rb);
                goto write_err;
            }
            xSemaphoreGive(rb->lock);
            xSemaphoreGive(rb->can_read);
            // wait till we have some empty space to write
            if (xSemaphoreTake(rb->can_write, block_ticks) != pdTRUE) {
                ret_val = ESP_GMF_IO_TIMEOUT;
                ESP_LOGD(TAG, "WR:%p, timeout:%d\r\n", rb, block_ticks);
                goto write_err;
            }
            continue;
        }

        if ((rb->p_w + write_size) > (rb->p_o + rb->size)) {
            int wlen1 = rb->p_o + rb->size - rb->p_w;
            int wlen2 = write_size - wlen1;
            memcpy(rb->p_w, buf, wlen1);
            memcpy(rb->p_o, buf + wlen1, wlen2);
            rb->p_w = rb->p_o + wlen2;
        } else {
            memcpy(rb->p_w, buf, write_size);
            rb->p_w = rb->p_w + write_size;
        }

        buf_len -= write_size;
        rb->fill_cnt += write_size;
        total_write_size += write_size;
        buf += write_size;
        xSemaphoreGive(rb->lock);
        if (buf_len == 0) {
            break;
        }
    }
write_err:
    if (total_write_size > 0) {
        xSemaphoreGive(rb->can_read);
        ret_val = ESP_GMF_IO_OK;
    }
    ESP_LOGV(TAG, "RLS_WR-:%p, ret:%d, ws:%d, fill:%ld", rb, ret_val, total_write_size, rb->fill_cnt);
    if (blk->is_last) {
        esp_gmf_rb_done_write(rb);
        ret_val = ESP_GMF_IO_OK;
    }
    return ret_val;
}

esp_gmf_err_t esp_gmf_rb_abort(esp_gmf_rb_handle_t handle)
{
    struct esp_gmf_ringbuffer *rb = (struct esp_gmf_ringbuffer *)handle;
    if (rb == NULL) {
        return ESP_GMF_ERR_INVALID_ARG;
    }
    ESP_LOGD(TAG, "Abort, rb:%p", rb);
    rb->abort_read = 1;
    xSemaphoreGive(rb->can_read);
    rb->abort_write = 1;
    xSemaphoreGive(rb->can_write);
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_rb_done_write(esp_gmf_rb_handle_t handle)
{
    struct esp_gmf_ringbuffer *rb = (struct esp_gmf_ringbuffer *)handle;
    if (rb == NULL) {
        return ESP_GMF_ERR_INVALID_ARG;
    }
    rb->is_done_write = 1;
    ESP_LOGD(TAG, "Set done write, rb:%p", rb);
    xSemaphoreGive(rb->can_read);
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_rb_reset_done_write(esp_gmf_rb_handle_t handle)
{
    struct esp_gmf_ringbuffer *rb = (struct esp_gmf_ringbuffer *)handle;
    if (rb == NULL) {
        return ESP_GMF_ERR_INVALID_ARG;
    }
    rb->is_done_write = 0;
    ESP_LOGD(TAG, "Reset done write, rb:%p", rb);
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_rb_bytes_available(esp_gmf_rb_handle_t handle, uint32_t *available_size)
{
    struct esp_gmf_ringbuffer *rb = (struct esp_gmf_ringbuffer *)handle;
    if (rb && available_size) {
        *available_size = (rb->size - rb->fill_cnt);
        return ESP_GMF_ERR_OK;
    }
    return ESP_GMF_ERR_INVALID_ARG;
}

esp_gmf_err_t esp_gmf_rb_bytes_filled(esp_gmf_rb_handle_t handle, uint32_t *filled_size)
{
    struct esp_gmf_ringbuffer *rb = (struct esp_gmf_ringbuffer *)handle;
    if (rb && filled_size) {
        *filled_size = rb->fill_cnt;
        return ESP_GMF_ERR_OK;
    }
    return ESP_GMF_ERR_INVALID_ARG;
}

esp_gmf_err_t esp_gmf_rb_get_size(esp_gmf_rb_handle_t handle, uint32_t *valid_size)
{
    struct esp_gmf_ringbuffer *rb = (struct esp_gmf_ringbuffer *)handle;
    if (rb == NULL || valid_size == NULL) {
        return ESP_GMF_ERR_INVALID_ARG;
    }
    *valid_size = rb->size;
    return ESP_GMF_ERR_OK;
}
