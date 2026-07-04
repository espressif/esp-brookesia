/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * view.switch: boolean toggle switch.
 */

#include "brookesia/gui_interface/examples/example.hpp"
#include "common/example_base.hpp"

namespace esp_brookesia::gui::examples {

#if BROOKESIA_GUI_INTERFACE_EXAMPLE_VIEW_SWITCH
class ViewSwitchExample final : public detail::JsonExample {
public:
    ViewSwitchExample()
        : JsonExample(
              ExampleInfo{"view.switch", "View", "Switch", "Boolean toggle switch"},
              "examples/view/switch.json",
              R"json({
    "version": "0.1.0",
    "assets": [
        {
            "type": "viewScreen",
            "id": "view_switch",
            "style": { "bgColor": "#0f172a" },
            "children": [
                {
                    "type": "switch",
                    "id": "toggle",
                    "toggleProps": { "checked": true },
                    "placement": { "mode": "relative", "align": "center", "width": "70dp", "height": "36dp" }
                }
            ]
        }
    ]
})json",
              "/view_switch")
    {
    }

protected:
    bool on_verify(detail::Checker &checker, Runtime &, const Environment &, DocumentId) override
    {
        checker.check_switch_checked("toggle", true);
        return true;
    }
};
BROOKESIA_PLUGIN_REGISTER(IExample, ViewSwitchExample, "view.switch");
#endif

} // namespace esp_brookesia::gui::examples
