/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * view.arc: arc gauge with gradient indicator.
 */

#include "brookesia/gui_interface/examples/example.hpp"
#include "common/example_base.hpp"

namespace esp_brookesia::gui::examples {

#if BROOKESIA_GUI_INTERFACE_EXAMPLE_VIEW_ARC
class ViewArcExample final : public detail::JsonExample {
public:
    ViewArcExample()
        : JsonExample(
              ExampleInfo{"view.arc", "View", "Arc", "Arc gauge with gradient indicator"},
              "examples/view/arc.json",
              R"json({
    "version": "0.1.0",
    "assets": [
        {
            "type": "viewScreen",
            "id": "view_arc",
            "style": { "bgColor": "#0f172a" },
            "children": [
                {
                    "type": "arc",
                    "id": "arc",
                    "rangeProps": { "value": 75, "min": 0, "max": 100 },
                    "partStyles": {
                        "indicator": { "arcColor": "#22c55e", "arcGradientColor": "#2563eb", "arcWidth": "8dp", "arcRounded": true, "arcGradientSegments": 48 }
                    },
                    "placement": { "mode": "relative", "align": "center", "width": "120dp", "height": "120dp" }
                }
            ]
        }
    ]
})json",
              "/view_arc")
    {
    }

protected:
    bool on_verify(detail::Checker &checker, Runtime &, const Environment &, DocumentId) override
    {
        checker.check_arc_value("arc", 75);
        return true;
    }
};
BROOKESIA_PLUGIN_REGISTER(IExample, ViewArcExample, "view.arc");
#endif

} // namespace esp_brookesia::gui::examples
