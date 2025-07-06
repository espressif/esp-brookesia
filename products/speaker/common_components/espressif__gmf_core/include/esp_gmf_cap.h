/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <string.h>
#include "stdbool.h"
#include "esp_gmf_err.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Enumeration for property types in capabilities
 */
typedef enum {
    ESP_GMF_PROP_TYPE_NONE     = 0,  /*!< No property type specified */
    ESP_GMF_PROP_TYPE_DISCRETE = 1,  /*!< Discrete property with distinct values */
    ESP_GMF_PROP_TYPE_STEPWISE = 2,  /*!< Stepwise property with specified min, max, and step */
    ESP_GMF_PROP_TYPE_MULTIPLE = 3,  /*!< Multiple property with min, max, and multiplicative factor */
    ESP_GMF_PROP_TYPE_CONSTANT = 4,  /*!< Constant property with a single value */
} esp_gmf_prop_type;

/**
 * @brief  Structure for discrete properties in capabilities
 */
typedef struct {
    void     *collection;  /*!< Pointer to the collection of discrete values */
    uint16_t  item_num;    /*!< Number of items in the collection */
    uint16_t  item_size;   /*!< Size of each item in bytes (1, 2, or 4) */
} esp_gmf_prop_discrete_t;

/**
 * @brief  Structure for stepwise properties in capabilities, e.g. An = min + (n-1) * step
 */
typedef struct {
    uint32_t  min;   /*!< Minimum value of the property */
    uint32_t  step;  /*!< Step size between values */
    uint32_t  max;   /*!< Maximum value of the property */
} esp_gmf_prop_stepwise_t;

/**
 * @brief  Structure for multiple properties in capabilities, e.g. M = factor * n, n:1~N
 */
typedef struct {
    uint32_t  min;     /*!< Minimum value of the property */
    uint16_t  factor;  /*!< Multiplicative factor between values */
    uint32_t  max;     /*!< Maximum value of the property */
} esp_gmf_prop_multiple_t;

/**
 * @brief  Structure for constant properties in capabilities
 */
typedef struct {
    uint32_t  data;  /*!< The constant value of the property */
} esp_gmf_prop_constant_t;

/**
 * @brief  Structure for attributes of a capability
 */
typedef struct esp_gmf_cap_attr {
    uint32_t                     fourcc;     /*!< Unique 4-character code identifying the attribute */
    uint8_t                      index;      /*!< Index of the discrete attribute only */
    uint8_t                      prop_type;  /*!< Type of property (discrete, stepwise, or multiple) */
    union {
        esp_gmf_prop_discrete_t  d;          /*!< Discrete property data */
        esp_gmf_prop_stepwise_t  s;          /*!< Stepwise property data */
        esp_gmf_prop_multiple_t  m;          /*!< Multiple property data */
        esp_gmf_prop_constant_t  c;          /*!< Constant property data */
    } value;                                 /*!< Union holding the property value based on prop_type */
} esp_gmf_cap_attr_t;

/**
 * @brief  Function pointer type for iterating over capability attributes
 */
typedef esp_gmf_err_t (*cap_attr_iter_fun)(uint32_t index, esp_gmf_cap_attr_t *attr);

/**
 * @brief  Structure for performance metrics of a capability
 */
typedef struct {
    uint32_t  oper_per_sec;  /*!< Number of operations per second (or a percentage) */
} esp_gmf_cap_perf_t;

/**
 * @brief  Structure representing a capability with performance metrics and attributes
 */
typedef struct esp_gmf_cap {
    struct esp_gmf_cap *next;          /*!< Pointer to the next capability in a linked list */
    uint64_t            cap_eightcc;   /*!< Unique 8-character code for identifying the capability */
    esp_gmf_cap_perf_t  perf;          /*!< Performance metrics associated with the capability */
    cap_attr_iter_fun   attr_fun;      /*!< Function pointer for iterating over attributes in the capability */
    void               *attr_fun_ctx;  /*!< Function pointer for iterating over attributes in the capability */
} esp_gmf_cap_t;

/**
 * @brief  Macro to set discrete attribute parameters in esp_gmf_cap_attr_t
 */
#define ESP_GMF_CAP_ATTR_SET_DISCRETE(attr, code, coll, num, size) do {  \
    (attr)->fourcc = (code);                                             \
    (attr)->index = 0;                                                   \
    (attr)->prop_type = ESP_GMF_PROP_TYPE_DISCRETE;                      \
    (attr)->value.d.collection = (void *)(coll);                         \
    (attr)->value.d.item_num = (num);                                    \
    (attr)->value.d.item_size = (size);                                  \
} while (0)

/**
 * @brief  Macro to set stepwise attribute parameters in esp_gmf_cap_attr_t
 */
#define ESP_GMF_CAP_ATTR_SET_STEPWISE(attr, code, min_val, step_size, max_val) do {  \
    (attr)->fourcc = (code);                                                         \
    (attr)->index = 0;                                                               \
    (attr)->prop_type = ESP_GMF_PROP_TYPE_STEPWISE;                                  \
    (attr)->value.s.min = (min_val);                                                 \
    (attr)->value.s.step = (step_size);                                              \
    (attr)->value.s.max = (max_val);                                                 \
} while (0)

/**
 * @brief  Macro to set multiple attribute parameters in esp_gmf_cap_attr_t
 */
