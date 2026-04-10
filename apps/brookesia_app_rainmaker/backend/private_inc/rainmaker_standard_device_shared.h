/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "rainmaker_standard_device.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t rm_standard_device_shared_set_power(struct rm_device_item *device, bool power_on, const char **out_err_reason);
const void *rm_standard_device_shared_get_icon_src(rm_app_device_type_t type, bool power_on);

#ifdef __cplusplus
}
#endif
