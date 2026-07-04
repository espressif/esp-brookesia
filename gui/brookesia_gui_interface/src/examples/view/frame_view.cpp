/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * view.frame_view: frameView output surface placeholder.
 */

#include "brookesia/gui_interface/examples/example.hpp"
#include "common/example_base.hpp"

namespace esp_brookesia::gui::examples {

#if BROOKESIA_GUI_INTERFACE_EXAMPLE_VIEW_FRAME_VIEW
class ViewFrameViewExample final : public detail::JsonExample {
public:
    ViewFrameViewExample()
        : JsonExample(
              ExampleInfo{"view.frame_view", "View", "Frame View", "frameView output surface placeholder"},
              "examples/view/frame_view.json",
              R"json({
    "version": "0.1.0",
    "assets": [
        {
            "type": "viewScreen",
            "id": "view_frame_view",
            "style": { "bgColor": "#0f172a" },
            "children": [
                {
                    "type": "frameView",
                    "id": "preview",
                    "frameViewProps": { "autoRegisterOutput": false, "outputName": "ExamplePreview", "colorFormat": "RGB565" },
                    "style": { "bgColor": "#1e293b", "radius": "8dp" },
                    "placement": { "mode": "relative", "align": "center", "width": "160dp", "height": "120dp" }
                }
            ]
        }
    ]
})json",
              "/view_frame_view")
    {
    }

protected:
    bool on_verify(detail::Checker &checker, Runtime &, const Environment &, DocumentId) override
    {
        checker.require_view("preview");
        checker.check_frame_nonempty("preview");
        return true;
    }
};
BROOKESIA_PLUGIN_REGISTER(IExample, ViewFrameViewExample, "view.frame_view");
#endif

} // namespace esp_brookesia::gui::examples
