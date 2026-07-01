/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * styling.style_refs: compose multiple named styles on one node.
 */

#include "brookesia/gui_interface/examples/example.hpp"
#include "common/example_base.hpp"

namespace esp_brookesia::gui::examples {

#if BROOKESIA_GUI_INTERFACE_EXAMPLE_STYLING_STYLE_REFS
class StylingStyleRefsExample final : public detail::JsonExample {
public:
    StylingStyleRefsExample()
        : JsonExample(
              ExampleInfo{"styling.style_refs", "Styling", "Style Refs", "Compose multiple named styles on one node"},
              "examples/styling/style_refs.json",
              R"json({
    "version": "0.1.0",
    "assets": [
        {
            "type": "styleSet",
            "styles": {
                "example.base": { "bgColor": "#1e293b", "radius": "10dp", "padding": "14dp" },
                "example.accentBorder": { "borderColor": "#38bdf8", "borderWidth": "3dp" },
                "example.bigText": { "textColor": "#f8fafc", "fontSize": "18sp" }
            }
        },
        {
            "type": "viewScreen",
            "id": "styling_style_refs",
            "style": { "bgColor": "#0f172a" },
            "children": [
                {
                    "type": "container",
                    "id": "card",
                    "styleRefs": [ "example.base", "example.accentBorder" ],
                    "placement": { "mode": "relative", "align": "center", "width": "220dp", "height": "110dp" },
                    "children": [
                        {
                            "type": "label",
                            "id": "title",
                            "labelProps": { "text": "Composed styles" },
                            "styleRefs": [ "example.bigText" ],
                            "placement": { "mode": "relative", "align": "center" }
                        }
                    ]
                }
            ]
        }
    ]
})json",
              "/styling_style_refs")
    {
    }

protected:
    bool on_verify(detail::Checker &checker, Runtime &, const Environment &, DocumentId) override
    {
        checker.check_frame_nonempty("card");
        checker.check_label_text("card/title", "Composed styles");
        return true;
    }
};
BROOKESIA_PLUGIN_REGISTER(IExample, StylingStyleRefsExample, "styling.style_refs");
#endif

} // namespace esp_brookesia::gui::examples
