/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "boost/json.hpp"
#include "brookesia/gui_interface/enums.hpp"
#include "brookesia/gui_interface/handles.hpp"
#include "brookesia/gui_interface/macro_configs.h"
#include "brookesia/lib_utils/describe_helpers.hpp"

namespace esp_brookesia::gui {

struct Event {
    DocumentId document_id;
    std::string root_id;
    std::string node_id;
    std::string path;
    EventType type = EventType::Clicked;
    std::string action;
    boost::json::object payload;

    bool has_payload_key(std::string_view key) const
    {
        return payload.if_contains(key) != nullptr;
    }

    std::optional<bool> get_bool(std::string_view key) const
    {
        auto *value = payload.if_contains(key);
        if (value == nullptr || !value->is_bool()) {
            return std::nullopt;
        }
        return value->as_bool();
    }

    std::optional<int64_t> get_int64(std::string_view key) const
    {
        auto *value = payload.if_contains(key);
        if (value == nullptr) {
            return std::nullopt;
        }
        if (value->is_int64()) {
            return value->as_int64();
        }
        if (value->is_uint64()) {
            const auto unsigned_value = value->as_uint64();
            if (unsigned_value <= static_cast<uint64_t>(INT64_MAX)) {
                return static_cast<int64_t>(unsigned_value);
            }
        }
        return std::nullopt;
    }

    std::optional<double> get_double(std::string_view key) const
    {
        auto *value = payload.if_contains(key);
        if (value == nullptr) {
            return std::nullopt;
        }
        if (value->is_double()) {
            return value->as_double();
        }
        if (value->is_int64()) {
            return static_cast<double>(value->as_int64());
        }
        if (value->is_uint64()) {
            return static_cast<double>(value->as_uint64());
        }
        return std::nullopt;
    }

    std::optional<std::string_view> get_string(std::string_view key) const
    {
        auto *value = payload.if_contains(key);
        if (value == nullptr || !value->is_string()) {
            return std::nullopt;
        }
        return std::string_view(value->as_string().c_str(), value->as_string().size());
    }

    const boost::json::value *get_json(std::string_view key) const
    {
        return payload.if_contains(key);
    }
};

BROOKESIA_DESCRIBE_STRUCT(Event, (), (document_id, root_id, node_id, path, type, action, payload))

} // namespace esp_brookesia::gui
