/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <string.h>
#include "esp_log.h"
#include "esp_gmf_oal_mem.h"
#include "esp_gmf_oal_mutex.h"
#include "esp_gmf_element.h"
#include "esp_gmf_pipeline.h"
#include "esp_gmf_node.h"

#define PIPELINE_PRE_RUN_STATE  (1 << 0)
#define PIPELINE_PRE_STOP_STATE (1 << 1)

static const char *TAG = "ESP_GMF_PIPELINE";

static inline void register_close_jobs_to_task(esp_gmf_pipeline_handle_t pipeline)
{
    esp_gmf_node_t *node = (esp_gmf_node_t *)pipeline->head_el;
    esp_gmf_element_handle_t next_el = pipeline->head_el;
    do {
        ESP_LOGD(TAG, "Add close job, p:%p, tsk:%p, [el:%s-%p]", pipeline, pipeline->thread, OBJ_GET_TAG(next_el), next_el);
        esp_gmf_element_change_job_mask(next_el, ESP_GMF_ELEMENT_JOB_CLOSE);
        char name[ESP_GMF_JOB_LABLE_MAX_LEN] = "";
        esp_gmf_job_str_cat(name, ESP_GMF_JOB_LABLE_MAX_LEN, OBJ_GET_TAG(next_el), ESP_GMF_JOB_STR_CLOSE, strlen(ESP_GMF_JOB_STR_CLOSE));
        esp_gmf_task_register_ready_job(pipeline->thread, name, esp_gmf_element_process_close, ESP_GMF_JOB_TIMES_ONCE, next_el, true);
        node = (esp_gmf_node_t *)next_el;
    } while ((next_el = (esp_gmf_element_handle_t)esp_gmf_node_for_next(node)));
}

static inline esp_gmf_err_t register_working_jobs_to_task(esp_gmf_pipeline_handle_t pipeline, esp_gmf_element_handle_t el)
{
    esp_gmf_oal_mutex_lock(pipeline->lock);
    uint16_t job_mask = 0;
    esp_gmf_event_state_t st = ESP_GMF_EVENT_STATE_NONE;
    esp_gmf_element_get_state(el, &st);
    if (st != ESP_GMF_EVENT_STATE_INITIALIZED) {
        ESP_LOGD(TAG, "The element state is not INITIALIZED, %s, pipe:%p, [el:%s-%p]", esp_gmf_event_get_state_str(st), pipeline, OBJ_GET_TAG(el), el);
        esp_gmf_oal_mutex_unlock(pipeline->lock);
        return ESP_GMF_ERR_NOT_READY;
    }
    if (pipeline->thread == NULL) {
        ESP_LOGW(TAG, "There is no thread for add jobs, pipe:%p, tsk:%p, [el:%s-%p]", pipeline, pipeline->thread, OBJ_GET_TAG(el), el);
        esp_gmf_oal_mutex_unlock(pipeline->lock);
        return ESP_GMF_ERR_NOT_SUPPORT;
    }
    esp_gmf_element_get_job_mask(el, &job_mask);
    if ((job_mask & (ESP_GMF_ELEMENT_JOB_OPEN | ESP_GMF_ELEMENT_JOB_PROCESS)) == 0) {
        ESP_LOGD(TAG, "Add open and process jobs, p:%p, tsk:%p, [el:%s-%p]", pipeline, pipeline->thread, OBJ_GET_TAG(el), el);
        esp_gmf_element_change_job_mask(el, ESP_GMF_ELEMENT_JOB_OPEN);
        esp_gmf_element_change_job_mask(el, ESP_GMF_ELEMENT_JOB_PROCESS);
        char name[ESP_GMF_JOB_LABLE_MAX_LEN] = "";
        esp_gmf_job_str_cat(name, ESP_GMF_JOB_LABLE_MAX_LEN, OBJ_GET_TAG(el), ESP_GMF_JOB_STR_OPEN, strlen(ESP_GMF_JOB_STR_OPEN));
        esp_gmf_task_register_ready_job(pipeline->thread, name, esp_gmf_element_process_open, ESP_GMF_JOB_TIMES_ONCE, el, false);
        esp_gmf_job_str_cat(name, ESP_GMF_JOB_LABLE_MAX_LEN, OBJ_GET_TAG(el), ESP_GMF_JOB_STR_PROCESS, strlen(ESP_GMF_JOB_STR_PROCESS));
        esp_gmf_task_register_ready_job(pipeline->thread, name, esp_gmf_element_process_running, ESP_GMF_JOB_TIMES_INFINITE, el, true);
    }
    esp_gmf_oal_mutex_unlock(pipeline->lock);
    return ESP_GMF_ERR_OK;
}

