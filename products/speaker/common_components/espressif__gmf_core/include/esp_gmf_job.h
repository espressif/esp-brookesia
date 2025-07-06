/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once
#include <string.h>
#include "esp_gmf_err.h"
#include "esp_gmf_oal_mem.h"
#include "esp_log.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define ESP_GMF_JOB_LABLE_MAX_LEN (64)  // The maximum length of tag including the '\0' is 64 byte.
#define ESP_GMF_JOB_STR_OPEN      ("_open")
#define ESP_GMF_JOB_STR_PROCESS   ("_proc")
#define ESP_GMF_JOB_STR_CLOSE     ("_close")

/**
 * @brief  This enumeration defines different states that a GMF job can be in
 */
typedef enum {
    ESP_GMF_JOB_STATUS_SUSPENDED = 0,  /*!< The job is suspended */
    ESP_GMF_JOB_STATUS_READY     = 1,  /*!< The job is ready */
    ESP_GMF_JOB_STATUS_RUNNING   = 2   /*!< The job is running */
} esp_gmf_job_status_t;

/**
 * @brief  This enumeration specifies how many times a GMF job should be executed
 */
typedef enum {
    ESP_GMF_JOB_TIMES_NONE     = 0,  /*!< The job should not be executed */
    ESP_GMF_JOB_TIMES_ONCE     = 1,  /*!< The job should be executed once */
    ESP_GMF_JOB_TIMES_INFINITE = 2   /*!< The job should be executed indefinitely */
} esp_gmf_job_times_t;

/**
 * @brief  This enumeration specifies the error status of a GMF job
 */
typedef enum {
    ESP_GMF_JOB_ERR_TRUNCATE = 3,        /*!< The job has been truncated */
    ESP_GMF_JOB_ERR_DONE     = 2,        /*!< The job has been completed */
    ESP_GMF_JOB_ERR_CONTINUE = 1,        /*!< The job should continue */
    ESP_GMF_JOB_ERR_OK       = ESP_OK,   /*!< The job has executed successfully */
    ESP_GMF_JOB_ERR_FAIL     = ESP_FAIL  /*!< The job has failed to execute */
} esp_gmf_job_err_t;

/**
 * @brief  Function pointer type for GMF job functions
 *
 * @param[in]  self  A pointer to the job itself, allowing the function to access its internal data if needed
 * @param[in]  para  A pointer to additional parameters needed by the job function
 *
 * @return
 *       - A value of type esp_gmf_job_err_t indicating the execution status of the job function
 */
typedef esp_gmf_job_err_t (*esp_gmf_job_func)(void *self, void *para);

/**
 * @brief  This structure represents a job in the GMF
 *         A job encapsulates a function to be executed, along with its context and other properties
 */
typedef struct _esp_gmf_job_t {
    struct _esp_gmf_job_t *prev;   /*!< Pointer to the previous job in the linked list */
    struct _esp_gmf_job_t *next;   /*!< Pointer to the next job in the linked list */
    const char            *label;  /*!< Label identifying the job */
    esp_gmf_job_func       func;   /*!< Function pointer to the job's function */
    void                  *ctx;    /*!< Context pointer to be passed to the job's function */
    void                  *para;   /*!< Parameter pointer to be passed to the job's function */
    esp_gmf_job_times_t    times;  /*!< Times the job should be executed */
    esp_gmf_job_err_t      ret;    /*!< Return value of the job function */
} esp_gmf_job_t;

/**
 * @brief  Concatenate two strings into the target string
 *
 * @param[in,out]  target       Pointer to the target buffer where the concatenated string will be stored
 * @param[in]      target_size  Size of the target buffer
 * @param[in]      src1         Pointer to the null-terminated string to be concatenated from the beginning
 * @param[in]      src2         Pointer to the string to be concatenated
 * @param[in]      src2_len     Length of the src2 string to be concatenated
 */
static inline void esp_gmf_job_str_cat(char *target, int target_size,
                                       const char *src1, const char *src2, int src2_len)
{
    memset(target, 0, target_size);
    snprintf(target, target_size - 1 - src2_len, "%s", src1);
    strcat(target, src2);
}

typedef struct esp_gmf_job_node {
    struct esp_gmf_job_node *next;
    uint32_t                 node_addr;
} esp_gmf_job_node_t;

typedef struct {
    esp_gmf_job_node_t *top;
} esp_gmf_job_stack_t;

static inline esp_gmf_err_t esp_gmf_job_stack_create(esp_gmf_job_stack_t **stack)
{
    *stack = esp_gmf_oal_calloc(1, sizeof(esp_gmf_job_stack_t));
    ESP_GMF_MEM_CHECK("GMF_JOB_STACK", *stack, return ESP_GMF_ERR_MEMORY_LACK);
    return ESP_GMF_ERR_OK;
}

