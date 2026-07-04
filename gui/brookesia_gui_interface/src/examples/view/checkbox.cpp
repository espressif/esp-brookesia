/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * view.checkbox: checkbox with a text label.
 */

#include "brookesia/gui_interface/examples/example.hpp"
#include "common/example_base.hpp"

namespace esp_brookesia::gui::examples {

#if BROOKESIA_GUI_INTERFACE_EXAMPLE_VIEW_CHECKBOX
class ViewCheckboxExample final : public detail::JsonExample {
public:
    ViewCheckboxExample()
        : JsonExample(
              ExampleInfo{"view.checkbox", "View", "Checkbox", "Checkbox with a text label"},
              "examples/view/checkbox.json",
              R"json({
    "version": "0.1.0",
    "assets": [
        {
            "type": "viewScreen",
            "id": "view_checkbox",
            "style": { "bgColor": "#0f172a" },
            "children": [
                {
                    "type": "checkbox",
                    "id": "check",
                    "labelProps": { "text": "Accept terms" },
                    "toggleProps": { "checked": true },
                    "style": { "textColor": "#e2e8f0" },
                    "placement": { "mode": "relative", "align": "center", "width": "200dp", "height": "32dp" }
                }
            ]
        }
    ]
})json",
              "/view_checkbox")
    {
    }

protected:
    bool on_verify(detail::Checker &checker, Runtime &, const Environment &, DocumentId) override
    {
        checker.check_checkbox_checked("check", true);
        return true;
    }
};
BROOKESIA_PLUGIN_REGISTER(IExample, ViewCheckboxExample, "view.checkbox");
#endif

} // namespace esp_brookesia::gui::examples
