/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/gui_gsp/bundle.hpp"

namespace esp_brookesia::gui::gsp {

bool BundleContract::has_scene(uint16_t scene_id) const
{
    for (const auto &scene : scenes_) {
        if (scene.id == scene_id) {
            return true;
        }
    }
    return false;
}

bool BundleContract::has_component_property(
    uint16_t scene_id, uint32_t component, gsp_component_prop_kind_t property
) const
{
    for (const auto &scene : scenes_) {
        if (scene.id != scene_id) {
            continue;
        }
        for (const auto &item : scene.components) {
            if (item.key != component) {
                continue;
            }
            for (const auto &prop : item.properties) {
                if (prop.kind == static_cast<uint8_t>(property)) {
                    return true;
                }
            }
        }
    }
    return false;
}

std::expected<gsp_property_info_t, std::string> BundleContract::property_info(
    uint16_t scene_id, uint32_t component, uint32_t property
) const
{
    for (const auto &scene : scenes_) {
        if (scene.id != scene_id) {
            continue;
        }
        for (const auto &item : scene.components) {
            if (item.key != component) {
                continue;
            }
            for (const auto &prop : item.properties) {
                if (prop.scalar && prop.info.key == property && property != 0) {
                    return prop.info;
                }
            }
        }
    }
    return std::unexpected("GSP component or property not found in scene " + std::to_string(scene_id));
}

} // namespace esp_brookesia::gui::gsp
