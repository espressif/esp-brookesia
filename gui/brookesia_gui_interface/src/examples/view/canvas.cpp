/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * view.canvas: canvas filled with draw commands.
 */

#include "brookesia/gui_interface/examples/example.hpp"
#include "common/example_base.hpp"

namespace esp_brookesia::gui::examples {

#if BROOKESIA_GUI_INTERFACE_EXAMPLE_VIEW_CANVAS
class ViewCanvasExample final : public detail::JsonExample {
public:
    ViewCanvasExample()
        : JsonExample(
              ExampleInfo{"view.canvas", "View", "Canvas", "Canvas filled with draw commands"},
              "examples/view/canvas.json",
              R"json({
    "version": "0.1.0",
    "assets": [
        {
            "type": "viewScreen",
            "id": "view_canvas",
            "style": { "bgColor": "#0f172a" },
            "children": [
                {
                    "type": "canvas",
                    "id": "canvas",
                    "canvasProps": {
                        "commands": [
                            { "type": "fill", "color": "#f8fafc" },
                            { "type": "pixel", "x": 10, "y": 10, "color": "#ef4444" },
                            { "type": "pixel", "x": 20, "y": 20, "color": "#22c55e" }
                        ]
                    },
                    "placement": { "mode": "relative", "align": "center", "width": "80dp", "height": "80dp" }
                }
            ]
        }
    ]
})json",
              "/view_canvas")
    {
    }

protected:
    bool on_verify(detail::Checker &checker, Runtime &, const Environment &, DocumentId) override
    {
        checker.require_view("canvas");
        checker.check_frame_nonempty("canvas");
        return true;
    }
};
BROOKESIA_PLUGIN_REGISTER(IExample, ViewCanvasExample, "view.canvas");
#endif

} // namespace esp_brookesia::gui::examples
