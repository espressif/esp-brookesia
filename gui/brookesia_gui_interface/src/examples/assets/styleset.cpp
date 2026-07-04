/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * assets.styleset: named styles shared via styleRefs.
 */

#include "brookesia/gui_interface/examples/example.hpp"
#include "common/example_base.hpp"

namespace esp_brookesia::gui::examples {

#if BROOKESIA_GUI_INTERFACE_EXAMPLE_ASSETS_STYLESET
class AssetsStyleSetExample final : public detail::JsonExample {
public:
    AssetsStyleSetExample()
        : JsonExample(
              ExampleInfo{"assets.styleset", "Assets", "styleSet", "Named styles shared via styleRefs"},
              "examples/assets/styleset.json",
              R"json({
    "version": "0.1.0",
    "assets": [
        {
            "type": "styleSet",
            "styles": {
                "example.card": { "bgColor": "#1e293b", "radius": "10dp", "padding": "14dp" },
                "example.cardTitle": { "textColor": "#38bdf8", "fontSize": "18sp" }
            }
        },
        {
            "type": "viewScreen",
            "id": "assets_styleset",
            "style": { "bgColor": "#0f172a" },
            "children": [
                {
                    "type": "container",
                    "id": "card",
                    "styleRefs": [ "example.card" ],
                    "placement": { "mode": "relative", "align": "center", "width": "220dp", "height": "100dp" },
                    "children": [
                        {
                            "type": "label",
                            "id": "title",
                            "labelProps": { "text": "Shared styleSet" },
                            "styleRefs": [ "example.cardTitle" ],
                            "placement": { "mode": "relative", "align": "center" }
                        }
                    ]
                }
            ]
        }
    ]
})json",
              "/assets_styleset")
    {
    }

protected:
    bool on_verify(detail::Checker &checker, Runtime &, const Environment &, DocumentId) override
    {
        // The title pulls its text from labelProps and its look from the shared styleSet; the card
        // gets geometry from the referenced style, so a non-empty frame confirms styleRefs resolved.
        checker.check_label_text("card/title", "Shared styleSet");
        checker.check_frame_nonempty("card");
        return true;
    }
};
BROOKESIA_PLUGIN_REGISTER(IExample, AssetsStyleSetExample, "assets.styleset");
#endif

} // namespace esp_brookesia::gui::examples
