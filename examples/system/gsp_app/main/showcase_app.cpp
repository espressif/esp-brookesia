/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#define BROOKESIA_LOG_TAG "GspShowcase"
#include "brookesia/lib_utils/log.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>

#include "brookesia/gui_gsp/app_host.hpp"
#include "showcase_app.hpp"
#include "showcase_glue.h"

namespace esp_brookesia::gsp_app {

namespace {

bool parse_action(std::string_view action, unsigned &scene, unsigned &id, unsigned &arg)
{
    unsigned parsed_scene = 0;
    unsigned parsed_id = 0;
    unsigned parsed_arg = 0;
    const std::string text(action);
    if (std::sscanf(text.c_str(), "%u:%u:%u", &parsed_scene, &parsed_id, &parsed_arg) != 3) {
        return false;
    }
    scene = parsed_scene;
    id = parsed_id;
    arg = parsed_arg;
    return true;
}

} // namespace

std::atomic<bool> ShowcaseApp::exit_requested_{false};

ShowcaseApp::ShowcaseApp(hal::display::BacklightIface *backlight)
    : backlight_(backlight)
{
}

system::core::AppManifest ShowcaseApp::get_manifest() const
{
    system::core::AppManifest manifest;
    manifest.id = "gsp_showcase";
    manifest.name = "GSP Showcase";
    manifest.kind = system::core::AppKind::Native;
    manifest.visible = true;
    return manifest;
}

system::core::AppGuiDescriptor ShowcaseApp::get_gui_descriptor() const
{
    system::core::AppGuiDescriptor descriptor;
    descriptor.root_kind = system::core::GuiRootKind::GspBundle;
    return descriptor;
}

std::span<const std::byte> ShowcaseApp::gui_bundle() const
{
    size_t size = 0;
    const uint8_t *bytes = showcase_bundle_bytes(&size);
    return {reinterpret_cast<const std::byte *>(bytes), size};
}

const void *ShowcaseApp::gui_runtime_config() const
{
    return showcase_runtime_config();
}

std::expected<void, std::string> ShowcaseApp::on_start(system::core::AppContext &)
{
    exit_requested_.store(false);
    auto *host = gui::gsp::AppHost::current();
    if ((host == nullptr) || (host->handle() == nullptr)) {
        return std::unexpected("GSP session is not running");
    }
    if (!showcase_prepare(host->handle())) {
        return std::unexpected("Failed to prepare the showcase session");
    }
    BROOKESIA_LOGI("Showcase app started through system core");
    return {};
}

std::expected<void, std::string> ShowcaseApp::on_action(system::core::AppContext &context, std::string_view action)
{
    if (action.starts_with("scene:")) {
        unsigned scene = 0;
        if (std::sscanf(std::string(action).c_str(), "scene:%u", &scene) != 1) {
            return std::unexpected("Unrecognized GSP scene");
        }
        auto *host = gui::gsp::AppHost::current();
        if ((host == nullptr) || (host->handle() == nullptr)) {
            return std::unexpected("GSP session is not running");
        }
        if (!showcase_enter_scene(host->handle(), scene)) {
            return std::unexpected("Failed to enter GSP scene");
        }
        return {};
    }

    unsigned scene = 0;
    unsigned id = 0;
    unsigned arg = 0;
    if (!parse_action(action, scene, id, arg)) {
        return std::unexpected("Unrecognized GSP action");
    }

    if (showcase_scene_is_command(scene) && showcase_action_is_exit(id)) {
        auto stopped = context.system_service().stop_app(context.app_id());
        if (!stopped) {
            return stopped;
        }
        exit_requested_.store(true);
        BROOKESIA_LOGI("Exit requested");
        return {};
    }

    if (showcase_scene_is_command(scene) && showcase_action_is_brightness(id)) {
        const int brightness = static_cast<int>(arg);
        if ((backlight_ == nullptr) || !backlight_->set_brightness(static_cast<uint8_t>(brightness))) {
            return std::unexpected("Failed to set backlight");
        }
        auto *host = gui::gsp::AppHost::current();
        if ((host == nullptr) || (host->handle() == nullptr)) {
            return std::unexpected("GSP session is not running");
        }
        char text[48];
        std::snprintf(text, sizeof(text), "Brightness %d", brightness);
        if (!showcase_set_brightness_text(host->handle(), text)) {
            return std::unexpected("Failed to publish brightness");
        }
        BROOKESIA_LOGI("Backlight set to %1%", brightness);
        return {};
    }

    if (showcase_scene_is_command(scene) && showcase_action_is_quick(id)) {
        auto *host = gui::gsp::AppHost::current();
        if ((host == nullptr) || (host->handle() == nullptr)) {
            return std::unexpected("GSP session is not running");
        }
        if (!showcase_toggle_quick_settings(host->handle())) {
            return std::unexpected("Failed to toggle quick settings");
        }
        return {};
    }

    auto *host = gui::gsp::AppHost::current();
    if ((host == nullptr) || (host->handle() == nullptr)) {
        return std::unexpected("GSP session is not running");
    }
    if (showcase_scene_is_lab(scene) && showcase_action_is_lab_trigger(id)) {
        if (!showcase_handle_lab_trigger(host->handle())) {
            return std::unexpected("Failed to toggle Lab components");
        }
        return {};
    }
    if (showcase_scene_is_lab(scene) && showcase_action_is_modal_choice(id)) {
        if (!showcase_handle_modal_choice(host->handle(), arg)) {
            return std::unexpected("Failed to update Lab confirmation");
        }
        return {};
    }

    BROOKESIA_LOGI("GSP action scene %1% id %2% arg %3%", scene, id, arg);
    return {};
}

bool ShowcaseApp::exit_requested()
{
    return exit_requested_.load();
}

} // namespace esp_brookesia::gsp_app
