/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "lvgl.h"
#include "esp_brookesia.hpp"

namespace esp_brookesia::apps {

class Calculator: public systems::speaker::App {
public:
    Calculator();
    ~Calculator();

    // Core app interface methods
    bool run(void) override;
    bool back(void) override;
    bool close(void) override;
    bool init(void) override;
    bool deinit(void) override;

    // Calculator logic methods
    bool isStartZero(void);
    bool isStartNum(void);
    bool isStartPercent(void);
    bool isLegalDot(void);
    double calculate(const char *input);

    int formula_len;
    lv_obj_t *keyboard;
    lv_obj_t *history_label;
    lv_obj_t *formula_label;
    lv_obj_t *result_label;
    uint16_t _height;
    uint16_t _width;

protected:
    bool isStarting(void) const
    {
        return _is_starting;
    }
    bool isStopping(void) const
    {
        return _is_stopping;
    }

private:
    static void keyboard_event_cb(lv_event_t *e);

    std::atomic<bool> _is_starting = false;
    std::atomic<bool> _is_stopping = false;
};

} // namespace esp_brookesia::apps
