/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <cstdint>

#include "brookesia/gui_interface/backend.hpp"
#include "brookesia/gui_interface/document.hpp"
#include "brookesia/gui_interface/event.hpp"
#include "brookesia/gui_interface/macro_configs.h"
#include "brookesia/gui_interface/scoped_connection.hpp"
#include "brookesia/gui_interface/widget.hpp"
#include "brookesia/lib_utils/task_scheduler.hpp"

namespace esp_brookesia::gui {

struct LivePreviewOptions {
    int32_t poll_interval_ms = 250;
    bool log_reload = true;
};

struct BindingValueUpdate {
    std::string absolute_path;
    std::string key;
    std::string value;
};
BROOKESIA_DESCRIBE_STRUCT(BindingValueUpdate, (), (absolute_path, key, value))

struct RuntimeTaskConfig {
    std::shared_ptr<lib_utils::TaskScheduler> task_scheduler;
    lib_utils::TaskScheduler::Group gui_group;
    lib_utils::TaskScheduler::Group event_group;
    bool enable_fast_action_dispatch = false;
};
BROOKESIA_DESCRIBE_STRUCT(RuntimeTaskConfig, (), (gui_group, event_group))

struct RuntimeAnimationStartResult {
    SubscriptionId subscription_id = 0;
    int32_t resolved_from = 0;
    int32_t resolved_to = 0;
};

class Runtime {
public:
    using ActionHandler = std::function<void(const Event &)>;
    using BindingValueHandler =
        std::function<void(std::string_view absolute_path, std::string_view key, std::string_view value)>;
    using AnimationCompletedHandler = std::function<void()>;

    explicit Runtime(std::unique_ptr<IBackend> backend);
    Runtime(std::unique_ptr<IBackend> backend, RuntimeTaskConfig task_config);
    Runtime(const Runtime &) = delete;
    Runtime &operator=(const Runtime &) = delete;
    ~Runtime();

