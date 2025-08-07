/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include "esp_err.h"
#include "iot_button.h"

#if defined(__cplusplus)

class TouchSensor {
public:
    TouchSensor();
    ~TouchSensor();

    bool init();

    button_handle_t get_button_handle();

private:
    void (*_callback)(int);
    int _min;
    int _max;
};

#endif
