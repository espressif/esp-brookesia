/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * styling.placement: absolute anchor plus relativeTo out-anchor.
 */

#include "brookesia/gui_interface/examples/example.hpp"
#include "common/example_base.hpp"

namespace esp_brookesia::gui::examples {

#if BROOKESIA_GUI_INTERFACE_EXAMPLE_STYLING_PLACEMENT
class StylingPlacementExample final : public detail::JsonExample {
public:
    StylingPlacementExample()
        : JsonExample(
              ExampleInfo{"styling.placement", "Styling", "Placement", "Absolute anchor plus relativeTo out-anchor"},
              "examples/styling/placement.json",
              R"json({
    "version": "0.1.0",
    "assets": [
        {
            "type": "viewScreen",
            "id": "styling_placement",
            "style": { "bgColor": "#0f172a" },
            "children": [
                {
                    "type": "container",
                    "id": "anchor",
                    "style": { "bgColor": "#2563eb", "radius": "6dp" },
                    "placement": { "mode": "absolute", "align": "center", "width": "120dp", "height": "60dp" }
                },
                {
                    "type": "container",
                    "id": "follower",
                    "style": { "bgColor": "#22c55e", "radius": "6dp" },
                    "placement": { "mode": "relative", "relativeTo": "${view.anchor}", "align": "outBottomMid", "y": "8dp", "width": "120dp", "height": "30dp" }
                }
            ]
        }
    ]
})json",
              "/styling_placement")
    {
    }

protected:
    bool on_verify(detail::Checker &checker, Runtime &, const Environment &, DocumentId) override
    {
        // 'follower' uses relativeTo + outBottomMid, so it must end up below 'anchor'.
        checker.check_frame_nonempty("anchor");
        checker.check_frame_nonempty("follower");
        checker.check_below("follower", "anchor");
        return true;
    }
};
BROOKESIA_PLUGIN_REGISTER(IExample, StylingPlacementExample, "styling.placement");
#endif

} // namespace esp_brookesia::gui::examples
