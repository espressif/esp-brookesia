/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * assets.constant: a constant asset feeding text and color fields.
 */

#include "brookesia/gui_interface/examples/example.hpp"
#include "common/example_base.hpp"

namespace esp_brookesia::gui::examples {

#if BROOKESIA_GUI_INTERFACE_EXAMPLE_ASSETS_CONSTANT
class AssetsConstantExample final : public detail::JsonExample {
public:
    AssetsConstantExample()
        : JsonExample(
              ExampleInfo{"assets.constant", "Assets", "constant", "constant asset feeding text and color fields"},
              "examples/assets/constant.json",
              R"json({
    "version": "0.1.0",
    "assets": [
        {
            "type": "constant",
            "data": {
                "brand": { "name": "ESP-Brookesia", "accent": "#22c55e" }
            }
        },
        {
            "type": "viewScreen",
            "id": "assets_constant",
            "style": { "bgColor": "#0f172a" },
            "children": [
                {
                    "type": "label",
                    "id": "name",
                    "labelProps": { "text": "${constant.brand.name}" },
                    "style": { "textColor": "${constant.brand.accent}", "fontSize": "22sp" },
                    "placement": { "mode": "relative", "align": "center" }
                }
            ]
        }
    ]
})json",
              "/assets_constant")
    {
    }

protected:
    bool on_verify(detail::Checker &checker, Runtime &, const Environment &, DocumentId) override
    {
        // The label text is fed by ${constant.brand.name}; verify it resolved to the constant value.
        checker.check_label_text("name", "ESP-Brookesia");
        return true;
    }
};
BROOKESIA_PLUGIN_REGISTER(IExample, AssetsConstantExample, "assets.constant");
#endif

} // namespace esp_brookesia::gui::examples
