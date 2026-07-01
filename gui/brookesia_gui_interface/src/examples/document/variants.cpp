/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * document.variants: a constant overridden by a when-matched variant.
 */

#include "brookesia/gui_interface/examples/example.hpp"
#include "common/example_base.hpp"

namespace esp_brookesia::gui::examples {

#if BROOKESIA_GUI_INTERFACE_EXAMPLE_DOCUMENT_VARIANTS
class DocumentVariantsExample final : public detail::JsonExample {
public:
    DocumentVariantsExample()
        : JsonExample(
              ExampleInfo{"document.variants", "Document", "Variants (when)", "Constant overridden by a when-matched variant"},
              "examples/document/variants.json",
              R"json({
    "version": "0.1.0",
    "assets": [
        { "type": "constant", "data": { "text": { "mode": "base layout" } } },
        {
            "type": "viewScreen",
            "id": "document_variants",
            "style": { "bgColor": "#0f172a" },
            "children": [
                {
                    "type": "label",
                    "id": "title",
                    "labelProps": { "text": "${constant.text.mode}" },
                    "style": { "textColor": "#38bdf8", "fontSize": "20sp" },
                    "placement": { "mode": "relative", "align": "center" }
                }
            ]
        }
    ],
    "variants": [
        {
            "when": "${expr(${env.widthDp} >= 600dp)}",
            "assets": [
                { "type": "constant", "data": { "text": { "mode": "wide variant (width >= 600dp)" } } }
            ]
        }
    ]
})json",
              "/document_variants")
    {
    }

protected:
    bool on_verify(detail::Checker &checker, Runtime &, const Environment &environment, DocumentId) override
    {
        // The 800dp-wide PC window matches the `when` clause, so the wide variant overrides the base
        // constant. On narrower environments fall back to asserting the base text instead.
        const char *expected = (environment.width_px >= 600) ? "wide variant (width >= 600dp)" : "base layout";
        checker.check_label_text("title", expected);
        return true;
    }
};
BROOKESIA_PLUGIN_REGISTER(IExample, DocumentVariantsExample, "document.variants");
#endif

} // namespace esp_brookesia::gui::examples
