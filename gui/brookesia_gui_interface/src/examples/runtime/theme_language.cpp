/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * runtime.theme_language: set_theme switches the active palette at runtime.
 */

#include <string>
#include <string_view>

#include "brookesia/gui_interface/examples/example.hpp"
#include "brookesia/lib_utils.hpp"
#include "common/example_base.hpp"

namespace esp_brookesia::gui::examples {

#if BROOKESIA_GUI_INTERFACE_EXAMPLE_RUNTIME_THEME_LANGUAGE
// Register a theme at runtime, activate it, and restore the previous theme on stop.
class RuntimeThemeLanguageExample final : public IExample {
public:
    ExampleInfo info() const override
    {
        return ExampleInfo{"runtime.theme_language", "Runtime", "Theme / Language", "set_theme switches active palette at runtime"};
    }

    std::expected<void, std::string> run(Runtime &runtime, const Environment &environment) override
    {
        static constexpr std::string_view THEME_JSON = R"json({
    "type": "theme",
    "id": "examples.aqua",
    "assets": [
        {
            "type": "constant",
            "data": {
                "colors": { "bg": "#042f2e", "fg": "#5eead4", "accent": "#22d3ee" }
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
                "example.aquaScreen": { "bgColor": "${color.bg}" },
                "example.aquaTitle": { "textColor": "${color.fg}", "fontSize": "22sp" },
                "example.aquaPill": { "bgColor": "${color.accent}", "radius": "14dp" }
            }
        },
        {
            "type": "viewScreen",
            "id": "runtime_theme_language",
            "styleRefs": [ "example.aquaScreen" ],
            "layout": { "type": "flex", "flexFlow": "column", "mainAlign": "center", "crossAlign": "center", "gap": "18dp" },
            "children": [
                {
                    "type": "label", "id": "title", "labelProps": { "text": "Active theme: examples.aqua" },
                    "styleRefs": [ "example.aquaTitle" ],
                    "placement": { "width": "300dp", "height": "30dp" }
                },
                {
                    "type": "container", "id": "pill", "styleRefs": [ "example.aquaPill" ],
                    "placement": { "width": "120dp", "height": "28dp" }
                }
            ]
        }
    ]
})json";

        auto theme = runtime.load_theme_json(THEME_JSON);
        if (!theme) {
            return std::unexpected("load theme failed: " + theme.error());
        }
        previous_theme_ = runtime.get_theme();
        auto activated = runtime.set_theme("examples.aqua", false);
        if (!activated) {
            return std::unexpected("set_theme failed: " + activated.error());
        }

        BROOKESIA_LOGI("[RuntimeThemeLanguage] languages: %1%", runtime.list_supported_languages().size());

        // Load with the theme applied so ${color.*} resolves and the active theme is not reset back
        // to the environment default during the load.
        Environment themed_environment = environment;
        themed_environment.theme_id = "examples.aqua";
        auto loaded = runtime.load_json("examples/runtime/theme_language.json", DOC_JSON, detail::examples_asset_dir(), themed_environment);
        if (!loaded) {
            (void)runtime.set_theme(previous_theme_, true);
            return std::unexpected(loaded.error());
        }
        document_ = *loaded;
        loaded_ = true;

        auto mounted = runtime.mount_screen(document_, "/runtime_theme_language");
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
            runtime.unmount_screen(document_, "/runtime_theme_language");
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
        detail::Checker checker(runtime, environment, document_, "/runtime_theme_language", &input);
        // The runtime-activated theme must be live and the themed views laid out.
        checker.check_theme("examples.aqua");
        checker.check_label_text("title", "Active theme: examples.aqua");
        checker.check_frame_nonempty("pill");
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
BROOKESIA_PLUGIN_REGISTER(IExample, RuntimeThemeLanguageExample, "runtime.theme_language");
#endif

} // namespace esp_brookesia::gui::examples
