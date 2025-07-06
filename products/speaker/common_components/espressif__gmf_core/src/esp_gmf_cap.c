/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <string.h>
#include "esp_log.h"
#include "esp_gmf_cap.h"
#include "esp_gmf_single_node.h"
#include "esp_gmf_oal_mem.h"
#include "esp_fourcc.h"
#include "esp_gmf_caps_def.h"

static const char *TAG = "ESP_GMF_CAPS";

static void esp_gmf_cap_free(void *handle)
{
    esp_gmf_oal_free(handle);
}

esp_gmf_err_t esp_gmf_cap_append(esp_gmf_cap_t **caps, esp_gmf_cap_t *cap_value)
{
    ESP_GMF_NULL_CHECK(TAG, caps, return ESP_GMF_ERR_INVALID_ARG);
    ESP_GMF_NULL_CHECK(TAG, cap_value, return ESP_GMF_ERR_INVALID_ARG);
    esp_gmf_cap_t *node = (esp_gmf_cap_t *)esp_gmf_oal_calloc(1, sizeof(esp_gmf_cap_t));
    ESP_GMF_MEM_CHECK(TAG, node, return ESP_GMF_ERR_MEMORY_LACK);
    node->cap_eightcc = cap_value->cap_eightcc;
    node->attr_fun = cap_value->attr_fun;
    esp_gmf_single_node_append((esp_gmf_single_node_t **)caps, (esp_gmf_single_node_t *)node);
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_cap_destroy(esp_gmf_cap_t *caps)
{
    ESP_GMF_NULL_CHECK(TAG, caps, return ESP_GMF_ERR_INVALID_ARG);
    esp_gmf_single_node_destroy((esp_gmf_single_node_t *)caps, esp_gmf_cap_free);
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_cap_iterate_attr(esp_gmf_cap_t *caps, uint32_t attr_index, esp_gmf_cap_attr_t *out_attr)
{
    ESP_GMF_NULL_CHECK(TAG, caps, return ESP_GMF_ERR_INVALID_ARG);
    ESP_GMF_NULL_CHECK(TAG, out_attr, return ESP_GMF_ERR_INVALID_ARG);
    if (caps->attr_fun) {
        return caps->attr_fun(attr_index, out_attr);
    }
    return ESP_GMF_ERR_NOT_SUPPORT;
}

esp_gmf_err_t esp_gmf_cap_fetch_node(esp_gmf_cap_t *caps, uint64_t eight_cc, esp_gmf_cap_t **out_caps)
{
    ESP_GMF_NULL_CHECK(TAG, caps, return ESP_GMF_ERR_INVALID_ARG);
    ESP_GMF_NULL_CHECK(TAG, out_caps, return ESP_GMF_ERR_INVALID_ARG);
    esp_gmf_cap_t *current = caps;
    *out_caps = NULL;
    while (current != NULL) {
        if (current->cap_eightcc == eight_cc) {
            *out_caps = current;
            return ESP_GMF_ERR_OK;
        }
        current = current->next;
    }
    return ESP_GMF_ERR_NOT_FOUND;
}

esp_gmf_err_t esp_gmf_cap_find_attr(esp_gmf_cap_t *caps, uint32_t cc, esp_gmf_cap_attr_t *out_attr)
{
    ESP_GMF_NULL_CHECK(TAG, caps, return ESP_GMF_ERR_INVALID_ARG);
    ESP_GMF_NULL_CHECK(TAG, out_attr, return ESP_GMF_ERR_INVALID_ARG);
    if (caps->attr_fun == NULL) {
        ESP_LOGW(TAG, "There is no attribute iteration function, %p", caps);
        return ESP_GMF_ERR_NOT_SUPPORT;
    }
    esp_gmf_cap_attr_t tmp = {0};
    uint32_t idx = 0;
    int ret = ESP_GMF_ERR_FAIL;
    do {
        ret = esp_gmf_cap_iterate_attr(caps, idx++, &tmp);
        if (ret == ESP_GMF_ERR_OK) {
            ESP_LOGD(TAG, "Find dest:%s, src:%s", ESP_FOURCC_TO_STR(cc), ESP_FOURCC_TO_STR(tmp.fourcc));
            if (tmp.fourcc == cc) {
                memcpy(out_attr, &tmp, sizeof(*out_attr));
                return ESP_GMF_ERR_OK;
            }
        }
    } while (ret == ESP_GMF_ERR_OK);
    return ESP_GMF_ERR_NOT_FOUND;
}

esp_gmf_err_t esp_gmf_cap_attr_check_value(esp_gmf_cap_attr_t *attr, uint32_t val, bool *is_support)
{
    ESP_GMF_NULL_CHECK(TAG, attr, return ESP_GMF_ERR_INVALID_ARG);
    ESP_GMF_NULL_CHECK(TAG, is_support, return ESP_GMF_ERR_INVALID_ARG);
    switch (attr->prop_type) {
        case ESP_GMF_PROP_TYPE_DISCRETE:
            *is_support = false;
            for (int i = 0; i < attr->value.d.item_num; ++i) {
                void *p = attr->value.d.collection;
                if (attr->value.d.item_size == 1) {
                    if (val == ((uint8_t *)p)[i]) {
                        *is_support = true;
                        return ESP_GMF_ERR_OK;
                    }
                } else if (attr->value.d.item_size == 2) {
                    if (val == ((uint16_t *)p)[i]) {
                        *is_support = true;
                        return ESP_GMF_ERR_OK;
                    }
                } else if (attr->value.d.item_size == 4) {
                    if (val == ((uint32_t *)p)[i]) {
                        *is_support = true;
                        return ESP_GMF_ERR_OK;
                    }
                } else {
                    ESP_LOGE(TAG, "Value check failed: invalid item size, %d", attr->value.d.item_size);
                    return ESP_GMF_ERR_NOT_SUPPORT;
                }
            }
            return ESP_GMF_ERR_NOT_SUPPORT;
        case ESP_GMF_PROP_TYPE_STEPWISE:
            ESP_LOGD(TAG, "Check value:%s, type:%d", ESP_FOURCC_TO_STR(attr->fourcc), attr->prop_type);
            if (((attr->value.s.min > val) || (attr->value.s.max < val)) || ((val - attr->value.s.min) % attr->value.s.step)) {
                *is_support = false;
                return ESP_GMF_ERR_NOT_SUPPORT;
            }
            *is_support = true;
            return ESP_GMF_ERR_OK;
        case ESP_GMF_PROP_TYPE_MULTIPLE:
            ESP_LOGD(TAG, "Check MUL:%s, min:%ld, fac:%d, max:%ld", ESP_FOURCC_TO_STR(attr->fourcc), attr->value.m.min, attr->value.m.factor, attr->value.m.max);
            if (((attr->value.m.min > val) || (attr->value.m.max < val)) || (val % attr->value.m.factor)) {
                *is_support = false;
                return ESP_GMF_ERR_NOT_SUPPORT;
            }
            *is_support = true;
            return ESP_GMF_ERR_OK;
        case ESP_GMF_PROP_TYPE_CONSTANT:
            ESP_LOGD(TAG, "Check MUL:%s, min:%ld, fac:%d, max:%ld", ESP_FOURCC_TO_STR(attr->fourcc), attr->value.m.min, attr->value.m.factor, attr->value.m.max);
            if (val != attr->value.c.data) {
                *is_support = false;
                return ESP_GMF_ERR_NOT_SUPPORT;
            }
            *is_support = true;
            return ESP_GMF_ERR_OK;
    }
    return ESP_GMF_ERR_NOT_SUPPORT;
}

esp_gmf_err_t esp_gmf_cap_attr_iterator_value(esp_gmf_cap_attr_t *attr, uint32_t *val, bool *is_last)
{
    ESP_GMF_NULL_CHECK(TAG, attr, return ESP_GMF_ERR_INVALID_ARG);
    ESP_GMF_NULL_CHECK(TAG, val, return ESP_GMF_ERR_INVALID_ARG);
    ESP_GMF_NULL_CHECK(TAG, is_last, return ESP_GMF_ERR_INVALID_ARG);
    *is_last = false;
    *val = 0;
    switch (attr->prop_type) {
        case ESP_GMF_PROP_TYPE_DISCRETE:
            ESP_LOGD(TAG, "Iterate value DIS, sz:%d, num:%d", attr->value.d.item_size, attr->value.d.item_num);
            if (attr->value.d.item_size == 1) {
                *val = ((uint8_t *)attr->value.d.collection)[attr->index++];
            } else if (attr->value.d.item_size == 2) {
                *val = ((uint16_t *)attr->value.d.collection)[attr->index++];
            } else if (attr->value.d.item_size == 4) {
                *val = ((uint32_t *)attr->value.d.collection)[attr->index++];
            } else {
                ESP_LOGE(TAG, "Iterate value failed: invalid item size, %d", attr->value.d.item_size);
                return ESP_GMF_ERR_NOT_SUPPORT;
            }
            if (attr->index == attr->value.d.item_num) {
                *is_last = true;
                attr->index = 0;
            }
            return ESP_GMF_ERR_OK;
        case ESP_GMF_PROP_TYPE_STEPWISE:
            *val = attr->value.s.min + attr->value.s.step * attr->index++;
            if (*val >= attr->value.s.max) {
                *is_last = true;
                attr->index = 0;
            }
            return ESP_GMF_ERR_OK;
        case ESP_GMF_PROP_TYPE_MULTIPLE:
            attr->index++;
            *val = attr->value.m.factor * attr->index;
            if (*val >= attr->value.m.max) {
                *is_last = true;
                attr->index = 0;
            }
            return ESP_GMF_ERR_OK;
        case ESP_GMF_PROP_TYPE_CONSTANT:
            attr->index++;
            *val = attr->value.c.data;
            *is_last = true;
            return ESP_GMF_ERR_OK;
    }
    return ESP_GMF_ERR_NOT_SUPPORT;
}

esp_gmf_err_t esp_gmf_cap_attr_get_first_value(esp_gmf_cap_attr_t *attr, uint32_t *val)
{
    ESP_GMF_NULL_CHECK(TAG, attr, return ESP_GMF_ERR_INVALID_ARG);
    ESP_GMF_NULL_CHECK(TAG, val, return ESP_GMF_ERR_INVALID_ARG);
    *val = 0;
    switch (attr->prop_type) {
        case ESP_GMF_PROP_TYPE_DISCRETE:
            *val = ((uint32_t *)attr->value.d.collection)[0];
            return ESP_GMF_ERR_OK;
        case ESP_GMF_PROP_TYPE_STEPWISE:
            *val = attr->value.s.min;
            return ESP_GMF_ERR_OK;
        case ESP_GMF_PROP_TYPE_MULTIPLE:
            *val = attr->value.m.min;
            return ESP_GMF_ERR_OK;
        case ESP_GMF_PROP_TYPE_CONSTANT:
            *val = attr->value.c.data;
            return ESP_GMF_ERR_OK;
    }
    return ESP_GMF_ERR_NOT_SUPPORT;
}
