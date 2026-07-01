/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * document.root: viewScreen combined with a constant asset.
 */

#include "brookesia/gui_interface/examples/example.hpp"
#include "common/example_base.hpp"

namespace esp_brookesia::gui::examples {

#if BROOKESIA_GUI_INTERFACE_EXAMPLE_DOCUMENT_ROOT
class DocumentRootExample final : public detail::JsonExample {
public:
    DocumentRootExample()
        : JsonExample(
              ExampleInfo{"document.root", "Document", "Root + Assets", "viewScreen combined with a constant asset"},
              "examples/document/root.json",
              R"json({
    "version": "0.1.0",
    "assets": [
        { "type": "constant", "data": { "text": { "title": "Document Root" } } },
        {
            "type": "viewScreen",
            "id": "document_root",
            "style": { "bgColor": "#0f172a" },
            "children": [
                {
                    "type": "label",
                    "id": "title",
                    "labelProps": { "text": "${constant.text.title}" },
                    "style": { "textColor": "#e2e8f0", "fontSize": "22sp" },
                    "placement": { "mode": "relative", "align": "center" }
                }
            ]
        }
    ]
})json",
              "/document_root")
    {
    }

protected:
    bool on_verify(detail::Checker &checker, Runtime &, const Environment &, DocumentId) override
    {
        // The title text comes from ${constant.text.title}; verify the constant resolved.
        checker.check_label_text("title", "Document Root");
        return true;
    }
};
BROOKESIA_PLUGIN_REGISTER(IExample, DocumentRootExample, "document.root");
#endif

} // namespace esp_brookesia::gui::examples
