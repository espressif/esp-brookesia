/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * document.environment: dp/sp units scaled by density and font scale.
 */

#include "brookesia/gui_interface/examples/example.hpp"
#include "common/example_base.hpp"

namespace esp_brookesia::gui::examples {

#if BROOKESIA_GUI_INTERFACE_EXAMPLE_DOCUMENT_ENVIRONMENT
class DocumentEnvironmentExample final : public detail::JsonExample {
public:
    DocumentEnvironmentExample()
        : JsonExample(
              ExampleInfo{"document.environment", "Document", "Environment Units", "dp/sp units scaled by density and font scale"},
              "examples/document/environment.json",
              R"json({
    "version": "0.1.0",
    "assets": [
        {
            "type": "viewScreen",
            "id": "document_environment",
            "style": { "bgColor": "#0f172a" },
            "layout": { "type": "flex", "flexFlow": "column", "mainAlign": "center", "crossAlign": "center", "gap": "12dp" },
            "children": [
                {
                    "type": "label",
                    "id": "sp_text",
                    "labelProps": { "text": "Text sized in sp (font scale aware)" },
                    "style": { "textColor": "#e2e8f0", "fontSize": "16sp" },
                    "placement": { "width": "300dp", "height": "28dp" }
                },
                {
                    "type": "container",
                    "id": "dp_box",
                    "style": { "bgColor": "#1e293b", "borderColor": "#38bdf8", "borderWidth": "2dp", "radius": "6dp" },
                    "placement": { "width": "160dp", "height": "60dp" }
                }
            ]
        }
    ]
})json",
              "/document_environment")
    {
    }

protected:
    bool on_verify(detail::Checker &checker, Runtime &, const Environment &, DocumentId) override
    {
        // sp text resolves through font scale; the dp box geometry through density. A non-empty frame
        // on both confirms the unit pipeline produced concrete pixels.
        checker.check_label_text("sp_text", "Text sized in sp (font scale aware)");
        checker.check_frame_nonempty("sp_text");
        checker.check_frame_nonempty("dp_box");
        return true;
    }
};
BROOKESIA_PLUGIN_REGISTER(IExample, DocumentEnvironmentExample, "document.environment");
#endif

} // namespace esp_brookesia::gui::examples
