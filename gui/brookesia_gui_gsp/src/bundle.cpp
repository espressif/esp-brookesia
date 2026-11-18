/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <utility>

#include "esp_heap_caps.h"

#include "brookesia/gui_gsp/bundle.hpp"

namespace esp_brookesia::gui::gsp {

namespace {

constexpr size_t BUNDLE_ALIGNMENT = 64;

} // namespace

Bundle::~Bundle()
{
    // The deployable handle borrows bytes_, so release metadata first.
    if (deployable_ != nullptr) {
        esp_gsp_deployable_bundle_close(deployable_);
    }
    if (bytes_ != nullptr) {
        heap_caps_free(bytes_);
    }
}

std::expected<std::shared_ptr<Bundle>, std::string> Bundle::load(std::span<const std::byte> bytes)
{
    if (bytes.empty() || (bytes.data() == nullptr)) {
        return std::unexpected("GSP deployable bundle is empty or null");
    }

    return load(bytes.size(), [bytes](std::span<std::byte> target) -> std::expected<void, std::string> {
        std::memcpy(target.data(), bytes.data(), bytes.size());
        return {};
    });
}

std::expected<std::shared_ptr<Bundle>, std::string> Bundle::load(
    size_t size, const std::function<std::expected<void, std::string>(std::span<std::byte>)> &reader
)
{
    if ((size == 0) || !reader) {
        return std::unexpected("GSP deployable bundle size or reader is invalid");
    }
    std::unique_ptr<Bundle> bundle(new (std::nothrow) Bundle());
    if (!bundle) {
        return std::unexpected("Unable to allocate GSP deployable bundle metadata");
    }
    bundle->bytes_ = static_cast<std::byte *>(
                         heap_caps_aligned_alloc(BUNDLE_ALIGNMENT, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
                     );
    if (bundle->bytes_ == nullptr) {
        bundle->bytes_ = static_cast<std::byte *>(
                             heap_caps_aligned_alloc(BUNDLE_ALIGNMENT, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)
                         );
    }
    if (bundle->bytes_ == nullptr) {
        return std::unexpected("Unable to allocate 64-byte-aligned memory for GSP deployable bundle");
    }
    auto read_result = reader(std::span<std::byte>(bundle->bytes_, size));
    if (!read_result) {
        return std::unexpected(read_result.error());
    }

    const esp_gsp_err_t open_result = esp_gsp_deployable_bundle_open(
                                          bundle->bytes_, size, true, &bundle->deployable_
                                      );
    if ((open_result != ESP_GSP_OK) || (bundle->deployable_ == nullptr)) {
        return std::unexpected(
                   "Invalid GSP deployable bundle (CRC, structure, or ABI validation failed; code " +
                   std::to_string(static_cast<int>(open_result)) + ")"
               );
    }

    const esp_gsp_err_t info_result = esp_gsp_deployable_bundle_get_info(bundle->deployable_, &bundle->info_);
    if (info_result != ESP_GSP_OK) {
        return std::unexpected("Unable to read GSP deployable info; code " +
                               std::to_string(static_cast<int>(info_result)));
    }

    const esp_gsp_err_t config_result = esp_gsp_deployable_bundle_make_config(bundle->deployable_, &bundle->config_);
    if (config_result != ESP_GSP_OK) {
        return std::unexpected("Unable to create GSP deployable config; code " +
                               std::to_string(static_cast<int>(config_result)));
    }

    return std::shared_ptr<Bundle>(bundle.release());
}

bool Bundle::has_scene(uint16_t scene_id) const
{
    return scene_id < info_.scene_count;
}

std::expected<std::shared_ptr<const BundleContract>, std::string> Bundle::snapshot_contract() const
{
    try {
        auto contract = std::make_shared<BundleContract>();
        contract->scenes_.reserve(info_.scene_count);
        for (uint16_t scene_id = 0; scene_id < info_.scene_count; ++scene_id) {
            BundleContract::Scene scene;
            scene.id = scene_id;
            const auto *directory = (config_.directories != nullptr && scene_id < config_.directory_count)
                                    ? config_.directories[scene_id] : nullptr;
            if (directory != nullptr) {
                scene.components.reserve(directory->count);
                for (uint16_t i = 0; i < directory->count; ++i) {
                    const auto &entry = directory->entries[i];
                    BundleContract::Component component;
                    component.key = entry.key;
                    component.properties.reserve(entry.prop_count);
                    for (uint8_t j = 0; j < entry.prop_count; ++j) {
                        const auto &prop = entry.props[j];
                        BundleContract::Property property;
                        property.kind = prop.prop;
                        property.scalar = prop.storage != GSP_PROPERTY_STORAGE_TEXT &&
                                          prop.storage != GSP_PROPERTY_STORAGE_RESOURCE;
                        property.info.key = prop.key;
                        property.info.value_type = static_cast<gsp_value_type_t>(prop.value_type);
                        property.info.storage = static_cast<gsp_property_storage_t>(prop.storage);
                        property.info.codec = static_cast<gsp_property_codec_t>(prop.codec);
                        property.info.min_value = property.info.value_type == GSP_VALUE_I32 ?
                                                  static_cast<int64_t>(static_cast<int32_t>(prop.min_value)) : prop.min_value;
                        property.info.max_value = property.info.value_type == GSP_VALUE_I32 ?
                                                  static_cast<int64_t>(static_cast<int32_t>(prop.max_value)) : prop.max_value;
                        component.properties.push_back(property);
                    }
                    scene.components.push_back(std::move(component));
                }
            }
            contract->scenes_.push_back(std::move(scene));
        }
        return std::shared_ptr<const BundleContract>(std::move(contract));
    } catch (const std::bad_alloc &) {
        return std::unexpected("Unable to allocate GSP scene metadata");
    }
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
    uint32_t key = 0;
    const esp_gsp_err_t result = esp_gsp_deployable_bundle_action_key(
                                     deployable_, scene_id, action_id, &key
                                 );
    if (result != ESP_GSP_OK) {
        return std::unexpected(
                   "Unable to resolve GSP action key for scene " + std::to_string(scene_id) + ", action " +
                   std::to_string(action_id) + "; code " + std::to_string(static_cast<int>(result))
               );
    }
    return key;
}

} // namespace esp_brookesia::gui::gsp
