/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#define BROOKESIA_LOG_TAG "GspLauncher"
#include "brookesia/lib_utils/log.hpp"

#include <cstdio>
#include <string>

#define GSP_BUNDLE_ENABLE_RAW_IDS 1
#include "launcher_gsp.h"

#include "launcher_app.hpp"
#include "brookesia/gui_gsp/app_host.hpp"

namespace esp_brookesia::gsp_app {

esp_gsp_config_t launcher_dark_config();

std::expected<void, std::string> LauncherApp::on_install(system::core::AppContext &context)
{
    installed_context_ = &context;
    return {};
}

void LauncherApp::on_uninstall(system::core::AppContext &context)
{
    (void)context;
    installed_context_ = nullptr;
}

system::core::AppManifest LauncherApp::get_manifest() const
{
    system::core::AppManifest manifest;
    manifest.id = "gsp_launcher";
    manifest.name = "Home";
    manifest.kind = system::core::AppKind::Native;
    manifest.visible = true;
    return manifest;
}

system::core::AppGuiDescriptor LauncherApp::get_gui_descriptor() const
{
    system::core::AppGuiDescriptor descriptor;
    descriptor.root_kind = system::core::GuiRootKind::GspBundle;
    return descriptor;
}

std::span<const std::byte> LauncherApp::gui_bundle() const
{
    const auto config = *static_cast<const esp_gsp_config_t *>(gui_runtime_config());
    return {reinterpret_cast<const std::byte *>(config.bundle), config.bundle_size};
}

const void *LauncherApp::gui_runtime_config() const
{
    static esp_gsp_config_t config;
    const bool dark = installed_context_ != nullptr &&
                      installed_context_->gui().get_theme_language().theme_id == "dark";
    config = dark ? launcher_dark_config() : gsp_launcher_config();
    return &config;
}

std::expected<void, std::string> LauncherApp::on_start(system::core::AppContext &context)
{
    const auto active = context.gui().get_theme_language();
    auto *host = gui::gsp::AppHost::current();
    if (!host || !host->handle()) {
        return std::unexpected("Launcher GSP session is unavailable");
    }
    const auto handle = host->handle();
    const bool chinese = active.language == "zh_CN";
    if (esp_gsp_component_set_visible(handle, GSP_LAUNCHER_OBJ_KEY_SUBTITLE, !chinese) != ESP_GSP_OK ||
            esp_gsp_component_set_visible(handle, GSP_LAUNCHER_OBJ_KEY_SUBTITLE_ZH, chinese) != ESP_GSP_OK ||
            esp_gsp_component_set_visible(handle, GSP_LAUNCHER_OBJ_KEY_OPEN_SETTINGS, !chinese) != ESP_GSP_OK ||
            esp_gsp_component_set_visible(handle, GSP_LAUNCHER_OBJ_KEY_OPEN_SETTINGS_ZH, chinese) != ESP_GSP_OK) {
        return std::unexpected("Failed to apply launcher appearance");
    }
    return {};
}

std::expected<void, std::string> LauncherApp::on_action(system::core::AppContext &context, std::string_view action)
{
    if (action.starts_with("scene:")) {
        return {};
    }
    unsigned scene = 0;
    unsigned id = 0;
    unsigned arg = 0;
    const std::string text(action);
    if (std::sscanf(text.c_str(), "%u:%u:%u", &scene, &id, &arg) != 3) {
        return std::unexpected("Unrecognized GSP action");
    }
    (void)scene;
    (void)arg;
    if (id != GSP_LAUNCHER_ACT_ID_OPEN_SETTINGS) {
        BROOKESIA_LOGI("Ignore launcher action %1%", id);
        return {};
    }
    BROOKESIA_LOGI("Opening Settings");
    return context.system_service().stop_app(context.app_id());
}

} // namespace esp_brookesia::gsp_app
