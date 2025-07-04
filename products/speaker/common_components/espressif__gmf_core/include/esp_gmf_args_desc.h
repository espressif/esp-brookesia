/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <string.h>
#include "esp_gmf_err.h"
#include "esp_log.h"
#include "esp_gmf_oal_mem.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  GMF argument types enumeration
 */
typedef enum {
    ESP_GMF_ARGS_TYPE_NIL    = 0x00,  /*!< No type or null value */
    ESP_GMF_ARGS_TYPE_UINT8  = 0x01,  /*!< Unsigned 8-bit integer */
    ESP_GMF_ARGS_TYPE_INT8   = 0x02,  /*!< Signed 8-bit integer */
    ESP_GMF_ARGS_TYPE_UINT16 = 0x03,  /*!< Unsigned 16-bit integer */
    ESP_GMF_ARGS_TYPE_INT16  = 0x04,  /*!< Signed 16-bit integer */
    ESP_GMF_ARGS_TYPE_UINT32 = 0x05,  /*!< Unsigned 32-bit integer */
    ESP_GMF_ARGS_TYPE_INT32  = 0x06,  /*!< Signed 32-bit integer */
    ESP_GMF_ARGS_TYPE_UINT64 = 0x07,  /*!< Unsigned 64-bit integer */
    ESP_GMF_ARGS_TYPE_INT64  = 0x08,  /*!< Signed 64-bit integer */
    ESP_GMF_ARGS_TYPE_FLOAT  = 0x09,  /*!< Single-precision floating point */
    ESP_GMF_ARGS_TYPE_DOUBLE = 0x0a,  /*!< Double-precision floating point */
    ESP_GMF_ARGS_TYPE_ARRAY  = 0x0b,  /*!< Array data type */
} esp_gmf_args_type_t;

/**
 * @brief  GMF argument description structure
 */
typedef struct esp_gmf_args_desc {
    struct esp_gmf_args_desc *next;    /*!< Pointer to the next argument in the list */
    esp_gmf_args_type_t       type;    /*!< Data type of the argument */
    uint16_t                  offset;  /*!< Byte offset from the previous argument */
    const char               *name;    /*!< Name of the argument */
    struct esp_gmf_args_desc *val;     /*!< Pointer to nested value (used for arrays) */
    uint32_t                  size;    /*!< Size of the argument in bytes */
} esp_gmf_args_desc_t;

/**
 * @brief  Create a new argument description node
 *
 * @param[in]   name    Name of the argument
 * @param[in]   type    Type of the argument
 * @param[in]   val     Nested value for arrays or sublists
 * @param[in]   size    Size of the argument
 * @param[out]  handle  Pointer to the created node
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success
 *       - ESP_GMF_ERR_MEMORY_LACK  Memory allocation failure
 */
static inline esp_gmf_err_t esp_gmf_args_desc_create(const char *name, esp_gmf_args_type_t type, esp_gmf_args_desc_t *val,
                                                     uint32_t size, esp_gmf_args_desc_t **handle)
{
    esp_gmf_args_desc_t *node = (esp_gmf_args_desc_t *)esp_gmf_oal_calloc(1, sizeof(esp_gmf_args_desc_t));
    if (node) {
        if (name) {
            node->name = esp_gmf_oal_strdup(name);
            if (node->name == NULL) {
                esp_gmf_oal_free(node);
                return ESP_GMF_ERR_MEMORY_LACK;
            }
        }
        node->type = type;
        node->val = val;
        node->size = size;
        node->next = NULL;
        *handle = node;
        return ESP_GMF_ERR_OK;
    }
    return ESP_GMF_ERR_MEMORY_LACK;
}

/**
 * @brief  Destroy a esp_gmf_args_desc node and all its successors
 *
 * @param[in]  head  Pointer to the head of the list to be destroyed
 */
