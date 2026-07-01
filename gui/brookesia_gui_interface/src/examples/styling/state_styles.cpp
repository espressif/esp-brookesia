/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * styling.state_styles: button base style with a pressed state override.
 */

#include "brookesia/gui_interface/examples/example.hpp"
#include "common/example_base.hpp"

namespace esp_brookesia::gui::examples {

#if BROOKESIA_GUI_INTERFACE_EXAMPLE_STYLING_STATE_STYLES
class StylingStateStylesExample final : public detail::JsonExample {
public:
    StylingStateStylesExample()
        : JsonExample(
              ExampleInfo{"styling.state_styles", "Styling", "State Styles", "Button base style with a pressed state override"},
              "examples/styling/state_styles.json",
              R"json({
    "version": "0.1.0",
    "assets": [
        {
            "type": "viewScreen",
            "id": "styling_state_styles",
            "style": { "bgColor": "#0f172a" },
            "children": [
                {
                    "type": "button",
                    "id": "cta",
                    "style": { "bgColor": "#2563eb", "radius": "10dp" },
                    "stateStyles": {
                        "pressed": { "bgColor": "#1d4ed8" },
                        "disabled": { "bgColor": "#475569" }
                    },
                    "placement": { "mode": "relative", "align": "center", "width": "200dp", "height": "56dp" },
                    "children": [
                        {
                            "type": "label",
                            "id": "label",
                            "labelProps": { "text": "Press & hold me" },
                            "style": { "textColor": "#ffffff" },
                            "placement": { "mode": "relative", "align": "center" }
                        }
                    ]
                }
            ]
        }
    ]
})json",
              "/styling_state_styles")
    {
    }

protected:
    bool on_verify(detail::Checker &checker, Runtime &, const Environment &, DocumentId) override
    {
        checker.check_frame_nonempty("cta");
        checker.check_label_text("cta/label", "Press & hold me");
        return true;
    }
};
BROOKESIA_PLUGIN_REGISTER(IExample, StylingStateStylesExample, "styling.state_styles");
#endif

} // namespace esp_brookesia::gui::examples
