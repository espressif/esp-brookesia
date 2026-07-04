/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * assets.theme: register a theme, activate it, then load a document whose styleSet resolves
 * ${color.*} against the active theme.
 */

#include <string>
#include <string_view>

#include "brookesia/gui_interface/examples/example.hpp"
#include "common/example_base.hpp"

namespace esp_brookesia::gui::examples {

#if BROOKESIA_GUI_INTERFACE_EXAMPLE_ASSETS_THEME
class AssetsThemeExample final : public IExample {
public:
    ExampleInfo info() const override
    {
        return ExampleInfo{"assets.theme", "Assets", "theme", "Theme colors resolved through ${color.*}"};
    }

    std::expected<void, std::string> run(Runtime &runtime, const Environment &environment) override
    {
        static constexpr std::string_view THEME_JSON = R"json({
    "type": "theme",
    "id": "examples.sunrise",
    "assets": [
        {
            "type": "constant",
            "data": {
                "colors": { "accent": "#f97316", "surface": "#7c2d12", "text": "#fef3c7" }
            }
        }
    ]
})json";
        static constexpr std::string_view DOC_JSON = R"json({
    "version": "0.1.0",
    "assets": [
        {
            "type": "styleSet",
            "styles": {
                "example.themeCard": { "bgColor": "${color.surface}", "textColor": "${color.text}", "radius": "10dp", "padding": "16dp" }
            }
        },
        {
            "type": "viewScreen",
            "id": "assets_theme",
            "style": { "bgColor": "#0b1220" },
            "children": [
                {
                    "type": "label",
                    "id": "title",
                    "labelProps": { "text": "Theme colors via ${color.*}" },
                    "styleRefs": [ "example.themeCard" ],
                    "style": { "fontSize": "16sp" },
                    "placement": { "mode": "relative", "align": "center", "width": "260dp", "height": "90dp" }
                }
            ]
        }
    ]
})json";

        auto theme = runtime.load_theme_json(THEME_JSON);
        if (!theme) {
            return std::unexpected("Failed to load theme: " + theme.error());
        }
        previous_theme_ = runtime.get_theme();
        auto activated = runtime.set_theme("examples.sunrise", false);
        if (!activated) {
            return std::unexpected("Failed to activate theme: " + activated.error());
        }

        // Load with the theme applied so ${color.*} resolves and the active theme is not reset back
        // to the environment default during the load.
        Environment themed_environment = environment;
        themed_environment.theme_id = "examples.sunrise";
        auto loaded = runtime.load_json("examples/assets/theme.json", DOC_JSON, detail::examples_asset_dir(), themed_environment);
        if (!loaded) {
            (void)runtime.set_theme(previous_theme_, true);
            return std::unexpected(loaded.error());
        }
        document_ = *loaded;
        loaded_ = true;

        auto mounted = runtime.mount_screen(document_, "/assets_theme");
        if (!mounted) {
            runtime.unload(document_);
            loaded_ = false;
            (void)runtime.set_theme(previous_theme_, true);
            return std::unexpected(mounted.error());
        }
        return {};
    }

    void stop(Runtime &runtime) override
    {
        if (loaded_) {
            runtime.unmount_screen(document_, "/assets_theme");
            runtime.unload(document_);
            loaded_ = false;
            document_ = {};
        }
        if (!previous_theme_.empty()) {
            (void)runtime.set_theme(previous_theme_, true);
            previous_theme_.clear();
        }
    }

    VerifyOutcome verify(Runtime &runtime, const Environment &environment, const InputProbe &input) override
    {
        if (!loaded_) {
            return {VerifyOutcome::Status::Failed, {"example is not mounted"}};
        }
        detail::Checker checker(runtime, environment, document_, "/assets_theme", &input);
        // The custom theme must be active and the styled card laid out (proving ${color.*} resolved).
        checker.check_theme("examples.sunrise");
        checker.require_view("title");
        checker.check_frame_nonempty("title");
        if (checker.ok()) {
            return {VerifyOutcome::Status::Passed, {}};
        }
        return {VerifyOutcome::Status::Failed, checker.failures()};
    }

private:
    DocumentId document_;
    bool loaded_ = false;
    std::string previous_theme_;
};
BROOKESIA_PLUGIN_REGISTER(IExample, AssetsThemeExample, "assets.theme");
#endif

} // namespace esp_brookesia::gui::examples