static inline void esp_gmf_args_desc_destroy(esp_gmf_args_desc_t *head)
{
    esp_gmf_args_desc_t *current = head;
    esp_gmf_args_desc_t *next;

    while (current != NULL) {
        next = current->next;
        current->next = NULL;
        if (current->val != NULL) {
            esp_gmf_args_desc_destroy(current->val);
            current->val = NULL;
        }
        esp_gmf_oal_free((char *)current->name);
        esp_gmf_oal_free(current);
        current = next;
    }
}

/**
 * @brief  Calculate the total size of all argument values in a linked list of GMF argument descriptions
 *
 * @param[in]   head              Pointer to the head of the linked list of argument descriptions
 * @param[out]  total_value_size  Pointer to the variable where the total size will be stored
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success, the total size is stored in `total_value_size`
 *       - ESP_GMF_ERR_INVALID_ARG  If either the `head` or `total_value_size` pointer is `NULL`
 */
static inline esp_gmf_err_t esp_gmf_args_desc_get_total_size(esp_gmf_args_desc_t *head, size_t *total_value_size)
{
    if (head == NULL || total_value_size == NULL) {
        return ESP_GMF_ERR_INVALID_ARG;
    }
    esp_gmf_args_desc_t *current = head;
    while (current != NULL) {
        *total_value_size += current->size;
        ESP_LOGD("GMF_ARG", "Get total size %d, name:%s", *total_value_size, current->name);
        current = current->next;
    }
    return ESP_GMF_ERR_OK;
}

/**
 * @brief  Append a new argument description to the linked list of argument descriptions.
 *         This function creates a new argument description node and appends it to the end of the linked list.
 *         The new node is initialized with the specified `name`, `type`, `size`, `offset`, and a pointer to
 *         a `val` structure that contains additional argument-specific data.
 *
 * @param[in,out]  head    Pointer to the head of the linked list. This is a pointer to the first node
 *                         in the list, and it will be updated if the new node is added at the head
 * @param[in]      name    The name of the argument, which will be stored in the new node
 * @param[in]      type    The type of the argument (e.g., integer, array, etc.)
 * @param[in]      size    The size of the argument value
 * @param[in]      offset  The offset of the argument in the structure or buffer
 * @param[in]      val     Pointer to a structure containing additional data related to the argument
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success, the new argument description is appended to the list
 *       - ESP_GMF_ERR_INVALID_ARG  If `head` or `name` is `NULL`
 */
static inline esp_gmf_err_t esp_gmf_args_desc_append_base(esp_gmf_args_desc_t **head, const char *name, esp_gmf_args_type_t type,
                                                          uint32_t size, uint32_t offset, esp_gmf_args_desc_t *val)
{
    esp_gmf_args_desc_t *new_args = NULL;
    esp_gmf_args_desc_create(name, type, val, size, &new_args);
    if (new_args == NULL) {
        return ESP_GMF_ERR_MEMORY_LACK;
    }
    if (*head == NULL) {
        *head = new_args;
        return ESP_GMF_ERR_OK;
    }
    esp_gmf_args_desc_t *current = *head;
    while (current) {
        if (current->next == NULL) {
            break;
        }
        current = current->next;
    }
    if (type == ESP_GMF_ARGS_TYPE_ARRAY) {
        size_t sz = 0;
        esp_gmf_args_desc_get_total_size(*head, &sz);
        offset = sz;
        esp_gmf_args_desc_t *tmp = val;
        while (tmp) {
            tmp->offset += offset;
            if (tmp->next == NULL) {
                break;
            }
            tmp = tmp->next;
        }
    }
    new_args->offset = offset;
    current->next = new_args;
    return ESP_GMF_ERR_OK;
}

/**
 * @brief  Append a new argument description for a non-array argument to the linked list
 *
 * @param[in,out]  head    Pointer to the head of the linked list. This is a pointer to the first node
 *                         in the list, and it will be updated if the new node is added at the head
 * @param[in]      name    The name of the argument, which will be stored in the new node
 * @param[in]      type    The type of the argument (e.g., integer, array, etc.)
 * @param[in]      size    The size of the argument value
 * @param[in]      offset  The offset of the argument in the structure or buffer
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success, the new argument description is appended to the list
 *       - ESP_GMF_ERR_INVALID_ARG  If `head` or `name` is `NULL`
 */