#define ESP_GMF_CAP_ATTR_SET_MULTIPLE(attr, code, min_val, factor_val, max_val) do {  \
    (attr)->fourcc = (code);                                                          \
    (attr)->index = 0;                                                                \
    (attr)->prop_type = ESP_GMF_PROP_TYPE_MULTIPLE;                                   \
    (attr)->value.m.min = (min_val);                                                  \
    (attr)->value.m.factor = (factor_val);                                            \
    (attr)->value.m.max = (max_val);                                                  \
} while (0)

/**
 * @brief  Macro to set constant attribute parameters in esp_gmf_cap_attr_t
 */
#define ESP_GMF_CAP_ATTR_SET_CONSTANT(attr, code, factor_val) do {  \
    (attr)->fourcc = (code);                                        \
    (attr)->index = 0;                                              \
    (attr)->prop_type = ESP_GMF_PROP_TYPE_CONSTANT;                 \
    (attr)->value.c.data = (factor_val);                            \
} while (0)

/**
 * @brief  Create a new capability node based on the given cap_value and append it to the capability linked list
 *
 * @param[in, out]  caps       Pointer to the head of the capability linked list. If *caps is NULL, a new list will be created
 * @param[in]       cap_value  Pointer to the capability data to be appended
 *
 * @return
 *       - ESP_GMF_ERR_OK           Successfully appended the capability
 *       - ESP_GMF_ERR_MEMORY_LACK  Insufficient memory to create a new capability node
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument (e.g., NULL pointer)
 */
esp_gmf_err_t esp_gmf_cap_append(esp_gmf_cap_t **caps, esp_gmf_cap_t *cap_value);

/**
 * @brief  Destroy a linked list of capabilities
 *
 * @param[in]  caps  Pointer to the head of the capability linked list
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  The argument is invalid
 */
esp_gmf_err_t esp_gmf_cap_destroy(esp_gmf_cap_t *caps);

/**
 * @brief  Fetch a capability node from a capability list using an 8-character code
 *
 * @param[in]   caps      Pointer to the head of the capability list to search
 * @param[in]   eight_cc  8-character code representing the capability to find (as uint64_t)
 * @param[out]  out_caps  Pointer to the variable that will receive the found capability node
 *
 * @return
 *       - ESP_GMF_ERR_OK           The capability was found successfully
 *       - ESP_GMF_ERR_INVALID_ARG  One or more arguments are invalid (e.g., NULL pointers)
 *       - ESP_GMF_ERR_NOT_FOUND    No matching capability was found; `*out_caps` will be set to NULL
 */
esp_gmf_err_t esp_gmf_cap_fetch_node(esp_gmf_cap_t *caps, uint64_t eight_cc, esp_gmf_cap_t **out_caps);

/**
 * @brief  Iterate over capability attributes by index
 *
 * @param[in]   caps        Pointer to the capability being iterated
 * @param[in]   attr_index  Index of the attribute to retrieve
 * @param[out]  out_attr    Pointer to store the retrieved attribute
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  The argument isinvalid
 *       - ESP_GMF_ERR_NOT_SUPPORT  The cap_attr_iter_fun is invalid
 *       - ESP_GMF_ERR_NOT_FOUND    The attr_index is out of range
 */
esp_gmf_err_t esp_gmf_cap_iterate_attr(esp_gmf_cap_t *caps, uint32_t attr_index, esp_gmf_cap_attr_t *out_attr);

/**
 * @brief  Find an attribute from a given capability using a 4-character code
 *
 * @param[in]   caps      Pointer to the capability to search
 * @param[in]   cc        Character code of the attribute to find
 * @param[out]  out_attr  Pointer to store the found attribute
 *
 * @return
 *       - ESP_GMF_ERR_OK           The attribute is found
 *       - ESP_GMF_ERR_INVALID_ARG  The argument is invalid
 *       - ESP_GMF_ERR_NOT_FOUND    The attribute is not found
 */
esp_gmf_err_t esp_gmf_cap_find_attr(esp_gmf_cap_t *caps, uint32_t cc, esp_gmf_cap_attr_t *out_attr);

/**
 * @brief  Check if a given value is supported by an attribute
 *
 * @param[in]   attr        Pointer to the attribute to check
 * @param[in]   val         Value to check
 * @param[out]  is_support  Pointer to boolean indicating if the value is supported
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  The argument is invalid
 *       - ESP_GMF_ERR_NOT_SUPPORT  If the value or attribute type is not support
 */
esp_gmf_err_t esp_gmf_cap_attr_check_value(esp_gmf_cap_attr_t *attr, uint32_t val, bool *is_support);

/**
 * @brief  Iterate through values of an attribute
 *
 * @param[in,out]  attr     Pointer to the attribute iterator
 * @param[out]     val      Pointer to store the current value
 * @param[out]     is_last  Pointer to boolean indicating if this is the last value
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  The argument is invalid
 *       - ESP_GMF_ERR_NOT_SUPPORT  If the value or attribute type is not support
 */
esp_gmf_err_t esp_gmf_cap_attr_iterator_value(esp_gmf_cap_attr_t *attr, uint32_t *val, bool *is_last);

/**
 * @brief  Retrieve the first valid value of an attribute
 *
 * @param[in,out]  iter  Pointer to the attribute
 * @param[out]     val   Pointer to store the retrieved value
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument (e.g., `iter` or `val` is NULL)
 *       - ESP_GMF_ERR_NOT_SUPPORT  The attribute type does not support this operation
 */
esp_gmf_err_t esp_gmf_cap_attr_get_first_value(esp_gmf_cap_attr_t *attr, uint32_t *val);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
