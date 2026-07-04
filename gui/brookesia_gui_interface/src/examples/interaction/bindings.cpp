/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * interaction.bindings: runtime binding values applied to leaf fields.
 */

#include "brookesia/gui_interface/examples/example.hpp"
#include "common/example_base.hpp"

namespace esp_brookesia::gui::examples {

#if BROOKESIA_GUI_INTERFACE_EXAMPLE_INTERACTION_BINDINGS
class InteractionBindingsExample final : public detail::JsonExample {
public:
    InteractionBindingsExample()
        : JsonExample(
              ExampleInfo{"interaction.bindings", "Interaction", "Bindings", "Runtime binding values applied to leaf fields"},
              "examples/interaction/bindings.json",
              R"json({
    "version": "0.1.0",
    "assets": [
        {
            "type": "viewScreen",
            "id": "interaction_bindings",
            "style": { "bgColor": "#0f172a" },
            "layout": { "type": "flex", "flexFlow": "column", "mainAlign": "center", "crossAlign": "center", "gap": "16dp" },
            "children": [
                {
                    "type": "label",
                    "id": "status",
                    "bindings": { "labelProps.text": "status" },
                    "labelProps": { "text": "(unbound)" },
                    "style": { "textColor": "#e2e8f0", "fontSize": "18sp" },
                    "placement": { "width": "260dp", "height": "28dp" }
                },
                {
                    "type": "progressBar",
                    "id": "bar",
                    "bindings": { "rangeProps.value": "progress" },
                    "rangeProps": { "value": 0, "min": 0, "max": 100 },
                    "partStyles": { "indicator": { "bgColor": "#22c55e" } },
                    "placement": { "width": "240dp", "height": "20dp" }
                }
            ]
        }
    ]
})json",
              "/interaction_bindings")
    {
    }

protected:
    std::expected<void, std::string> on_mounted(Runtime &runtime, const Environment &, DocumentId document) override
    {
        runtime.set_binding_values(document, {
            {"/interaction_bindings/status", "status", "Bound via set_binding_values"},
            {"/interaction_bindings/bar", "progress", "72"},
        });
        return {};
    }

    bool on_verify(detail::Checker &checker, Runtime &, const Environment &, DocumentId) override
    {
        // Verify the bound values round-trip and the label reflects the bound text.
        checker.check_binding("status", "status", "Bound via set_binding_values");
        checker.check_binding("bar", "progress", "72");
        checker.check_label_text("status", "Bound via set_binding_values");
        checker.check_progress_value("bar", 72);
        return true;
    }
};
BROOKESIA_PLUGIN_REGISTER(IExample, InteractionBindingsExample, "interaction.bindings");
#endif

} // namespace esp_brookesia::gui::examples