static inline void _set_pipe_linked_el_state(esp_gmf_pipeline_handle_t pipeline, esp_gmf_event_state_t event)
{
    esp_gmf_element_handle_t next_el = (esp_gmf_element_handle_t)pipeline->head_el;
    while (next_el) {
        esp_gmf_event_state_t st = ESP_GMF_EVENT_STATE_NONE;
        esp_gmf_element_get_state(next_el, &st);
        if (st == ESP_GMF_EVENT_STATE_INITIALIZED) {
            esp_gmf_element_set_state(next_el, event);
        }
        next_el = (esp_gmf_element_handle_t)esp_gmf_node_for_next((esp_gmf_node_t *)next_el);
    }
}

static esp_gmf_err_t esp_gmf_task_evt(esp_gmf_event_pkt_t *evt, void *ctx)
{
    esp_gmf_pipeline_handle_t pipeline = (esp_gmf_pipeline_handle_t)ctx;
    esp_gmf_task_handle_t tsk = evt->from;
    ESP_LOGD(TAG, "TASK EVT, p:%p tsk:%s-%p, t:%x, sub:%s, pld:%p, sz:%d", pipeline,
             OBJ_GET_TAG(tsk), evt->from, evt->type, esp_gmf_event_get_state_str(evt->sub), evt->payload, evt->payload_size);
    int ret_val = ESP_GMF_ERR_OK;
    if (evt->type == ESP_GMF_EVT_TYPE_LOADING_JOB) {
        switch (evt->sub) {
            case ESP_GMF_EVENT_STATE_ERROR:
            case ESP_GMF_EVENT_STATE_STOPPED:
            case ESP_GMF_EVENT_STATE_FINISHED: {
                register_close_jobs_to_task(pipeline);
                if (pipeline->in) {
                    esp_gmf_io_close(pipeline->in);
                }
                if (pipeline->out) {
                    esp_gmf_io_close(pipeline->out);
                }
            } break;
        }
    } else if (evt->type == ESP_GMF_EVT_TYPE_CHANGE_STATE) {
        switch (evt->sub) {
            case ESP_GMF_EVENT_STATE_ERROR:
            case ESP_GMF_EVENT_STATE_STOPPED:
            case ESP_GMF_EVENT_STATE_FINISHED:
                _set_pipe_linked_el_state(pipeline, evt->sub);
                if (pipeline->user_cb) {
                    evt->from = pipeline;
                    pipeline->user_cb(evt, pipeline->user_ctx);
                }
                pipeline->state = evt->sub;
                break;
            case ESP_GMF_EVENT_STATE_PAUSED:
                _set_pipe_linked_el_state(pipeline, evt->sub);
                if (pipeline->user_cb) {
                    evt->from = pipeline;
                    pipeline->user_cb(evt, pipeline->user_ctx);
                }
                pipeline->state = evt->sub;
                break;
            case ESP_GMF_EVENT_STATE_RUNNING: {
                esp_gmf_event_state_t st = 0;
                esp_gmf_task_get_state(tsk, &st);
                if (st != ESP_GMF_EVENT_STATE_PAUSED) {
                    if (pipeline->in) {
                        ret_val = esp_gmf_io_open(pipeline->in);
                        if (ret_val != ESP_GMF_ERR_OK) {
                            evt->sub = ESP_GMF_EVENT_STATE_ERROR;
                            ESP_LOGE(TAG, "Failed to open the in port, ret:%d,[%p-%s]", ret_val, tsk, OBJ_GET_TAG(tsk));
                        }
                    }
                    if ((ret_val == ESP_GMF_ERR_OK) && pipeline->out) {
                        ret_val = esp_gmf_io_open(pipeline->out);
                        if (ret_val != ESP_GMF_ERR_OK) {
                            evt->sub = ESP_GMF_EVENT_STATE_ERROR;
                            ESP_LOGE(TAG, "Failed to open the out port, ret:%d,[%p-%s]", ret_val, tsk, OBJ_GET_TAG(tsk));
                        }
                    }
                    evt->sub = ESP_GMF_EVENT_STATE_OPENING;
                }
                evt->from = pipeline;
                _set_pipe_linked_el_state(pipeline, evt->sub);
                if (pipeline->user_cb) {
                    pipeline->user_cb(evt, pipeline->user_ctx);
                }
                pipeline->state = evt->sub;
                break;
            }
            default:
                break;
        }
    } else {
        ESP_LOGW(TAG, "Not supported event type(%d), [p:%p, tsk:%s-%p]", evt->type, pipeline, OBJ_GET_TAG(tsk), tsk);
    }
    return ret_val;
}

