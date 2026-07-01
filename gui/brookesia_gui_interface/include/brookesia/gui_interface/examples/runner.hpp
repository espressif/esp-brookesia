/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <expected>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "brookesia/gui_interface/document.hpp"
#include "brookesia/gui_interface/handles.hpp"
#include "brookesia/gui_interface/runtime.hpp"
#include "brookesia/gui_interface/scoped_connection.hpp"
#include "brookesia/gui_interface/examples/example.hpp"

namespace esp_brookesia::gui::examples {

/**
 * @brief Shared controller that turns the registered example plugins into a runnable menu app.
 *
 * The runner builds a categorized home page entirely from the JSON-UI protocol, mounts it on the
 * Default layer, and keeps a transparent, click-through "back" overlay on the Top layer while an
 * example is active. It is platform agnostic: the same instance is reused by the PC and (future)
 * ESP entry points. Only @ref Runtime, @ref Environment and the @ref ExampleRegistry are required.
 *
 * Threading model: UI event handlers (clicked actions) only enqueue navigation requests, so they
 * are safe to run on the backend/timer thread. The actual mount/unmount work happens in
 * @ref process_pending, which the host loop must call on the thread that owns the backend lock.
 */
class ExampleRunner {
public:
    ExampleRunner(Runtime &runtime, Environment environment);
    ExampleRunner(const ExampleRunner &) = delete;
    ExampleRunner &operator=(const ExampleRunner &) = delete;
    ~ExampleRunner();

    /**
     * @brief Build and mount the home menu, then subscribe to every example launch action.
     *
     * Must be called once before @ref process_pending. The caller is responsible for holding the
     * backend thread lock (e.g. gui::lvgl::lock_thread()) around this call.
     */
    std::expected<void, std::string> start();

    /**
     * @brief Apply any queued navigation request (enter example / return to menu).
     *
     * Call this from the host loop while holding the backend thread lock. It is a no-op when there
     * is nothing pending.
     */
    void process_pending();

    /** @brief Number of examples discovered in the registry. */
    std::size_t example_count() const;

private:
    enum class PendingKind {
        None,
        RunExample,
        ReturnToMenu,
    };

    std::string build_menu_json() const;
    std::expected<void, std::string> show_menu();
    std::expected<void, std::string> run_example(const std::string &id);
    void return_to_menu();
    std::expected<void, std::string> mount_back_overlay();

    Runtime &runtime_;
    Environment environment_;

    DocumentId menu_document_;
    bool menu_mounted_ = false;
    std::string menu_screen_path_;

    DocumentId back_document_;
    bool back_mounted_ = false;
    std::string back_screen_path_;

    std::shared_ptr<IExample> active_example_;

    std::vector<ScopedConnection> menu_connections_;
    ScopedConnection back_connection_;

    mutable std::mutex pending_mutex_;
    PendingKind pending_kind_ = PendingKind::None;
    std::string pending_example_id_;
};

} // namespace esp_brookesia::gui::examples
