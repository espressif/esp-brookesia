/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "brookesia/gui_interface/examples/runner.hpp"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "brookesia/lib_utils.hpp"
#include "../common/example_base.hpp"

namespace esp_brookesia::gui::examples {

namespace {

constexpr std::string_view LOG_TAG = "ExampleRunner";
constexpr std::string_view MENU_SCREEN_PATH = "/example_menu";
constexpr std::string_view BACK_SCREEN_PATH = "/example_back_overlay";
constexpr std::string_view RUN_ACTION_PREFIX = "ex.run.";
constexpr std::string_view BACK_ACTION = "ex.back";

void append_escaped(std::string &out, std::string_view value)
{
    for (const char ch : value) {
        switch (ch) {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\n':
            out += "\\n";
            break;
        default:
            out.push_back(ch);
            break;
        }
    }
}

// Node ids must not contain '.' (reserved for placement.relativeTo view paths). Example ids and
// categories use dots/spaces, so build a safe id for menu nodes while keeping the raw id for actions.
void append_sanitized_id(std::string &out, std::string_view value)
{
    for (const char ch : value) {
        const bool ok = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                        (ch >= '0' && ch <= '9') || ch == '_' || ch == '-';
        out.push_back(ok ? ch : '_');
    }
}

const char *BACK_OVERLAY_JSON = R"({
    "version": "0.1.0",
    "assets": [
        {
            "type": "viewScreen",
            "id": "example_back_overlay",
            "commonProps": {
                "clickable": false,
                "scrollable": false
            },
            "children": [
                {
                    "type": "button",
                    "id": "back",
                    "events": [
                        {
                            "type": "clicked",
                            "action": "ex.back"
                        }
                    ],
                    "style": {
                        "bgColor": "#0f172a",
                        "radius": "22dp",
                        "borderWidth": "2dp",
                        "borderColor": "#38bdf8",
                        "padding": "0dp"
                    },
                    "placement": {
                        "mode": "absolute",
                        "x": "12dp",
                        "y": "12dp",
                        "width": "44dp",
                        "height": "44dp"
                    },
                    "children": [
                        {
                            "type": "label",
                            "id": "icon",
                            "labelProps": {
                                "text": "<"
                            },
                            "style": {
                                "textColor": "#e2e8f0",
                                "fontSize": "22sp"
                            },
                            "placement": {
                                "mode": "relative",
                                "align": "center"
                            }
                        }
                    ]
                }
            ]
        }
    ]
})";

} // namespace

ExampleRunner::ExampleRunner(Runtime &runtime, Environment environment)
    : runtime_(runtime)
    , environment_(std::move(environment))
    , menu_screen_path_(MENU_SCREEN_PATH)
    , back_screen_path_(BACK_SCREEN_PATH)
{
}

ExampleRunner::~ExampleRunner()
{
    menu_connections_.clear();
    back_connection_.disconnect();
    if (active_example_) {
        active_example_->stop(runtime_);
        active_example_.reset();
    }
    if (back_document_.is_valid()) {
        runtime_.unload(back_document_);
    }
    if (menu_document_.is_valid()) {
        runtime_.unload(menu_document_);
    }
}

std::size_t ExampleRunner::example_count() const
{
    return ExampleRegistry::get_plugin_count();
}

std::string ExampleRunner::build_menu_json() const
{
    // category -> [(id, title)] (sorted within category by id thanks to get_all_instances ordering).
    std::map<std::string, std::vector<std::pair<std::string, std::string>>> grouped;
    for (auto &[name, instance] : ExampleRegistry::get_all_instances()) {
        if (!instance) {
            continue;
        }
        const auto info = instance->info();
        grouped[info.category].emplace_back(info.id, info.title);
    }

    std::string json;
    json.reserve(4096);
    json += R"({"version":"0.1.0","assets":[{"type":"viewScreen","id":"example_menu",)";
    json += R"("commonProps":{"scrollable":true},)";
    json += R"("style":{"bgColor":"#0b1220","padding":"14dp"},)";
    json += R"("layout":{"type":"flex","flexFlow":"column","mainAlign":"start","crossAlign":"center","gap":"6dp"},)";
    json += R"("children":[)";

    bool first_child = true;
    auto add_comma = [&]() {
        if (!first_child) {
            json += ',';
        }
        first_child = false;
    };

    // Title banner.
    add_comma();
    json += R"({"type":"label","id":"banner","labelProps":{"text":"JSON-UI Examples"},)";
    json += R"("style":{"textColor":"#f8fafc","fontSize":"22sp"},)";
    json += R"("placement":{"width":"360dp","height":"34dp"}})";

    for (auto &[category, entries] : grouped) {
        add_comma();
        json += R"({"type":"label","id":"cat_)";
        append_sanitized_id(json, category);
        json += R"(","labelProps":{"text":")";
        append_escaped(json, category);
        json += R"("},"style":{"textColor":"#38bdf8","fontSize":"16sp"},)";
        json += R"("placement":{"width":"360dp","height":"26dp"}})";

        for (auto &[id, title] : entries) {
            add_comma();
            json += R"({"type":"button","id":"btn_)";
            append_sanitized_id(json, id);
            json += R"(","events":[{"type":"clicked","action":")";
            json += RUN_ACTION_PREFIX;
            append_escaped(json, id);
            json += R"("}],"style":{"bgColor":"#1e293b","radius":"8dp","borderWidth":"0dp"},)";
            json += R"("placement":{"width":"360dp","height":"40dp"},)";
            json += R"("children":[{"type":"label","id":"label","labelProps":{"text":")";
            append_escaped(json, title);
            json += R"("},"style":{"textColor":"#e2e8f0","fontSize":"15sp"},)";
            json += R"("placement":{"mode":"relative","align":"center"}}]})";
        }
    }

    json += R"(]}]})";
    return json;
}