static inline esp_gmf_err_t esp_gmf_args_desc_append(esp_gmf_args_desc_t **head, const char *name, esp_gmf_args_type_t type,
                                                     uint32_t size, uint32_t offset)
{
    return esp_gmf_args_desc_append_base(head, name, type, size, offset, NULL);
}

/**
 * @brief  Append a new argument description for an array argument to the linked list
 *
 *         This function wraps the `esp_gmf_args_desc_append_base` function to append a new argument description
 *         for an array argument (where `val` is a pointer to the array's value structure) to the linked list
 *         It sets the argument type to `ESP_GMF_ARGS_TYPE_ARRAY` and uses the provided `name`, `size`, and `offset`
 *         to create the new argument description
 *
 * @param[in,out]  head    Pointer to the head of the linked list. This is a pointer to the first node
 *                         in the list, and it will be updated if the new node is added at the head
 * @param[in]      name    The name of the argument, which will be stored in the new node
 * @param[in]      val     Pointer to a structure containing the array data associated with the argument
 * @param[in]      size    The size of the array (e.g., the total number of elements)
 * @param[in]      offset  The offset of the argument in the structure or buffer
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success, the new argument description is appended to the list
 *       - ESP_GMF_ERR_INVALID_ARG  If `head` or `name` is `NULL`
 */
static inline esp_gmf_err_t esp_gmf_args_desc_append_array(esp_gmf_args_desc_t **head, const char *name, esp_gmf_args_desc_t *val,
                                                           uint32_t size, uint32_t offset)
{
    return esp_gmf_args_desc_append_base(head, name, ESP_GMF_ARGS_TYPE_ARRAY, size, offset, val);
}

/**
 * @brief  Copy an entire GMF argument description list
 *
 * @param[in]   head      Pointer to the head of the list to be copied
 * @param[out]  new_args  Pointer to the head of the newly copied list
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success
 *       - ESP_GMF_ERR_MEMORY_LACK  Memory allocation failure
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid input arguments
 */
static inline esp_gmf_err_t esp_gmf_args_desc_copy(const esp_gmf_args_desc_t *head, esp_gmf_args_desc_t **new_args)
{
    if (head == NULL) {
        return ESP_GMF_ERR_INVALID_ARG;
    }
    esp_gmf_args_desc_t *new_head = NULL;
    esp_gmf_args_desc_create(head->name, head->type, head->val, head->size, &new_head);
    if (new_head == NULL) {
        return ESP_GMF_ERR_MEMORY_LACK;
    }
    new_head->offset = head->offset;
    // Recursively copy the value (for array or nested structures)
    if (head->val) {
        esp_gmf_args_desc_copy(head->val, &new_head->val);
        if (!new_head->val) {
            esp_gmf_oal_free((void *)new_head->name);
            esp_gmf_oal_free(new_head);
            return ESP_GMF_ERR_MEMORY_LACK;
        }
    } else {
        new_head->val = NULL;
    }
    // Recursively copy the next node
    if (head->next) {
        esp_gmf_args_desc_copy(head->next, &new_head->next);
        if (!new_head->next) {
            if (new_head->val) {
                esp_gmf_oal_free(new_head->val);
            }
            esp_gmf_oal_free((void *)new_head->name);
            esp_gmf_oal_free(new_head);
            return ESP_GMF_ERR_MEMORY_LACK;
        }
    } else {
        new_head->next = NULL;
    }
    *new_args = new_head;
    return ESP_GMF_ERR_OK;
}

/**
 * @brief  Count the number of nodes in a GMF argument description list, including nested lists
 *
 * @param[in]  head  Pointer to the head of the list
 *
 * @return
 *       - The  total number of nodes in the list, including nested lists
 */
static inline size_t esp_gmf_args_desc_count(esp_gmf_args_desc_t *head)
{
    size_t count = 0;
    const esp_gmf_args_desc_t *current = head;
    while (current != NULL) {
        count++;
        current = current->next;
    }
    return count;
}

