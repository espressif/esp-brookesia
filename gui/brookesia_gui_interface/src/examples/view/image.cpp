/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * view.image: image resolved from an imageSet asset.
 */

#include "brookesia/gui_interface/examples/example.hpp"
#include "common/example_base.hpp"

namespace esp_brookesia::gui::examples {

#if BROOKESIA_GUI_INTERFACE_EXAMPLE_VIEW_IMAGE
class ViewImageExample final : public detail::JsonExample {
public:
    ViewImageExample()
        : JsonExample(
              ExampleInfo{"view.image", "View", "Image", "Image resolved from an imageSet asset"},
              "examples/view/image.json",
              R"json({
    "version": "0.1.0",
    "assets": [
        {
            "type": "imageSet",
            "images": [
                { "id": "logo", "src": "images/brookesia_logo.png", "width": 96, "height": 96 }
            ]
        },
        {
            "type": "viewScreen",
            "id": "view_image",
            "style": { "bgColor": "#0f172a" },
            "children": [
                {
                    "type": "image",
                    "id": "logo",
                    "imageProps": { "src": "${image.logo}" },
                    "placement": { "mode": "relative", "align": "center" }
                }
            ]
        }
    ]
})json",
              "/view_image")
    {
    }

protected:
    bool on_verify(detail::Checker &checker, Runtime &, const Environment &, DocumentId) override
    {
        // The src binds to ${image.logo}; verify the image exists and resolved a non-empty source.
        auto v = checker.view("logo");
        if (!checker.check(v.valid(), "image not found: " + checker.resolve("logo"))) {
            return true;
        }
        checker.check(!v.as_image().src().empty(), "image 'logo' resolved an empty src");
        return true;
    }
};
BROOKESIA_PLUGIN_REGISTER(IExample, ViewImageExample, "view.image");
#endif

} // namespace esp_brookesia::gui::examples
