/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <string.h>
#include "sys/queue.h"
#include "esp_log.h"
#include "esp_gmf_element.h"
#include "esp_gmf_pipeline.h"
#include "esp_gmf_data_bus.h"
#include "esp_gmf_new_databus.h"
#include "esp_gmf_pool.h"
#include "esp_gmf_io.h"
#include "esp_gmf_oal_mem.h"
#include "esp_gmf_err.h"

static const char *TAG = "ESP_GMF_POOL";

typedef struct gmp_pool_io_item {
    STAILQ_ENTRY(gmp_pool_io_item)  next;
    esp_gmf_io_handle_t             instance;
} esp_gmf_io_item_t;
typedef STAILQ_HEAD(gmp_io_list, gmp_pool_io_item) gmp_io_list_t;

typedef struct esp_gmf_element_item {
    STAILQ_ENTRY(esp_gmf_element_item)  next;
    esp_gmf_element_handle_t            instance;
} esp_gmf_element_item_t;
typedef STAILQ_HEAD(esp_gmf_element_list, esp_gmf_element_item) esp_gmf_element_list_t;

struct esp_gmf_pool {
    esp_gmf_element_list_t  el_list;
    gmp_io_list_t           io_list;
} esp_gmf_pool_t;

static inline esp_gmf_element_item_t *__get_element_item_by_tag(esp_gmf_pool_handle_t handle, const char *tag)
{
    esp_gmf_element_item_t *item, *tmp;
    STAILQ_FOREACH_SAFE(item, &handle->el_list, next, tmp) {
        char *el_tag = (char *)"NULL";
        esp_gmf_obj_get_tag((esp_gmf_obj_handle_t)item->instance, &el_tag);
        ESP_LOGD(TAG, "Get EL items:%p-%s", item->instance, el_tag);
        if (el_tag && strcasecmp(el_tag, tag) == 0) {
            return item;
        }
    }
    return NULL;
}

static inline esp_gmf_io_item_t *_get_io_item_by_tag(esp_gmf_pool_handle_t handle, const char *tag, esp_gmf_io_dir_t dir)
{
    esp_gmf_io_item_t *item, *tmp;
    STAILQ_FOREACH_SAFE(item, &handle->io_list, next, tmp) {
        char *io_tag = (char *)"NULL";
        esp_gmf_obj_get_tag((esp_gmf_obj_handle_t)item->instance, &io_tag);
        ESP_LOGD(TAG, "Get IO items: %p-%s, dir:%d", item->instance, io_tag, ((esp_gmf_io_t *)item->instance)->dir);
        if (io_tag && (strcasecmp(io_tag, tag) == 0) && (((esp_gmf_io_t *)item->instance)->dir == dir)) {
            return item;
        }
    }
    return NULL;
}

