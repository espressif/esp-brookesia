/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * view.screen: root viewScreen with a centered label.
 */

#include "brookesia/gui_interface/examples/example.hpp"
#include "common/example_base.hpp"

namespace esp_brookesia::gui::examples {

#if BROOKESIA_GUI_INTERFACE_EXAMPLE_VIEW_SCREEN
class ViewScreenExample final : public detail::JsonExample {
public:
    ViewScreenExample()
        : JsonExample(
              ExampleInfo{"view.screen", "View", "Screen", "Root viewScreen with a centered label"},
              "examples/view/screen.json",
              R"json({
    "version": "0.1.0",
    "assets": [
        {
            "type": "viewScreen",
            "id": "view_screen",
            "style": { "bgColor": "#0f172a" },
            "children": [
                {
                    "type": "label",
                    "id": "title",
                    "labelProps": { "text": "viewScreen" },
                    "style": { "textColor": "#e2e8f0", "fontSize": "24sp" },
                    "placement": { "mode": "relative", "align": "center" }
                }
            ]
        }
    ]
})json",
              "/view_screen")
    {
    }

protected:
    bool on_verify(detail::Checker &checker, Runtime &, const Environment &, DocumentId) override
    {
        checker.check_label_text("title", "viewScreen");
        checker.check_frame_nonempty("title");
        return true;
    }
};
BROOKESIA_PLUGIN_REGISTER(IExample, ViewScreenExample, "view.screen");
#endif

} // namespace esp_brookesia::gui::examples
