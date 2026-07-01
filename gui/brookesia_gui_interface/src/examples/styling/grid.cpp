/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * styling.grid: grid tracks with a spanning, stretched cell.
 */

#include "brookesia/gui_interface/examples/example.hpp"
#include "common/example_base.hpp"

namespace esp_brookesia::gui::examples {

#if BROOKESIA_GUI_INTERFACE_EXAMPLE_STYLING_GRID
class StylingGridExample final : public detail::JsonExample {
public:
    StylingGridExample()
        : JsonExample(
              ExampleInfo{"styling.grid", "Styling", "Grid Layout", "Grid tracks with a spanning, stretched cell"},
              "examples/styling/grid.json",
              R"json({
    "version": "0.1.0",
    "assets": [
        {
            "type": "viewScreen",
            "id": "styling_grid",
            "style": { "bgColor": "#0f172a" },
            "children": [
                {
                    "type": "container",
                    "id": "host",
                    "style": { "bgColor": "#111c30", "radius": "8dp" },
                    "layout": { "type": "grid", "gridTemplateColumns": [ "80dp", "80dp", "80dp" ], "gridTemplateRows": [ "60dp", "60dp" ] },
                    "placement": { "mode": "relative", "align": "center", "width": "260dp", "height": "140dp" },
                    "children": [
                        { "type": "container", "id": "span", "style": { "bgColor": "#7c3aed", "radius": "6dp" }, "placement": { "mode": "flow", "gridColumn": 0, "gridRow": 0, "gridColumnSpan": 2, "gridRowSpan": 2, "alignSelf": "stretch" } },
                        { "type": "container", "id": "top", "style": { "bgColor": "#2563eb", "radius": "6dp" }, "placement": { "mode": "flow", "gridColumn": 2, "gridRow": 0, "alignSelf": "stretch" } },
                        { "type": "container", "id": "bottom", "style": { "bgColor": "#22c55e", "radius": "6dp" }, "placement": { "mode": "flow", "gridColumn": 2, "gridRow": 1, "alignSelf": "stretch" } }
                    ]
                }
            ]
        }
    ]
})json",
              "/styling_grid")
    {
    }

protected:
    bool on_verify(detail::Checker &checker, Runtime &, const Environment &, DocumentId) override
    {
        checker.check_frame_nonempty("host/span");
        checker.check_frame_nonempty("host/top");
        checker.check_frame_nonempty("host/bottom");
        // The span cell covers 2 columns, so it must be wider than a single-column cell.
        auto span = checker.frame("host/span");
        auto top = checker.frame("host/top");
        if (span && top) {
            checker.check(span->width > top->width,
                          "expected grid 'span' wider than 'top' (span.w=" + std::to_string(span->width)
                          + ", top.w=" + std::to_string(top->width) + ")");
        }
        return true;
    }
};
BROOKESIA_PLUGIN_REGISTER(IExample, StylingGridExample, "styling.grid");
#endif

} // namespace esp_brookesia::gui::examples
