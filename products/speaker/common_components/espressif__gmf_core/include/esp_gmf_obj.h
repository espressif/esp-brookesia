/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_gmf_err.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define ESP_GMF_TAG_MAX_LEN (32)  // The maximum length of tag including the '\0' is 32 byte.

typedef void *esp_gmf_obj_handle_t;

/**
 * @brief  GMF object structure
 *
 *         This structure represents a generic GMF object with basic functionalities,
 *         such as creating a new object, deleting an object, and storing object-related
 *         information.
 */
typedef struct esp_gmf_obj_ {
    struct esp_gmf_obj_ *prev;                                           /*!< Pointer to the previous GMF object in the list */
    struct esp_gmf_obj_ *next;                                           /*!< Pointer to the next GMF object in the list */
    esp_gmf_err_t (*new_obj)(void *cfg, esp_gmf_obj_handle_t *new_obj);  /*!< Function pointer to create a new object */
    esp_gmf_err_t (*del_obj)(esp_gmf_obj_handle_t obj);                  /*!< Function pointer to delete an object */
    const char          *tag;                                            /*!< Pointer to a tag or identifier associated with the object */
    void                *cfg;                                            /*!< Pointer to the configuration data for the object */
} esp_gmf_obj_t;

#define OBJ_GET_TAG(x) ((x) ? ((((esp_gmf_obj_t *)x)->tag) ? ((esp_gmf_obj_t *)x)->tag : "NULL") : "NULL")

#define OBJ_GET_CFG(x) ((x) ? ((((esp_gmf_obj_t *)x)->cfg) ? ((esp_gmf_obj_t *)x)->cfg : NULL) : NULL)

/**
 * @brief  Duplicate a GMF object
 *
 * @param[in]   old_obj  Handle of the old object to duplicate
 * @param[out]  new_obj  Pointer to store the handle of the new duplicated object
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success (new_obj contains the duplicated object handle)
 *       - ESP_GMF_ERR_INVALID_ARG  If old_obj is invalid or new_obj is NULL
 *       - ESP_GMF_ERR_MEMORY_LACK  Insufficient memory for operation
 */
esp_gmf_err_t esp_gmf_obj_dupl(esp_gmf_obj_handle_t old_obj, esp_gmf_obj_handle_t *new_obj);

/**
 * @brief  Create a new GMF object based on an existing object with additional configuration
 *
 * @param[in]   old_obj  Handle of the existing object to use as a base
 * @param[in]   cfg      Pointer to the additional configuration for the new object
 * @param[out]  new_obj  Pointer to store the handle of the newly created object
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument provided
 *       - ESP_GMF_ERR_MEMORY_LACK  Insufficient memory for operation
 */
esp_gmf_err_t esp_gmf_obj_new(esp_gmf_obj_handle_t old_obj, void *cfg, esp_gmf_obj_handle_t *new_obj);

/**
 * @brief  Delete a GMF object
 *
 * @param[in]  obj  Handle of the object to delete
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument provided
 */
esp_gmf_err_t esp_gmf_obj_delete(esp_gmf_obj_handle_t obj);

/**
 * @brief  Set configuration for a GMF object
 *
 * @param[in]  obj       Handle of the object to set the configuration for
 * @param[in]  cfg       Pointer to the configuration data
 * @param[in]  cfg_size  Size of the configuration data
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument provided
 *       - ESP_GMF_ERR_MEMORY_LACK  Insufficient memory for operation
 */
esp_gmf_err_t esp_gmf_obj_set_config(esp_gmf_obj_handle_t obj, void *cfg, int cfg_size);

/**
 * @brief  Set a tag for a GMF object
 *
 * @param[in]  obj  Handle of the object to set the tag for
 * @param[in]  tag  Pointer to the tag string
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument provided
 *       - ESP_GMF_ERR_MEMORY_LACK  Insufficient memory for operation
 */
esp_gmf_err_t esp_gmf_obj_set_tag(esp_gmf_obj_handle_t obj, const char *tag);

/**
 * @brief  Get the tag associated with a GMF object
 *
 * @param[in]   obj  Handle of the object to get the tag from
 * @param[out]  tag  Pointer to store the tag string
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument provided
 */
esp_gmf_err_t esp_gmf_obj_get_tag(esp_gmf_obj_handle_t obj, char **tag);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
