/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Doubly linked list node structure
 */
typedef struct esp_gmf_node_t {
    struct esp_gmf_node_t *prev;  /*!< Pointer to the previous node in the linked list */
    struct esp_gmf_node_t *next;  /*!< Pointer to the next node in the linked list */
} esp_gmf_node_t;

/**
 * @brief  Function pointer type for node cleanup
 */
typedef int (*node_free)(void *p);

/**
 * @brief  Get the head node of a linked list
 *
 * @param[in]  node  The node of the linked list
 * @param[in]  num   The number of the node start from given 'node'
 *
 * @return
 *       - NULL    If the linked list is empty
 *       - Others  Pointer to the head node of the linked list
 */
esp_gmf_node_t *esp_gmf_node_get_head(esp_gmf_node_t *node, int *num);

/**
 * @brief  Get the tail node of a linked list
 *
 * @param[in]  root  Root node of the linked list
 *
 * @return
 *       - NULL    If the linked list is empty
 *       - Others  Pointer to the tail node of the linked list
 */
esp_gmf_node_t *esp_gmf_node_get_tail(esp_gmf_node_t *root);

/**
 * @brief  Add a node to the end of a linked list
 *
 * @param[in]  root      Root node of the linked list
 * @param[in]  new_node  Node to be added to the end of the linked list
 */
void esp_gmf_node_add_last(esp_gmf_node_t *root, esp_gmf_node_t *new_node);

/**
 * @brief  Clear a linked list, freeing associated resources
 *
 * @param[in,out]  root  Pointer to the root node of the linked list
 * @param[in]      del   Function pointer for freeing node resources
 */
void esp_gmf_node_clear(esp_gmf_node_t **root, node_free del);

/**
 * @brief  Get the size of a linked list
 *
 * @param[in]  root  Root node of the linked list
 *
 * @return
 *       - > 0  Number of nodes in the linked list
 *       - 0    Invalid argument provided or the list is empty
 */
int esp_gmf_node_get_size(esp_gmf_node_t *root);

/**
 * @brief  Insert a new node after a specified node in a linked list
 *
 * @param[in]  prev  Node after which the new node will be inserted
 * @param[in]  new   Node to be inserted
 */
void esp_gmf_node_insert_after(esp_gmf_node_t *prev, esp_gmf_node_t *new);

/**
 * @brief  Delete a specified node from a linked list, freeing associated resources
 *
 * @param[in,out]  root  Pointer to the root node of the linked list
 * @param[in]      del   Node to be deleted
 */
void esp_gmf_node_del_at(esp_gmf_node_t **root, esp_gmf_node_t *del);

/**
 * @brief  Get the next node in a linked list
 *
 * @param[in]  node  Current node in the linked list
 *
 * @return
 *       - NULL    If the current node is the tail
 *       - Others  Pointer to the next node
 */
esp_gmf_node_t *esp_gmf_node_for_next(esp_gmf_node_t *node);

/**
 * @brief  Get the previous node in a linked list
 *
 * @param[in]  node  Current node in the linked list
 *
 * @return
 *       - NULL    If the current node is the head
 *       - Others  Pointer to the previous node
 */
esp_gmf_node_t *esp_gmf_node_for_prev(esp_gmf_node_t *node);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
