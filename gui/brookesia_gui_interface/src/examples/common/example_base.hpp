/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "brookesia/gui_interface/examples/example.hpp"
#include "example_macros.h"

namespace esp_brookesia::gui::examples::detail {

/// Root directory that holds the shared binary assets (fonts/images) used by some examples.
inline std::string examples_asset_dir()
{
#if defined(ESP_PLATFORM)
    return "/littlefs/assets";
#elif defined(EXAMPLES_ASSETS_DIR)
    return std::string(EXAMPLES_ASSETS_DIR);
#else
    return "assets_bin";
#endif
}

/// Build an absolute path to a shared binary asset, e.g. make_asset_path("images/logo.png").
inline std::string make_asset_path(std::string_view relative)
{
    auto base = examples_asset_dir();
    if (!base.empty() && base.back() != '/') {
        base.push_back('/');
    }
    base.append(relative);
    return base;
}

/**
 * @brief Assertion helper for example self-verification.
 *
 * A Checker is handed to an example after its screen is mounted. It resolves node paths (relative
 * to the example screen root, or absolute when they start with '/'), reads back live view state via
 * the Runtime, and accumulates a human-readable message for every failed assertion. The owning
 * @ref JsonExample turns the accumulated failures into a @ref VerifyOutcome.
 */
class Checker {
public:
    Checker(Runtime &runtime, const Environment &environment, DocumentId document, std::string screen_path,
            const InputProbe *input = nullptr)
        : runtime_(runtime)
        , environment_(environment)
        , document_(document)
        , screen_path_(std::move(screen_path))
        , input_(input)
    {
    }

    Runtime &runtime() const
    {
        return runtime_;
    }

    const Environment &environment() const
    {
        return environment_;
    }

    DocumentId document() const
    {
        return document_;
    }

    const std::vector<std::string> &failures() const
    {
        return failures_;
    }

    bool ok() const
    {
        return failures_.empty();
    }

    /// Resolve a node path: absolute (leading '/') is used as-is, otherwise appended to the screen root.
    std::string resolve(std::string_view path) const
    {
        if (!path.empty() && path.front() == '/') {
            return std::string(path);
        }
        std::string full = screen_path_;
        if (!path.empty()) {
            full.push_back('/');
            full.append(path);
        }
        return full;
    }

    /// Find the view at @p path (relative to the screen root, or absolute).
    View view(std::string_view path) const
    {
        return runtime_.find_view(document_, resolve(path));
    }

    /// Record a failure unconditionally and return false (convenience for early-out chains).
    bool fail(std::string message)
    {
        failures_.push_back(std::move(message));
        return false;
    }

    /// Record a failure with @p message when @p condition is false.
    bool check(bool condition, std::string message)
    {
        if (!condition) {
            return fail(std::move(message));
        }
        return true;
    }

    bool require_view(std::string_view path)
    {
        if (!view(path).valid()) {
            return fail("view not found: " + resolve(path));
        }
        return true;
    }

    bool check_label_text(std::string_view path, std::string_view expected)
    {
        auto v = view(path);
        if (!v.valid()) {
            return fail("label not found: " + resolve(path));
        }
        const std::string actual = v.as_label().text();
        if (actual != expected) {
            return fail("label '" + resolve(path) + "' text expected '" + std::string(expected) + "' but got '"
                        + actual + "'");
        }
        return true;
    }

    bool check_slider_value(std::string_view path, int32_t expected)
    {
        return check_int_value(path, expected, "slider", [](const View & v) {
            return v.as_slider().value();
        });
    }

    bool check_progress_value(std::string_view path, int32_t expected)
    {
        return check_int_value(path, expected, "progressBar",
        [](const View & v) {
            return v.as_progress_bar().value();
        });
    }

    bool check_arc_value(std::string_view path, int32_t expected)
    {
        return check_int_value(path, expected, "arc", [](const View & v) {
            return v.as_arc().value();
        });
    }

    bool check_switch_checked(std::string_view path, bool expected)
    {
        auto v = view(path);
        if (!v.valid()) {
            return fail("switch not found: " + resolve(path));
        }
        const bool actual = v.as_switch().checked();
        if (actual != expected) {
            return fail("switch '" + resolve(path) + "' checked expected " + bool_str(expected) + " but got "
                        + bool_str(actual));
        }
        return true;
    }

    bool check_checkbox_checked(std::string_view path, bool expected)
    {
        auto v = view(path);
        if (!v.valid()) {
            return fail("checkbox not found: " + resolve(path));
        }
        const bool actual = v.as_checkbox().checked();
        if (actual != expected) {
            return fail("checkbox '" + resolve(path) + "' checked expected " + bool_str(expected) + " but got "
                        + bool_str(actual));
        }
        return true;
    }

