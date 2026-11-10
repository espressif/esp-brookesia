/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "esp_gsp_deployable.h"

namespace esp_brookesia::gui::gsp {

struct DesktopBundle;

/** @brief Compact scene and component metadata independent of renderer and bundle bytes. */
class BundleContract {
public:
    bool has_scene(uint16_t scene_id) const;
    bool has_component_property(uint16_t scene_id, uint32_t component, gsp_component_prop_kind_t property) const;
    std::expected<gsp_property_info_t, std::string> property_info(
        uint16_t scene_id, uint32_t component, uint32_t property
    ) const;

private:
    friend class Bundle;
    struct Property {
        uint8_t kind = 0;
        bool scalar = false;
        gsp_property_info_t info{};
    };
    struct Component {
        uint32_t key = 0;
        std::vector<Property> properties;
    };
    struct Scene {
        uint16_t id = 0;
        std::vector<Component> components;
    };
    std::vector<Scene> scenes_;
};

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

    /** @brief Read directly into the bundle's aligned storage and validate it. */
    static std::expected<std::shared_ptr<Bundle>, std::string> load(
        size_t size, const std::function<std::expected<void, std::string>(std::span<std::byte>)> &reader
    );

    /** @brief Configuration backed by this object's owned bytes. */
    const esp_gsp_config_t &config() const;

    /** @brief Parsed package requirements. */
    const esp_gsp_deployable_info_t &info() const;

    /** @brief Resolve a scene-local action ID to its authored-name key. */
    std::expected<uint32_t, std::string> action_key(uint16_t scene_id, uint16_t action_id) const;

    /** @brief Check whether the bundle contains a scene. */
    bool has_scene(uint16_t scene_id) const;

    /** @brief Copy scene metadata so the bundle bytes can be released while a session is suspended. */
    std::expected<std::shared_ptr<const BundleContract>, std::string> snapshot_contract() const;

private:
    Bundle() = default;
#if defined(BROOKESIA_GUI_GSP_DESKTOP)
    std::shared_ptr<const DesktopBundle> desktop_bundle_;
#endif

    std::byte *bytes_ = nullptr;
    esp_gsp_deployable_bundle_t *deployable_ = nullptr;
    esp_gsp_config_t config_{};
    esp_gsp_deployable_info_t info_{};
};

} // namespace esp_brookesia::gui::gsp
