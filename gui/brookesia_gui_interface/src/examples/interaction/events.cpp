/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * interaction.events: clicked effects mutate other views (setProperties).
 */

#include "brookesia/gui_interface/examples/example.hpp"
#include "common/example_base.hpp"

namespace esp_brookesia::gui::examples {

#if BROOKESIA_GUI_INTERFACE_EXAMPLE_INTERACTION_EVENTS
class InteractionEventsExample final : public detail::JsonExample {
public:
    InteractionEventsExample()
        : JsonExample(
              ExampleInfo{"interaction.events", "Interaction", "Event Effects", "clicked effects mutate other views (setProperties)"},
              "examples/interaction/events.json",
              R"json({
    "version": "0.1.0",
    "assets": [
        {
            "type": "viewScreen",
            "id": "interaction_events",
            "style": { "bgColor": "#0f172a" },
            "layout": { "type": "flex", "flexFlow": "column", "mainAlign": "center", "crossAlign": "center", "gap": "20dp" },
            "children": [
                {
                    "type": "label",
                    "id": "status",
                    "labelProps": { "text": "Tap the button" },
                    "style": { "textColor": "#e2e8f0", "fontSize": "18sp" },
                    "placement": { "width": "240dp", "height": "28dp" }
                },
                {
                    "type": "button",
                    "id": "cta",
                    "style": { "bgColor": "#2563eb", "radius": "10dp" },
                    "events": [
                        {
                            "type": "clicked",
                            "effects": [
                                { "type": "setProperties", "updates": [
                                    { "target": "/interaction_events/status", "field": "labelProps.text", "value": "Effect fired!" },
                                    { "target": "/interaction_events/status", "field": "style.textColor", "value": "#22c55e" }
                                ] }
                            ]
                        }
                    ],
                    "placement": { "width": "180dp", "height": "52dp" },
                    "children": [
                        {
                            "type": "label",
                            "id": "label",
                            "labelProps": { "text": "Run effect" },
                            "style": { "textColor": "#ffffff" },
                            "placement": { "mode": "relative", "align": "center" }
                        }
                    ]
                }
            ]
        }
    ]
})json",
              "/interaction_events")
    {
    }

protected:
    bool on_verify(detail::Checker &checker, Runtime &, const Environment &, DocumentId) override
    {
        // Initial state from JSON.
        checker.check_label_text("status", "Tap the button");
        // With a real input injector, click the button and confirm the clicked-effect ran. Without
        // one, keep the structural assertion above (the example still passes).
        if (checker.has_input()) {
            if (checker.tap("cta")) {
                checker.check_label_text("status", "Effect fired!");
            }
        }
        return true;
    }
};
BROOKESIA_PLUGIN_REGISTER(IExample, InteractionEventsExample, "interaction.events");
#endif

} // namespace esp_brookesia::gui::examples
