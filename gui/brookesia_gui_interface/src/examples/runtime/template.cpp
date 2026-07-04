/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * runtime.template: create_view instances then destroy_view one.
 */

#include <string>

#include "brookesia/gui_interface/examples/example.hpp"
#include "common/example_base.hpp"

namespace esp_brookesia::gui::examples {

#if BROOKESIA_GUI_INTERFACE_EXAMPLE_RUNTIME_TEMPLATE
class RuntimeTemplateExample final : public detail::JsonExample {
public:
    RuntimeTemplateExample()
        : JsonExample(
              ExampleInfo{"runtime.template", "Runtime", "Template create/destroy", "create_view instances then destroy_view one"},
              "examples/runtime/template.json",
              R"json({
    "version": "0.1.0",
    "assets": [
        {
            "type": "viewTemplate",
            "id": "row",
            "node": {
                "type": "container",
                "style": { "bgColor": "#1e293b", "radius": "8dp", "padding": "10dp" },
                "placement": { "mode": "flow", "width": "240dp", "height": "40dp" },
                "children": [
                    {
                        "type": "label",
                        "id": "title",
                        "bindings": { "labelProps.text": "title" },
                        "labelProps": { "text": "row" },
                        "style": { "textColor": "#e2e8f0" },
                        "placement": { "mode": "relative", "align": "leftMid" }
                    }
                ]
            }
        },
        {
            "type": "viewScreen",
            "id": "runtime_template",
            "style": { "bgColor": "#0f172a" },
            "children": [
                {
                    "type": "container",
                    "id": "list",
                    "style": { "bgColor": "#0f172a", "padding": "0dp", "borderWidth": "0dp" },
                    "layout": { "type": "flex", "flexFlow": "column", "gap": "10dp", "mainAlign": "center", "crossAlign": "center" },
                    "placement": { "mode": "relative", "align": "center", "width": "280dp", "height": "220dp" }
                }
            ]
        }
    ]
})json",
              "/runtime_template")
    {
    }

protected:
    std::expected<void, std::string> on_mounted(Runtime &runtime, const Environment &, DocumentId document) override
    {
        for (int i = 1; i <= 3; ++i) {
            const std::string instance = "row_" + std::to_string(i);
            auto created = runtime.create_view(document, "row", "/runtime_template/list", instance);
            if (!created) {
                return std::unexpected("create " + instance + " failed: " + created.error());
            }
            runtime.set_binding_value(
                document, "/runtime_template/list/" + instance + "/title", "title", "Instance #" + std::to_string(i));
        }
        // Demonstrate destroy_view by removing the middle instance.
        runtime.destroy_view(document, "/runtime_template/list/row_2");
        return {};
    }

    bool on_verify(detail::Checker &checker, Runtime &, const Environment &, DocumentId) override
    {
        // row_1 and row_3 remain after create_view; row_2 was destroyed; bindings drove the titles.
        checker.require_view("/runtime_template/list/row_1");
        checker.require_view("/runtime_template/list/row_3");
        checker.check(!checker.view("/runtime_template/list/row_2").valid(),
                      "expected row_2 to be destroyed but it is still present");
        checker.check_label_text("/runtime_template/list/row_1/title", "Instance #1");
        return true;
    }
};
BROOKESIA_PLUGIN_REGISTER(IExample, RuntimeTemplateExample, "runtime.template");
#endif

} // namespace esp_brookesia::gui::examples
