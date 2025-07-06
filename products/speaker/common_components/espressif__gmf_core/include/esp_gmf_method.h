/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_gmf_err.h"
#include "esp_gmf_oal_mem.h"
#include "esp_gmf_args_desc.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Function pointer type for GMF method functions
 *         This typedef defines the function pointer type for GMF method functions
 */
typedef esp_gmf_err_t (*esp_gmf_method_func)(void *handle, esp_gmf_args_desc_t *arg_desc, uint8_t *buf, int buf_len);

/**
 * @brief  Structure for GMF methods
 *         This structure defines a linked list node for storing GMF methods
 */
typedef struct esp_gmf_method {
    struct esp_gmf_method *next;       /*!< Pointer to the next method node */
    const char            *name;       /*!< Name of the method */
    esp_gmf_method_func    func;       /*!< Function pointer to the method implementation */
    uint16_t               args_cnt;   /*!< Number of the argument description */
    esp_gmf_args_desc_t   *args_desc;  /*!< A pointer to argument description structure */
} esp_gmf_method_t;

/**
 * @brief  Create a new GMF method
 *         This function creates a new GMF method and initializes its members
 *
 * @param[in]   name    Name of the method
 * @param[in]   func    Function pointer to the method implementation
 * @param[in]   args    The arguments description list
 * @param[out]  handle  Pointer to the handle for the newly created method
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success
 *       - ESP_GMF_ERR_MEMORY_LACK  Memory allocation failure
 */
static inline esp_gmf_err_t esp_gmf_method_create(const char *name, esp_gmf_method_func func, esp_gmf_args_desc_t *args, esp_gmf_method_t **handle)
{
    esp_gmf_method_t *new_method = (esp_gmf_method_t *)esp_gmf_oal_calloc(1, sizeof(esp_gmf_method_t));
    if (new_method) {
        if (name) {
            new_method->name = esp_gmf_oal_strdup(name);
            if (new_method->name == NULL) {
                esp_gmf_oal_free(new_method);
                return ESP_GMF_ERR_MEMORY_LACK;
            }
        }
        new_method->args_cnt  = esp_gmf_args_desc_count(args);
        new_method->func      = func;
        new_method->args_desc = args;
        new_method->next = NULL;
        *handle = new_method;
        return ESP_GMF_ERR_OK;
    }
    return ESP_GMF_ERR_MEMORY_LACK;
}

/**
 * @brief  Append a new GMF method to the method list
 *         This function creates a new GMF method and appends it to the provided method list
 *
 * @param[in,out]  head  Pointer to the head of the method list
 * @param[in]      name  Name of the method
 * @param[in]      func  Function pointer to the method implementation
 * @param[in]      args  The arguments description list
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success
 *       - ESP_GMF_ERR_MEMORY_LACK  Memory allocation failure
 */
static inline esp_gmf_err_t esp_gmf_method_append(esp_gmf_method_t **head, const char *name, esp_gmf_method_func func, esp_gmf_args_desc_t *args)
{
    esp_gmf_method_t *new_method = NULL;
    esp_gmf_method_create(name, func, args, &new_method);
    if (new_method == NULL) {
        return ESP_GMF_ERR_MEMORY_LACK;
    }
    if (*head == NULL) {
        *head = new_method;
    } else {
        esp_gmf_method_t *current = *head;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = new_method;
    }
    return ESP_GMF_ERR_OK;
}

/**
 * @brief  Destroy a GMF method list
 *
 * @param[in]  head  Pointer to the head of the method list
 */
static inline void esp_gmf_method_destroy(esp_gmf_method_t *head)
{
    esp_gmf_method_t *current = head;
    esp_gmf_method_t *next;
    while (current != NULL) {
        next = current->next;
        if (current->name) {
            esp_gmf_oal_free((char *)current->name);
        }
        if (current->args_desc) {
            esp_gmf_args_desc_destroy(current->args_desc);
            current->args_desc = NULL;
        }
        esp_gmf_oal_free(current);
        current = next;
    }
}

/**
 * @brief  Display the list of GMF methods and their argument descriptions
 *
 * @param[in]  head  Pointer to the head of the method list
 */
static inline void esp_gmf_method_show(esp_gmf_method_t *head)
{
    esp_gmf_method_t *current = head;
    while (current != NULL) {
        printf("Method:%s\n", current->name);
        if (current->args_desc) {
            ESP_GMF_ARGS_DESC_PRINT(current->args_desc);
        }
        current = current->next;
    }
}

/**
 * @brief  Find a method by name in a linked list of methods
 *
 * @param[in]   head           Pointer to the head of the linked list of methods
 * @param[in]   wanted_name    The name of the method to search for
 * @param[out]  wanted_method  Pointer to store the found method, if any. Set to NULL if not found.
 *
 * @return
 *       - ESP_GMF_ERR_OK         The method was found successfully
 *       - ESP_GMF_ERR_NOT_FOUND  The method with the specified name was not found
 */
static inline esp_gmf_err_t esp_gmf_method_found(const esp_gmf_method_t *head, const char *wanted_name, const esp_gmf_method_t **wanted_method)
{
    if ((head == NULL) || (wanted_method == NULL)) {
        return ESP_GMF_ERR_INVALID_ARG;
    }
    const esp_gmf_method_t *current = head;
    while (current != NULL) {
        ESP_LOGD("GMF_Method", "name:%s, want:%s", current->name, wanted_name);
        if (strncasecmp(current->name, wanted_name, strlen(current->name)) == 0) {
            *wanted_method = current;
            return ESP_GMF_ERR_OK;
        }
        current = current->next;
    }
    *wanted_method = NULL;
    return ESP_GMF_ERR_NOT_FOUND;
}

/**
 * @brief  Query the argument descriptors of a method
 *
 * @param[in]   head  Pointer to the method structure from which arguments will be queried
 * @param[out]  args  Pointer to store the retrieved argument descriptors
 *                    Set to NULL if the method has no arguments
 *
 * @return
 *       - ESP_GMF_ERR_OK           The argument descriptors were successfully retrieved
 *       - ESP_GMF_ERR_INVALID_ARG  The `head` or `args` pointer is invalid (e.g., NULL)
 */
static inline esp_gmf_err_t esp_gmf_method_query_args(esp_gmf_method_t *head, esp_gmf_args_desc_t **args)
{
    if (head) {
        *args = head->args_desc;
        return ESP_GMF_ERR_OK;
    }
    *args = NULL;
    return ESP_GMF_ERR_INVALID_ARG;
}

#ifdef __cplusplus
}
#endif  /* __cplusplus */
