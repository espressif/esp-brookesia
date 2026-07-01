/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

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
        ScrollToView,
    };

    Type type = Type::SetBindings;
    std::vector<gui::BindingValueUpdate> binding_updates;
    std::string path;
    std::string src;
    gui::SubscriptionId animation_id = 0;
    gui::Animation animation;
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
