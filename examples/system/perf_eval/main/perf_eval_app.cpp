/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0 */
#include "perf_eval_app.hpp"

#include "esp_log.h"

using esp_brookesia::system::core::AppManifest;
using esp_brookesia::system::core::AppGuiDescriptor;
using esp_brookesia::system::core::AppContext;
using esp_brookesia::system::core::AppKind;
using esp_brookesia::system::core::GuiRootKind;
using esp_brookesia::system::core::GuiAppLayer;

namespace esp_brookesia {
namespace perf_eval {

static const char *APP_TAG = "perf_eval_app";

AppManifest PerfEvalApp::get_manifest() const
{
    AppManifest manifest;
    manifest.id = "perf_eval_bench";
    manifest.name = "Perf Eval Bench";
    manifest.version = "0.1.0";
    manifest.kind = AppKind::Native;
    manifest.visible = false;
    // Absolute path so resolve_app_resource_dir() passes it through untouched; the
    // littlefs corpus is materialised by the harness, not installed under the
    // framework's app storage root.
    manifest.resource_dir = resource_dir_;
    return manifest;
}

AppGuiDescriptor PerfEvalApp::get_gui_descriptor() const
{
    AppGuiDescriptor descriptor;
    descriptor.root_kind = GuiRootKind::JsonString;
    descriptor.root = root_json_;
    descriptor.screen_flows.push_back({
        .screen_flow = "settings_content",
        .layer = GuiAppLayer::AppDefault,
        .mount_mode = esp_brookesia::gui::MountStackMode::Replace,
        .z_order = 0,
    });
    return descriptor;
}

std::expected<void, std::string> PerfEvalApp::on_start(AppContext &)
{
    // start_screen_flow already mounted the flow's initial screen (settings_home)
    // before on_start runs. The harness moves this document to /debug through
    // System::gui_trigger_screen_flow with the wildcard settings.open.debug action;
    // the flow JSON only declares that transition with from: [], so no document id
    // is needed here and none is exposed to apps either.
    running_.store(true, std::memory_order_release);
    ESP_LOGI(APP_TAG, "started; settings_content flow live on app document");
    return {};
}

std::expected<void, std::string> PerfEvalApp::on_stop(AppContext &)
{
    running_.store(false, std::memory_order_release);
    return {};
}

} // namespace perf_eval
} // namespace esp_brookesia
