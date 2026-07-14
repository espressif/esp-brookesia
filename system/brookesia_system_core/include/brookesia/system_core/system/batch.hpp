/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "boost/json.hpp"
#include "brookesia/gui_interface.hpp"

namespace esp_brookesia::system::core {

struct GuiBatchCommand {
    enum class Type {
        SetBindings,
        SetViewSrc,
        StopAnimation,
        StartViewAnimation,
        ScrollTo,
        ScrollToView,
    };

    Type type = Type::SetBindings;
    std::vector<gui::BindingValueUpdate> binding_updates;
    std::string path;
    std::string src;
    gui::SubscriptionId animation_id = 0;
    gui::Animation animation;
    int32_t scroll_x = 0;
    int32_t scroll_y = 0;
    bool animated = true;
};

struct GuiBatchCommandResult {
    std::string name;
    bool success = false;
    std::string error_message;
    boost::json::object data;
};

struct GuiBatchResult {
    bool success = false;
    std::vector<GuiBatchCommandResult> results;
};

} // namespace esp_brookesia::system::core
