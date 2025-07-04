/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once
#include "esp_gmf_oal_mem.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Single node structure for a singly linked list
 */
typedef struct esp_gmf_single_node {
    struct esp_gmf_single_node *next;  /*!< Pointer to the next node in the list */
} esp_gmf_single_node_t;

/**
 * @brief  Function pointer type for freeing a node, with context passed as a parameter
 */
typedef void (*single_node_free)(void *ctx);

/**
 * @brief  Append a node to the end of a singly linked list
 *
 * @param[in,out]  head  Pointer to the head of the linked list
 * @param[in]      node  Node to append to the list
 */
static inline void esp_gmf_single_node_append(esp_gmf_single_node_t **head, esp_gmf_single_node_t *node)
{
    if (*head == NULL) {
        *head = node;
    } else {
        esp_gmf_single_node_t *temp = *head;
        while (temp->next) {
            temp = temp->next;
        }
        temp->next = node;
    }
}

/**
 * @brief  Remove a specified node from the singly linked list
 *         If the node is not found, the function does nothing
 *
 * @param[in,out]  head  Pointer to the head of the linked list
 * @param[in]      del   Node to remove from the list
 *
 */
static inline void esp_gmf_single_node_remove(esp_gmf_single_node_t **head, esp_gmf_single_node_t *del)
{
    if (*head == NULL || del == NULL) {
        return;
    }
    if (*head == del) {
        *head = del->next;
        esp_gmf_oal_free(del);
        return;
    }
    esp_gmf_single_node_t *temp = *head;
    while (temp != NULL && temp->next != del) {
        temp = temp->next;
    }
    if (temp == NULL) {
        return;
    }
    temp->next = del->next;
    esp_gmf_oal_free(del);
}

/**
 * @brief  Get the count of nodes in the singly linked list
 *
 * @param[in]  head  Pointer to the head of the linked list
 * @return
 *       - > 0  Number of nodes in the list
 */
static inline int esp_gmf_single_node_get_count(esp_gmf_single_node_t *head)
{
    int count = 0;
    esp_gmf_single_node_t *current = head;
    while (current != NULL) {
        count++;
        current = current->next;
    }
    return count;
}

/**
 * @brief  Destroy the entire singly linked list, freeing each node using the provided callback function
 *
 * @param[in]  head     Pointer to the head of the linked list
 * @param[in]  free_cb  Callback function to free each node in the list
 *
 */
static inline void esp_gmf_single_node_destroy(esp_gmf_single_node_t *head, single_node_free free_cb)
{
    esp_gmf_single_node_t *temp = head;
    while (temp) {
        head = temp->next;
        free_cb(temp);
        temp = head;
    }
}

#ifdef __cplusplus
}
#endif  /* __cplusplus */
