/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * view.progress_bar: progress bar with gradient indicator.
 */

#include "brookesia/gui_interface/examples/example.hpp"
#include "common/example_base.hpp"

namespace esp_brookesia::gui::examples {

#if BROOKESIA_GUI_INTERFACE_EXAMPLE_VIEW_PROGRESS_BAR
class ViewProgressBarExample final : public detail::JsonExample {
public:
    ViewProgressBarExample()
        : JsonExample(
              ExampleInfo{"view.progress_bar", "View", "Progress Bar", "Progress bar with gradient indicator"},
              "examples/view/progress_bar.json",
              R"json({
    "version": "0.1.0",
    "assets": [
        {
            "type": "viewScreen",
            "id": "view_progress_bar",
            "style": { "bgColor": "#0f172a" },
            "children": [
                {
                    "type": "progressBar",
                    "id": "bar",
                    "rangeProps": { "value": 60, "min": 0, "max": 100 },
                    "partStyles": {
                        "indicator": { "bgColor": "#22c55e", "bgGradientColor": "#2563eb", "bgGradientDirection": "horizontal" }
                    },
                    "placement": { "mode": "relative", "align": "center", "width": "240dp", "height": "24dp" }
                }
            ]
        }
    ]
})json",
              "/view_progress_bar")
    {
    }

protected:
    bool on_verify(detail::Checker &checker, Runtime &, const Environment &, DocumentId) override
    {
        checker.check_progress_value("bar", 60);
        return true;
    }
};
BROOKESIA_PLUGIN_REGISTER(IExample, ViewProgressBarExample, "view.progress_bar");
#endif

} // namespace esp_brookesia::gui::examples
