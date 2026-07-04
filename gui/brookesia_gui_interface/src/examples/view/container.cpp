/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * view.container: container holding a child label.
 */

#include "brookesia/gui_interface/examples/example.hpp"
#include "common/example_base.hpp"

namespace esp_brookesia::gui::examples {

#if BROOKESIA_GUI_INTERFACE_EXAMPLE_VIEW_CONTAINER
class ViewContainerExample final : public detail::JsonExample {
public:
    ViewContainerExample()
        : JsonExample(
              ExampleInfo{"view.container", "View", "Container", "Container holding a child label"},
              "examples/view/container.json",
              R"json({
    "version": "0.1.0",
    "assets": [
        {
            "type": "viewScreen",
            "id": "view_container",
            "style": { "bgColor": "#0f172a" },
            "children": [
                {
                    "type": "container",
                    "id": "card",
                    "style": { "bgColor": "#1e293b", "radius": "10dp", "padding": "16dp" },
                    "placement": { "mode": "relative", "align": "center", "width": "180dp", "height": "100dp" },
                    "children": [
                        {
                            "type": "label",
                            "id": "title",
                            "labelProps": { "text": "Container" },
                            "style": { "textColor": "#e2e8f0" },
                            "placement": { "mode": "relative", "align": "center" }
                        }
                    ]
                }
            ]
        }
    ]
})json",
              "/view_container")
    {
    }

protected:
    bool on_verify(detail::Checker &checker, Runtime &, const Environment &, DocumentId) override
    {
        checker.check_frame_nonempty("card");
        checker.check_label_text("card/title", "Container");
        return true;
    }
};
BROOKESIA_PLUGIN_REGISTER(IExample, ViewContainerExample, "view.container");
#endif

} // namespace esp_brookesia::gui::examples
