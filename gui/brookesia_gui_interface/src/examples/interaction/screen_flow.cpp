/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * interaction.screen_flow: two screens switched by a screenFlow state machine.
 */

#include <vector>

#include "brookesia/gui_interface/examples/example.hpp"
#include "common/example_base.hpp"

namespace esp_brookesia::gui::examples {

#if BROOKESIA_GUI_INTERFACE_EXAMPLE_INTERACTION_SCREEN_FLOW
// Screen flow drives mutually-exclusive top-level screens. Buttons emit actions that the example
// forwards to Runtime::trigger_screen_flow.
class InteractionScreenFlowExample final : public IExample {
public:
    ExampleInfo info() const override
    {
        return ExampleInfo{"interaction.screen_flow", "Interaction", "Screen Flow", "Two screens switched by a screenFlow state machine"};
    }

    std::expected<void, std::string> run(Runtime &runtime, const Environment &environment) override
    {
        static constexpr std::string_view DOC_JSON = R"json({
    "version": "0.1.0",
    "assets": [
        {
            "type": "viewScreen",
            "id": "flow_home",
            "style": { "bgColor": "#0f172a" },
            "layout": { "type": "flex", "flexFlow": "column", "mainAlign": "center", "crossAlign": "center", "gap": "20dp" },
            "children": [
                {
                    "type": "label", "id": "title", "labelProps": { "text": "Home screen" },
                    "style": { "textColor": "#e2e8f0", "fontSize": "22sp" },
                    "placement": { "width": "240dp", "height": "30dp" }
                },
                {
                    "type": "button", "id": "go", "style": { "bgColor": "#2563eb", "radius": "10dp" },
                    "events": [ { "type": "clicked", "action": "flow.open_settings" } ],
                    "placement": { "width": "200dp", "height": "52dp" },
                    "children": [ { "type": "label", "id": "label", "labelProps": { "text": "Open settings" }, "style": { "textColor": "#ffffff" }, "placement": { "mode": "relative", "align": "center" } } ]
                }
            ]
        },
        {
            "type": "viewScreen",
            "id": "flow_settings",
            "style": { "bgColor": "#1e293b" },
            "layout": { "type": "flex", "flexFlow": "column", "mainAlign": "center", "crossAlign": "center", "gap": "20dp" },
            "children": [
                {
                    "type": "label", "id": "title", "labelProps": { "text": "Settings screen" },
                    "style": { "textColor": "#f8fafc", "fontSize": "22sp" },
                    "placement": { "width": "240dp", "height": "30dp" }
                },
                {
                    "type": "button", "id": "back", "style": { "bgColor": "#22c55e", "radius": "10dp" },
                    "events": [ { "type": "clicked", "action": "flow.open_home" } ],
                    "placement": { "width": "200dp", "height": "52dp" },
                    "children": [ { "type": "label", "id": "label", "labelProps": { "text": "Back home" }, "style": { "textColor": "#06281a" }, "placement": { "mode": "relative", "align": "center" } } ]
                }
            ]
        },
        {
            "type": "screenFlow",
            "id": "main",
            "screens": [ "flow_home", "flow_settings" ],
            "initial": "flow_home",
            "transitions": [
                { "from": [], "action": "open_home", "to": "flow_home" },
                { "from": [], "action": "open_settings", "to": "flow_settings" }
            ]
        }
    ]
})json";

        auto loaded = runtime.load_json("examples/interaction/screen_flow.json", DOC_JSON, detail::examples_asset_dir(), environment);
        if (!loaded) {
            return std::unexpected(loaded.error());
        }
        document_ = *loaded;
        loaded_ = true;

        connections_.push_back(runtime.subscribe_event_action(document_, "flow.open_settings", [&runtime, this](const Event &) {
            (void)runtime.trigger_screen_flow(document_, "main", "open_settings");
        }));
        connections_.push_back(runtime.subscribe_event_action(document_, "flow.open_home", [&runtime, this](const Event &) {
            (void)runtime.trigger_screen_flow(document_, "main", "open_home");
        }));

        auto started = runtime.start_screen_flow(document_, "main");
        if (!started) {
            connections_.clear();
            runtime.unload(document_);
            loaded_ = false;
            return std::unexpected(started.error());
        }
        return {};
    }

    void stop(Runtime &runtime) override
    {
        connections_.clear();
        if (loaded_) {
            runtime.stop_screen_flow(document_, "main");
            runtime.unload(document_);
            loaded_ = false;
            document_ = {};
        }
    }

    VerifyOutcome verify(Runtime &runtime, const Environment &environment, const InputProbe &input) override
    {
        if (!loaded_) {
            return {VerifyOutcome::Status::Failed, {"example is not mounted"}};
        }
        detail::Checker checker(runtime, environment, document_, "/flow_home", &input);

        // The flow starts on the home screen.
        auto state = runtime.get_screen_flow_state(document_, "main");
        checker.check(state.has_value() && *state == "flow_home",
                      "screen flow 'main' expected initial state 'flow_home' but got '"
                      + state.value_or("<none>") + "'");
        checker.check_label_text("/flow_home/title", "Home screen");

        // With a real input injector, click "Open settings" and confirm the flow transitions.
        if (checker.has_input()) {
            if (checker.tap("/flow_home/go")) {
                auto next = runtime.get_screen_flow_state(document_, "main");
                checker.check(next.has_value() && *next == "flow_settings",
                              "after click expected screen flow state 'flow_settings' but got '"
                              + next.value_or("<none>") + "'");
            }
        }

        if (checker.ok()) {
            return {VerifyOutcome::Status::Passed, {}};
        }
        return {VerifyOutcome::Status::Failed, checker.failures()};
    }

private:
    DocumentId document_;
    bool loaded_ = false;
    std::vector<ScopedConnection> connections_;
};
BROOKESIA_PLUGIN_REGISTER(IExample, InteractionScreenFlowExample, "interaction.screen_flow");
#endif

} // namespace esp_brookesia::gui::examples