static inline esp_gmf_err_t esp_gmf_job_stack_push(esp_gmf_job_stack_t *stack, uint32_t node_addr)
{
    ESP_GMF_NULL_CHECK("GMF_JOB_STACK", stack, return ESP_GMF_ERR_INVALID_ARG);
    esp_gmf_job_node_t *node = esp_gmf_oal_calloc(1, sizeof(esp_gmf_job_node_t));
    ESP_GMF_MEM_CHECK("GMF_JOB_STACK", node, return ESP_GMF_ERR_MEMORY_LACK);
    node->node_addr = node_addr;
    node->next = stack->top;
    stack->top = node;
    return ESP_GMF_ERR_OK;
}

static inline esp_gmf_err_t esp_gmf_job_stack_pop(esp_gmf_job_stack_t *stack, uint32_t *node_addr)
{
    ESP_GMF_NULL_CHECK("GMF_JOB_STACK", stack, return ESP_GMF_ERR_INVALID_ARG);
    ESP_GMF_NULL_CHECK("GMF_JOB_STACK", node_addr, return ESP_GMF_ERR_INVALID_ARG);
    ESP_GMF_CHECK("GMF_JOB_STACK", stack->top, return ESP_GMF_ERR_NOT_READY, "Job stack is empty");
    esp_gmf_job_node_t *node = stack->top;
    if (node->next == NULL) {
        *node_addr = node->node_addr;
        return ESP_GMF_ERR_OK;
    }
    stack->top = node->next;
    node->next = NULL;
    *node_addr = node->node_addr;
    esp_gmf_oal_free(node);
    return ESP_GMF_ERR_OK;
}

static inline void esp_gmf_job_stack_clear(esp_gmf_job_stack_t *stack)
{
    if (!stack) {
        return;
    }
    esp_gmf_job_node_t *curr = stack->top;
    while (curr) {
        esp_gmf_job_node_t *temp = curr;
        curr = curr->next;
        esp_gmf_oal_free(temp);
    }
    stack->top = NULL;
}

static inline esp_gmf_err_t esp_gmf_job_stack_remove(esp_gmf_job_stack_t *stack, uint32_t node_addr)
{
    ESP_GMF_NULL_CHECK("GMF_JOB_STACK", stack, return ESP_GMF_ERR_INVALID_ARG);
    if (stack->top == NULL) {
        return ESP_GMF_ERR_NOT_READY;
    }
    if (stack->top->node_addr == node_addr) {
        esp_gmf_job_node_t *removed = stack->top;
        stack->top = removed->next;
        removed->next = NULL;
        esp_gmf_oal_free(removed);
        return ESP_GMF_ERR_OK;
    }
    esp_gmf_job_node_t *prev = stack->top;
    esp_gmf_job_node_t *curr = stack->top->next;
    while (curr) {
        if (curr->node_addr == node_addr) {
            prev->next = curr->next;
            curr->next = NULL;
            esp_gmf_oal_free(curr);
            return ESP_GMF_ERR_OK;
        }
        prev = curr;
        curr = curr->next;
    }
    return ESP_GMF_ERR_NOT_FOUND;
}

static inline esp_gmf_err_t esp_gmf_job_stack_is_empty(const esp_gmf_job_stack_t *stack, bool *empty)
{
    ESP_GMF_NULL_CHECK("GMF_JOB_STACK", stack, return ESP_GMF_ERR_INVALID_ARG);
    *empty = false;
    if (stack->top == NULL) {
        *empty = true;
    }
    return ESP_GMF_ERR_OK;
}

static inline void esp_gmf_job_stack_destroy(esp_gmf_job_stack_t *stack)
{
    ESP_GMF_NULL_CHECK("GMF_JOB_STACK", stack, return);
    esp_gmf_job_node_t *curr = stack->top;
    while (curr) {
        esp_gmf_job_node_t *temp = curr;
        curr = curr->next;
        esp_gmf_oal_free(temp);
    }
    esp_gmf_oal_free(stack);
}

static inline void esp_gmf_job_stack_show(const esp_gmf_job_stack_t *stack, int line)
{
    const esp_gmf_job_node_t *cur = stack ? stack->top : NULL;
    printf("Job Stack [line:%d] (top -> bottom): ", line);
    while (cur) {
        esp_gmf_job_t *job = (esp_gmf_job_t *)cur->node_addr;
        printf("%s(%p) ", job->label, job->func);
        cur = cur->next;
    }
    printf("\n");
}

#define ESP_GMF_JOB_STACK_SHOW(stack) esp_gmf_job_stack_show(stack, __LINE__)

#ifdef __cplusplus
}
#endif  /* __cplusplus */