/**
 * @brief  Find an argument description by its name in the linked list
 *
 * @param[in]   head         Pointer to the head of the linked list
 * @param[in]   wanted_name  The name of the argument that is being searched for
 * @param[out]  wanted_arg   A pointer to a pointer where the found argument description will be stored
 *                           If the argument is found, `wanted_arg` will point to the corresponding node
 * @return
 *       - ESP_GMF_ERR_OK           Success, the argument description was found, and `wanted_arg` is updated
 *       - ESP_GMF_ERR_NOT_FOUND    If no argument with the specified `wanted_name` is found
 *       - ESP_GMF_ERR_INVALID_ARG  If `head`, `wanted_name`, or `wanted_arg` is `NULL`
 */
static inline esp_gmf_err_t esp_gmf_args_desc_found(esp_gmf_args_desc_t *head, const char *wanted_name, esp_gmf_args_desc_t **wanted_arg)
{
    esp_gmf_args_desc_t *current = head;
    esp_gmf_args_desc_t *next;
    while (current != NULL) {
        next = current->next;
        ESP_LOGV("GMF_ARG", "Desc_Find, %s, want:%s, offset:%d", current->name, wanted_name, current->offset);
        if (strncasecmp(current->name, wanted_name, strlen(current->name)) == 0) {
            *wanted_arg = current;
            ESP_LOGD("GMF_ARG", "Found %s, want:%s, offset:%d\r\n", current->name, wanted_name, current->offset);
            return ESP_GMF_ERR_OK;
        } else if (current->val) {
            esp_gmf_args_desc_found(current->val, wanted_name, wanted_arg);
        }
        current = next;
    }
    return ESP_GMF_ERR_NOT_FOUND;
}

/**
 * @brief  Extracts a specific type value from the raw data buffer
 *
 * @param[in]   head     Pointer to the argument descriptor list
 * @param[in]   name     The name of the argument whose value needs to be extracted
 * @param[in]   buf      The buffer containing raw data from which the value will be extracted
 * @param[in]   buf_len  The size of the buffer `buf`
 * @param[out]  value    Pointer to store the extracted value
 *
 * @return
 *       - ESP_OK             If the value was successfully extracted from the buffer
 *       - ESP_ERR_NOT_FOUND  If the specified argument name was not found
 */
static inline esp_gmf_err_t esp_gmf_args_extract_value(esp_gmf_args_desc_t *head, const char *name, uint8_t *buf, uint32_t buf_len,
                                                       uint32_t *value)
{
    esp_gmf_args_desc_t *args = NULL;
    esp_gmf_args_desc_found(head, name, &args);
    if (args) {
        ESP_LOGD("GMF_ARG", "extract:%s-%p, offset:%d, sz:%ld\r\n", args->name, args, args->offset, args->size);
        memcpy(value, buf + args->offset, args->size);
        return ESP_GMF_ERR_OK;
    }
    return ESP_GMF_ERR_NOT_FOUND;
}

/**
 * @brief  Sets a specific value into the raw data buffer
 *
 * @param[in]  head       Pointer to the argument descriptor list
 * @param[in]  name       The name of the argument where the value needs to be set
 * @param[in]  buf        The buffer where the value will be set, it stores raw data
 * @param[in]  value      The value that will be written into the `buf`
 * @param[in]  value_len  The size of the value in bytes
 *
 * @return
 *       - ESP_OK             If the value was successfully extracted from the buffer
 *       - ESP_ERR_NOT_FOUND  If the specified argument name was not found
 */
static inline esp_gmf_err_t esp_gmf_args_set_value(esp_gmf_args_desc_t *head, const char *name, uint8_t *buf, uint8_t *value,
                                                   uint32_t value_len)
{
    esp_gmf_args_desc_t *set_args = NULL;
    esp_gmf_args_desc_found(head, name, &set_args);
    ESP_LOGD("GMF_ARG", "Set, %p", set_args);
    if (set_args) {
        ESP_LOGD("GMF_ARG", "Set value %s, offset:%d, sz:%ld", set_args->name, set_args->offset, set_args->size);
        memcpy(buf + set_args->offset, value, set_args->size);
        return ESP_GMF_ERR_OK;
    }
    return ESP_GMF_ERR_NOT_FOUND;
}

