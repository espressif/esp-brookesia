/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * view.text_input: single-line text input with placeholder.
 */

#include "brookesia/gui_interface/examples/example.hpp"
#include "common/example_base.hpp"

namespace esp_brookesia::gui::examples {

#if BROOKESIA_GUI_INTERFACE_EXAMPLE_VIEW_TEXT_INPUT
class ViewTextInputExample final : public detail::JsonExample {
public:
    ViewTextInputExample()
        : JsonExample(
              ExampleInfo{"view.text_input", "View", "Text Input", "Single-line text input with placeholder"},
              "examples/view/text_input.json",
              R"json({
    "version": "0.1.0",
    "assets": [
        {
            "type": "viewScreen",
            "id": "view_text_input",
            "style": { "bgColor": "#0f172a" },
            "children": [
                {
                    "type": "textInput",
                    "id": "input",
                    "textInputProps": { "text": "hello", "placeholder": "type here", "maxLength": 32 },
                    "placement": { "mode": "relative", "align": "center", "width": "240dp", "height": "44dp" }
                }
            ]
        }
    ]
})json",
              "/view_text_input")
    {
    }

protected:
    bool on_verify(detail::Checker &checker, Runtime &, const Environment &, DocumentId) override
    {
        checker.check_text_input_text("input", "hello");
        return true;
    }
};
BROOKESIA_PLUGIN_REGISTER(IExample, ViewTextInputExample, "view.text_input");
#endif

} // namespace esp_brookesia::gui::examples
