/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * PC entry point that runs the whole JSON-UI example suite in a single SDL window.
 *
 * It boots the LVGL display source through the Brookesia service stack (no system_super), then
 * hands a gui::Runtime + lvgl::Backend to the shared ExampleRunner. The runner builds a
 * categorized home menu (itself authored in the JSON-UI protocol), launches the selected example
 * on a fresh screen, and keeps a floating "back" button on the top layer to return to the menu.
 *
 * In addition to the interactive menu, a non-interactive `--smoke` mode mounts each selected (or
 * every) example for a fixed duration and then tears it down, which is handy for CI / regression
 * checks of the whole example set.
 */

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "boost/chrono.hpp"
#include "boost/json.hpp"
#include "boost/thread.hpp"
#include "brookesia/gui_interface.hpp"
#include "brookesia/gui_lvgl.hpp"
#include "brookesia/gui_lvgl/backend.hpp"
#include "brookesia/gui_lvgl/display_source.hpp"
#include "brookesia/hal_linux.hpp"
#include "brookesia/hal_linux/display/device.hpp"
#include "brookesia/lib_utils.hpp"
#include "brookesia/service_display/service_display.hpp"
#include "brookesia/service_helper.hpp"
#include "brookesia/service_manager.hpp"

using namespace esp_brookesia;
using DisplayHelper = service::helper::Display;
using ExampleRegistry = gui::examples::ExampleRegistry;

namespace {

#ifndef BROOKESIA_GUI_LVGL_EXAMPLES_WINDOW_WIDTH
#define BROOKESIA_GUI_LVGL_EXAMPLES_WINDOW_WIDTH 800
#endif
#ifndef BROOKESIA_GUI_LVGL_EXAMPLES_WINDOW_HEIGHT
#define BROOKESIA_GUI_LVGL_EXAMPLES_WINDOW_HEIGHT 480
#endif

constexpr int32_t WINDOW_WIDTH = BROOKESIA_GUI_LVGL_EXAMPLES_WINDOW_WIDTH;
constexpr int32_t WINDOW_HEIGHT = BROOKESIA_GUI_LVGL_EXAMPLES_WINDOW_HEIGHT;
constexpr int TIMER_DELAY_MS = 5;
constexpr uint32_t DISPLAY_SERVICE_TIMEOUT_MS = 1000;
constexpr int DEFAULT_SMOKE_DURATION_MS = 800;
// How long a synthetic press/release is held so the LVGL timer thread can observe each phase.
constexpr int TAP_HOLD_MS = 60;
constexpr int TAP_SETTLE_MS = 60;

struct CliOptions {
    bool help = false;
    bool list = false;
    bool smoke = false;
    bool verify = true;   ///< Run each example's self-verification after it mounts (smoke mode).
    bool strict = false;  ///< Treat examples without checks (SKIP) as failures.
    int smoke_duration_ms = DEFAULT_SMOKE_DURATION_MS;
    std::vector<std::string> examples;  ///< Explicit example ids; empty in smoke mode means "all".
};

int fail(std::string_view stage, std::string_view error)
{
    BROOKESIA_LOGE("GUI LVGL examples %1% failed: %2%", stage, error);
    std::cerr << "GUI LVGL examples " << stage << " failed: " << error << '\n';
    return EXIT_FAILURE;
}

void print_usage(const char *program)
{
    std::cout
            << "Usage: " << program << " [--smoke] [--example=ID]... [--duration-ms=N] [--list]\n"
            << "\n"
            << "Without options the interactive menu is shown (click an entry to run an example,\n"
            << "use the top-left back button to return).\n"
            << "\n"
            << "Options:\n"
            << "  --smoke           Non-interactive: mount each selected example, wait, then unmount.\n"
            << "  --example=ID      Restrict to example ID (repeatable). Implies --smoke. Without any\n"
            << "                    --example in smoke mode, every registered example runs.\n"
            << "  --duration-ms=N   How long each example stays mounted in smoke mode. Default: "
            << DEFAULT_SMOKE_DURATION_MS << ".\n"
            << "  --no-verify       Skip example self-verification in smoke mode (mount-only).\n"
            << "  --strict          Treat examples without checks (SKIP) as failures.\n"
            << "  --list            Print all registered example ids and exit.\n"
            << "  --help            Show this help.\n";
}

std::expected<int, std::string> parse_positive_int(std::string_view value)
{
    if (value.empty()) {
        return std::unexpected("integer value is empty");
    }
    int result = 0;
    const auto *begin = value.data();
    const auto *end = value.data() + value.size();
    auto [ptr, error] = std::from_chars(begin, end, result);
    if ((error != std::errc()) || (ptr != end) || (result < 0)) {
        return std::unexpected("invalid positive integer: " + std::string(value));
    }
    return result;
}

std::expected<CliOptions, std::string> parse_cli(int argc, char **argv)
{
    CliOptions options;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--help") {
            options.help = true;
        } else if (arg == "--list") {
            options.list = true;
        } else if (arg == "--smoke") {
            options.smoke = true;
        } else if (arg == "--no-verify") {
            options.verify = false;
        } else if (arg == "--strict") {
            options.strict = true;
        } else if (arg.starts_with("--duration-ms=")) {
            auto duration = parse_positive_int(arg.substr(std::string_view("--duration-ms=").size()));
            if (!duration) {
                return std::unexpected(duration.error());
            }
            options.smoke_duration_ms = *duration;
        } else if (arg == "--duration-ms") {
            if ((i + 1) >= argc) {
                return std::unexpected("--duration-ms requires a value");
            }
            auto duration = parse_positive_int(argv[++i]);
            if (!duration) {
                return std::unexpected(duration.error());
            }
            options.smoke_duration_ms = *duration;
        } else if (arg.starts_with("--example=")) {
            auto id = std::string(arg.substr(std::string_view("--example=").size()));
            if (id.empty()) {
                return std::unexpected("--example requires an id");
            }
            options.examples.push_back(std::move(id));
            options.smoke = true;
        } else if (arg == "--example") {
            if ((i + 1) >= argc) {
                return std::unexpected("--example requires an id");
            }
            std::string id(argv[++i]);
            if (id.empty()) {
                return std::unexpected("--example requires an id");
            }
            options.examples.push_back(std::move(id));
            options.smoke = true;
        } else {
            return std::unexpected("unknown option: " + std::string(arg));
        }
    }
    return options;
}