    bool check_dropdown_index(std::string_view path, int32_t expected)
    {
        auto v = view(path);
        if (!v.valid()) {
            return fail("dropdown not found: " + resolve(path));
        }
        const int32_t actual = v.as_dropdown().selected_index();
        if (actual != expected) {
            return fail("dropdown '" + resolve(path) + "' selectedIndex expected " + std::to_string(expected)
                        + " but got " + std::to_string(actual));
        }
        return true;
    }

    bool check_image_src(std::string_view path, std::string_view expected)
    {
        auto v = view(path);
        if (!v.valid()) {
            return fail("image not found: " + resolve(path));
        }
        const std::string actual = v.as_image().src();
        if (actual != expected) {
            return fail("image '" + resolve(path) + "' src expected '" + std::string(expected) + "' but got '"
                        + actual + "'");
        }
        return true;
    }

    bool check_text_input_text(std::string_view path, std::string_view expected)
    {
        auto v = view(path);
        if (!v.valid()) {
            return fail("textInput not found: " + resolve(path));
        }
        const std::string actual = v.as_text_input().text();
        if (actual != expected) {
            return fail("textInput '" + resolve(path) + "' text expected '" + std::string(expected) + "' but got '"
                        + actual + "'");
        }
        return true;
    }

    bool check_binding(std::string_view path, std::string_view key, std::string_view expected)
    {
        auto value = runtime_.get_binding_value(document_, resolve(path), key);
        if (!value) {
            return fail("binding '" + std::string(key) + "' not found on '" + resolve(path) + "'");
        }
        if (*value != expected) {
            return fail("binding '" + std::string(key) + "' on '" + resolve(path) + "' expected '"
                        + std::string(expected) + "' but got '" + *value + "'");
        }
        return true;
    }

    /// Read the resolved on-screen frame (geometry) of a node, if available.
    std::optional<ViewFrame> frame(std::string_view path) const
    {
        auto state = runtime_.get_view_state(document_, resolve(path), ViewStateKind::Frame);
        if (!state) {
            return std::nullopt;
        }
        if (const auto *value = std::get_if<ViewFrame>(&*state)) {
            return *value;
        }
        return std::nullopt;
    }

    bool check_frame_nonempty(std::string_view path)
    {
        auto f = frame(path);
        if (!f) {
            return fail("frame not available for '" + resolve(path) + "'");
        }
        if (f->width <= 0 || f->height <= 0) {
            return fail("frame for '" + resolve(path) + "' is empty (" + std::to_string(f->width) + "x"
                        + std::to_string(f->height) + ")");
        }
        return true;
    }

    /// Assert that @p below_path is positioned entirely below @p above_path (non-overlapping in Y).
    bool check_below(std::string_view below_path, std::string_view above_path)
    {
        auto below = frame(below_path);
        auto above = frame(above_path);
        if (!below) {
            return fail("frame not available for '" + resolve(below_path) + "'");
        }
        if (!above) {
            return fail("frame not available for '" + resolve(above_path) + "'");
        }
        if (below->y < above->y + above->height) {
            return fail("expected '" + resolve(below_path) + "' (y=" + std::to_string(below->y) + ") below '"
                        + resolve(above_path) + "' (y+h=" + std::to_string(above->y + above->height) + ")");
        }
        return true;
    }

    /// Whether a real input injector is available (for behavioral/click verification).
    bool has_input() const
    {
        return (input_ != nullptr) && static_cast<bool>(input_->tap);
    }

    /**
     * @brief Perform a real click at the center of the view at @p path.
     *
     * Resolves the on-screen frame, computes its center, and asks the platform input injector to
     * synthesize a press+release there. Returns false (without recording a failure) when no injector
     * is available, so callers can fall back to structural checks; records a failure if the view has
     * no resolvable geometry.
     */
    bool tap(std::string_view path)
    {
        if (!has_input()) {
            return false;
        }
        auto f = frame(path);
        if (!f) {
            return fail("cannot tap '" + resolve(path) + "': frame not available");
        }
        const int32_t cx = f->x + (f->width / 2);
        const int32_t cy = f->y + (f->height / 2);
        if (!input_->tap(cx, cy)) {
            return fail("input injector failed to tap '" + resolve(path) + "'");
        }
        return true;
    }

    bool check_theme(std::string_view expected)
    {
        const std::string actual = runtime_.get_theme();
        if (actual != expected) {
            return fail("active theme expected '" + std::string(expected) + "' but got '" + actual + "'");
        }
        return true;
    }

