/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/gui_gsp/bundle.hpp"
#include "private/desktop.hpp"

namespace esp_brookesia::gui::gsp {

Bundle::~Bundle() = default;

std::expected<std::shared_ptr<Bundle>, std::string> Bundle::load(std::span<const std::byte> bytes)
{
    auto registered = find_desktop_bundle(bytes);
    if (!registered) {
        return std::unexpected("Desktop GSP requires an exact registered bundle and matching GSPC sidecar");
    }
    std::shared_ptr<Bundle> result(new Bundle());
    result->desktop_bundle_ = std::move(registered);
    result->config_ = ESP_GSP_CONFIG_INIT();
    result->config_.bundle = result->desktop_bundle_->bytes.data();
    result->config_.bundle_size = result->desktop_bundle_->bytes.size();
    result->info_.struct_size = sizeof(result->info_);
    result->info_.scene_count = result->desktop_bundle_->metadata.at("scenes").as_array().size();
    return result;
}

const esp_gsp_config_t &Bundle::config() const
{
    return config_;
}
const esp_gsp_deployable_info_t &Bundle::info() const
{
    return info_;
}

std::expected<uint32_t, std::string> Bundle::action_key(uint16_t scene_id, uint16_t action_id) const
{
    for (const auto &scene_value : desktop_bundle_->metadata.at("scenes").as_array()) {
        const auto &scene = scene_value.as_object();
        if (scene.at("scene_id").to_number<uint16_t>() != scene_id) {
            continue;
        }
        for (const auto &callback_value : scene.at("callbacks").as_array()) {
            const auto &callback = callback_value.as_object();
            if (callback.at("action_id").to_number<uint16_t>() == action_id) {
                return desktop_name_key(callback.at("name").as_string());
            }
        }
    }
    return std::unexpected("GSP action is absent from the registered compiler metadata");
}
} // namespace esp_brookesia::gui::gsp
