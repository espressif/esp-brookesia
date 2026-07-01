/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * document.constants: a constant tree referenced via ${...} and ${expr(...)}.
 */

#include "brookesia/gui_interface/examples/example.hpp"
#include "common/example_base.hpp"

namespace esp_brookesia::gui::examples {

#if BROOKESIA_GUI_INTERFACE_EXAMPLE_DOCUMENT_CONSTANTS
class DocumentConstantsExample final : public detail::JsonExample {
public:
    DocumentConstantsExample()
        : JsonExample(
              ExampleInfo{"document.constants", "Document", "Constants + expr", "Constant tree referenced via ${...} and ${expr(...)}"},
              "examples/document/constants.json",
              R"json({
    "version": "0.1.0",
    "assets": [
        {
            "type": "constant",
            "data": {
                "text": { "title": "Constants & Expressions" },
                "colors": { "title": "#38bdf8" },
                "metrics": { "boxX": "16dp", "boxY": "24dp", "boxWidth": "160dp", "boxInset": "20dp", "scale": 2 }
            }
        },
        {
            "type": "viewScreen",
            "id": "document_constants",
            "style": { "bgColor": "#0f172a" },
            "children": [
                {
                    "type": "label",
                    "id": "title",
                    "labelProps": { "text": "${constant.text.title}" },
                    "style": { "textColor": "${constant.colors.title}", "fontSize": "20sp" },
                    "placement": { "mode": "absolute", "x": "${constant.metrics.boxX}", "y": "${constant.metrics.boxY}" }
                },
                {
                    "type": "container",
                    "id": "expr_box",
                    "style": { "bgColor": "#1e293b", "radius": "8dp" },
                    "placement": {
                        "mode": "absolute",
                        "x": "${constant.metrics.boxX}",
                        "y": "${expr(${constant.metrics.boxY} * ${constant.metrics.scale})}",
                        "width": "${expr(${constant.metrics.boxWidth} - ${constant.metrics.boxInset})}",
                        "height": "${expr(40dp + 8dp)}"
                    }
                }
            ]
        }
    ]
})json",
              "/document_constants")
    {
    }

protected:
    bool on_verify(detail::Checker &checker, Runtime &, const Environment &, DocumentId) override
    {
        // The title text resolves from ${constant.text.title}; the box geometry comes from
        // constant/expr metrics, so a non-empty frame confirms the expression pipeline ran.
        checker.check_label_text("title", "Constants & Expressions");
        checker.check_frame_nonempty("expr_box");
        return true;
    }
};
BROOKESIA_PLUGIN_REGISTER(IExample, DocumentConstantsExample, "document.constants");
#endif

} // namespace esp_brookesia::gui::examples
