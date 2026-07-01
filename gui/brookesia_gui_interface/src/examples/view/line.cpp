/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * view.line: polyline drawn from explicit points.
 */

#include "brookesia/gui_interface/examples/example.hpp"
#include "common/example_base.hpp"

namespace esp_brookesia::gui::examples {

#if BROOKESIA_GUI_INTERFACE_EXAMPLE_VIEW_LINE
class ViewLineExample final : public detail::JsonExample {
public:
    ViewLineExample()
        : JsonExample(
              ExampleInfo{"view.line", "View", "Line", "Polyline drawn from explicit points"},
              "examples/view/line.json",
              R"json({
    "version": "0.1.0",
    "assets": [
        {
            "type": "viewScreen",
            "id": "view_line",
            "style": { "bgColor": "#0f172a" },
            "children": [
                {
                    "type": "line",
                    "id": "line",
                    "lineProps": { "points": [ { "x": 0, "y": 40 }, { "x": 60, "y": 0 }, { "x": 120, "y": 40 } ] },
                    "style": { "lineColor": "#38bdf8", "lineWidth": "3dp" },
                    "placement": { "mode": "relative", "align": "center", "width": "120dp", "height": "40dp" }
                }
            ]
        }
    ]
})json",
              "/view_line")
    {
    }

protected:
    bool on_verify(detail::Checker &checker, Runtime &, const Environment &, DocumentId) override
    {
        checker.require_view("line");
        checker.check_frame_nonempty("line");
        return true;
    }
};
BROOKESIA_PLUGIN_REGISTER(IExample, ViewLineExample, "view.line");
#endif

} // namespace esp_brookesia::gui::examples
