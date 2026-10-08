/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <expected>
#include <span>
#include <string_view>

#include "brookesia/system_core.hpp"

namespace esp_brookesia::gsp_app {

class LauncherApp final: public system::core::IApp {
public:
    std::expected<void, std::string> on_install(system::core::AppContext &context) override;
    void on_uninstall(system::core::AppContext &context) override;
    system::core::AppManifest get_manifest() const override;
    system::core::AppGuiDescriptor get_gui_descriptor() const override;
    std::span<const std::byte> gui_bundle() const override;
    const void *gui_runtime_config() const override;
    std::expected<void, std::string> on_start(system::core::AppContext &context) override;
    std::expected<void, std::string> on_action(
        system::core::AppContext &context,
        std::string_view action
    ) override;

private:
    system::core::AppContext *installed_context_ = nullptr;
};

} // namespace esp_brookesia::gsp_app
