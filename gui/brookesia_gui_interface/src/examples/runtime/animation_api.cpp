/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * runtime.animation_api: start_view_animation drives a node imperatively.
 */

#include "brookesia/gui_interface/examples/example.hpp"
#include "common/example_base.hpp"

namespace esp_brookesia::gui::examples {

#if BROOKESIA_GUI_INTERFACE_EXAMPLE_RUNTIME_ANIMATION_API
class RuntimeAnimationApiExample final : public detail::JsonExample {
public:
    RuntimeAnimationApiExample()
        : JsonExample(
              ExampleInfo{"runtime.animation_api", "Runtime", "Animation API", "start_view_animation drives a node imperatively"},
              "examples/runtime/animation_api.json",
              R"json({
    "version": "0.1.0",
    "assets": [
        {
            "type": "viewScreen",
            "id": "runtime_animation_api",
            "style": { "bgColor": "#0f172a" },
            "children": [
                {
                    "type": "container",
                    "id": "ball",
                    "style": { "bgColor": "#38bdf8", "radius": "24dp" },
                    "placement": { "mode": "absolute", "align": "topMid", "y": "40dp", "width": "48dp", "height": "48dp" }
                }
            ]
        }
    ]
})json",
              "/runtime_animation_api")
    {
    }

protected:
    std::expected<void, std::string> on_mounted(Runtime &runtime, const Environment &, DocumentId document) override
    {
        Animation animation;
        animation.id = "ball_bounce";
        animation.trigger = AnimationTrigger::Manual;
        animation.property = AnimationProperty::Y;
        animation.from_mode = AnimationValueMode::Current;
        animation.from = 0;
        animation.to_mode = AnimationValueMode::Relative;
        animation.to = 200;
        animation.duration = 700;
        animation.easing = AnimationEasing::EaseInOut;
        animation.repeat = 1000000;
        animation.playback = true;
        animation_ = runtime.start_view_animation(document, "/runtime_animation_api/ball", animation);
        return {};
    }

    void on_stopping(Runtime &, DocumentId) override
    {
        animation_.disconnect();
    }

    bool on_verify(detail::Checker &checker, Runtime &, const Environment &, DocumentId) override
    {
        // The imperatively animated node must exist and have concrete geometry.
        checker.require_view("ball");
        checker.check_frame_nonempty("ball");
        return true;
    }

private:
    ScopedConnection animation_;
};
BROOKESIA_PLUGIN_REGISTER(IExample, RuntimeAnimationApiExample, "runtime.animation_api");
#endif

} // namespace esp_brookesia::gui::examples
