/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * view.slider: slider with gradient indicator and knob states.
 */

#include "brookesia/gui_interface/examples/example.hpp"
#include "common/example_base.hpp"

namespace esp_brookesia::gui::examples {

#if BROOKESIA_GUI_INTERFACE_EXAMPLE_VIEW_SLIDER
class ViewSliderExample final : public detail::JsonExample {
public:
    ViewSliderExample()
        : JsonExample(
              ExampleInfo{"view.slider", "View", "Slider", "Slider with gradient indicator and knob states"},
              "examples/view/slider.json",
              R"json({
    "version": "0.1.0",
    "assets": [
        {
            "type": "viewScreen",
            "id": "view_slider",
            "style": { "bgColor": "#0f172a" },
            "children": [
                {
                    "type": "slider",
                    "id": "slider",
                    "rangeProps": { "value": 42, "min": 0, "max": 100 },
                    "partStyles": {
                        "indicator": { "bgColor": "#2563eb", "bgGradientColor": "#38bdf8", "bgGradientDirection": "horizontal" },
                        "knob": {
                            "style": { "bgColor": "#ffffff", "radius": "12dp" },
                            "stateStyles": { "pressed": { "bgColor": "#dbeafe" } }
                        }
                    },
                    "placement": { "mode": "relative", "align": "center", "width": "240dp", "height": "24dp" }
                }
            ]
        }
    ]
})json",
              "/view_slider")
    {
    }

protected:
    bool on_verify(detail::Checker &checker, Runtime &, const Environment &, DocumentId) override
    {
        // The JSON configures value 42; verify it, then round-trip a programmatic set_value.
        if (!checker.check_slider_value("slider", 42)) {
            return true;
        }
        checker.view("slider").as_slider().set_value(80);
        checker.check_slider_value("slider", 80);
        return true;
    }
};
BROOKESIA_PLUGIN_REGISTER(IExample, ViewSliderExample, "view.slider");
#endif

} // namespace esp_brookesia::gui::examples
