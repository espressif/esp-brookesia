/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdlib.h>
#include <string.h>
#include "esp_gmf_io.h"
#include "esp_log.h"
#include "esp_gmf_info_file.h"
#include "esp_gmf_oal_mem.h"
#include "esp_gmf_job.h"

static const char *TAG = "ESP_GMF_IO";

esp_gmf_err_t esp_gmf_io_init(esp_gmf_io_handle_t handle, esp_gmf_io_cfg_t *io_cfg)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    esp_gmf_io_t *io = (esp_gmf_io_t *)handle;
    esp_gmf_info_file_init(&io->attr);
    int ret = ESP_GMF_ERR_OK;
    if (io_cfg && io_cfg->thread.stack > 0) {
        esp_gmf_task_cfg_t cfg = DEFAULT_ESP_GMF_TASK_CONFIG();
        cfg.thread.stack = io_cfg->thread.stack;
        cfg.thread.prio = io_cfg->thread.prio;
        cfg.thread.core = io_cfg->thread.core;
        cfg.thread.stack_in_ext = io_cfg->thread.stack_in_ext;
        cfg.name = OBJ_GET_TAG(io);
        cfg.ctx = handle;
        cfg.cb = NULL;
        if (esp_gmf_task_init(&cfg, &io->task_hd) != ESP_GMF_ERR_OK) {
            ESP_LOGE(TAG, "Failed to create new IO task, [%p-%s]", io, OBJ_GET_TAG(io));
            return ESP_GMF_ERR_FAIL;
        }
        char name[ESP_GMF_JOB_LABLE_MAX_LEN] = "";
        esp_gmf_job_str_cat(name, ESP_GMF_JOB_LABLE_MAX_LEN, OBJ_GET_TAG(io), ESP_GMF_JOB_STR_PROCESS, strlen(ESP_GMF_JOB_STR_PROCESS));
        ret = esp_gmf_task_register_ready_job(io->task_hd, name, io->process, ESP_GMF_JOB_TIMES_INFINITE, handle, true);
    }
    ESP_LOGD(TAG, "Initialize a GMF IO[%p-%s], stack:%d, thread:%p-%s", handle, OBJ_GET_TAG(io),
             io_cfg == NULL ? -1 : io_cfg->thread.stack, io->task_hd, OBJ_GET_TAG(io->task_hd));
    return ret;
}

esp_gmf_err_t esp_gmf_io_deinit(esp_gmf_io_handle_t handle)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    esp_gmf_io_t *io = (esp_gmf_io_t *)handle;
    esp_gmf_info_file_deinit(&io->attr);
    ESP_LOGD(TAG, "Deinitialize a GMF IO[%p-%s], thread:%p-%s", io, OBJ_GET_TAG(io), io->task_hd, OBJ_GET_TAG(io->task_hd));
    if (io->task_hd) {
        esp_gmf_task_deinit(io->task_hd);
    }
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_io_open(esp_gmf_io_handle_t handle)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    esp_gmf_io_t *io = (esp_gmf_io_t *)handle;
    if (io->open == NULL) {
        return ESP_GMF_ERR_NOT_READY;
    }
    int ret = io->open(io);
    ESP_GMF_RET_ON_ERROR(TAG, ret, return ret, "esp_gmf_io_open failed");
    if (io->task_hd) {
        ret = esp_gmf_task_run(io->task_hd);
    }
    return ret;
}

esp_gmf_err_t esp_gmf_io_seek(esp_gmf_io_handle_t handle, uint64_t seek_byte_pos)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    esp_gmf_io_t *io = (esp_gmf_io_t *)handle;
    if (io->seek == NULL) {
        return ESP_GMF_ERR_NOT_READY;
    }
    int ret = ESP_GMF_ERR_OK;
    if (io->prev_close) {
        io->prev_close(io);
    }
    if (io->task_hd) {
        ret = esp_gmf_task_pause(io->task_hd);
    }
    if (ret == ESP_GMF_ERR_OK) {
        ret = io->seek(io, seek_byte_pos);
    }
    if (io->task_hd) {
        esp_gmf_event_state_t st = 0;
        esp_gmf_task_get_state(io->task_hd, &st);
        if (st == ESP_GMF_EVENT_STATE_PAUSED) {
            ret = esp_gmf_task_resume(io->task_hd);
        } else {
            char name[ESP_GMF_JOB_LABLE_MAX_LEN] = "";
            esp_gmf_job_str_cat(name, ESP_GMF_JOB_LABLE_MAX_LEN, OBJ_GET_TAG(io), ESP_GMF_JOB_STR_PROCESS, strlen(ESP_GMF_JOB_STR_PROCESS));
            ret = esp_gmf_task_register_ready_job(io->task_hd, "name", io->process, ESP_GMF_JOB_TIMES_INFINITE, handle, true);
            ret = esp_gmf_task_run(io->task_hd);
        }
    }
    return ret;
}

esp_gmf_err_t esp_gmf_io_close(esp_gmf_io_handle_t handle)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    esp_gmf_io_t *io = (esp_gmf_io_t *)handle;
    if (io->close == NULL) {
        return ESP_GMF_ERR_NOT_READY;
    }
    ESP_LOGD(TAG, "%s, [%p-%s]", __func__, io, OBJ_GET_TAG(io));
    if (io->prev_close) {
        io->prev_close(io);
    }
    if (io->task_hd) {
        esp_gmf_task_stop(io->task_hd);
    }
    return io->close(io);
}

