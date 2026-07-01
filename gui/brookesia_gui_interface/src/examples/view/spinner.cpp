/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * view.spinner: indeterminate loading spinner.
 */

#include "brookesia/gui_interface/examples/example.hpp"
#include "common/example_base.hpp"

namespace esp_brookesia::gui::examples {

#if BROOKESIA_GUI_INTERFACE_EXAMPLE_VIEW_SPINNER
class ViewSpinnerExample final : public detail::JsonExample {
public:
    ViewSpinnerExample()
        : JsonExample(
              ExampleInfo{"view.spinner", "View", "Spinner", "Indeterminate loading spinner"},
              "examples/view/spinner.json",
              R"json({
    "version": "0.1.0",
    "assets": [
        {
            "type": "viewScreen",
            "id": "view_spinner",
            "style": { "bgColor": "#0f172a" },
            "children": [
                {
                    "type": "spinner",
                    "id": "spin",
                    "placement": { "mode": "relative", "align": "center", "width": "64dp", "height": "64dp" }
                }
            ]
        }
    ]
})json",
              "/view_spinner")
    {
    }

protected:
    bool on_verify(detail::Checker &checker, Runtime &, const Environment &, DocumentId) override
    {
        checker.require_view("spin");
        checker.check_frame_nonempty("spin");
        return true;
    }
};
BROOKESIA_PLUGIN_REGISTER(IExample, ViewSpinnerExample, "view.spinner");
#endif

} // namespace esp_brookesia::gui::examples
