/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include "esp_timer.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Opaque handle for DOA tracker instance
 */
typedef struct doa_tracker doa_tracker_t;

/**
 * @brief  Callback function type for DOA angle updates
 *
 * @param  angle     Filtered DOA angle in degrees (0-180)
 * @param  user_ctx  User-defined context pointer
 */
typedef void (*doa_update_cb_t)(float angle, void *user_ctx);

/**
 * @brief  Configuration structure for DOA tracker
 */
typedef struct {
    float  slow_alpha;          /**< Smoothing coefficient for normal mode (default: 0.15) */
    float  fast_alpha;          /**< Smoothing coefficient for fast mode (default: 0.6) */
    float  large_diff_deg;      /**< Large angle change threshold in degrees (default: 12.0) */
    int    update_interval_ms;  /**< Update interval in milliseconds (default: 400) */
} doa_tracker_config_t;

/**
 * @brief  Create a DOA tracker with default configuration
 *
 * @param  cb        Callback function to be called when angle is updated
 * @param  user_ctx  User-defined context pointer passed to callback
 * @return
 */
doa_tracker_t *doa_tracker_create(doa_update_cb_t cb, void *user_ctx);

/**
 * @brief  Create a DOA tracker with custom configuration
 *
 * @param  config    Configuration structure. If NULL, default values will be used.
 * @param  cb        Callback function to be called when angle is updated
 * @param  user_ctx  User-defined context pointer passed to callback
 * @return
 */
doa_tracker_t *doa_tracker_create_with_config(const doa_tracker_config_t *config, doa_update_cb_t cb, void *user_ctx);

/**
 * @brief  Destroy a DOA tracker instance
 *
 * @param  t  Tracker instance to destroy
 */
void doa_tracker_destroy(doa_tracker_t *t);

/**
 * @brief  Start the DOA tracker timer
 *
 *         Starts periodic timer to process accumulated DOA samples and update filtered angle.
 *
 * @param  t  Tracker instance
 */
void doa_tracker_start(doa_tracker_t *t);

/**
 * @brief  Stop the DOA tracker timer
 *
 * @param  t  Tracker instance
 */
void doa_tracker_stop(doa_tracker_t *t);

/**
 * @brief  Set VAD (Voice Activity Detection) state
 *
 *         When VAD becomes active, tracker resets accumulated samples and performs
 *         quick synchronization to the current angle. This ensures accurate initial
 *         angle when voice activity starts.
 *
 * @param  t       Tracker instance
 * @param  active  True to activate VAD, false to deactivate
 */
void doa_tracker_set_vad_state(doa_tracker_t *t, bool active);

/**
 * @brief  Feed a new DOA angle sample to the tracker
 *
 *         This function should be called whenever a new DOA angle measurement is available.
 *         The tracker will accumulate samples and process them periodically according to
 *         the configured update interval.
 *
 * @param  t          Tracker instance
 * @param  doa_angle  DOA angle in degrees (0-180). Values outside this range will be clamped.
 */
void doa_tracker_feed(doa_tracker_t *t, float doa_angle);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
