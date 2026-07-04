/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/system_super/macro_configs.h"
#if !BROOKESIA_SYSTEM_SUPER_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/system_super/system.hpp"

#include <filesystem>
#include <string>

namespace esp_brookesia::system::super {
namespace {

std::string make_component_version(int major, int minor, int patch)
{
    return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
}

std::string make_super_system_resource_path(std::string_view path)
{
    std::filesystem::path resource_path(path);
    if (resource_path.is_absolute()) {
        return resource_path.generic_string();
    }
    return (std::filesystem::path("super") / resource_path).generic_string();
}

} // namespace

std::expected<void, std::string> System::init()
{
    return init(Config{});
}

std::expected<void, std::string> System::init(Config config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    config.core_config.system_type = BROOKESIA_SYSTEM_SUPER_SYSTEM_TYPE;
    config.core_config.startup_overlay.enabled = BROOKESIA_SYSTEM_SUPER_ENABLE_STARTUP_OVERLAY;
    config.core_config.startup_overlay.root_path =
        make_super_system_resource_path(BROOKESIA_SYSTEM_SUPER_STARTUP_ROOT_JSON);
    config.core_config.startup_overlay.screen_path = "/startup";
    config.core_config.startup_overlay.target = {
        .display_id = {},
        .layer = gui::GuiLayer::System,
    };
    config.core_config.app_launch_transition.enabled = false;
    if (!config.core_config.gui_backend) {
        return std::unexpected("Super system requires a GUI backend");
    }
    return core::System::init(std::move(config.core_config));
}

core::SystemInfo System::on_get_system_info() const
{
    return {
        .name = BROOKESIA_SYSTEM_SUPER_SYSTEM_NAME,
        .version = make_component_version(
            BROOKESIA_SYSTEM_SUPER_VER_MAJOR,
            BROOKESIA_SYSTEM_SUPER_VER_MINOR,
            BROOKESIA_SYSTEM_SUPER_VER_PATCH
        ),
    };
}

} // namespace esp_brookesia::system::super
