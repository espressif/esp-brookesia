/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include <expected>
#include <functional>
#include <string>
#include <vector>

#include "brookesia/gui_interface/document.hpp"
#include "brookesia/gui_interface/runtime.hpp"
#include "brookesia/lib_utils/plugin.hpp"

namespace esp_brookesia::gui::examples {

/**
 * @brief Platform-supplied input injection used by example self-verification.
 *
 * The runner that hosts the examples may provide a way to synthesize real pointer input (e.g. the
 * PC runner injects synthetic touch points through the display service and pumps LVGL). Examples use
 * it via @ref Checker to verify event-driven behavior with a genuine click. When no injector is
 * available (e.g. headless or a platform without input wiring), @ref tap is empty and event-driven
 * examples gracefully fall back to structural checks.
 */
struct InputProbe {
    /// Perform a complete press+release at absolute screen coordinates. Returns false if unsupported.
    std::function<bool(int32_t x, int32_t y)> tap;
};

/**
 * @brief Result of an optional example self-verification pass.
 *
 * Examples may verify their own mounted UI against expectations (see @ref IExample::verify). An
 * example that does not implement any checks reports @ref Status::Skipped. A passing run reports
 * @ref Status::Passed with no failures; a failing run reports @ref Status::Failed and one human
 * readable message per failed assertion.
 */
struct VerifyOutcome {
    enum class Status {
        Skipped, ///< The example implements no checks.
        Passed,  ///< All checks passed.
        Failed,  ///< One or more checks failed (see @ref failures).
    };

    Status status = Status::Skipped;
    std::vector<std::string> failures; ///< Populated only when @ref status is @ref Status::Failed.
};

/**
 * @brief Static metadata describing a single JSON-UI example.
 *
 * The `id` doubles as the plugin registry key and the menu action suffix, so it must be unique
 * across the whole example suite. The `category` groups entries on the launcher home page.
 */
struct ExampleInfo {
    std::string id;          ///< Unique example id, e.g. "view.button".
    std::string category;    ///< Launcher category, e.g. "View".
    std::string title;       ///< Short human-readable title shown on the menu button.
    std::string description; ///< One-line description of what the example demonstrates.
};

/**
 * @brief Interface implemented by every JSON-UI example plugin.
 *
 * Each example owns one JSON document. `run()` loads and mounts a visible screen and returns
 * (the example stays on screen for inspection); `stop()` tears the document down again. Instances
 * are created and cached by the @ref ExampleRegistry and driven by the shared `ExampleRunner`.
 */
class IExample {
public:
    virtual ~IExample() = default;

    /** @brief Return the static metadata for this example. */
    virtual ExampleInfo info() const = 0;

    /**
     * @brief Load and mount the example UI.
     *
     * @param[in] runtime     Runtime used to load/mount the document.
     * @param[in] environment Active environment (resolution, density, language, theme).
     * @return Empty on success, or an error string describing the failure.
     */
    virtual std::expected<void, std::string> run(Runtime &runtime, const Environment &environment) = 0;

    /** @brief Unmount and unload everything created by @ref run. */
    virtual void stop(Runtime &runtime) = 0;

    /**
     * @brief Optionally verify the mounted UI matches expectations.
     *
     * Called by the runner after @ref run succeeds and the UI has settled, but before @ref stop.
     * The default implementation performs no checks and returns @ref VerifyOutcome::Status::Skipped,
     * so examples without verification need not override it.
     *
     * @param[in] runtime     Runtime used to query the live view state.
     * @param[in] environment Active environment.
     * @param[in] input       Optional input injector for behavioral (click) verification.
     * @return The verification outcome.
     */
    virtual VerifyOutcome verify(Runtime &runtime, const Environment &environment, const InputProbe &input)
    {
        (void)runtime;
        (void)environment;
        (void)input;
        return {};
    }
};

/// Registry of all compiled-in example plugins, keyed by @ref ExampleInfo::id.
using ExampleRegistry = esp_brookesia::lib_utils::PluginRegistry<IExample>;

} // namespace esp_brookesia::gui::examples