esp_gmf_err_t esp_gmf_pool_init(esp_gmf_pool_handle_t *handle)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    esp_gmf_pool_handle_t ph = esp_gmf_oal_calloc(1, sizeof(struct esp_gmf_pool));
    ESP_GMF_MEM_CHECK(TAG, ph, return ESP_GMF_ERR_MEMORY_LACK);
    STAILQ_INIT(&ph->el_list);
    STAILQ_INIT(&ph->io_list);
    *handle = ph;
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_pool_deinit(esp_gmf_pool_handle_t handle)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    /// Unregister all of element and io objects
    esp_gmf_element_item_t *el_item, *tmp;
    STAILQ_FOREACH_SAFE(el_item, &handle->el_list, next, tmp) {
        STAILQ_REMOVE(&handle->el_list, el_item, esp_gmf_element_item, next);
        ESP_LOGD(TAG, "%s, el:[%p-%s]", __FUNCTION__, el_item->instance, OBJ_GET_TAG(el_item->instance));
        esp_gmf_obj_delete(el_item->instance);
        esp_gmf_oal_free(el_item);
    }
    esp_gmf_io_item_t *io_item, *tmp1;
    STAILQ_FOREACH_SAFE(io_item, &handle->io_list, next, tmp1) {
        STAILQ_REMOVE(&handle->io_list, io_item, gmp_pool_io_item, next);
        ESP_LOGD(TAG, "%s, io:[%p-%s]", __FUNCTION__, io_item->instance, OBJ_GET_TAG(io_item->instance));
        esp_gmf_obj_delete(io_item->instance);
        esp_gmf_oal_free(io_item);
    }
    esp_gmf_oal_free(handle);
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_pool_register_element(esp_gmf_pool_handle_t handle, esp_gmf_element_handle_t el, const char *tag)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    ESP_GMF_NULL_CHECK(TAG, el, return ESP_GMF_ERR_INVALID_ARG);
    esp_gmf_element_item_t *el_item = (esp_gmf_element_item_t *)esp_gmf_oal_calloc(1, sizeof(esp_gmf_element_item_t));
    ESP_GMF_MEM_CHECK(TAG, el_item, return ESP_GMF_ERR_MEMORY_LACK);
    if (tag) {
        int ret = esp_gmf_obj_set_tag((esp_gmf_obj_t *)el, tag);
        ESP_GMF_RET_ON_ERROR(TAG, ret, return ret, "Set EL tag failed, obj:%p, tag:%s", el, OBJ_GET_TAG((esp_gmf_obj_t *)el));
    }
    el_item->instance = el;
    STAILQ_INSERT_TAIL(&handle->el_list, el_item, next);
    ESP_LOGD(TAG, "REG el:[%p-%s], item:%p", el, OBJ_GET_TAG(el), el_item);
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_pool_register_io(esp_gmf_pool_handle_t handle, esp_gmf_io_handle_t io, const char *tag)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    ESP_GMF_NULL_CHECK(TAG, io, return ESP_GMF_ERR_INVALID_ARG);
    esp_gmf_io_item_t *io_item = (esp_gmf_io_item_t *)esp_gmf_oal_calloc(1, sizeof(esp_gmf_io_item_t));
    ESP_GMF_MEM_CHECK(TAG, io_item, return ESP_GMF_ERR_MEMORY_LACK);
    if (tag) {
        int ret = esp_gmf_obj_set_tag((esp_gmf_obj_t *)io, tag);
        ESP_GMF_RET_ON_ERROR(TAG, ret, return ret, "Set IO tag failed, IO:%p, tag:%s", io, OBJ_GET_TAG((esp_gmf_obj_t *)io));
    }
    io_item->instance = io;
    STAILQ_INSERT_TAIL(&handle->io_list, io_item, next);
    ESP_LOGD(TAG, "REG IO:[%p-%s], item:%p, pool:%p", io, OBJ_GET_TAG(io), io_item, handle);
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_pool_new_io(esp_gmf_pool_handle_t handle, const char *name, esp_gmf_io_dir_t dir, esp_gmf_io_handle_t *new_io)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    ESP_GMF_NULL_CHECK(TAG, name, return ESP_GMF_ERR_INVALID_ARG);
    ESP_GMF_NULL_CHECK(TAG, new_io, return ESP_GMF_ERR_INVALID_ARG);
    if (dir < ESP_GMF_IO_DIR_READER) {
        ESP_LOGD(TAG, "Invalid direction, dir:%x, name:%s, pool:%p", dir, name, handle);
        return ESP_GMF_ERR_INVALID_ARG;
    }
    esp_gmf_io_item_t *io_item = _get_io_item_by_tag(handle, name, dir);
    if (io_item == NULL) {
        ESP_LOGE(TAG, "Not found %s port, name:%s, pool:%p", dir > ESP_GMF_IO_DIR_READER ? "WRITER" : "READER", name, handle);
        return ESP_GMF_ERR_NOT_FOUND;
    }
    esp_gmf_obj_handle_t new_io_obj = NULL;
    int ret = esp_gmf_obj_dupl((esp_gmf_obj_handle_t)io_item->instance, (void *)&new_io_obj);
    ESP_GMF_RET_ON_ERROR(TAG, ret, return ESP_GMF_ERR_MEMORY_LACK, "Failed to create %s IO object, name:%s, [%p-%s]",
                         dir > ESP_GMF_IO_DIR_READER ? "WRITER" : "READER", name, io_item->instance, OBJ_GET_TAG(io_item->instance));
    *new_io = new_io_obj;
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_pool_new_element(esp_gmf_pool_handle_t handle, const char *el_name, esp_gmf_element_handle_t *new_el)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    ESP_GMF_NULL_CHECK(TAG, el_name, return ESP_GMF_ERR_INVALID_ARG);
    ESP_GMF_NULL_CHECK(TAG, new_el, return ESP_GMF_ERR_INVALID_ARG);
    esp_gmf_element_item_t *el_item = __get_element_item_by_tag(handle, el_name);
    if (el_item == NULL) {
        ESP_LOGE(TAG, "Can't found the element[%s]", el_name);
        return ESP_GMF_ERR_NOT_FOUND;
    }
    esp_gmf_obj_handle_t new_el_obj = NULL;
    int ret = esp_gmf_obj_dupl(el_item->instance, (void *)&new_el_obj);
    ESP_GMF_RET_ON_ERROR(TAG, ret, return ESP_GMF_ERR_MEMORY_LACK, "Failed to create element object, [%p-%s]", el_item->instance, OBJ_GET_TAG(el_item->instance));
    *new_el = new_el_obj;
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_pool_new_pipeline(esp_gmf_pool_handle_t handle, const char *in_name,
                                        const char *el_name[], int num_of_el_name,
                                        const char *out_name, esp_gmf_pipeline_handle_t *pipeline)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    ESP_GMF_NULL_CHECK(TAG, el_name, return ESP_GMF_ERR_INVALID_ARG);
    ESP_GMF_NULL_CHECK(TAG, pipeline, return ESP_GMF_ERR_INVALID_ARG);
    if (num_of_el_name < 1) {
        ESP_LOGE(TAG, "The number of name is too short, %d", num_of_el_name);
        return ESP_GMF_ERR_INVALID_ARG;
    }
    esp_gmf_pipeline_t *pl = NULL;
    esp_gmf_pipeline_create(&pl);
    ESP_GMF_MEM_CHECK(TAG, pl, return ESP_GMF_ERR_MEMORY_LACK);
    *pipeline = pl;
    int el_cnt = 0;
    esp_gmf_obj_handle_t new_last_el_obj = NULL;
    esp_gmf_obj_handle_t new_prev_el_obj = NULL;
    esp_gmf_obj_handle_t new_first_el_obj = NULL;
    int ret = ESP_GMF_ERR_OK;
    // Link the elements
    for (int i = 0; i < num_of_el_name; ++i) {
        esp_gmf_element_handle_t new_el = NULL;
        ret = esp_gmf_pool_new_element(handle, el_name[i], &new_el);
        if (ret != ESP_GMF_ERR_OK) {
            goto NEW_PIPE_FAIL;
        }
        ESP_LOGD(TAG, "TO link elements, [%p-%s]", new_el, OBJ_GET_TAG(new_el));
        el_cnt++;
        if (el_cnt == 1) {
            new_first_el_obj = new_el;
            new_prev_el_obj = new_el;
            new_last_el_obj = new_el;
        }
        if ((num_of_el_name > 1) && (el_cnt > 1)) {
            esp_gmf_port_handle_t out_port = NULL;
            esp_gmf_port_handle_t in_port = NULL;
            out_port = NEW_ESP_GMF_PORT_OUT_BLOCK(NULL, NULL, NULL, NULL, (ESP_GMF_ELEMENT_GET(new_first_el_obj)->out_attr.data_size), ESP_GMF_MAX_DELAY);
            ESP_GMF_NULL_CHECK(TAG, out_port, {ret = ESP_GMF_ERR_MEMORY_LACK; goto NEW_PIPE_FAIL;});
            in_port = NEW_ESP_GMF_PORT_IN_BLOCK(NULL, NULL, NULL, NULL, (ESP_GMF_ELEMENT_GET(new_first_el_obj)->in_attr.data_size), ESP_GMF_MAX_DELAY);
            ESP_GMF_NULL_CHECK(TAG, in_port, {ret = ESP_GMF_ERR_MEMORY_LACK; goto NEW_PIPE_FAIL;});
            esp_gmf_element_register_out_port((esp_gmf_element_handle_t)new_prev_el_obj, out_port);
            esp_gmf_element_register_in_port(new_el, in_port);
            new_prev_el_obj = new_el;
            new_last_el_obj = new_el;
        }
        esp_gmf_pipeline_register_el(pl, new_el);
    }
    // Found the input io if already registered
    if (in_name) {
        esp_gmf_io_handle_t new_in = NULL;
        ret = esp_gmf_pool_new_io(handle, in_name, ESP_GMF_IO_DIR_READER, &new_in);
        if (ret != ESP_GMF_ERR_OK) {
            goto NEW_PIPE_FAIL;
        }
        esp_gmf_pipeline_set_io(pl, new_in, ESP_GMF_IO_DIR_READER);

        esp_gmf_io_type_t io_type = 0;
        esp_gmf_io_get_type(new_in, &io_type);
        esp_gmf_port_handle_t in_port = NULL;

        if (io_type == ESP_GMF_IO_TYPE_BYTE) {
            in_port = NEW_ESP_GMF_PORT_IN_BYTE(esp_gmf_io_acquire_read, esp_gmf_io_release_read, NULL, new_in,
                                               (ESP_GMF_ELEMENT_GET(new_first_el_obj)->in_attr.data_size), ESP_GMF_MAX_DELAY);
        } else if (io_type == ESP_GMF_IO_TYPE_BLOCK) {
            in_port = NEW_ESP_GMF_PORT_IN_BLOCK(esp_gmf_io_acquire_read, esp_gmf_io_release_read, NULL, new_in,
                                                (ESP_GMF_ELEMENT_GET(new_first_el_obj)->in_attr.data_size), ESP_GMF_MAX_DELAY);
        } else {
            ESP_LOGE(TAG, "The IN type is incorrect,%d, [%p-%s]", io_type, new_in, OBJ_GET_TAG(new_in));
            ret = ESP_GMF_ERR_NOT_SUPPORT;
            goto NEW_PIPE_FAIL;
        }
        ESP_GMF_NULL_CHECK(TAG, in_port, {ret = ESP_GMF_ERR_MEMORY_LACK; goto NEW_PIPE_FAIL;});
        esp_gmf_element_register_in_port((esp_gmf_element_handle_t)new_first_el_obj, in_port);
        ESP_LOGD(TAG, "TO link IN port, [%p-%s],new:%p", new_in, OBJ_GET_TAG(new_in), in_port);
    }

    // Found the output io if already registered
    if (out_name) {
        esp_gmf_io_handle_t new_out = NULL;
        ret = esp_gmf_pool_new_io(handle, out_name, ESP_GMF_IO_DIR_WRITER, &new_out);
        if (ret != ESP_GMF_ERR_OK) {
            goto NEW_PIPE_FAIL;
        }
        esp_gmf_pipeline_set_io(pl, new_out, ESP_GMF_IO_DIR_WRITER);

        esp_gmf_io_type_t io_type = 0;
        esp_gmf_io_get_type(new_out, &io_type);
        esp_gmf_port_handle_t out_port = NULL;

        if (io_type == ESP_GMF_IO_TYPE_BYTE) {
            out_port = NEW_ESP_GMF_PORT_OUT_BYTE(esp_gmf_io_acquire_write, esp_gmf_io_release_write, NULL, new_out,
                                                 (ESP_GMF_ELEMENT_GET(new_last_el_obj)->out_attr.data_size), ESP_GMF_MAX_DELAY);
        } else if (io_type == ESP_GMF_IO_TYPE_BLOCK) {
            out_port = NEW_ESP_GMF_PORT_OUT_BLOCK(esp_gmf_io_acquire_write, esp_gmf_io_release_write, NULL, new_out,
                                                  (ESP_GMF_ELEMENT_GET(new_last_el_obj)->out_attr.data_size), ESP_GMF_MAX_DELAY);
        } else {
            ESP_LOGE(TAG, "The OUT type is incorrect, %d, [%p-%s]", io_type, new_out, OBJ_GET_TAG(new_out));
            ret = ESP_GMF_ERR_NOT_SUPPORT;
            goto NEW_PIPE_FAIL;
        }
        ESP_GMF_NULL_CHECK(TAG, out_port, {ret = ESP_GMF_ERR_MEMORY_LACK; goto NEW_PIPE_FAIL;});
        esp_gmf_element_register_out_port((esp_gmf_element_handle_t)new_last_el_obj, out_port);
        ESP_LOGD(TAG, "TO link OUT port, [%p-%s], new:%p, sz:%d", new_out, OBJ_GET_TAG(new_out), out_port, (ESP_GMF_ELEMENT_GET(new_last_el_obj)->out_attr.data_size));
    }
    return ESP_GMF_ERR_OK;

NEW_PIPE_FAIL:
    esp_gmf_pipeline_destroy(pl);
    *pipeline = NULL;
    return ret;
}

void esp_gmf_pool_show_lists(esp_gmf_pool_handle_t handle, int line, const char *func)
{
    esp_gmf_io_item_t *item, *tmp;
    ESP_LOGI(TAG, "Registered items on pool:%p, %s-%d", handle, func, line);
    STAILQ_FOREACH_SAFE(item, &handle->io_list, next, tmp) {
        ESP_LOGI(TAG, "IO, Item:%p, H:%p, TAG:%s", item, item->instance, OBJ_GET_TAG(item->instance));
    }
    esp_gmf_element_item_t *el_item, *el_tmp;
    STAILQ_FOREACH_SAFE(el_item, &handle->el_list, next, el_tmp) {
        ESP_LOGI(TAG, "EL, Item:%p, H:%p, TAG:%s", el_item, el_item->instance, OBJ_GET_TAG(el_item->instance));
    }
}