esp_gmf_err_io_t esp_gmf_io_acquire_read(esp_gmf_io_handle_t handle, esp_gmf_payload_t *load, uint32_t wanted_size, int wait_ticks)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_IO_FAIL);
    ESP_GMF_NULL_CHECK(TAG, load, return ESP_GMF_IO_FAIL);
    esp_gmf_io_t *io = (esp_gmf_io_t *)handle;
    if (io->acquire_read == NULL) {
        return ESP_GMF_IO_FAIL;
    }
    return io->acquire_read(io, load, wanted_size, wait_ticks);
}

esp_gmf_err_io_t esp_gmf_io_release_read(esp_gmf_io_handle_t handle, esp_gmf_payload_t *load, int wait_ticks)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_IO_FAIL);
    ESP_GMF_NULL_CHECK(TAG, load, return ESP_GMF_IO_FAIL);
    esp_gmf_io_t *io = (esp_gmf_io_t *)handle;
    if (io->release_read == NULL) {
        return ESP_GMF_IO_FAIL;
    }
    return io->release_read(io, load, wait_ticks);
}

esp_gmf_err_io_t esp_gmf_io_acquire_write(esp_gmf_io_handle_t handle, esp_gmf_payload_t *load, uint32_t wanted_size, int wait_ticks)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_IO_FAIL);
    ESP_GMF_NULL_CHECK(TAG, load, return ESP_GMF_IO_FAIL);
    esp_gmf_io_t *io = (esp_gmf_io_t *)handle;
    if (io->acquire_write == NULL) {
        return ESP_GMF_IO_FAIL;
    }
    return io->acquire_write(io, load, wanted_size, wait_ticks);
}

esp_gmf_err_io_t esp_gmf_io_release_write(esp_gmf_io_handle_t handle, esp_gmf_payload_t *load, int wait_ticks)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_IO_FAIL);
    ESP_GMF_NULL_CHECK(TAG, load, return ESP_GMF_IO_FAIL);
    esp_gmf_io_t *io = (esp_gmf_io_t *)handle;
    if (io->release_write == NULL) {
        return ESP_GMF_IO_FAIL;
    }
    return io->release_write(io, load, wait_ticks);
}

esp_gmf_err_t esp_gmf_io_set_info(esp_gmf_io_handle_t handle, esp_gmf_info_file_t *info)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    ESP_GMF_NULL_CHECK(TAG, info, return ESP_GMF_ERR_INVALID_ARG);
    esp_gmf_io_t *io = (esp_gmf_io_t *)handle;
    memcpy(&io->attr, info, sizeof(esp_gmf_info_file_t));
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_io_get_info(esp_gmf_io_handle_t handle, esp_gmf_info_file_t *info)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    ESP_GMF_NULL_CHECK(TAG, info, return ESP_GMF_ERR_INVALID_ARG);
    esp_gmf_io_t *io = (esp_gmf_io_t *)handle;
    memcpy(info, &io->attr, sizeof(esp_gmf_info_file_t));
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_io_set_uri(esp_gmf_io_handle_t handle, const char *uri)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    esp_gmf_io_t *io = (esp_gmf_io_t *)handle;
    int ret = esp_gmf_info_file_set_uri(&io->attr, uri);
    return ret;
}
esp_gmf_err_t esp_gmf_io_get_uri(esp_gmf_io_handle_t handle, char **uri)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    ESP_GMF_NULL_CHECK(TAG, uri, return ESP_GMF_ERR_INVALID_ARG);
    esp_gmf_io_t *io = (esp_gmf_io_t *)handle;
    return esp_gmf_info_file_get_uri(&io->attr, uri);
}

esp_gmf_err_t esp_gmf_io_set_pos(esp_gmf_io_handle_t handle, uint64_t byte_pos)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    esp_gmf_io_t *io = (esp_gmf_io_t *)handle;
    return esp_gmf_info_file_set_pos(&io->attr, byte_pos);
}
esp_gmf_err_t esp_gmf_io_update_pos(esp_gmf_io_handle_t handle, uint64_t byte_pos)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    esp_gmf_io_t *io = (esp_gmf_io_t *)handle;
    return esp_gmf_info_file_update_pos(&io->attr, byte_pos);
}

esp_gmf_err_t esp_gmf_io_get_pos(esp_gmf_io_handle_t handle, uint64_t *byte_pos)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    ESP_GMF_NULL_CHECK(TAG, byte_pos, return ESP_GMF_ERR_INVALID_ARG);
    esp_gmf_io_t *io = (esp_gmf_io_t *)handle;
    return esp_gmf_info_file_get_pos(&io->attr, byte_pos);
}

esp_gmf_err_t esp_gmf_io_set_size(esp_gmf_io_handle_t handle, uint64_t total_size)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    esp_gmf_io_t *io = (esp_gmf_io_t *)handle;
    return esp_gmf_info_file_set_size(&io->attr, total_size);
}

esp_gmf_err_t esp_gmf_io_get_size(esp_gmf_io_handle_t handle, uint64_t *total_size)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    ESP_GMF_NULL_CHECK(TAG, total_size, return ESP_GMF_ERR_INVALID_ARG);
    esp_gmf_io_t *io = (esp_gmf_io_t *)handle;
    return esp_gmf_info_file_get_size(&io->attr, total_size);
}

esp_gmf_err_t esp_gmf_io_get_type(esp_gmf_io_handle_t handle, esp_gmf_io_type_t *type)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    if (type) {
        esp_gmf_io_t *io = (esp_gmf_io_t *)handle;
        *type = io->type;
        return ESP_GMF_ERR_OK;
    }
    return ESP_GMF_ERR_INVALID_ARG;
}