std::expected<void, std::string> ExampleRunner::show_menu()
{
    auto mounted = runtime_.mount_screen(menu_document_, menu_screen_path_);
    if (!mounted) {
        return std::unexpected(mounted.error());
    }
    menu_mounted_ = true;
    return {};
}

std::expected<void, std::string> ExampleRunner::mount_back_overlay()
{
    MountTarget target;
    target.layer = GuiLayer::Top;
    auto mounted = runtime_.mount_screen(back_document_, back_screen_path_, target);
    if (!mounted) {
        return std::unexpected(mounted.error());
    }
    back_mounted_ = true;
    return {};
}

std::expected<void, std::string> ExampleRunner::start()
{
    const auto menu_json = build_menu_json();
    auto menu_loaded = runtime_.load_json(
                           "examples/menu/root.json", menu_json, detail::examples_asset_dir(), environment_);
    if (!menu_loaded) {
        return std::unexpected("Failed to load menu document: " + menu_loaded.error());
    }
    menu_document_ = *menu_loaded;

    auto back_loaded = runtime_.load_json(
                           "examples/menu/back.json", BACK_OVERLAY_JSON, detail::examples_asset_dir(), environment_);
    if (!back_loaded) {
        return std::unexpected("Failed to load back overlay document: " + back_loaded.error());
    }
    back_document_ = *back_loaded;

    // Subscribe one launch action per example.
    for (auto &[name, instance] : ExampleRegistry::get_all_instances()) {
        if (!instance) {
            continue;
        }
        const auto id = instance->info().id;
        std::string action(RUN_ACTION_PREFIX);
        action += id;
        auto connection = runtime_.subscribe_event_action(
        menu_document_, action, [this, id](const Event &) {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            pending_kind_ = PendingKind::RunExample;
            pending_example_id_ = id;
        });
        menu_connections_.push_back(std::move(connection));
    }

    back_connection_ = runtime_.subscribe_event_action(
    back_document_, std::string(BACK_ACTION), [this](const Event &) {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        pending_kind_ = PendingKind::ReturnToMenu;
    });

    auto shown = show_menu();
    if (!shown) {
        return std::unexpected("Failed to mount menu screen: " + shown.error());
    }

    BROOKESIA_LOGI("[%1%] Started with %2% example(s)", LOG_TAG, example_count());
    return {};
}

std::expected<void, std::string> ExampleRunner::run_example(const std::string &id)
{
    auto instance = ExampleRegistry::get_instance(id);
    if (!instance) {
        return std::unexpected("Unknown example: " + id);
    }

    if (menu_mounted_) {
        runtime_.unmount_screen(menu_document_, menu_screen_path_);
        menu_mounted_ = false;
    }

    auto ran = instance->run(runtime_, environment_);
    if (!ran) {
        // Restore the menu so the user is not stuck on a blank screen.
        (void)show_menu();
        return std::unexpected("Example '" + id + "' failed: " + ran.error());
    }

    active_example_ = std::move(instance);

    auto overlay = mount_back_overlay();
    if (!overlay) {
        BROOKESIA_LOGW("[%1%] Failed to mount back overlay: %2%", LOG_TAG, overlay.error());
    }
    BROOKESIA_LOGI("[%1%] Running example '%2%'", LOG_TAG, id);
    return {};
}

void ExampleRunner::return_to_menu()
{
    if (back_mounted_) {
        runtime_.unmount_screen(back_document_, back_screen_path_);
        back_mounted_ = false;
    }
    if (active_example_) {
        active_example_->stop(runtime_);
        active_example_.reset();
    }
    auto shown = show_menu();
    if (!shown) {
        BROOKESIA_LOGE("[%1%] Failed to restore menu: %2%", LOG_TAG, shown.error());
    }
}

void ExampleRunner::process_pending()
{
    PendingKind kind = PendingKind::None;
    std::string id;
    {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        kind = pending_kind_;
        id = pending_example_id_;
        pending_kind_ = PendingKind::None;
        pending_example_id_.clear();
    }

    switch (kind) {
    case PendingKind::RunExample: {
        // Ignore launch requests while an example is already active.
        if (active_example_) {
            break;
        }
        auto ran = run_example(id);
        if (!ran) {
            BROOKESIA_LOGE("[%1%] %2%", LOG_TAG, ran.error());
        }
        break;
    }
    case PendingKind::ReturnToMenu:
        if (active_example_ || back_mounted_) {
            return_to_menu();
        }
        break;
    case PendingKind::None:
        break;
    }
}

} // namespace esp_brookesia::gui::examples
