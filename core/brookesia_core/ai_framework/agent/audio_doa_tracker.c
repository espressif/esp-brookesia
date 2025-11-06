/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "esp_log.h"
#include "audio_doa_tracker.h"

#define TAG                    "DOA_TRACKER"
#define DOA_BUF_SIZE           128
#define DISCARD_RATIO          0.1f  // Discard top and bottom 10% each
#define MIN_SAMPLES_FOR_UPDATE 3     // Minimum number of samples for stable initial value

struct doa_tracker {
    float  filtered_angle;
    float  last_sent_angle;
    float  slow_alpha;
    float  fast_alpha;
    float  large_diff_deg;
    int    update_interval_ms;

    bool  vad_active;
    bool  vad_just_started;

    float  doa_buf[DOA_BUF_SIZE];
    int    doa_cnt;

    esp_timer_handle_t  timer;
    doa_update_cb_t     on_update;
    void               *user_ctx;
};

/*---------------------------------- Utility Functions ----------------------------------*/
static float compute_robust_mean(float *buf, int n)
{
    if (n <= 0) {
        return 0;
    }

    float tmp[DOA_BUF_SIZE];
    memcpy(tmp, buf, sizeof(float) * n);

    // Sort
    for (int i = 0; i < n - 1; i++) {
        for (int j = i + 1; j < n; j++) {
            if (tmp[i] > tmp[j]) {
                float t = tmp[i];
                tmp[i] = tmp[j];
                tmp[j] = t;
            }
        }
    }

    int discard = (int)(n * DISCARD_RATIO);
    if (discard * 2 >= n) {
        discard = 0;
    }

    float sum = 0;
    int cnt = 0;
    for (int i = discard; i < n - discard; i++) {
        sum += tmp[i];
        cnt++;
    }

    return (cnt > 0) ? (sum / cnt) : tmp[n / 2];
}

/*---------------------------------- Timer Callback ----------------------------------*/
static void doa_timer_cb(void *arg)
{
    doa_tracker_t *t = (doa_tracker_t *)arg;
    if (!t->vad_active || t->doa_cnt == 0) {
        return;
    }

    // Wait for more data if sample count is too low
    if (t->doa_cnt < MIN_SAMPLES_FOR_UPDATE && !t->vad_just_started) {
        return;
    }

    float sample = compute_robust_mean(t->doa_buf, t->doa_cnt);
    if (sample < 0) {
        sample = 0;
    }
    if (sample > 180) {
        sample = 180;
    }

    float diff = fabsf(sample - t->filtered_angle);
    float alpha = t->slow_alpha;

    if (t->vad_just_started) {
        t->vad_just_started = false;
        t->filtered_angle = sample;
        t->last_sent_angle = sample;
        if (t->on_update) {
            t->on_update(sample, t->user_ctx);
        }
        ESP_LOGI(TAG, "VAD start: quick sync %.1f°", sample);
        return;
    } else if (diff >= t->large_diff_deg) {
        // If the difference is large, need to determine if it's a real change or noise
        // If sample count is sufficient, it's likely a real change, use fast response
        // If sample count is low, it's likely noise, use more conservative update
        if (t->doa_cnt >= MIN_SAMPLES_FOR_UPDATE * 2) {
            alpha = t->fast_alpha;  // Sufficient samples, fast response to real change
        } else {
            // Low sample count, likely noise, use medium speed update to avoid being biased
            alpha = (t->slow_alpha + t->fast_alpha) * 0.5f;
        }
    }

    t->filtered_angle = alpha * sample + (1 - alpha) * t->filtered_angle;

    if (fabsf(t->filtered_angle - t->last_sent_angle) >= (t->large_diff_deg * 0.5f)) {
        t->last_sent_angle = t->filtered_angle;
        if (t->on_update) {
            t->on_update(t->filtered_angle, t->user_ctx);
        }
        ESP_LOGD(TAG, "DOA update: %.1f° (sample: %.1f°, diff: %.1f°)",
                 t->filtered_angle, sample, diff);
    }
}

/*---------------------------------- External Interface ----------------------------------*/
doa_tracker_t *doa_tracker_create(doa_update_cb_t cb, void *user_ctx)
{
    // Use default configuration
    doa_tracker_config_t default_config = {
        .slow_alpha = 0.15f,
        .fast_alpha = 0.6f,
        .large_diff_deg = 12.0f,
        .update_interval_ms = 400,
    };
    return doa_tracker_create_with_config(&default_config, cb, user_ctx);
}

doa_tracker_t *doa_tracker_create_with_config(const doa_tracker_config_t *config, doa_update_cb_t cb, void *user_ctx)
{
    doa_tracker_t *t = calloc(1, sizeof(doa_tracker_t));
    if (!t) {
        return NULL;
    }

    // Use configuration parameters, use default values if config is NULL
    if (config) {
        t->slow_alpha = config->slow_alpha;
        t->fast_alpha = config->fast_alpha;
        t->large_diff_deg = config->large_diff_deg;
        t->update_interval_ms = config->update_interval_ms;
    } else {
        // Default configuration
        t->slow_alpha = 0.15f;
        t->fast_alpha = 0.6f;
        t->large_diff_deg = 12.0f;
        t->update_interval_ms = 400;
    }

    t->on_update = cb;
    t->user_ctx = user_ctx;
    t->doa_cnt = 0;

    const esp_timer_create_args_t timer_args = {
        .callback = doa_timer_cb,
        .arg = t,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "doa_timer"
    };
    esp_timer_create(&timer_args, &t->timer);
    return t;
}

void doa_tracker_destroy(doa_tracker_t *t)
{
    if (!t) {
        return;
    }
    esp_timer_stop(t->timer);
    esp_timer_delete(t->timer);
    free(t);
}

void doa_tracker_start(doa_tracker_t *t)
{
    if (!t) {
        return;
    }
    esp_timer_start_periodic(t->timer, t->update_interval_ms * 1000);
}

void doa_tracker_stop(doa_tracker_t *t)
{
    if (!t) {
        return;
    }
    esp_timer_stop(t->timer);
}

void doa_tracker_set_vad_state(doa_tracker_t *t, bool active)
{
    if (!t) {
        return;
    }
    if (active && !t->vad_active) {
        t->vad_active = true;
        t->vad_just_started = true;
        t->doa_cnt = 0;
        // Clear buffer to ensure no old data affects the result
        memset(t->doa_buf, 0, sizeof(t->doa_buf));
    } else if (!active) {
        t->vad_active = false;
    }
}

void doa_tracker_feed(doa_tracker_t *t, float doa_angle)
{
    if (!t) {
        return;
    }
    if (doa_angle < 0) {
        doa_angle = 0;
    }
    if (doa_angle > 180) {
        doa_angle = 180;
    }

    // Only collect data when VAD is active
    if (!t->vad_active) {
        return;
    }

    if (t->doa_cnt < DOA_BUF_SIZE) {
        t->doa_buf[t->doa_cnt++] = doa_angle;
    } else {
        memmove(&t->doa_buf[0], &t->doa_buf[1], sizeof(float) * (DOA_BUF_SIZE - 1));
        t->doa_buf[DOA_BUF_SIZE - 1] = doa_angle;
    }
}