void pump_for(gui::Runtime &runtime, int duration_ms)
{
    const auto started_at = boost::chrono::steady_clock::now();
    while (boost::chrono::duration_cast<boost::chrono::milliseconds>(
                boost::chrono::steady_clock::now() - started_at).count() < duration_ms) {
        {
            gui::lvgl::lock_thread();
            lib_utils::FunctionGuard unlock([]() {
                gui::lvgl::unlock_thread();
            });
            runtime.process_backend();
        }
        boost::this_thread::sleep_for(boost::chrono::milliseconds(TIMER_DELAY_MS));
    }
}

// Build an InputProbe that synthesizes a real press+release through the display service.
//
// The probe is invoked from inside an example's verify() while the caller holds the LVGL lock. To
// let the DisplaySource timer thread actually process the press and release (read the injected
// snapshot, dispatch PRESSED/RELEASED/CLICKED), we temporarily drop the lock during each hold and
// re-acquire it before returning, so the lock balance the caller relies on is preserved.
gui::examples::InputProbe make_input_probe(const std::string &output_name)
{
    gui::examples::InputProbe probe;
    probe.tap = [output_name](int32_t x, int32_t y) -> bool {
        auto &display = service::Display::get_instance();
        auto pressed = display.inject_touch(output_name, x, y);
        if (!pressed)
        {
            BROOKESIA_LOGE("[smoke] inject_touch press failed: %1%", pressed.error());
            return false;
        }
        gui::lvgl::unlock_thread();
        boost::this_thread::sleep_for(boost::chrono::milliseconds(TAP_HOLD_MS));
        gui::lvgl::lock_thread();

        (void)display.inject_touch(output_name, std::vector<service::Display::TouchPoint>{});
        gui::lvgl::unlock_thread();
        boost::this_thread::sleep_for(boost::chrono::milliseconds(TAP_SETTLE_MS));
        gui::lvgl::lock_thread();

        (void)display.clear_injected_touch(output_name);
        return true;
    };
    return probe;
}

std::vector<std::string> all_example_ids()
{
    std::vector<std::string> ids;
    for (auto &[name, instance] : ExampleRegistry::get_all_instances()) {
        if (instance) {
            ids.push_back(instance->info().id);
        }
    }
    return ids;
}

