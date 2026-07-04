/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * view.label: text label with color and font size.
 */

#include "brookesia/gui_interface/examples/example.hpp"
#include "common/example_base.hpp"

namespace esp_brookesia::gui::examples {

#if BROOKESIA_GUI_INTERFACE_EXAMPLE_VIEW_LABEL
class ViewLabelExample final : public detail::JsonExample {
public:
    ViewLabelExample()
        : JsonExample(
              ExampleInfo{"view.label", "View", "Label", "Text label with color and font size"},
              "examples/view/label.json",
              R"json({
    "version": "0.1.0",
    "assets": [
        {
            "type": "viewScreen",
            "id": "view_label",
            "style": { "bgColor": "#0f172a" },
            "children": [
                {
                    "type": "label",
                    "id": "title",
                    "labelProps": { "text": "Hello, Brookesia" },
                    "style": { "textColor": "#38bdf8", "fontSize": "20sp" },
                    "placement": { "mode": "relative", "align": "center" }
                }
            ]
        }
    ]
})json",
              "/view_label")
    {
    }

protected:
    bool on_verify(detail::Checker &checker, Runtime &, const Environment &, DocumentId) override
    {
        checker.check_label_text("title", "Hello, Brookesia");
        checker.check_frame_nonempty("title");
        return true;
    }
};
BROOKESIA_PLUGIN_REGISTER(IExample, ViewLabelExample, "view.label");
#endif

} // namespace esp_brookesia::gui::examples
