/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/gui_gsp/bundle.hpp"
#include "private/desktop.hpp"
#include <new>

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

std::expected<std::shared_ptr<Bundle>, std::string> Bundle::load(
    size_t size, const std::function<std::expected<void, std::string>(std::span<std::byte>)> &reader
)
{
    if ((size == 0) || !reader) {
        return std::unexpected("GSP deployable bundle size or reader is invalid");
    }
    std::unique_ptr<std::byte[]> bytes(new (std::nothrow) std::byte[size]);
    if (!bytes) {
        return std::unexpected("Unable to allocate GSP deployable bundle buffer");
    }
    auto result = reader(std::span<std::byte>(bytes.get(), size));
    if (!result) {
        return std::unexpected(result.error());
    }
    return load(std::span<const std::byte>(bytes.get(), size));
}

const esp_gsp_config_t &Bundle::config() const
{
    return config_;
}
const esp_gsp_deployable_info_t &Bundle::info() const
{
    return info_;
}

bool Bundle::has_scene(uint16_t scene_id) const
{
    for (const auto &scene : desktop_bundle_->metadata.at("scenes").as_array()) {
        if (scene.as_object().at("scene_id").to_number<uint16_t>() == scene_id) {
            return true;
        }
    }
    return false;
}

std::expected<std::shared_ptr<const BundleContract>, std::string> Bundle::snapshot_contract() const
{
    try {
        auto contract = std::make_shared<BundleContract>();
        const auto &scenes = desktop_bundle_->metadata.at("scenes").as_array();
        contract->scenes_.reserve(scenes.size());
        for (const auto &scene_value : scenes) {
            const auto &scene_object = scene_value.as_object();
            BundleContract::Scene scene;
            scene.id = scene_object.at("scene_id").to_number<uint16_t>();
            const auto &objects = scene_object.at("objects").as_array();
            scene.components.reserve(objects.size());
            for (const auto &object_value : objects) {
                const auto &object = object_value.as_object();
                BundleContract::Component component;
                component.key = object.at("component_key").to_number<uint32_t>();
                const auto &properties = object.at("properties").as_array();
                component.properties.reserve(properties.size());
                for (const auto &value : properties) {
                    const auto &entry = value.as_object();
                    const std::string kind(entry.at("value_kind").as_string());
                    BundleContract::Property property;
                    property.info.key = entry.at("property_key").to_number<uint32_t>();
                    property.kind = property.info.key == desktop_name_key("text") ? GSP_COMPONENT_PROP_TEXT :
                                    property.info.key == desktop_name_key("value") ? GSP_COMPONENT_PROP_VALUE : UINT8_MAX;
                    property.scalar = true;
                    if (kind == "i32") {
                        property.info.value_type = GSP_VALUE_I32;
                        property.info.storage = GSP_PROPERTY_STORAGE_I32;
                    } else if (kind == "u32") {
                        property.info.value_type = GSP_VALUE_U32;
                        property.info.storage = GSP_PROPERTY_STORAGE_U32;
                    } else if (kind == "color") {
                        property.info.value_type = GSP_VALUE_COLOR;
                        property.info.storage = GSP_PROPERTY_STORAGE_COLOR;
                    } else if (kind == "bool") {
                        property.info.value_type = GSP_VALUE_BOOL;
                        property.info.storage = GSP_PROPERTY_STORAGE_VISIBLE;
                    } else {
                        property.scalar = false;
                    }
                    property.info.min_value = entry.at("min").to_number<int64_t>();
                    property.info.max_value = entry.at("max").to_number<int64_t>();
                    component.properties.push_back(property);
                }
                scene.components.push_back(std::move(component));
            }
            contract->scenes_.push_back(std::move(scene));
        }
        return std::shared_ptr<const BundleContract>(std::move(contract));
    } catch (const std::bad_alloc &) {
        return std::unexpected("Unable to allocate GSP scene metadata");
    }
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
