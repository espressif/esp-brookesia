/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * styling.flex: row-wrap flex with gap and centered alignment.
 */

#include "brookesia/gui_interface/examples/example.hpp"
#include "common/example_base.hpp"

namespace esp_brookesia::gui::examples {

#if BROOKESIA_GUI_INTERFACE_EXAMPLE_STYLING_FLEX
class StylingFlexExample final : public detail::JsonExample {
public:
    StylingFlexExample()
        : JsonExample(
              ExampleInfo{"styling.flex", "Styling", "Flex Layout", "Row-wrap flex with gap and centered alignment"},
              "examples/styling/flex.json",
              R"json({
    "version": "0.1.0",
    "assets": [
        {
            "type": "viewScreen",
            "id": "styling_flex",
            "style": { "bgColor": "#0f172a" },
            "layout": { "type": "flex", "flexFlow": "column", "mainAlign": "center", "crossAlign": "center", "gap": "12dp" },
            "children": [
                {
                    "type": "container",
                    "id": "host",
                    "style": { "bgColor": "#111c30", "padding": "10dp", "borderWidth": "0dp", "radius": "8dp" },
                    "layout": { "type": "flex", "flexFlow": "rowWrap", "mainAlign": "center", "crossAlign": "center", "gap": "10dp" },
                    "placement": { "mode": "flow", "width": "260dp", "height": "120dp" },
                    "children": [
                        { "type": "container", "id": "a", "style": { "bgColor": "#2563eb", "radius": "6dp" }, "placement": { "mode": "flow", "width": "60dp", "height": "40dp" } },
                        { "type": "container", "id": "b", "style": { "bgColor": "#22c55e", "radius": "6dp" }, "placement": { "mode": "flow", "width": "60dp", "height": "40dp" } },
                        { "type": "container", "id": "c", "style": { "bgColor": "#f97316", "radius": "6dp" }, "placement": { "mode": "flow", "width": "60dp", "height": "40dp" } },
                        { "type": "container", "id": "d", "style": { "bgColor": "#a855f7", "radius": "6dp" }, "placement": { "mode": "flow", "width": "60dp", "height": "40dp" } }
                    ]
                },
                {
                    "type": "container",
                    "id": "grow_row",
                    "style": { "bgColor": "#111c30", "padding": "8dp", "borderWidth": "0dp", "radius": "8dp" },
                    "layout": { "type": "flex", "flexFlow": "row", "mainAlign": "start", "crossAlign": "center", "gap": "8dp" },
                    "placement": { "mode": "flow", "width": "260dp", "height": "52dp" },
                    "children": [
                        { "type": "container", "id": "left", "style": { "bgColor": "#2563eb", "radius": "6dp" }, "placement": { "mode": "flow", "width": "56dp", "height": "28dp" } },
                        { "type": "container", "id": "spacer", "placement": { "mode": "flow", "width": "wrap", "height": "1dp", "flexGrow": 1 } },
                        { "type": "container", "id": "trailing", "style": { "bgColor": "#22c55e", "radius": "6dp" }, "placement": { "mode": "flow", "width": "48dp", "height": "28dp" } }
                    ]
                }
            ]
        }
    ]
})json",
              "/styling_flex")
    {
    }

protected:
    bool on_verify(detail::Checker &checker, Runtime &, const Environment &, DocumentId) override
    {
        // The flex host plus all four flow children must lay out with concrete geometry.
        checker.check_frame_nonempty("host");
        checker.check_frame_nonempty("host/a");
        checker.check_frame_nonempty("host/b");
        checker.check_frame_nonempty("host/c");
        checker.check_frame_nonempty("host/d");
        checker.check_frame_nonempty("grow_row");
        checker.check_frame_nonempty("grow_row/left");
        checker.check_frame_nonempty("grow_row/spacer");
        checker.check_frame_nonempty("grow_row/trailing");

        auto row = checker.frame("grow_row");
        auto trailing = checker.frame("grow_row/trailing");
        if (row && trailing) {
            checker.check(
                trailing->x + trailing->width <= row->x + row->width,
                "expected flex grow trailing child to remain inside grow_row"
            );
        }
        return true;
    }
};
BROOKESIA_PLUGIN_REGISTER(IExample, StylingFlexExample, "styling.flex");
#endif

} // namespace esp_brookesia::gui::examples
