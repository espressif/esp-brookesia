/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * view.button: clickable button with a child label.
 */

#include "brookesia/gui_interface/examples/example.hpp"
#include "common/example_base.hpp"

namespace esp_brookesia::gui::examples {

#if BROOKESIA_GUI_INTERFACE_EXAMPLE_VIEW_BUTTON
class ViewButtonExample final : public detail::JsonExample {
public:
    ViewButtonExample()
        : JsonExample(
              ExampleInfo{"view.button", "View", "Button", "Clickable button with a child label"},
              "examples/view/button.json",
              R"json({
    "version": "0.1.0",
    "assets": [
        {
            "type": "viewScreen",
            "id": "view_button",
            "style": { "bgColor": "#0f172a" },
            "children": [
                {
                    "type": "button",
                    "id": "cta",
                    "style": { "bgColor": "#2563eb", "radius": "10dp" },
                    "placement": { "mode": "relative", "align": "center", "width": "180dp", "height": "52dp" },
                    "children": [
                        {
                            "type": "label",
                            "id": "label",
                            "labelProps": { "text": "Press Me" },
                            "style": { "textColor": "#ffffff" },
                            "placement": { "mode": "relative", "align": "center" }
                        }
                    ]
                }
            ]
        }
    ]
})json",
              "/view_button")
    {
    }

protected:
    bool on_verify(detail::Checker &checker, Runtime &, const Environment &, DocumentId) override
    {
        checker.check_frame_nonempty("cta");
        checker.check_label_text("cta/label", "Press Me");
        return true;
    }
};
BROOKESIA_PLUGIN_REGISTER(IExample, ViewButtonExample, "view.button");
#endif

} // namespace esp_brookesia::gui::examples
