/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * runtime.mount_stack: push_transient_screen overlays a base screen on a higher layer.
 */

#include "brookesia/gui_interface/examples/example.hpp"
#include "brookesia/lib_utils.hpp"
#include "common/example_base.hpp"

namespace esp_brookesia::gui::examples {

#if BROOKESIA_GUI_INTERFACE_EXAMPLE_RUNTIME_MOUNT_STACK
// Mount a base screen on the default layer, then push a transient overlay onto the top layer.
class RuntimeMountStackExample final : public IExample {
public:
    ExampleInfo info() const override
    {
        return ExampleInfo{"runtime.mount_stack", "Runtime", "Mount Stack", "push_transient_screen overlays a base screen on a higher layer"};
    }

    std::expected<void, std::string> run(Runtime &runtime, const Environment &environment) override
    {
        static constexpr std::string_view DOC_JSON = R"json({
    "version": "0.1.0",
    "assets": [
        {
            "type": "viewScreen",
            "id": "stack_base",
            "style": { "bgColor": "#0f172a" },
            "children": [
                {
                    "type": "label", "id": "title", "labelProps": { "text": "Base screen (Default layer)" },
                    "style": { "textColor": "#e2e8f0", "fontSize": "18sp" },
                    "placement": { "mode": "relative", "align": "center" }
                }
            ]
        },
        {
            "type": "viewScreen",
            "id": "stack_overlay",
            "commonProps": { "clickable": false },
            "children": [
                {
                    "type": "container",
                    "id": "toast",
                    "style": { "bgColor": "#2563eb", "radius": "12dp", "shadowWidth": "16dp", "shadowColor": "#020617" },
                    "placement": { "mode": "absolute", "align": "bottomMid", "y": "-40dp", "width": "260dp", "height": "60dp" },
                    "children": [
                        {
                            "type": "label", "id": "text", "labelProps": { "text": "Transient overlay (Top layer)" },
                            "style": { "textColor": "#ffffff" },
                            "placement": { "mode": "relative", "align": "center" }
                        }
                    ]
                }
            ]
        }
    ]
})json";

        auto loaded = runtime.load_json("examples/runtime/mount_stack.json", DOC_JSON, detail::examples_asset_dir(), environment);
        if (!loaded) {
            return std::unexpected(loaded.error());
        }
        document_ = *loaded;
        loaded_ = true;

        auto base = runtime.mount_screen(document_, "/stack_base");
        if (!base) {
            runtime.unload(document_);
            loaded_ = false;
            return std::unexpected(base.error());
        }
        base_mounted_ = true;

        MountTarget overlay_target;
        overlay_target.layer = GuiLayer::Top;
        auto overlay = runtime.push_transient_screen(document_, "/stack_overlay", overlay_target);
        if (!overlay) {
            BROOKESIA_LOGW("[RuntimeMountStack] push overlay failed: %1%", overlay.error());
        } else {
            overlay_id_ = *overlay;
            overlay_pushed_ = true;
        }
        return {};
    }

    void stop(Runtime &runtime) override
    {
        if (overlay_pushed_) {
            runtime.pop_transient_screen(overlay_id_);
            overlay_pushed_ = false;
        }
        if (base_mounted_) {
            runtime.unmount_screen(document_, "/stack_base");
            base_mounted_ = false;
        }
        if (loaded_) {
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
        detail::Checker checker(runtime, environment, document_, "/stack_base", &input);
        // The base screen label must be present on the default layer.
        checker.check_label_text("/stack_base/title", "Base screen (Default layer)");
        // The transient overlay (pushed onto the Top layer) and its toast text must also be live.
        checker.check(overlay_pushed_, "transient overlay was not pushed");
        if (overlay_pushed_) {
            checker.require_view("/stack_overlay/toast");
            checker.check_label_text("/stack_overlay/toast/text", "Transient overlay (Top layer)");
        }
        if (checker.ok()) {
            return {VerifyOutcome::Status::Passed, {}};
        }
        return {VerifyOutcome::Status::Failed, checker.failures()};
    }

private:
    DocumentId document_;
    bool loaded_ = false;
    bool base_mounted_ = false;
    bool overlay_pushed_ = false;
    TransientMountId overlay_id_{};
};
BROOKESIA_PLUGIN_REGISTER(IExample, RuntimeMountStackExample, "runtime.mount_stack");
#endif

} // namespace esp_brookesia::gui::examples
