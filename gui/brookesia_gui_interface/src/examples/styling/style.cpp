/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * styling.style: gradient, radius, border, shadow and clipCorner.
 */

#include "brookesia/gui_interface/examples/example.hpp"
#include "common/example_base.hpp"

namespace esp_brookesia::gui::examples {

#if BROOKESIA_GUI_INTERFACE_EXAMPLE_STYLING_STYLE
class StylingStyleExample final : public detail::JsonExample {
public:
    StylingStyleExample()
        : JsonExample(
              ExampleInfo{"styling.style", "Styling", "Style Props", "Gradient, radius, border, shadow and clipCorner"},
              "examples/styling/style.json",
              R"json({
    "version": "0.1.0",
    "assets": [
        {
            "type": "viewScreen",
            "id": "styling_style",
            "style": { "bgColor": "#0f172a" },
            "children": [
                {
                    "type": "container",
                    "id": "card",
                    "style": {
                        "bgColor": "#2563eb",
                        "bgGradientColor": "#7c3aed",
                        "bgGradientDirection": "vertical",
                        "radius": "18dp",
                        "borderColor": "#e2e8f0",
                        "borderWidth": "2dp",
                        "shadowWidth": "16dp",
                        "shadowOffsetY": "6dp",
                        "shadowColor": "#1e1b4b",
                        "clipCorner": true
                    },
                    "placement": { "mode": "relative", "align": "center", "width": "200dp", "height": "120dp" }
                }
            ]
        }
    ]
})json",
              "/styling_style")
    {
    }

protected:
    bool on_verify(detail::Checker &checker, Runtime &, const Environment &, DocumentId) override
    {
        checker.require_view("card");
        checker.check_frame_nonempty("card");
        return true;
    }
};
BROOKESIA_PLUGIN_REGISTER(IExample, StylingStyleExample, "styling.style");
#endif

} // namespace esp_brookesia::gui::examples
