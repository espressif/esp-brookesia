/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * view.keyboard: on-screen keyboard in number mode.
 */

#include "brookesia/gui_interface/examples/example.hpp"
#include "common/example_base.hpp"

namespace esp_brookesia::gui::examples {

#if BROOKESIA_GUI_INTERFACE_EXAMPLE_VIEW_KEYBOARD
class ViewKeyboardExample final : public detail::JsonExample {
public:
    ViewKeyboardExample()
        : JsonExample(
              ExampleInfo{"view.keyboard", "View", "Keyboard", "On-screen keyboard in number mode"},
              "examples/view/keyboard.json",
              R"json({
    "version": "0.1.0",
    "assets": [
        {
            "type": "viewScreen",
            "id": "view_keyboard",
            "style": { "bgColor": "#0f172a" },
            "children": [
                {
                    "type": "keyboard",
                    "id": "kb",
                    "keyboardProps": { "mode": "number", "popovers": true },
                    "placement": { "mode": "relative", "align": "bottomMid", "width": "300dp", "height": "160dp" }
                }
            ]
        }
    ]
})json",
              "/view_keyboard")
    {
    }

protected:
    bool on_verify(detail::Checker &checker, Runtime &, const Environment &, DocumentId) override
    {
        checker.require_view("kb");
        checker.check_frame_nonempty("kb");
        return true;
    }
};
BROOKESIA_PLUGIN_REGISTER(IExample, ViewKeyboardExample, "view.keyboard");
#endif

} // namespace esp_brookesia::gui::examples
