/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * interaction.animations: mount fade/slide plus event-driven manual animation.
 */

#include "brookesia/gui_interface/examples/example.hpp"
#include "common/example_base.hpp"

namespace esp_brookesia::gui::examples {

#if BROOKESIA_GUI_INTERFACE_EXAMPLE_INTERACTION_ANIMATIONS
class InteractionAnimationsExample final : public detail::JsonExample {
public:
    InteractionAnimationsExample()
        : JsonExample(
              ExampleInfo{"interaction.animations", "Interaction", "Animations", "mount fade/slide plus event-driven manual animation"},
              "examples/interaction/animations.json",
              R"json({
    "version": "0.1.0",
    "assets": [
        {
            "type": "viewScreen",
            "id": "interaction_animations",
            "style": { "bgColor": "#0f172a" },
            "layout": { "type": "flex", "flexFlow": "column", "mainAlign": "center", "crossAlign": "center", "gap": "24dp" },
            "children": [
                {
                    "type": "label",
                    "id": "title",
                    "labelProps": { "text": "Animations" },
                    "style": { "textColor": "#e2e8f0", "fontSize": "22sp" },
                    "animations": [
                        { "id": "title_fade", "trigger": "mount", "property": "opacity", "from": 0, "to": 255, "duration": 400, "easing": "easeOut" }
                    ],
                    "placement": { "width": "200dp", "height": "30dp" }
                },
                {
                    "type": "button",
                    "id": "cta",
                    "style": { "bgColor": "#2563eb", "radius": "10dp" },
                    "animations": [
                        { "id": "press_down", "trigger": "manual", "property": "scale", "fromMode": "current", "from": 0, "toMode": "absolute", "to": 230, "duration": 120, "easing": "easeOut" },
                        { "id": "press_up", "trigger": "manual", "property": "scale", "fromMode": "current", "from": 0, "toMode": "absolute", "to": 256, "duration": 160, "easing": "overshoot" }
                    ],
                    "events": [
                        { "type": "pressed", "effects": [ { "type": "startAnimation", "target": "self", "animationId": "press_down" } ] },
                        { "type": "released", "effects": [ { "type": "startAnimation", "target": "self", "animationId": "press_up" } ] }
                    ],
                    "placement": { "width": "200dp", "height": "56dp" },
                    "children": [
                        {
                            "type": "label",
                            "id": "label",
                            "labelProps": { "text": "Press to animate" },
                            "style": { "textColor": "#ffffff" },
                            "placement": { "mode": "relative", "align": "center" }
                        }
                    ]
                }
            ]
        }
    ]
})json",
              "/interaction_animations")
    {
    }

protected:
    bool on_verify(detail::Checker &checker, Runtime &, const Environment &, DocumentId) override
    {
        // Structural: the animated title and button laid out; the mount fade leaves them present.
        checker.check_label_text("title", "Animations");
        checker.check_frame_nonempty("title");
        checker.check_frame_nonempty("cta");
        checker.check_label_text("cta/label", "Press to animate");
        return true;
    }
};
BROOKESIA_PLUGIN_REGISTER(IExample, InteractionAnimationsExample, "interaction.animations");
#endif

} // namespace esp_brookesia::gui::examples