/**
 * @brief  Extract a uint8_t value from the GMF argument description list
 *
 * @param[in]   head     Pointer to the head of the argument list
 * @param[in]   name     The name of the argument to extract
 * @param[in]   buf      Pointer to the buffer containing argument values
 * @param[in]   buf_len  Length of the buffer (not used in this function)
 * @param[out]  value    Pointer to store the extracted uint64_t value
 *
 * @return
 *       - ESP_GMF_ERR_OK         Value extracted successfully
 *       - ESP_GMF_ERR_NOT_FOUND  Argument not found in the list
 */
static inline esp_gmf_err_t esp_gmf_args_extract_uint8(esp_gmf_args_desc_t *head, const char *name, uint8_t *buf, uint8_t *buf_len,
                                                       uint8_t *value)
{
    esp_gmf_args_desc_t *args = NULL;
    esp_gmf_args_desc_found(head, name, &args);
    if (args) {
        memcpy(value, buf + args->offset, args->size);
        return ESP_GMF_ERR_OK;
    }
    return ESP_GMF_ERR_NOT_FOUND;
}

/**
 * @brief  Extract a uint16_t value from the GMF argument description list
 *
 * @param[in]   head     Pointer to the head of the argument list
 * @param[in]   name     The name of the argument to extract
 * @param[in]   buf      Pointer to the buffer containing argument values
 * @param[in]   buf_len  Length of the buffer (not used in this function)
 * @param[out]  value    Pointer to store the extracted uint64_t value
 *
 * @return
 *       - ESP_GMF_ERR_OK         Value extracted successfully
 *       - ESP_GMF_ERR_NOT_FOUND  Argument not found in the list
 */
static inline esp_gmf_err_t esp_gmf_args_extract_uint16(esp_gmf_args_desc_t *head, const char *name, uint8_t *buf, uint8_t *buf_len,
                                                        uint16_t *value)
{
    esp_gmf_args_desc_t *args = NULL;
    esp_gmf_args_desc_found(head, name, &args);
    if (args) {
        memcpy(value, buf + args->offset, args->size);
        return ESP_GMF_ERR_OK;
    }
    return ESP_GMF_ERR_NOT_FOUND;
}

/**
 * @brief  Extract a uint32_t value from the GMF argument description list
 *
 * @param[in]   head     Pointer to the head of the argument list
 * @param[in]   name     The name of the argument to extract
 * @param[in]   buf      Pointer to the buffer containing argument values
 * @param[in]   buf_len  Length of the buffer (not used in this function)
 * @param[out]  value    Pointer to store the extracted uint64_t value
 *
 * @return
 *       - ESP_GMF_ERR_OK         Value extracted successfully
 *       - ESP_GMF_ERR_NOT_FOUND  Argument not found in the list
 */
static inline esp_gmf_err_t esp_gmf_args_extract_uint32(esp_gmf_args_desc_t *head, const char *name, uint8_t *buf, uint8_t *buf_len,
                                                        uint32_t *value)
{
    esp_gmf_args_desc_t *args = NULL;
    esp_gmf_args_desc_found(head, name, &args);
    if (args) {
        memcpy(value, buf + args->offset, args->size);
        return ESP_GMF_ERR_OK;
    }
    return ESP_GMF_ERR_NOT_FOUND;
}

/**
 * @brief  Extract a uint64_t value from the GMF argument description list
 *
 * @param[in]   head     Pointer to the head of the argument list
 * @param[in]   name     The name of the argument to extract
 * @param[in]   buf      Pointer to the buffer containing argument values
 * @param[in]   buf_len  Length of the buffer (not used in this function)
 * @param[out]  value    Pointer to store the extracted uint64_t value
 *
 * @return
 *       - ESP_GMF_ERR_OK         Value extracted successfully
 *       - ESP_GMF_ERR_NOT_FOUND  Argument not found in the list
 */
