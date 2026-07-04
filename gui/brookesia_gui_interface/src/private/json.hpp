/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <string>

#include "boost/json.hpp"
#include "brookesia/gui_interface/enums.hpp"
#include "brookesia/lib_utils/describe_helpers.hpp"

namespace esp_brookesia::gui {

template <typename T>
inline void normalize_enum_field(boost::json::object &object, boost::json::string_view key)
{
    auto it = object.find(key);
    if (it == object.end() || !it->value().is_string()) {
        return;
    }

    T value {};
    const auto enum_name = boost::json::value_to<std::string>(it->value());
    if (BROOKESIA_DESCRIBE_STR_TO_ENUM_FLEXIBLE(enum_name, value)) {
        it->value() = BROOKESIA_DESCRIBE_ENUM_TO_STR(value);
    }
}

inline void normalize_node_json(boost::json::object &node)
{
    normalize_enum_field<NodeType>(node, "type");

    auto layout_it = node.find("layout");
    if (layout_it != node.end() && layout_it->value().is_object()) {
        auto &layout = layout_it->value().as_object();
        normalize_enum_field<LayoutType>(layout, "type");
        normalize_enum_field<FlexFlow>(layout, "flex_flow");
        normalize_enum_field<Align>(layout, "main_align");
        normalize_enum_field<Align>(layout, "cross_align");
    }

    auto placement_it = node.find("placement");
    if (placement_it != node.end() && placement_it->value().is_object()) {
        auto &placement = placement_it->value().as_object();
        normalize_enum_field<PlacementMode>(placement, "mode");
        normalize_enum_field<PlacementAlign>(placement, "align");
        normalize_enum_field<Align>(placement, "align_self");
    }

    auto events_it = node.find("events");
    if (events_it != node.end() && events_it->value().is_array()) {
        for (auto &event : events_it->value().as_array()) {
            if (event.is_object()) {
                auto &event_object = event.as_object();
                normalize_enum_field<EventType>(event_object, "type");
                auto effects_it = event_object.find("effects");
                if (effects_it != event_object.end() && effects_it->value().is_array()) {
                    for (auto &effect : effects_it->value().as_array()) {
                        if (effect.is_object()) {
                            normalize_enum_field<EventEffectType>(effect.as_object(), "type");
                        }
                    }
                }
            }
        }
    }

    auto children_it = node.find("children");
    if (children_it != node.end() && children_it->value().is_array()) {
        for (auto &child : children_it->value().as_array()) {
            if (child.is_object()) {
                normalize_node_json(child.as_object());
            }
        }
    }
}

inline void normalize_document_json(boost::json::value &document)
{
    if (!document.is_object()) {
        return;
    }

    auto &object = document.as_object();
    auto root_it = object.find("root");
    if (root_it != object.end() && root_it->value().is_object()) {
        normalize_node_json(root_it->value().as_object());
    }
}

} // namespace esp_brookesia::gui
