/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <memory>
#include <cstdlib>
#include "lvgl.h"
#include "style/esp_brookesia_gui_style.hpp"

namespace esp_brookesia::gui {

class LvAnimation {
public:
    struct UserData {
        LvAnimation *animation;
        void *user_data;
    };

    using VariableExecutionMethod = std::function<void(void *variable, int value)>;
    using CompletedMethod = std::function<void(void *user_data)>;

    LvAnimation();
    ~LvAnimation();

    void setStyleAttribute(const StyleAnimation &attribute) const;
    void setVariableExecutionMethod(void *variable, VariableExecutionMethod method) const;
    void setCompletedMethod(CompletedMethod method) const;
    void setUserData(void *user_data) const;

    bool start(void) const;
    bool stop(void) const;

    bool isRunning(void) const;

private:
    mutable lv_anim_t _native{};
    mutable UserData _user_data{};
    mutable VariableExecutionMethod _execution_method{nullptr};
    mutable CompletedMethod _completed_method{nullptr};
};

using LvAnimationUniquePtr = std::unique_ptr<LvAnimation>;
using LvAnimationSharedPtr = std::shared_ptr<LvAnimation>;

} // namespace esp_brookesia::gui
