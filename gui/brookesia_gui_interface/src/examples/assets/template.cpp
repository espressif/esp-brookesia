/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * assets.template: instantiate a viewTemplate at runtime.
 */

#include "brookesia/gui_interface/examples/example.hpp"
#include "common/example_base.hpp"

namespace esp_brookesia::gui::examples {

#if BROOKESIA_GUI_INTERFACE_EXAMPLE_ASSETS_TEMPLATE
class AssetsTemplateExample final : public detail::JsonExample {
public:
    AssetsTemplateExample()
        : JsonExample(
              ExampleInfo{"assets.template", "Assets", "viewTemplate", "Instantiate a viewTemplate at runtime"},
              "examples/assets/template.json",
              R"json({
    "version": "0.1.0",
    "assets": [
        {
            "type": "viewTemplate",
            "id": "badge",
            "node": {
                "type": "container",
                "style": { "bgColor": "#1e293b", "radius": "8dp", "padding": "10dp" },
                "placement": { "mode": "flow", "width": "220dp", "height": "40dp" },
                "children": [
                    {
                        "type": "label",
                        "id": "title",
                        "bindings": { "labelProps.text": "title" },
                        "labelProps": { "text": "Badge" },
                        "style": { "textColor": "#e2e8f0" },
                        "placement": { "mode": "relative", "align": "leftMid" }
                    }
                ]
            }
        },
        {
            "type": "viewScreen",
            "id": "assets_template",
            "style": { "bgColor": "#0f172a" },
            "children": [
                {
                    "type": "container",
                    "id": "list",
                    "style": { "bgColor": "#0f172a", "padding": "0dp", "borderWidth": "0dp" },
                    "layout": { "type": "flex", "flexFlow": "column", "gap": "10dp", "mainAlign": "center", "crossAlign": "center" },
                    "placement": { "mode": "relative", "align": "center", "width": "260dp", "height": "200dp" }
                }
            ]
        }
    ]
})json",
              "/assets_template")
    {
    }

protected:
    std::expected<void, std::string> on_mounted(Runtime &runtime, const Environment &, DocumentId document) override
    {
        auto first = runtime.create_view(document, "badge", "/assets_template/list", "badge_1");
        if (!first) {
            return std::unexpected("create badge_1 failed: " + first.error());
        }
        auto second = runtime.create_view(document, "badge", "/assets_template/list", "badge_2");
        if (!second) {
            return std::unexpected("create badge_2 failed: " + second.error());
        }
        runtime.set_binding_value(document, "/assets_template/list/badge_1/title", "title", "Template instance #1");
        runtime.set_binding_value(document, "/assets_template/list/badge_2/title", "title", "Template instance #2");
        return {};
    }

    bool on_verify(detail::Checker &checker, Runtime &, const Environment &, DocumentId) override
    {
        // Both template instances must exist and reflect the bindings set in on_mounted.
        checker.require_view("/assets_template/list/badge_1");
        checker.require_view("/assets_template/list/badge_2");
        checker.check_label_text("/assets_template/list/badge_1/title", "Template instance #1");
        checker.check_label_text("/assets_template/list/badge_2/title", "Template instance #2");
        return true;
    }
};
BROOKESIA_PLUGIN_REGISTER(IExample, AssetsTemplateExample, "assets.template");
#endif

} // namespace esp_brookesia::gui::examples