static inline esp_gmf_err_t esp_gmf_args_extract_uint64(esp_gmf_args_desc_t *head, const char *name, uint8_t *buf, uint8_t *buf_len,
                                                        uint64_t *value)
{
    esp_gmf_args_desc_t *args = NULL;
    esp_gmf_args_desc_found(head, name, &args);
    if (args) {
        memcpy(value, buf + args->offset, args->size);
        return ESP_GMF_ERR_OK;
    }
    return ESP_GMF_ERR_NOT_FOUND;
}

/**
 * @brief  Extract a float value from the GMF argument description list
 *
 * @param[in]   head     Pointer to the head of the argument list
 * @param[in]   name     The name of the argument to extract
 * @param[in]   buf      Pointer to the buffer containing argument values
 * @param[in]   buf_len  Length of the buffer (not used in this function)
 * @param[out]  value    Pointer to store the extracted float value
 *
 * @return
 *       - ESP_GMF_ERR_OK         Value extracted successfully
 *       - ESP_GMF_ERR_NOT_FOUND  Argument not found in the list
 */
static inline esp_gmf_err_t esp_gmf_args_extract_float(esp_gmf_args_desc_t *head, const char *name, uint8_t *buf, uint8_t *buf_len,
                                                       uint64_t *value)
{
    esp_gmf_args_desc_t *args = NULL;
    esp_gmf_args_desc_found(head, name, &args);
    if (args) {
        memcpy(value, buf + args->offset, args->size);
        return ESP_GMF_ERR_OK;
    }
    return ESP_GMF_ERR_NOT_FOUND;
}

/**
 * @brief  Extract a double value from the GMF argument description list
 *
 * @param[in]   head     Pointer to the head of the argument list
 * @param[in]   name     The name of the argument to extract
 * @param[in]   buf      Pointer to the buffer containing argument values
 * @param[in]   buf_len  Length of the buffer (not used in this function)
 * @param[out]  value    Pointer to store the extracted double value
 *
 * @return
 *       - ESP_GMF_ERR_OK         Value extracted successfully
 *       - ESP_GMF_ERR_NOT_FOUND  Argument not found in the list
 */
static inline esp_gmf_err_t esp_gmf_args_extract_double(esp_gmf_args_desc_t *head, const char *name, uint8_t *buf, uint8_t *buf_len,
                                                        uint64_t *value)
{
    esp_gmf_args_desc_t *args = NULL;
    esp_gmf_args_desc_found(head, name, &args);
    if (args) {
        memcpy(value, buf + args->offset, args->size);
        return ESP_GMF_ERR_OK;
    }
    return ESP_GMF_ERR_NOT_FOUND;
}

/**
 * @brief  Display the argument descriptions in the linked list
 *         The function uses indentation ("--") to visually represent the depth of any nested structures or arrays
 *
 * @param[in]  head  Pointer to the head of the linked list containing argument descriptions
 * @param[in]  func  The name of the function from which this logging function is called
 * @param[in]  line  The line number in the source code from which this function is called
 */
static void inline esp_gmf_args_desc_show(esp_gmf_args_desc_t *head, const char *func, size_t line)
{
    esp_gmf_args_desc_t *current = head;
    esp_gmf_args_desc_t *next;
    static uint8_t k = 0;
    if (k == 0) {
        printf("ARGS DESC on [%s,line:%d]\n", func, line);
    }
    int n = 16 - (k * 2);
    while (current != NULL) {
        next = current->next;
        for (int i = 0; i < k; ++i) {
            printf("--");
        }
        printf("name:%-*s offset:%-8d sz:%-8ld\r\n", n, current->name, current->offset, current->size);
        if (current->val != NULL) {
            k++;
            esp_gmf_args_desc_show(current->val, func, line);
        }
        current = next;
    }
    if (k) {
        k--;
    } else {
        printf("\n");
    }
}

/**
 * @brief  A macro to print the argument descriptor details
 */
#define ESP_GMF_ARGS_DESC_PRINT(x) esp_gmf_args_desc_show((x), __func__, __LINE__)

#ifdef __cplusplus
}
#endif  /* __cplusplus */