int run_list()
{
    auto instances = ExampleRegistry::get_all_instances();
    std::cout << "Registered examples (" << instances.size() << "):\n";
    // get_all_instances() is sorted by registry key; print grouped by category.
    std::string last_category;
    for (auto &[name, instance] : instances) {
        if (!instance) {
            continue;
        }
        const auto info = instance->info();
        if (info.category != last_category) {
            std::cout << "  [" << info.category << "]\n";
            last_category = info.category;
        }
        std::cout << "    " << info.id << "  -  " << info.title << "\n";
    }
    return EXIT_SUCCESS;
}

int run_smoke(gui::Runtime &runtime, const gui::Environment &environment, const CliOptions &options,
              const std::string &output_name)
{
    std::vector<std::string> ids = options.examples;
    if (ids.empty()) {
        ids = all_example_ids();
    }

    const gui::examples::InputProbe input_probe = make_input_probe(output_name);

    std::size_t tested = 0;
    std::size_t run_failed = 0;
    std::size_t verified = 0;
    std::size_t verify_failed = 0;
    std::size_t skipped = 0;
    for (const auto &id : ids) {
        auto instance = ExampleRegistry::get_instance(id);
        if (!instance) {
            BROOKESIA_LOGE("[smoke] Unknown example: %1%", id);
            ++run_failed;
            continue;
        }

        BROOKESIA_LOGI("[smoke] Running example '%1%'", id);
        std::expected<void, std::string> ran;
        {
            gui::lvgl::lock_thread();
            lib_utils::FunctionGuard unlock([]() {
                gui::lvgl::unlock_thread();
            });
            ran = instance->run(runtime, environment);
            runtime.process_backend();
        }
        if (!ran) {
            BROOKESIA_LOGE("[smoke] Example '%1%' failed to run: %2%", id, ran.error());
            ++run_failed;
            continue;
        }
        ++tested;

        pump_for(runtime, options.smoke_duration_ms);

        if (options.verify) {
            gui::examples::VerifyOutcome outcome;
            {
                gui::lvgl::lock_thread();
                lib_utils::FunctionGuard unlock([]() {
                    gui::lvgl::unlock_thread();
                });
                outcome = instance->verify(runtime, environment, input_probe);
            }
            switch (outcome.status) {
            case gui::examples::VerifyOutcome::Status::Passed:
                ++verified;
                BROOKESIA_LOGI("[smoke] PASS '%1%'", id);
                break;
            case gui::examples::VerifyOutcome::Status::Failed:
                ++verify_failed;
                BROOKESIA_LOGE("[smoke] FAIL '%1%' (%2% issue(s))", id, outcome.failures.size());
                for (const auto &failure : outcome.failures) {
                    BROOKESIA_LOGE("[smoke]   - %1%", failure);
                }
                break;
            case gui::examples::VerifyOutcome::Status::Skipped:
                ++skipped;
                BROOKESIA_LOGI("[smoke] SKIP '%1%' (no checks)", id);
                break;
            }
        }

        {
            gui::lvgl::lock_thread();
            lib_utils::FunctionGuard unlock([]() {
                gui::lvgl::unlock_thread();
            });
            instance->stop(runtime);
            runtime.process_backend();
        }
        pump_for(runtime, TIMER_DELAY_MS * 2);
    }

    BROOKESIA_LOGI(
        "[smoke] Completed: tested(%1%), verified(%2%), verify-failed(%3%), skipped(%4%), run-failed(%5%)",
        tested, verified, verify_failed, skipped, run_failed);

    bool success = (run_failed == 0) && (verify_failed == 0);
    if (options.strict && skipped > 0) {
        success = false;
    }
    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}

int run_interactive(gui::Runtime &runtime, const gui::Environment &environment)
{
    auto &display_device = hal::DisplayLinuxDevice::get_instance();

    gui::examples::ExampleRunner runner(runtime, environment);
    {
        gui::lvgl::lock_thread();
        lib_utils::FunctionGuard unlock([]() {
            gui::lvgl::unlock_thread();
        });
        auto started = runner.start();
        if (!started) {
            return fail("example runner start", started.error());
        }
    }

    BROOKESIA_LOGI("GUI LVGL examples interactive mode started (%1% examples)", runner.example_count());
    while (!display_device.is_quit_requested()) {
        {
            gui::lvgl::lock_thread();
            lib_utils::FunctionGuard unlock([]() {
                gui::lvgl::unlock_thread();
            });
            runtime.process_backend();
            runner.process_pending();
        }
        boost::this_thread::sleep_for(boost::chrono::milliseconds(TIMER_DELAY_MS));
    }
    return EXIT_SUCCESS;
}

