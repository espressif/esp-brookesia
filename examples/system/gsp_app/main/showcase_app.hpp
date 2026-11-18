/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <atomic>
#include <cstddef>
#include <expected>
#include <span>
#include <string_view>

#include "brookesia/hal_interface/interfaces/display/backlight.hpp"
#include "brookesia/system_core.hpp"

namespace esp_brookesia::gsp_app {

class ShowcaseApp final: public system::core::IApp {
public:
    explicit ShowcaseApp(hal::display::BacklightIface *backlight);

    system::core::AppManifest get_manifest() const override;
    system::core::AppGuiDescriptor get_gui_descriptor() const override;
    std::span<const std::byte> gui_bundle() const override;
    const void *gui_runtime_config() const override;
    std::expected<void, std::string> on_start(system::core::AppContext &context) override;
    std::expected<void, std::string> on_action(
        system::core::AppContext &context, std::string_view action
    ) override;

    static bool exit_requested();

private:
    hal::display::BacklightIface *backlight_ = nullptr;
    static std::atomic<bool> exit_requested_;
};

} // namespace esp_brookesia::gsp_app
