/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "audio_doa.h"
#include "esp_gmf_task.h"
#include "esp_gmf_oal_thread.h"
#include "esp_gmf_data_bus.h"
#include "esp_gmf_new_databus.h"
#include "esp_doa.h"
#include "esp_log.h"

#define TAG "AUDIO_DOA"

#define AUDIO_DOA_DATA_BUS_SIZE 2048

#define DOA_WINDOW_SIZE 7
#define GAUSSIAN_SIGMA  1.0

#define START_BIT (1 << 0)

typedef enum {
    AUDIO_DOA_STATE_IDLE,
    AUDIO_DOA_STATE_RUNNING,
    AUDIO_DOA_STATE_ERROR,
} audio_doa_state_t;

typedef enum {
    MIC_DIRECTION_LEFT,
    MIC_DIRECTION_RIGHT,
    MIC_DIRECTION_MAX,
} mic_direction_t;

typedef struct {
    audio_doa_state_t     state;
    audio_doa_callback_t  cb;
    void                 *ctx;
    uint8_t              *audio_data;
    int                   audio_data_size;
    doa_handle_t         *doa_handle;
    esp_gmf_db_handle_t   data_bus;
    esp_gmf_oal_thread_t  task_handle;
    EventGroupHandle_t    event_group;
    int16_t              *mic_data[MIC_DIRECTION_MAX];
    float                 doa_history[DOA_WINDOW_SIZE];
    int                   doa_history_index;
    float                *gaussian_weights;
} audio_doa_t;

static float moving_weighted_average(float *data, int window_size, float *weights, int current_index)
{
    float sum = 0.0f;
    float weight_sum = 0.0f;

    for (int i = 0; i < window_size; i++) {
        int data_index = (current_index - i + window_size) % window_size;
        sum += data[data_index] * weights[i];
        weight_sum += weights[i];
    }

    return sum / weight_sum;
}

static void generate_gaussian_weights(float *weights, int size, float sigma)
{
    float sum = 0.0f;
    float center = (size - 1) / 2.0f;

    for (int i = 0; i < size; i++) {
        float x = i - center;
        weights[i] = expf(-(x * x) / (2 * sigma * sigma));
        sum += weights[i];
    }

    for (int i = 0; i < size; i++) {
        weights[i] /= sum;
    }
}

static float doa_angle_calibration(float raw_angle)
{
    if (raw_angle < 0) {
        raw_angle = 0;
    }
    if (raw_angle > 180) {
        raw_angle = 180;
    }
    float center = 90.0f;
    float offset_from_center = raw_angle - center;
    float abs_offset = fabsf(offset_from_center);
    float correction_factor = 1.0f + (abs_offset / 90.0f) * 0.25f;

    float corrected_angle;
    if (offset_from_center >= 0) {
        corrected_angle = center + offset_from_center * correction_factor;
    } else {
        corrected_angle = center + offset_from_center * correction_factor;
    }
    if (corrected_angle < 0) {
        corrected_angle = 0;
    }
    if (corrected_angle > 180) {
        corrected_angle = 180;
    }
    ESP_LOGD(TAG, "DOA calibration: %.2f -> %.2f (correction: %.3f)",
             raw_angle, corrected_angle, correction_factor);

    return corrected_angle;
}

static inline void extract_mic_data(audio_doa_t *doa)
{
    int16_t *audio_buffer = (int16_t *)doa->audio_data;
    int size = doa->audio_data_size / sizeof(int16_t);
    for (int i = 0; i < size; i += 4) {
        doa->mic_data[MIC_DIRECTION_LEFT][i / 4] = audio_buffer[i + 1];
        doa->mic_data[MIC_DIRECTION_RIGHT][i / 4] = audio_buffer[i + 3];
    }
}