    std::expected<DocumentId, std::string> load(
        std::string_view root_path, const Document &document, const Environment &environment
    );
    std::expected<DocumentId, std::string> load_json(
        std::string_view root_path, std::string_view json, std::string_view base_dir, const Environment &environment
    );
    std::expected<DocumentId, std::string> load_file(std::string_view path, const Environment &environment);
    std::expected<void, std::string> load_theme(const ThemeAsset &theme);
    std::expected<void, std::string> load_theme_json(std::string_view json, std::string_view base_dir = {});
    std::expected<void, std::string> load_theme_file(std::string_view path);
    std::vector<std::string> list_supported_themes() const;
    std::expected<void, std::string> set_theme(std::string_view theme_id);
    std::expected<void, std::string> set_theme(std::string_view theme_id, bool reapply_loaded_documents);
    std::string get_theme() const;
    std::expected<void, std::string> register_font(const RuntimeFontResource &resource);
    bool unregister_font(std::string_view id);
    std::expected<void, std::string> register_font_json(std::string_view json, std::string_view base_dir = {});
    std::expected<void, std::string> register_font_file(std::string_view path);
    std::vector<std::string> list_supported_fonts(std::string_view language = {}) const;
    std::vector<std::string> list_supported_languages() const;
    std::vector<std::string> list_supported_languages(std::string_view font_id) const;
    std::expected<void, std::string> set_language(std::string_view language);
    std::expected<void, std::string> set_language(std::string_view language, bool reapply_loaded_documents);
    std::string get_language() const;
    std::expected<void, std::string> set_default_font_for_language(
        std::string_view language,
        std::string_view font_id
    );
    std::optional<std::string> get_default_font_for_language(std::string_view language) const;
    std::expected<void, std::string> register_image(const RuntimeImageResource &resource);
    bool unregister_image(std::string_view id);
    std::expected<void, std::string> register_image_json(std::string_view json, std::string_view base_dir = {});
    std::expected<void, std::string> register_image_file(std::string_view path);
    void process_backend();
    void set_view_debug_enabled(bool enabled);
    bool is_view_debug_enabled() const;
    std::expected<void, std::string> enable_live_preview(DocumentId id, const LivePreviewOptions &options = {});
    bool disable_live_preview(DocumentId id);
    void poll_live_preview();
    bool unload(DocumentId id);
    SubscriptionId subscribe_event_action_with_id(
        DocumentId id,
        std::string_view action,
        ActionHandler handler
    );
    bool unsubscribe_subscription(SubscriptionId subscription_id);
    ScopedConnection subscribe_event_action(
        DocumentId id,
        std::string_view action,
        ActionHandler handler
    );
    std::vector<GuiDisplayInfo> list_displays() const;
    std::vector<GuiLayer> list_layers() const;
    std::expected<View, std::string> mount_screen(
        DocumentId id,
        std::string_view absolute_path,
        const MountTarget &target = {}
    );
    bool unmount_screen(DocumentId id, std::string_view absolute_path);
    std::expected<TransientMountId, std::string> push_transient_screen(
        DocumentId id,
        std::string_view absolute_path,
        const MountTarget &target = {}
    );
    bool pop_transient_screen(TransientMountId id);
    std::expected<void, std::string> start_screen_flow(
        DocumentId id,
        std::string_view flow_id,
        const MountTarget &target = {}
    );
    std::expected<void, std::string> trigger_screen_flow(
        DocumentId id,
        std::string_view flow_id,
        std::string_view action
    );
    bool stop_screen_flow(DocumentId id, std::string_view flow_id);
    bool has_screen_flow(DocumentId id, std::string_view flow_id) const;
    std::optional<std::string> get_screen_flow_state(DocumentId id, std::string_view flow_id) const;
    View find_view(DocumentId id, std::string_view absolute_path) const;
    std::expected<View, std::string> create_view(
        DocumentId id,
        std::string_view template_id,
        std::string_view parent_absolute_path,
        std::string_view instance_id
    );
    bool destroy_view(DocumentId id, std::string_view absolute_path);
    std::expected<void, std::string> update(DocumentId id, std::string_view file_path, const Environment &environment);
    std::expected<void, std::string> reapply_styles(DocumentId id);
    std::expected<boost::json::value, std::string> get_constant_value(
        DocumentId id,
        std::string_view path
    ) const;
    std::vector<RuntimeFontResource> list_font_resources(DocumentId id) const;
    std::vector<RuntimeImageResource> list_image_resources(DocumentId id) const;
    std::optional<ViewStateValue> get_view_state(const View &view, ViewStateKind kind) const;
    std::optional<ViewStateValue> get_view_state(DocumentId id, std::string_view absolute_path, ViewStateKind kind) const;
    bool scroll_view_to_visible(DocumentId id, std::string_view absolute_path, bool animated = true) const;
    bool scroll_view_to_visible(const View &view, bool animated = true) const;
    void set_binding_value(
        DocumentId id,
        std::string_view absolute_path,
        std::string_view key,
        std::string value) const;
    void set_binding_values(DocumentId id, const std::vector<BindingValueUpdate> &updates) const;
    std::optional<std::string> get_binding_value(
        DocumentId id,
        std::string_view absolute_path,
        std::string_view key) const;
    SubscriptionId subscribe_binding_value_with_id(
        DocumentId id,
        std::string_view absolute_path,
        std::string_view key,
        BindingValueHandler handler) const;
    ScopedConnection subscribe_binding_value(
        DocumentId id,
        std::string_view absolute_path,
        std::string_view key,
        BindingValueHandler handler) const;
    SubscriptionId start_view_animation_with_id(
        DocumentId id,
        std::string_view absolute_path,
        const Animation &animation,
        AnimationCompletedHandler completed_handler = {});
    RuntimeAnimationStartResult start_view_animation_with_result(
        DocumentId id,
        std::string_view absolute_path,
        const Animation &animation,
        AnimationCompletedHandler completed_handler = {});
    ScopedConnection start_view_animation(
        DocumentId id,
        std::string_view absolute_path,
        const Animation &animation,
        AnimationCompletedHandler completed_handler = {});
    SubscriptionId start_view_animation_with_id(
        const View &view,
        const Animation &animation,
        AnimationCompletedHandler completed_handler = {});
    RuntimeAnimationStartResult start_view_animation_with_result(
        const View &view,
        const Animation &animation,
        AnimationCompletedHandler completed_handler = {});
    ScopedConnection start_view_animation(
        const View &view,
        const Animation &animation,
        AnimationCompletedHandler completed_handler = {});
    bool set_view_src(DocumentId id, std::string_view absolute_path, std::string_view src) const;

private:
    friend class View;
    friend class Screen;
    friend class Container;
    friend class Label;
    friend class Button;
    friend class Image;
    friend class TextInput;
    friend class Slider;
    friend class Switch;
    friend class Checkbox;
    friend class Dropdown;
    friend class ProgressBar;
    friend class Arc;
    friend class Table;
    friend class Spinner;
    friend class Line;
    friend class Keyboard;
    friend class Canvas;

    class Impl;
    std::unique_ptr<Impl> impl_;

    bool is_view_valid(const View &view) const;
    std::string get_view_id(const View &view) const;
    std::string get_view_absolute_path(const View &view) const;
    std::optional<ViewStateValue> get_view_state_internal(const View &view, ViewStateKind kind) const;
    bool set_view_hidden(const View &view, bool hidden) const;
    bool set_view_text(const View &view, std::string_view text) const;
    std::string get_view_text(const View &view) const;
    bool set_view_src(const View &view, std::string_view src) const;
    std::string get_view_src(const View &view) const;
    bool set_view_value(const View &view, int32_t value) const;
    int32_t get_view_value(const View &view) const;
    bool set_view_checked(const View &view, bool checked) const;
    bool get_view_checked(const View &view) const;
    bool set_view_selected_index(const View &view, int32_t index) const;
    int32_t get_view_selected_index(const View &view) const;
    bool set_table_cell_text(const View &view, int32_t row, int32_t column, std::string_view text) const;
    esp_brookesia::lib_utils::connection connect_view_event(const View &view, EventType type, View::EventHandler handler) const;
    esp_brookesia::lib_utils::connection connect_button_click(const Button &button, Button::ClickHandler handler) const;
};

} // namespace esp_brookesia::gui
