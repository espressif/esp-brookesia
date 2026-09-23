/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <string>

#include "esp_gsp_deployable.h"

namespace esp_brookesia::gui::gsp {

/**
 * @brief Owns validated, aligned deployable GSP bytes and their parsed metadata.
 *
 * Keep this object alive until the GSP session has stopped successfully. The
 * configuration and parsed handle both borrow the owned bytes.
 */
class Bundle {
public:
    ~Bundle();

    Bundle(const Bundle &) = delete;
    Bundle &operator=(const Bundle &) = delete;

    /** @brief Copy and validate a compiled GSP deployable with CRC checking. */
    static std::expected<std::shared_ptr<Bundle>, std::string> load(std::span<const std::byte> bytes);

    /** @brief Configuration backed by this object's owned bytes. */
    const esp_gsp_config_t &config() const;

    /** @brief Parsed package requirements. */
    const esp_gsp_deployable_info_t &info() const;

    /** @brief Resolve a scene-local action ID to its authored-name key. */
    std::expected<uint32_t, std::string> action_key(uint16_t scene_id, uint16_t action_id) const;

private:
    Bundle() = default;

    std::byte *bytes_ = nullptr;
    esp_gsp_deployable_bundle_t *deployable_ = nullptr;
    esp_gsp_config_t config_{};
    esp_gsp_deployable_info_t info_{};
};

} // namespace esp_brookesia::gui::gsp