    bool check_language(std::string_view expected)
    {
        const std::string actual = runtime_.get_language();
        if (actual != expected) {
            return fail("active language expected '" + std::string(expected) + "' but got '" + actual + "'");
        }
        return true;
    }

private:
    static std::string bool_str(bool value)
    {
        return value ? "true" : "false";
    }

    template <typename Getter>
    bool check_int_value(std::string_view path, int32_t expected, std::string_view kind, Getter getter)
    {
        auto v = view(path);
        if (!v.valid()) {
            return fail(std::string(kind) + " not found: " + resolve(path));
        }
        const int32_t actual = getter(v);
        if (actual != expected) {
            return fail(std::string(kind) + " '" + resolve(path) + "' value expected " + std::to_string(expected)
                        + " but got " + std::to_string(actual));
        }
        return true;
    }

    Runtime &runtime_;
    const Environment &environment_;
    DocumentId document_;
    std::string screen_path_;
    const InputProbe *input_ = nullptr;
    std::vector<std::string> failures_;
};

/**
 * @brief Convenience base for examples whose UI is fully described by an inline JSON document.
 *
 * Subclasses provide their @ref ExampleInfo, the JSON text, a virtual root path and the absolute
 * screen path to mount. They may override @ref on_mounted / @ref on_stopping to drive the Runtime
 * API (bindings, animations, themes, templates, screen flows) after the screen is on display.
 */
class JsonExample : public IExample {
public:
    JsonExample(ExampleInfo info, std::string root_path, std::string json, std::string screen_path)
        : info_(std::move(info))
        , root_path_(std::move(root_path))
        , json_(std::move(json))
        , screen_path_(std::move(screen_path))
    {
    }

    ExampleInfo info() const override
    {
        return info_;
    }

    std::expected<void, std::string> run(Runtime &runtime, const Environment &environment) override
    {
        auto loaded = runtime.load_json(root_path_, json_, examples_asset_dir(), environment);
        if (!loaded) {
            return std::unexpected(loaded.error());
        }
        document_id_ = *loaded;
        loaded_ = true;

        auto mounted = runtime.mount_screen(document_id_, screen_path_);
        if (!mounted) {
            runtime.unload(document_id_);
            loaded_ = false;
            return std::unexpected(mounted.error());
        }

        auto hook = on_mounted(runtime, environment, document_id_);
        if (!hook) {
            runtime.unmount_screen(document_id_, screen_path_);
            runtime.unload(document_id_);
            loaded_ = false;
            return std::unexpected(hook.error());
        }
        return {};
    }

    void stop(Runtime &runtime) override
    {
        if (!loaded_) {
            return;
        }
        on_stopping(runtime, document_id_);
        runtime.unmount_screen(document_id_, screen_path_);
        runtime.unload(document_id_);
        loaded_ = false;
        document_id_ = {};
    }

    VerifyOutcome verify(Runtime &runtime, const Environment &environment, const InputProbe &input) override
    {
        if (!loaded_) {
            return {VerifyOutcome::Status::Failed, {"example is not mounted"}};
        }
        Checker checker(runtime, environment, document_id_, screen_path_, &input);
        if (!on_verify(checker, runtime, environment, document_id_)) {
            return {}; // Subclass implements no checks -> Skipped.
        }
        if (checker.ok()) {
            return {VerifyOutcome::Status::Passed, {}};
        }
        return {VerifyOutcome::Status::Failed, checker.failures()};
    }

protected:
    /// Hook invoked after the screen is mounted. Default does nothing.
    virtual std::expected<void, std::string> on_mounted(Runtime &, const Environment &, DocumentId)
    {
        return {};
    }

    /// Hook invoked right before the document is unmounted/unloaded. Default does nothing.
    virtual void on_stopping(Runtime &, DocumentId) {}

    /**
     * @brief Hook for self-verification. Override to assert on the mounted UI via @p checker.
     *
     * Return false (the default) to indicate the example implements no checks (reported as
     * @ref VerifyOutcome::Status::Skipped). Return true to run verification; the outcome is then
     * Passed/Failed depending on whether @p checker accumulated any failures.
     */
    virtual bool on_verify(Checker &, Runtime &, const Environment &, DocumentId)
    {
        return false;
    }

    const std::string &screen_path() const
    {
        return screen_path_;
    }

private:
    ExampleInfo info_;
    std::string root_path_;
    std::string json_;
    std::string screen_path_;
    DocumentId document_id_;
    bool loaded_ = false;
};

} // namespace esp_brookesia::gui::examples::detail
