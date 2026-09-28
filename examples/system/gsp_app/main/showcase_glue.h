/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

const uint8_t *showcase_bundle_bytes(size_t *size);
const void *showcase_runtime_config(void);
int showcase_prepare(void *ui);
int showcase_set_brightness_text(void *ui, const char *text);
int showcase_toggle_quick_settings(void *ui);
int showcase_scene_is_command(unsigned scene);
int showcase_action_is_exit(unsigned id);
int showcase_action_is_brightness(unsigned id);
int showcase_action_is_quick(unsigned id);
int showcase_scene_is_lab(unsigned scene);
int showcase_action_is_lab_trigger(unsigned id);
int showcase_action_is_modal_choice(unsigned id);
int showcase_handle_lab_trigger(void *ui);
int showcase_handle_modal_choice(void *ui, unsigned arg);
int showcase_enter_scene(void *ui, unsigned scene);

#ifdef __cplusplus
}
#endif