static esp_gmf_err_t pipeline_element_events(esp_gmf_event_pkt_t *evt, void *ctx)
{
    esp_gmf_pipeline_handle_t pipeline = (esp_gmf_pipeline_handle_t)ctx;
    if (pipeline == NULL) {
        return ESP_GMF_ERR_INVALID_ARG;
    }
    esp_gmf_element_handle_t el = evt->from;
    esp_gmf_element_handle_t next_el = (esp_gmf_element_handle_t)pipeline->head_el;

    // 0. Confirm whether the notification target is the first element of another pipeline or
    //    the element of the current pipeline.
    while (next_el) {
        if (next_el == el) {
            break;
        }
        next_el = (esp_gmf_element_handle_t)esp_gmf_node_for_next((esp_gmf_node_t *)next_el);
    }
    if (next_el) {
        next_el = (esp_gmf_element_handle_t)esp_gmf_node_for_next((esp_gmf_node_t *)next_el);
    } else {
        next_el = (esp_gmf_element_handle_t)pipeline->head_el;
    }

    ESP_LOGD(TAG, "EL EVT Start, from:%s-%p, p:%p, next_el:%s-%p", OBJ_GET_TAG(el), el, pipeline, OBJ_GET_TAG(next_el), next_el);

    // 1.Notify the element event
    while (next_el) {
        if ((ESP_GMF_ELEMENT_GET_DEPENDENCY(next_el)) == 0) {
            next_el = (esp_gmf_element_handle_t)esp_gmf_node_for_next((esp_gmf_node_t *)next_el);
        } else {
            break;
        }
    }
    ESP_LOGD(TAG, "EL EVT, p:%p, el:%s-%p, type:%x, sub:%s, payload:%p, size:%d", pipeline,
             OBJ_GET_TAG(el), el, evt->type, esp_gmf_event_get_state_str(evt->sub), evt->payload, evt->payload_size);
    if (next_el) {
        // Notify the element event to next one only
        int ret = esp_gmf_element_receive_event(next_el, evt, ctx);
        if (ret != ESP_GMF_ERR_OK) {
            ESP_LOGE(TAG, "Error notifying event,p:%p, el:%s-%p", pipeline, OBJ_GET_TAG(next_el), next_el);
            return ESP_GMF_ERR_FAIL;
        }
    } else {
        // Notify the element event to other pipeline
        esp_gmf_event_item_t *item = pipeline->evt_conveyor;
        while (item && item->cb) {
            item->cb(evt, item->ctx);
            item = item->next;
        }
    }

    // 2.Add the INITIALIZED jobs to working list
    next_el = (esp_gmf_element_handle_t)pipeline->head_el;
    if (evt->type == ESP_GMF_EVT_TYPE_REPORT_INFO) {
        if (next_el) {
            esp_gmf_element_handle_t tmp = next_el;
            while (tmp) {
                esp_gmf_event_state_t st = ESP_GMF_EVENT_STATE_NONE;
                esp_gmf_element_get_state(tmp, &st);
                if ((st == ESP_GMF_EVENT_STATE_INITIALIZED) && (tmp != el)) {
                    break;
                }
                tmp = (esp_gmf_element_handle_t)esp_gmf_node_for_next((esp_gmf_node_t *)tmp);
            }
            if (tmp == NULL) {
                // The event emit by others pipeline
                tmp = pipeline->head_el;
            }
            next_el = tmp;
            while (next_el) {
                int ret = register_working_jobs_to_task(pipeline, next_el);
                if (ret != ESP_GMF_ERR_OK) {
                    break;
                }
                next_el = (esp_gmf_element_handle_t)esp_gmf_node_for_next((esp_gmf_node_t *)next_el);
            }
        }
        if (el == pipeline->last_el && pipeline->user_cb) {
            pipeline->user_cb(evt, pipeline->user_ctx);
        }
        ESP_LOGD(TAG, "ESP_GMF_EVT_TYPE_REPORT_INFO, [p:%p, el:%s-%p]", pipeline, OBJ_GET_TAG(el), el);
    } else if (evt->type == ESP_GMF_EVT_TYPE_CHANGE_STATE) {
        // Notify the ESP_GMF_EVENT_STATE_RUNNING event to user
        if (el == pipeline->last_el && pipeline->user_cb) {
            pipeline->user_cb(evt, pipeline->user_ctx);
        }
    } else {
        ESP_LOGE(TAG, "Not supported event type(%d), [p:%p, el:%s-%p]", evt->type, pipeline, OBJ_GET_TAG(el), el);
    }
    ESP_LOGD(TAG, "EL EVT END, from:%s-%p, p:%p", OBJ_GET_TAG(el), el, pipeline);
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_pipeline_create(esp_gmf_pipeline_handle_t *pipeline)
{
    ESP_GMF_NULL_CHECK(TAG, pipeline, return ESP_GMF_ERR_INVALID_ARG);
    *pipeline = esp_gmf_oal_calloc(1, sizeof(esp_gmf_pipeline_t));
    ESP_GMF_MEM_CHECK(TAG, *pipeline, return ESP_GMF_ERR_MEMORY_LACK);
    (*pipeline)->lock = esp_gmf_oal_mutex_create();
    ESP_GMF_MEM_CHECK(TAG, (*pipeline)->lock, {esp_gmf_oal_free(*pipeline); return ESP_GMF_ERR_MEMORY_LACK;});
    (*pipeline)->evt_acceptor = pipeline_element_events;
    (*pipeline)->state = ESP_GMF_EVENT_STATE_NONE;
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_pipeline_destroy(esp_gmf_pipeline_handle_t pipeline)
{
    ESP_GMF_NULL_CHECK(TAG, pipeline, return ESP_GMF_ERR_INVALID_ARG);
    ESP_LOGD(TAG, "Pipeline destroying, %p", pipeline);
    esp_gmf_oal_mutex_lock(pipeline->lock);
    if (pipeline->in) {
        esp_gmf_element_unregister_in_port(pipeline->head_el, NULL);
        esp_gmf_obj_delete((esp_gmf_obj_handle_t)pipeline->in);
    }
    if (pipeline->out) {
        esp_gmf_element_unregister_out_port(pipeline->last_el, NULL);
        esp_gmf_obj_delete((esp_gmf_obj_handle_t)pipeline->out);
    }

    esp_gmf_event_item_t *item = pipeline->evt_conveyor;
    while (item) {
        esp_gmf_event_item_t *tmp = item->next;
        esp_gmf_oal_free(item);
        item = tmp;
    }
    esp_gmf_node_clear((esp_gmf_node_t **)&pipeline->head_el, (void *)esp_gmf_obj_delete);
    esp_gmf_oal_mutex_unlock(pipeline->lock);
    esp_gmf_oal_mutex_destroy(pipeline->lock);
    esp_gmf_oal_free(pipeline);
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_pipeline_set_io(esp_gmf_pipeline_handle_t pipeline, esp_gmf_io_handle_t io, int dir)
{
    ESP_GMF_NULL_CHECK(TAG, pipeline, return ESP_GMF_ERR_INVALID_ARG);
    if (dir == ESP_GMF_IO_DIR_READER) {
        pipeline->in = io;
    } else if (dir == ESP_GMF_IO_DIR_WRITER) {
        pipeline->out = io;
    }
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_pipeline_register_el(esp_gmf_pipeline_handle_t pipeline, esp_gmf_element_handle_t el)
{
    ESP_GMF_NULL_CHECK(TAG, pipeline, return ESP_GMF_ERR_INVALID_ARG);
    esp_gmf_element_set_event_func(el, pipeline_element_events, pipeline);
    if (pipeline->head_el == NULL) {
        pipeline->head_el = el;
        pipeline->last_el = el;
    } else {
        pipeline->last_el = el;
        esp_gmf_element_link_el(pipeline->head_el, el);
    }
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_pipeline_list_el(esp_gmf_pipeline_handle_t pipeline)
{
    ESP_GMF_NULL_CHECK(TAG, pipeline, return ESP_GMF_ERR_INVALID_ARG);
    esp_gmf_node_t *head = (esp_gmf_node_t *)pipeline->head_el;
    while (head) {
        printf("%p-%s, prev:%p-%s, next:%p-%s \r\n", head, OBJ_GET_TAG(head), head->prev, OBJ_GET_TAG(head->prev), head->next, OBJ_GET_TAG(head->next));
        head = head->next;
    }
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_pipeline_set_event(esp_gmf_pipeline_handle_t pipeline, esp_gmf_event_cb cb, void *ctx)
{
    ESP_GMF_NULL_CHECK(TAG, pipeline, return ESP_GMF_ERR_INVALID_ARG);
    pipeline->user_cb = cb;
    pipeline->user_ctx = ctx;
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_pipeline_bind_task(esp_gmf_pipeline_handle_t pipeline, esp_gmf_task_handle_t task)
{
    ESP_GMF_NULL_CHECK(TAG, pipeline, return ESP_GMF_ERR_INVALID_ARG);
    if (task == NULL) {
        // Allow clear bind task, so that can recreate task and bind
        pipeline->thread = NULL;
        return ESP_GMF_ERR_OK;
    }
    ESP_GMF_NULL_CHECK(TAG, task, return ESP_GMF_ERR_INVALID_ARG);
    pipeline->thread = task;
    esp_gmf_task_set_event_func(task, esp_gmf_task_evt, pipeline);
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_pipeline_loading_jobs(esp_gmf_pipeline_handle_t pipeline)
{
    ESP_GMF_NULL_CHECK(TAG, pipeline, return ESP_GMF_ERR_INVALID_ARG);
    if (pipeline->thread == NULL) {
        ESP_LOGE(TAG, "No task for pipeline, %p", pipeline);
        return ESP_GMF_ERR_INVALID_ARG;
    }
    esp_gmf_node_t *node = (esp_gmf_node_t *)pipeline->head_el;
    esp_gmf_element_handle_t el = pipeline->head_el;
    do {
        int ret = register_working_jobs_to_task(pipeline, el);
        if ((ret != ESP_GMF_ERR_OK) && (el == pipeline->head_el)) {
            ESP_LOGW(TAG, "The first element not ready to register job, ret:0x%x", ret);
            break;
        }
        node = (esp_gmf_node_t *)el;
    } while ((el = (esp_gmf_element_handle_t)esp_gmf_node_for_next(node)));
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_pipeline_set_in_uri(esp_gmf_pipeline_handle_t pipeline, const char *uri)
{
    ESP_GMF_NULL_CHECK(TAG, pipeline, return ESP_GMF_ERR_INVALID_ARG);
    ESP_GMF_NULL_CHECK(TAG, uri, return ESP_GMF_ERR_INVALID_ARG);
    int ret = ESP_GMF_ERR_OK;
    if (pipeline->in) {
        ret = esp_gmf_io_set_uri(pipeline->in, uri);
    }
    return ret;
}

esp_gmf_err_t esp_gmf_pipeline_set_out_uri(esp_gmf_pipeline_handle_t pipeline, const char *uri)
{
    ESP_GMF_NULL_CHECK(TAG, pipeline, return ESP_GMF_ERR_INVALID_ARG);
    ESP_GMF_NULL_CHECK(TAG, uri, return ESP_GMF_ERR_INVALID_ARG);
    int ret = ESP_GMF_ERR_OK;
    if (pipeline->out) {
        ret = esp_gmf_io_set_uri(pipeline->out, uri);
    }
    return ret;
}

esp_gmf_err_t esp_gmf_pipeline_reg_event_recipient(esp_gmf_pipeline_handle_t connector, esp_gmf_pipeline_handle_t connectee)
{
    ESP_GMF_NULL_CHECK(TAG, connector, return ESP_GMF_ERR_INVALID_ARG);
    esp_gmf_event_item_t *item = connector->evt_conveyor;
    esp_gmf_event_item_t *new_item = esp_gmf_oal_calloc(1, sizeof(esp_gmf_event_item_t));
    ESP_GMF_MEM_CHECK(TAG, new_item, return ESP_GMF_ERR_MEMORY_LACK);
    if (item == NULL) {
        new_item->cb = connectee->evt_acceptor;
        new_item->ctx = connectee;
        new_item->next = NULL;
        connector->evt_conveyor = new_item;
    } else {
        while (item->next) {
            item = item->next;
        }
        new_item->cb = connectee->evt_acceptor;
        new_item->ctx = connectee;
        new_item->next = NULL;
        item->next = new_item;
    }
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_pipeline_connect_pipe(esp_gmf_pipeline_handle_t connector, const char *connector_name, esp_gmf_port_handle_t connector_port,
                                            esp_gmf_pipeline_handle_t connectee, const char *connectee_name, esp_gmf_port_handle_t connectee_port)
{
    ESP_GMF_NULL_CHECK(TAG, connector, return ESP_GMF_ERR_INVALID_ARG);
    ESP_GMF_NULL_CHECK(TAG, connector_name, return ESP_GMF_ERR_INVALID_ARG);
    ESP_GMF_NULL_CHECK(TAG, connectee, return ESP_GMF_ERR_INVALID_ARG);
    ESP_GMF_NULL_CHECK(TAG, connectee_port, return ESP_GMF_ERR_INVALID_ARG);
    ESP_GMF_NULL_CHECK(TAG, connectee_name, return ESP_GMF_ERR_INVALID_ARG);
    esp_gmf_err_t ret;
    if (connector_port) {
        esp_gmf_element_handle_t connector_el = NULL;
        ret = esp_gmf_pipeline_get_el_by_name(connector, connector_name, &connector_el);
        ESP_GMF_RET_ON_ERROR(TAG, ret, return ret, "The connector[%s] is not found", connector_name);

        ret = esp_gmf_element_register_out_port(connector_el, connector_port);
        ESP_GMF_RET_ON_ERROR(TAG, ret, return ret, "Register connector out port failed, [%s]", connectee_name);
    }
    esp_gmf_element_handle_t connectee_el = NULL;
    ret = esp_gmf_pipeline_get_el_by_name(connectee, connectee_name, &connectee_el);
    ESP_GMF_RET_ON_ERROR(TAG, ret, return ret, "The connectee[%s] is not found", connectee_name);

    ret = esp_gmf_element_register_in_port(connectee_el, connectee_port);
    ESP_GMF_RET_ON_ERROR(TAG, ret, return ret, "Register connectee in port failed, [%s]", connectee_name);

    return esp_gmf_pipeline_reg_event_recipient(connector, connectee);
}

esp_gmf_err_t esp_gmf_pipeline_set_prev_run_cb(esp_gmf_pipeline_handle_t pipeline, esp_gmf_pipeline_prev_act prev_run, void *ctx)
{
    ESP_GMF_NULL_CHECK(TAG, pipeline, return ESP_GMF_ERR_INVALID_ARG);
    ESP_LOGD(TAG, "Set prev run:%p pipeline:%p", prev_run, pipeline);
    pipeline->prev_run = prev_run;
    pipeline->prev_run_ctx = ctx;
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_pipeline_set_prev_stop_cb(esp_gmf_pipeline_handle_t pipeline, esp_gmf_pipeline_prev_act prev_stop, void *ctx)
{
    ESP_GMF_NULL_CHECK(TAG, pipeline, return ESP_GMF_ERR_INVALID_ARG);
    ESP_LOGD(TAG, "Set prev stop:%p pipeline:%p", prev_stop, pipeline);
    pipeline->prev_stop = prev_stop;
    pipeline->prev_stop_ctx = ctx;
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_pipeline_prev_run(esp_gmf_pipeline_handle_t pipeline)
{
    ESP_GMF_NULL_CHECK(TAG, pipeline, return ESP_GMF_ERR_INVALID_ARG);
    if (pipeline->prev_run == NULL) {
        return ESP_GMF_ERR_OK;
    }
    if (pipeline->prev_state & PIPELINE_PRE_RUN_STATE) {
        return ESP_GMF_ERR_OK;
    }
    pipeline->prev_run(pipeline->prev_run_ctx);
    pipeline->prev_state |= PIPELINE_PRE_RUN_STATE;
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_pipeline_prev_stop(esp_gmf_pipeline_handle_t pipeline)
{
    ESP_GMF_NULL_CHECK(TAG, pipeline, return ESP_GMF_ERR_INVALID_ARG);
    if (pipeline->prev_stop == NULL) {
        return ESP_GMF_ERR_OK;
    }
    if (pipeline->prev_state & PIPELINE_PRE_STOP_STATE) {
        return ESP_GMF_ERR_OK;
    }
    pipeline->prev_stop(pipeline->prev_stop_ctx);
    pipeline->prev_state |= PIPELINE_PRE_STOP_STATE;
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_pipeline_run(esp_gmf_pipeline_handle_t pipeline)
{
    ESP_GMF_NULL_CHECK(TAG, pipeline, return ESP_GMF_ERR_INVALID_ARG);
    esp_gmf_err_t ret = esp_gmf_pipeline_prev_run(pipeline);
    ESP_GMF_RET_ON_ERROR(TAG, ret, return ret, "Fail to prev run for %p", pipeline);
    return esp_gmf_task_run(pipeline->thread);
}

esp_gmf_err_t esp_gmf_pipeline_stop(esp_gmf_pipeline_handle_t pipeline)
{
    ESP_GMF_NULL_CHECK(TAG, pipeline, return ESP_GMF_ERR_INVALID_ARG);
    esp_gmf_err_t ret = ESP_GMF_ERR_OK;
    ESP_LOGD(TAG, "Pipeline going to stop, %p", pipeline);
    ret = esp_gmf_pipeline_prev_stop(pipeline);
    ESP_GMF_RET_ON_ERROR(TAG, ret, return ret, "Fail to prev stop for %p", pipeline);
    ret = esp_gmf_task_stop(pipeline->thread);
    return ret;
}

esp_gmf_err_t esp_gmf_pipeline_pause(esp_gmf_pipeline_handle_t pipeline)
{
    ESP_GMF_NULL_CHECK(TAG, pipeline, return ESP_GMF_ERR_INVALID_ARG);
    int ret = ESP_GMF_ERR_OK;
    ret = esp_gmf_task_pause(pipeline->thread);
    return ret;
}

esp_gmf_err_t esp_gmf_pipeline_resume(esp_gmf_pipeline_handle_t pipeline)
{
    ESP_GMF_NULL_CHECK(TAG, pipeline, return ESP_GMF_ERR_INVALID_ARG);
    int ret = ESP_GMF_ERR_OK;
    ret = esp_gmf_task_resume(pipeline->thread);
    return ret;
}

esp_gmf_err_t esp_gmf_pipeline_reset(esp_gmf_pipeline_handle_t pipeline)
{
    ESP_GMF_NULL_CHECK(TAG, pipeline, return ESP_GMF_ERR_INVALID_ARG);
    int ret = ESP_GMF_ERR_OK;
    pipeline->state = ESP_GMF_EVENT_STATE_NONE;
    ret = esp_gmf_task_reset(pipeline->thread);
    esp_gmf_element_handle_t next_el = pipeline->head_el;
    do {
        esp_gmf_element_reset_state(next_el);
        esp_gmf_element_reset_port(next_el);
        esp_gmf_element_set_job_mask(next_el, 0);
        ESP_LOGD(TAG, "Pipeline reset, %p, %p-%s", pipeline, next_el, OBJ_GET_TAG(next_el));
    } while ((next_el = (esp_gmf_element_handle_t)esp_gmf_node_for_next(next_el)));
    return ret;
}

esp_gmf_err_t esp_gmf_pipeline_seek(esp_gmf_pipeline_handle_t pipeline, uint64_t pos)
{
    ESP_GMF_NULL_CHECK(TAG, pipeline, return ESP_GMF_ERR_INVALID_ARG);
    if ((pipeline->state != ESP_GMF_EVENT_STATE_PAUSED)
        && (pipeline->state != ESP_GMF_EVENT_STATE_STOPPED)
        && (pipeline->state != ESP_GMF_EVENT_STATE_FINISHED)) {
        ESP_LOGE(TAG, "The pipeline status is %s, can't be seek.", esp_gmf_event_get_state_str(pipeline->state));
        return ESP_GMF_ERR_INVALID_STATE;
    }
    if (pipeline->in == NULL) {
        ESP_LOGE(TAG, "This pipeline no in port, can't be seek");
        return ESP_GMF_ERR_NOT_SUPPORT;
    }
    int ret = ESP_GMF_ERR_OK;
    ret = esp_gmf_io_seek(pipeline->in, pos);
    ESP_LOGD(TAG, "Seek to %lld, ret:%d", pos, ret);
    return ret;
}

esp_gmf_err_t esp_gmf_pipeline_get_linked_pipeline(esp_gmf_pipeline_handle_t connector, const void **link, esp_gmf_pipeline_handle_t *connectee)
{
    ESP_GMF_NULL_CHECK(TAG, connector, return ESP_GMF_ERR_INVALID_ARG);
    ESP_GMF_NULL_CHECK(TAG, link, return ESP_GMF_ERR_INVALID_ARG);
    ESP_GMF_NULL_CHECK(TAG, connectee, return ESP_GMF_ERR_INVALID_ARG);

    esp_gmf_event_item_t *item;
    if (*link == NULL) {
        item = connector->evt_conveyor;
    } else {
        item = (esp_gmf_event_item_t *)*link;
        item = item->next;
    }
    if (item) {
        *connectee = (esp_gmf_pipeline_handle_t)item->ctx;
        *link = item;
        return ESP_GMF_ERR_OK;
    }
    *connectee = NULL;
    return ESP_GMF_ERR_NOT_FOUND;
}

esp_gmf_err_t esp_gmf_pipeline_get_in(esp_gmf_pipeline_handle_t pipeline, esp_gmf_io_handle_t *io_handle)
{
    ESP_GMF_NULL_CHECK(TAG, pipeline, return ESP_GMF_ERR_INVALID_ARG);
    ESP_GMF_NULL_CHECK(TAG, io_handle, return ESP_GMF_ERR_INVALID_ARG);
    *io_handle = pipeline->in;
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_pipeline_get_out(esp_gmf_pipeline_handle_t pipeline, esp_gmf_io_handle_t *io_handle)
{
    ESP_GMF_NULL_CHECK(TAG, pipeline, return ESP_GMF_ERR_INVALID_ARG);
    ESP_GMF_NULL_CHECK(TAG, io_handle, return ESP_GMF_ERR_INVALID_ARG);
    *io_handle = pipeline->out;
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_pipeline_replace_in(esp_gmf_pipeline_handle_t pipeline, esp_gmf_io_handle_t new)
{
    ESP_GMF_NULL_CHECK(TAG, pipeline, return ESP_GMF_ERR_INVALID_ARG);
    ESP_GMF_NULL_CHECK(TAG, new, return ESP_GMF_ERR_INVALID_ARG);
    if ((pipeline->state == ESP_GMF_EVENT_STATE_RUNNING)
        || (pipeline->state == ESP_GMF_EVENT_STATE_INITIALIZED)) {
        ESP_LOGE(TAG, "Can't replace in port, st:%s, new:%p",
                 esp_gmf_event_get_state_str(pipeline->state), new);
        return ESP_GMF_ERR_INVALID_STATE;
    }
    pipeline->in = new;
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_pipeline_replace_out(esp_gmf_pipeline_handle_t pipeline, esp_gmf_io_handle_t new)
{
    ESP_GMF_NULL_CHECK(TAG, pipeline, return ESP_GMF_ERR_INVALID_ARG);
    ESP_GMF_NULL_CHECK(TAG, new, return ESP_GMF_ERR_INVALID_ARG);
    if ((pipeline->state != ESP_GMF_EVENT_STATE_RUNNING)
        || (pipeline->state != ESP_GMF_EVENT_STATE_INITIALIZED)) {
        ESP_LOGE(TAG, "Can't replace out port, st:%s, new:%p",
                 esp_gmf_event_get_state_str(pipeline->state), new);
        return ESP_GMF_ERR_INVALID_STATE;
    }
    pipeline->out = new;
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_pipeline_get_head_el(esp_gmf_pipeline_handle_t pipeline, esp_gmf_element_handle_t *head)
{
    ESP_GMF_NULL_CHECK(TAG, pipeline, return ESP_GMF_ERR_INVALID_ARG);
    ESP_GMF_NULL_CHECK(TAG, head, return ESP_GMF_ERR_INVALID_ARG);
    *head = pipeline->head_el;
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_pipeline_get_next_el(esp_gmf_pipeline_handle_t pipeline, esp_gmf_element_handle_t head, esp_gmf_element_handle_t *next)
{
    ESP_GMF_NULL_CHECK(TAG, pipeline, return ESP_GMF_ERR_INVALID_ARG);
    ESP_GMF_NULL_CHECK(TAG, head, return ESP_GMF_ERR_INVALID_ARG);
    ESP_GMF_NULL_CHECK(TAG, next, return ESP_GMF_ERR_INVALID_ARG);
    *next = (esp_gmf_element_handle_t)esp_gmf_node_for_next((esp_gmf_node_t *)head);
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_pipeline_get_el_by_name(esp_gmf_pipeline_handle_t pipeline, const char *tag, esp_gmf_element_handle_t *out_handle)
{
    ESP_GMF_NULL_CHECK(TAG, pipeline, return ESP_GMF_ERR_INVALID_ARG);
    ESP_GMF_NULL_CHECK(TAG, tag, return ESP_GMF_ERR_INVALID_ARG);
    ESP_GMF_NULL_CHECK(TAG, out_handle, return ESP_GMF_ERR_INVALID_ARG);

    esp_gmf_node_t *node = (esp_gmf_node_t *)pipeline->head_el;
    esp_gmf_element_handle_t next_el = node;
    do {
        if (strncasecmp(tag, OBJ_GET_TAG(next_el), ESP_GMF_TAG_MAX_LEN) == 0) {
            ESP_LOGD(TAG, "Find EL %s-%p", OBJ_GET_TAG(next_el), next_el);
            *out_handle = next_el;
            return ESP_GMF_ERR_OK;
        }
        node = (esp_gmf_node_t *)next_el;
    } while ((next_el = (esp_gmf_element_handle_t)esp_gmf_node_for_next(node)));
    return ESP_GMF_ERR_NOT_FOUND;
}

esp_gmf_err_t esp_gmf_pipeline_reg_el_port(esp_gmf_pipeline_handle_t pipeline,
                                           const char *tag, esp_gmf_io_dir_t io_dir, esp_gmf_io_handle_t port)
{
    ESP_GMF_NULL_CHECK(TAG, pipeline, return ESP_GMF_ERR_INVALID_ARG);
    ESP_GMF_NULL_CHECK(TAG, tag, return ESP_GMF_ERR_INVALID_ARG);
    ESP_GMF_NULL_CHECK(TAG, port, return ESP_GMF_ERR_INVALID_ARG);
    esp_gmf_element_handle_t target_el = NULL;
    int ret = esp_gmf_pipeline_get_el_by_name(pipeline, tag, &target_el);
    ESP_GMF_RET_ON_ERROR(TAG, ret, return ret, "Find the element error, p:%p, tag:%s, ret:%d", pipeline, tag, ret);
    if (io_dir == ESP_GMF_IO_DIR_READER) {
        ret = esp_gmf_element_register_in_port((esp_gmf_element_handle_t)target_el, port);
    } else if (io_dir == ESP_GMF_IO_DIR_WRITER) {
        ret = esp_gmf_element_register_out_port((esp_gmf_element_handle_t)target_el, port);
    } else {
        ESP_LOGE(TAG, "Unsupported IO type,%d, [%p]", io_dir, pipeline);
        return ESP_GMF_ERR_NOT_SUPPORT;
    }
    return ret;
}

esp_gmf_err_t esp_gmf_pipeline_report_info(esp_gmf_pipeline_handle_t pipeline, esp_gmf_info_type_t info_type, void *value, int len)
{
    ESP_GMF_NULL_CHECK(TAG, pipeline, return ESP_GMF_ERR_INVALID_ARG);
    esp_gmf_element_t *el = (esp_gmf_element_t *)pipeline->head_el;
    esp_gmf_event_pkt_t evt = {
        .from = NULL,
        .type = ESP_GMF_EVT_TYPE_REPORT_INFO,
        .sub = info_type,
        .payload = value,
        .payload_size = len,
    };
    if (el->event_func == NULL) {
        ESP_LOGW(TAG, "Report info failed[pipe:%p], due to [el:%p-%s] no registered callback", pipeline, el, OBJ_GET_TAG(el));
        return ESP_GMF_ERR_FAIL;
    }
    return el->event_func(&evt, el->ctx);
}

esp_gmf_err_t esp_gmf_pipeline_show(esp_gmf_pipeline_handle_t pipeline)
{
    ESP_GMF_NULL_CHECK(TAG, pipeline, return ESP_GMF_ERR_INVALID_ARG);
    esp_gmf_node_t *node = (esp_gmf_node_t *)pipeline->head_el;
    esp_gmf_element_handle_t next_el = node;
    ESP_LOGI(TAG, "SHOW PIPELINE MEMBERS:");
    ESP_LOGI(TAG, "The IN port, [%p-%s]", pipeline->in, OBJ_GET_TAG(pipeline->in));
    do {
        ESP_LOGI(TAG, "The EL, [%p-%s]", next_el, OBJ_GET_TAG(next_el));
        node = (esp_gmf_node_t *)next_el;
    } while ((next_el = (esp_gmf_element_handle_t)esp_gmf_node_for_next(node)));
    ESP_LOGI(TAG, "The OUT port, [%p-%s]", pipeline->out, OBJ_GET_TAG(pipeline->out));
    return ESP_GMF_ERR_OK;
}
