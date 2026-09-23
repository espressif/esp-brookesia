/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <cstring>
#include <memory>
#include <string>

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

    std::unique_ptr<Bundle> bundle(new Bundle());
    bundle->bytes_ = static_cast<std::byte *>(
                         heap_caps_aligned_alloc(BUNDLE_ALIGNMENT, bytes.size(), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
                     );
    if (bundle->bytes_ == nullptr) {
        bundle->bytes_ = static_cast<std::byte *>(
                             heap_caps_aligned_alloc(BUNDLE_ALIGNMENT, bytes.size(), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)
                         );
    }
    if (bundle->bytes_ == nullptr) {
        return std::unexpected("Unable to allocate 64-byte-aligned memory for GSP deployable bundle");
    }
    std::memcpy(bundle->bytes_, bytes.data(), bytes.size());

    const esp_gsp_err_t open_result = esp_gsp_deployable_bundle_open(
                                          bundle->bytes_, bytes.size(), true, &bundle->deployable_
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
