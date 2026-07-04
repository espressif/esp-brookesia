/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * view.dropdown: dropdown selection list.
 */

#include "brookesia/gui_interface/examples/example.hpp"
#include "common/example_base.hpp"

namespace esp_brookesia::gui::examples {

#if BROOKESIA_GUI_INTERFACE_EXAMPLE_VIEW_DROPDOWN
class ViewDropdownExample final : public detail::JsonExample {
public:
    ViewDropdownExample()
        : JsonExample(
              ExampleInfo{"view.dropdown", "View", "Dropdown", "Dropdown selection list"},
              "examples/view/dropdown.json",
              R"json({
    "version": "0.1.0",
    "assets": [
        {
            "type": "viewScreen",
            "id": "view_dropdown",
            "style": { "bgColor": "#0f172a" },
            "children": [
                {
                    "type": "dropdown",
                    "id": "menu",
                    "dropdownProps": { "options": ["One", "Two", "Three"], "selectedIndex": 1 },
                    "placement": { "mode": "relative", "align": "center", "width": "200dp", "height": "44dp" }
                }
            ]
        }
    ]
})json",
              "/view_dropdown")
    {
    }

protected:
    bool on_verify(detail::Checker &checker, Runtime &, const Environment &, DocumentId) override
    {
        checker.check_dropdown_index("menu", 1);
        return true;
    }
};
BROOKESIA_PLUGIN_REGISTER(IExample, ViewDropdownExample, "view.dropdown");
#endif

} // namespace esp_brookesia::gui::examples