static void audio_doa_thread(void *arg)
{
    audio_doa_t *doa = (audio_doa_t *)arg;
    while (1) {
        uint32_t bits = xEventGroupWaitBits(doa->event_group, START_BIT, pdFALSE, pdFALSE, pdMS_TO_TICKS(10));
        if (!(bits & START_BIT)) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        doa->state = AUDIO_DOA_STATE_RUNNING;
        esp_gmf_data_bus_block_t blk = {0};
        blk.buf = doa->audio_data;
        blk.buf_length = AUDIO_DOA_DATA_BUS_SIZE;

        esp_gmf_err_io_t ret = esp_gmf_db_acquire_read(doa->data_bus, &blk, AUDIO_DOA_DATA_BUS_SIZE, pdMS_TO_TICKS(10));
        if (ret < 0) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        extract_mic_data(doa);
        float estimated_direction = esp_doa_process(doa->doa_handle, doa->mic_data[MIC_DIRECTION_LEFT], doa->mic_data[MIC_DIRECTION_RIGHT]);
        doa->doa_history[doa->doa_history_index] = estimated_direction;
        float filtered_direction = moving_weighted_average(doa->doa_history, DOA_WINDOW_SIZE, doa->gaussian_weights, doa->doa_history_index);
        doa->doa_history_index = (doa->doa_history_index + 1) % DOA_WINDOW_SIZE;
        float calibrated_direction = doa_angle_calibration(filtered_direction);
        if (doa->cb) {
            doa->cb(calibrated_direction, doa->ctx);
        }
        esp_gmf_db_release_read(doa->data_bus, &blk, pdMS_TO_TICKS(10));

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

esp_err_t audio_doa_new(audio_doa_handle_t *doa_handle, audio_doa_config_t *config)
{
    if (doa_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    audio_doa_t *doa = (audio_doa_t *)malloc(sizeof(audio_doa_t));
    if (doa == NULL) {
        return ESP_ERR_NO_MEM;
    }
    memset(doa, 0, sizeof(audio_doa_t));
    doa->state = AUDIO_DOA_STATE_IDLE;
    doa->cb = NULL;
    doa->ctx = NULL;
    doa->doa_handle = NULL;
    doa->doa_history_index = 0;

    doa->event_group = xEventGroupCreate();
    if (doa->event_group == NULL) {
        free(doa);
        return ESP_ERR_NO_MEM;
    }
    int ret_db = esp_gmf_db_new_ringbuf(1, AUDIO_DOA_DATA_BUS_SIZE * 3, &doa->data_bus);
    if (ret_db != ESP_GMF_ERR_OK || doa->data_bus == NULL) {
        vEventGroupDelete(doa->event_group);
        free(doa);
        return ESP_ERR_NO_MEM;
    }
    int samples = AUDIO_DOA_DATA_BUS_SIZE / (sizeof(int16_t) * 4);
    doa->doa_handle = esp_doa_create(16000, 10, 0.06, samples);
    if (doa->doa_handle == NULL) {
        esp_gmf_db_deinit(doa->data_bus);
        vEventGroupDelete(doa->event_group);
        free(doa);
        return ESP_ERR_NO_MEM;
    }
    doa->audio_data = (uint8_t *)calloc(AUDIO_DOA_DATA_BUS_SIZE, sizeof(uint8_t));
    if (doa->audio_data == NULL) {
        esp_doa_destroy(doa->doa_handle);
        esp_gmf_db_deinit(doa->data_bus);
        vEventGroupDelete(doa->event_group);
        free(doa);
        return ESP_ERR_NO_MEM;
    }
    doa->audio_data_size = AUDIO_DOA_DATA_BUS_SIZE;

    doa->gaussian_weights = (float *)calloc(DOA_WINDOW_SIZE, sizeof(float));
    if (doa->gaussian_weights == NULL) {
        free(doa->audio_data);
        esp_doa_destroy(doa->doa_handle);
        esp_gmf_db_deinit(doa->data_bus);
        vEventGroupDelete(doa->event_group);
        free(doa);
        return ESP_ERR_NO_MEM;
    }
    generate_gaussian_weights(doa->gaussian_weights, DOA_WINDOW_SIZE, GAUSSIAN_SIGMA);

    for (int i = 0; i < MIC_DIRECTION_MAX; i++) {
        doa->mic_data[i] = (int16_t *)calloc(AUDIO_DOA_DATA_BUS_SIZE / 4, sizeof(int16_t));
        if (doa->mic_data[i] == NULL) {
            for (int j = 0; j < i; j++) {
                free(doa->mic_data[j]);
            }
            free(doa->gaussian_weights);
            free(doa->audio_data);
            esp_doa_destroy(doa->doa_handle);
            esp_gmf_db_deinit(doa->data_bus);
            vEventGroupDelete(doa->event_group);
            free(doa);
            return ESP_ERR_NO_MEM;
        }
    }

    esp_gmf_err_t ret = esp_gmf_oal_thread_create(&doa->task_handle, "audio_doa_thread", audio_doa_thread, doa, 4096, 10, false, 0);
    if (ret != ESP_GMF_ERR_OK) {
        for (int i = 0; i < MIC_DIRECTION_MAX; i++) {
            free(doa->mic_data[i]);
        }
        free(doa->gaussian_weights);
        free(doa->audio_data);
        esp_doa_destroy(doa->doa_handle);
        esp_gmf_db_deinit(doa->data_bus);
        vEventGroupDelete(doa->event_group);
        free(doa);
        return ESP_FAIL;
    }

    *doa_handle = (audio_doa_handle_t)doa;
    return ESP_OK;
}
esp_err_t audio_doa_delete(audio_doa_handle_t doa_handle)
{
    if (doa_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    audio_doa_t *doa = (audio_doa_t *)doa_handle;

    audio_doa_stop(doa_handle);

    vTaskDelay(pdMS_TO_TICKS(100));

    if (doa->mic_data[MIC_DIRECTION_LEFT]) {
        free(doa->mic_data[MIC_DIRECTION_LEFT]);
    }
    if (doa->mic_data[MIC_DIRECTION_RIGHT]) {
        free(doa->mic_data[MIC_DIRECTION_RIGHT]);
    }
    if (doa->gaussian_weights) {
        free(doa->gaussian_weights);
    }
    if (doa->audio_data) {
        free(doa->audio_data);
    }
    if (doa->doa_handle) {
        esp_doa_destroy(doa->doa_handle);
    }
    if (doa->data_bus) {
        esp_gmf_db_deinit(doa->data_bus);
    }
    if (doa->event_group) {
        vEventGroupDelete(doa->event_group);
    }
    free(doa);
    return ESP_OK;
}

esp_err_t audio_doa_set_doa_result_callback(audio_doa_handle_t doa_handle, audio_doa_callback_t cb, void *ctx)
{
    if (doa_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    audio_doa_t *doa = (audio_doa_t *)doa_handle;
    doa->cb = cb;
    doa->ctx = ctx;
    return ESP_OK;
}

esp_err_t audio_doa_start(audio_doa_handle_t doa_handle)
{
    if (doa_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    audio_doa_t *doa = (audio_doa_t *)doa_handle;
    if (doa->doa_handle == NULL) {
        return ESP_FAIL;
    }
    doa->state = AUDIO_DOA_STATE_RUNNING;
    xEventGroupSetBits(doa->event_group, START_BIT);
    return ESP_OK;
}

esp_err_t audio_doa_stop(audio_doa_handle_t doa_handle)
{
    if (doa_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    audio_doa_t *doa = (audio_doa_t *)doa_handle;
    doa->state = AUDIO_DOA_STATE_IDLE;
    xEventGroupClearBits(doa->event_group, START_BIT);
    return ESP_OK;
}

esp_err_t audio_doa_data_write(audio_doa_handle_t doa_handle, uint8_t *data, int data_size)
{
    if (doa_handle == NULL || data == NULL || data_size <= 0) {
        return ESP_ERR_INVALID_ARG;
    }
    audio_doa_t *doa = (audio_doa_t *)doa_handle;

    if (doa->data_bus == NULL) {
        return ESP_FAIL;
    }
    esp_gmf_data_bus_block_t blk = {0};
    blk.buf = data;
    blk.buf_length = data_size;
    blk.valid_size = data_size;
    esp_gmf_err_io_t ret = esp_gmf_db_acquire_write(doa->data_bus, &blk, data_size, pdMS_TO_TICKS(10));
    if (ret < 0) {
        return ESP_FAIL;
    }
    ret = esp_gmf_db_release_write(doa->data_bus, &blk, pdMS_TO_TICKS(10));
    if (ret < 0) {
        return ESP_FAIL;
    }
    return ESP_OK;
}