int run_main(int argc, char **argv)
{
    auto cli = parse_cli(argc, argv);
    if (!cli) {
        std::cerr << "Invalid arguments: " << cli.error() << '\n';
        print_usage(argv[0]);
        return 2;
    }
    if (cli->help) {
        print_usage(argv[0]);
        return EXIT_SUCCESS;
    }
    if (cli->list) {
        return run_list();
    }

    auto &display_device = hal::DisplayLinuxDevice::get_instance();
    if (!display_device.configure(hal::DisplayLinuxDevice::Config{
    .width_px = static_cast<uint16_t>(WINDOW_WIDTH),
        .height_px = static_cast<uint16_t>(WINDOW_HEIGHT),
        .window_title = "Brookesia JSON-UI Examples",
        .render_driver = "software",
    })) {
        return fail("display configure", "Failed to configure HAL Linux display");
    }

    if (!service::ServiceManager::get_instance().start()) {
        return fail("service manager start", "Failed to start service manager");
    }
    lib_utils::FunctionGuard service_manager_cleanup([]() {
        service::ServiceManager::get_instance().deinit();
    });

    auto display_binding = service::ServiceManager::get_instance().bind(DisplayHelper::get_name().data());
    if (!display_binding.is_valid()) {
        return fail("display binding", "Failed to bind display service");
    }

    auto outputs_result = DisplayHelper::call_function_sync<boost::json::array>(
                              DisplayHelper::FunctionId::GetOutputs,
                              service::helper::Timeout(DISPLAY_SERVICE_TIMEOUT_MS)
                          );
    if (!outputs_result) {
        return fail("get outputs", "Failed to get display outputs: " + outputs_result.error());
    }
    std::vector<DisplayHelper::OutputInfo> outputs;
    if (!BROOKESIA_DESCRIBE_FROM_JSON(outputs_result.value(), outputs)) {
        return fail("parse outputs", "Failed to parse display outputs");
    }
    auto backlight_output = std::find_if(
                                outputs.begin(), outputs.end(),
    [](const DisplayHelper::OutputInfo & output) {
        return output.backlight.has_value();
    }
                            );
    if (backlight_output != outputs.end()) {
        (void)DisplayHelper::call_function_sync(
            DisplayHelper::FunctionId::SetBacklightOnOff,
            static_cast<double>(backlight_output->id),
            true,
            service::helper::Timeout(DISPLAY_SERVICE_TIMEOUT_MS)
        );
    }

    auto &display_source = gui::lvgl::DisplaySource::get_instance();
    gui::lvgl::DisplaySourceConfig display_source_config;
    display_source_config.tick_period_ms = TIMER_DELAY_MS;
    if (!display_source.start(display_source_config)) {
        return fail("display source start", "Failed to start LVGL Display source");
    }
    lib_utils::FunctionGuard display_source_cleanup([]() {
        gui::lvgl::DisplaySource::get_instance().stop();
    });

    auto active_source_result = DisplayHelper::call_function_sync(
                                    DisplayHelper::FunctionId::SetActiveSourceRole,
                                    display_source.output_name(),
                                    display_source_config.source_role,
                                    service::helper::Timeout(DISPLAY_SERVICE_TIMEOUT_MS)
                                );
    if (!active_source_result) {
        return fail("display source activate", active_source_result.error());
    }

    gui::Environment environment{
        .width_px = static_cast<int32_t>(display_source.width()),
        .height_px = static_cast<int32_t>(display_source.height()),
        .density = 1.0F,
        .font_scale = 1.0F,
        .language = "en",
        .theme_id = "default",
    };

    auto backend = std::make_unique<gui::lvgl::Backend>();
    gui::Runtime runtime(std::move(backend));
    runtime.set_view_debug_enabled(false);

    const int rc = cli->smoke
                   ? run_smoke(runtime, environment, *cli, std::string(display_source.output_name()))
                   : run_interactive(runtime, environment);

    display_source.stop_timers();
    display_source.release_display_service();
    display_source.stop();
    display_source_cleanup.release();
    return rc;
}

} // namespace

int main(int argc, char **argv) noexcept
{
    try {
        return run_main(argc, argv);
    } catch (const std::exception &e) {
        return fail("unhandled exception", e.what());
    } catch (...) {
        return fail("unhandled exception", "unknown exception");
    }
}
