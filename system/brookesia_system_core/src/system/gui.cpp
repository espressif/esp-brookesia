/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/system_core/macro_configs.h"
#if !BROOKESIA_SYSTEM_CORE_SYSTEM_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include <algorithm>
#include <fstream>
#include <set>
#include <sstream>

#include "boost/json.hpp"

#include "private/utils.hpp"
#include "private/heap_trace.hpp"
#include "private/system/impl.hpp"

namespace esp_brookesia::system::core {

namespace {

std::string make_binding_update_key(std::string_view absolute_path, std::string_view key)
{
    std::string update_key(absolute_path);
    update_key.push_back('\x1f');
    update_key.append(key);
    return update_key;
}

std::string gui_batch_command_name(const GuiBatchCommand &command)
{
    switch (command.type) {
    case GuiBatchCommand::Type::SetBindings:
        return "SetBindings";
    case GuiBatchCommand::Type::SetViewSrc:
        return "SetViewSrc";
    case GuiBatchCommand::Type::StopAnimation:
        return "StopAnimation";
    case GuiBatchCommand::Type::StartViewAnimation:
        return "StartViewAnimation";
    case GuiBatchCommand::Type::ScrollToView:
        return "ScrollToView";
    default:
        return "Unknown";
    }
}

bool is_deferable_gui_batch_command(const GuiBatchCommand &command)
{
    return command.type != GuiBatchCommand::Type::StartViewAnimation;
}

bool can_defer_gui_batch_commands(const std::vector<GuiBatchCommand> &commands)
{
    return std::all_of(commands.begin(), commands.end(), is_deferable_gui_batch_command);
}

GuiBatchResult make_deferred_gui_batch_result(const std::vector<GuiBatchCommand> &commands)
{
    GuiBatchResult batch_result;
    batch_result.success = true;
    batch_result.results.reserve(commands.size());
    for (const auto &command : commands) {
        GuiBatchCommandResult command_result;
        command_result.name = gui_batch_command_name(command);
        command_result.success = true;
        batch_result.results.push_back(std::move(command_result));
    }
    return batch_result;
}

#if defined(__EMSCRIPTEN__)
void collect_gui_action_strings(const boost::json::value &value, std::set<std::string> &actions)
{
    if (value.is_object()) {
        const auto &object = value.as_object();
        for (const auto &item : object) {
            if (item.key() == "action" && item.value().is_string()) {
                const auto &action = item.value().as_string();
                if (!action.empty()) {
                    actions.emplace(action.c_str(), action.size());
                }
            }
            collect_gui_action_strings(item.value(), actions);
        }
        return;
    }

    if (value.is_array()) {
        for (const auto &item : value.as_array()) {
            collect_gui_action_strings(item, actions);
        }
    }
}

std::expected<std::string, std::string> read_text_file(std::string_view path)
{
    std::ifstream input(std::string(path), std::ios::binary);
    if (!input) {
        return std::unexpected("Failed to open GUI root file: " + std::string(path));
    }
    std::ostringstream output;
    output << input.rdbuf();
    return output.str();
}
#endif

} // namespace

std::expected<GuiBatchResult, std::string> System::Impl::execute_gui_batch_now(
    AppId app_id,
    const std::vector<GuiBatchCommand> &commands
)
{
    auto record_result = get_record(app_id);
    if (!record_result) {
        return std::unexpected(record_result.error());
    }
    auto &record = *record_result.value();
    if (!gui_runtime_ || !record.document_id.has_value()) {
        return std::unexpected("App GUI document is not loaded");
    }

    GuiBatchResult batch_result;
    batch_result.success = true;

    for (const auto &command : commands) {
        GuiBatchCommandResult command_result;
        command_result.name = gui_batch_command_name(command);
        switch (command.type) {
        case GuiBatchCommand::Type::SetBindings:
            gui_runtime_->set_binding_values(*record.document_id, command.binding_updates);
            command_result.success = true;
            break;
        case GuiBatchCommand::Type::SetViewSrc:
            command_result.success = gui_runtime_->set_view_src(*record.document_id, command.path, command.src);
            if (!command_result.success) {
                command_result.error_message = "Failed to set view image source";
            }
            break;
        case GuiBatchCommand::Type::StopAnimation:
            (void)gui_runtime_->unsubscribe_subscription(command.animation_id);
            command_result.success = true;
            break;
        case GuiBatchCommand::Type::StartViewAnimation: {
            auto start_result = gui_runtime_->start_view_animation_with_result(
                                    *record.document_id,
                                    command.path,
                                    command.animation
                                );
            command_result.success = start_result.subscription_id != 0;
            if (!command_result.success) {
                command_result.error_message = "Failed to start view animation";
            } else {
                command_result.data["AnimationId"] = start_result.subscription_id;
                command_result.data["ResolvedFrom"] = start_result.resolved_from;
                command_result.data["ResolvedTo"] = start_result.resolved_to;
            }
            break;
        }
        case GuiBatchCommand::Type::ScrollToView:
            flush_pending_gui_bindings(app_id);
            command_result.success = gui_runtime_->scroll_view_to_visible(
                                         *record.document_id,
                                         command.path,
                                         command.animated
                                     );
            if (!command_result.success) {
                command_result.error_message = "Failed to scroll view to visible";
            }
            break;
        default:
            command_result.success = false;
            command_result.error_message = "Unsupported GUI batch command";
            break;
        }

        const auto success = command_result.success;
        batch_result.results.push_back(std::move(command_result));
        if (!success) {
            batch_result.success = false;
            break;
        }
    }

    return batch_result;
}

std::expected<void, std::string> System::Impl::enqueue_gui_binding_update(
    AppId app_id,
    std::string_view absolute_path,
    std::string_view key,
    std::string value
)
{
    std::vector<gui::BindingValueUpdate> updates;
    updates.push_back({
        .absolute_path = std::string(absolute_path),
        .key = std::string(key),
        .value = std::move(value),
    });
    return enqueue_gui_binding_updates(app_id, std::move(updates));
}


std::expected<void, std::string> System::Impl::enqueue_gui_binding_updates(
    AppId app_id,
    std::vector<gui::BindingValueUpdate> updates
)
{
    if (updates.empty()) {
        return {};
    }

    auto heap_before_enqueue = heap_trace::capture();
    if constexpr (heap_trace::enabled) {
        const auto app_id_string = std::to_string(app_id);
        const auto details = "app_id=" + app_id_string + " update_count=" + std::to_string(updates.size());
        heap_trace::log_detail(
            "system.gui.binding",
            "enqueue before pending buffer update",
            app_id_string,
            details,
            heap_before_enqueue
        );
    }
    bool should_post_flush = false;
    std::size_t pending_update_count = 0;
    std::size_t pending_index_count = 0;
    {
        std::lock_guard lock(gui_binding_mutex_);
        auto &buffer = pending_gui_bindings_[app_id];
        for (auto &update : updates) {
            const auto update_key = make_binding_update_key(update.absolute_path, update.key);
            auto index_it = buffer.indexes.find(update_key);
            if (index_it != buffer.indexes.end()) {
                buffer.updates[index_it->second] = std::move(update);
                continue;
            }
            buffer.indexes.emplace(update_key, buffer.updates.size());
            buffer.updates.push_back(std::move(update));
        }
        if (!buffer.flush_posted) {
            buffer.flush_posted = true;
            should_post_flush = true;
        }
        pending_update_count = buffer.updates.size();
        pending_index_count = buffer.indexes.size();
    }
    if constexpr (heap_trace::enabled) {
        const auto app_id_string = std::to_string(app_id);
        const auto details = "app_id=" + app_id_string +
                             " pending_updates=" + std::to_string(pending_update_count) +
                             " pending_indexes=" + std::to_string(pending_index_count) +
                             " post_flush=" + std::to_string(should_post_flush);
        heap_trace::log_detail(
            "system.gui.binding",
            "enqueue after pending buffer update",
            app_id_string,
            details,
            heap_trace::capture(),
            &heap_before_enqueue
        );
    }

    if (!should_post_flush) {
        return {};
    }

    auto post_result = post_gui_task([this, app_id]() {
        flush_pending_gui_bindings(app_id);
    });
    if (!post_result) {
        std::lock_guard lock(gui_binding_mutex_);
        auto buffer_it = pending_gui_bindings_.find(app_id);
        if (buffer_it != pending_gui_bindings_.end()) {
            buffer_it->second.flush_posted = false;
        }
    }
    return post_result;
}


void System::Impl::flush_pending_gui_bindings(AppId app_id)
{
    auto heap_before_flush = heap_trace::capture();
    if constexpr (heap_trace::enabled) {
        const auto app_id_string = std::to_string(app_id);
        heap_trace::log_detail(
            "system.gui.binding",
            "flush before move pending buffer",
            app_id_string,
            "app_id=" + app_id_string,
            heap_before_flush
        );
    }
    std::vector<gui::BindingValueUpdate> updates;
    {
        std::lock_guard lock(gui_binding_mutex_);
        auto buffer_it = pending_gui_bindings_.find(app_id);
        if (buffer_it == pending_gui_bindings_.end()) {
            return;
        }
        updates = std::move(buffer_it->second.updates);
        buffer_it->second.updates.clear();
        buffer_it->second.indexes.clear();
    }

    if (!updates.empty()) {
        if constexpr (heap_trace::enabled) {
            const auto app_id_string = std::to_string(app_id);
            const auto details = "app_id=" + app_id_string + " update_count=" + std::to_string(updates.size());
            heap_trace::log_detail(
                "system.gui.binding",
                "flush before runtime apply",
                app_id_string,
                details,
                heap_trace::capture(),
                &heap_before_flush
            );
        }
        auto record_result = get_record(app_id);
        if (!record_result || !gui_runtime_ || !record_result.value()->document_id.has_value()) {
            BROOKESIA_LOGW("Skip pending GUI binding flush: app_id(%1%), count(%2%)", app_id, updates.size());
        } else {
            gui_runtime_->set_binding_values(*record_result.value()->document_id, updates);
        }
        if constexpr (heap_trace::enabled) {
            const auto app_id_string = std::to_string(app_id);
            const auto details = "app_id=" + app_id_string + " update_count=" + std::to_string(updates.size());
            heap_trace::log_detail(
                "system.gui.binding",
                "flush after runtime apply",
                app_id_string,
                details,
                heap_trace::capture(),
                &heap_before_flush
            );
        }
    }

    bool should_post_again = false;
    {
        std::lock_guard lock(gui_binding_mutex_);
        auto buffer_it = pending_gui_bindings_.find(app_id);
        if (buffer_it == pending_gui_bindings_.end()) {
            return;
        }
        if (buffer_it->second.updates.empty()) {
            pending_gui_bindings_.erase(buffer_it);
            return;
        }
        should_post_again = true;
    }

    if (!should_post_again) {
        return;
    }

    auto post_result = post_gui_task([this, app_id]() {
        flush_pending_gui_bindings(app_id);
    });
    if (!post_result) {
        std::lock_guard lock(gui_binding_mutex_);
        auto buffer_it = pending_gui_bindings_.find(app_id);
        if (buffer_it != pending_gui_bindings_.end()) {
            buffer_it->second.flush_posted = false;
        }
        BROOKESIA_LOGW(
            "Failed to post pending GUI binding flush: app_id(%1%), error(%2%)",
            app_id,
            post_result.error()
        );
    }
}


void System::Impl::clear_pending_gui_bindings(AppId app_id)
{
    std::lock_guard lock(gui_binding_mutex_);
    pending_gui_bindings_.erase(app_id);
}

std::expected<void, std::string> System::Impl::enable_live_preview_for_document(
    gui::DocumentId document_id,
    const gui::LivePreviewOptions &options
)
{
    if (!gui_runtime_) {
        return std::unexpected("GUI runtime is not available");
    }
    auto enable_result = gui_runtime_->enable_live_preview(document_id, options);
    if (!enable_result) {
        return std::unexpected(enable_result.error());
    }
    live_preview_document_ids_.insert(document_id.value());
    if (!ensure_live_preview_poll_started()) {
        if (!gui_runtime_->disable_live_preview(document_id)) {
            BROOKESIA_LOGW("Failed to rollback GUI live preview enable: document_id(%1%)", document_id.value());
        }
        live_preview_document_ids_.erase(document_id.value());
        return std::unexpected("Failed to start GUI live preview poll task");
    }
    return {};
}

bool System::Impl::disable_live_preview_for_document(gui::DocumentId document_id)
{
    if (!gui_runtime_) {
        return false;
    }
    const bool disabled = gui_runtime_->disable_live_preview(document_id);
    live_preview_document_ids_.erase(document_id.value());
    stop_live_preview_poll_if_idle();
    return disabled;
}

void System::Impl::remove_live_preview_document(gui::DocumentId document_id)
{
    live_preview_document_ids_.erase(document_id.value());
    stop_live_preview_poll_if_idle();
}

void System::Impl::poll_live_preview_documents()
{
    if (!gui_runtime_ || live_preview_document_ids_.empty()) {
        return;
    }
    gui_runtime_->poll_live_preview();
}

void System::Impl::stop_live_preview_poll()
{
    if (task_scheduler_ && live_preview_poll_task_id_.has_value()) {
        task_scheduler_->cancel(*live_preview_poll_task_id_);
    }
    live_preview_poll_task_id_.reset();
    live_preview_document_ids_.clear();
}

void System::Impl::stop_live_preview_poll_if_idle()
{
    if (!live_preview_document_ids_.empty()) {
        return;
    }
    if (task_scheduler_ && live_preview_poll_task_id_.has_value()) {
        task_scheduler_->cancel(*live_preview_poll_task_id_);
    }
    live_preview_poll_task_id_.reset();
}

bool System::Impl::ensure_live_preview_poll_started()
{
    if (live_preview_poll_task_id_.has_value()) {
        return true;
    }
    if (!task_scheduler_ || !task_scheduler_->is_running()) {
        return false;
    }

    const auto interval_ms = std::max<int32_t>(1, config_.gui_live_preview_poll_interval_ms);
    lib_utils::TaskScheduler::TaskId task_id = 0;
    auto poll_task = [this]() -> bool {
        if (!gui_runtime_ || live_preview_document_ids_.empty())
        {
            live_preview_poll_task_id_.reset();
            return false;
        }
        gui_runtime_->poll_live_preview();
        return true;
    };
    if (!task_scheduler_->post_periodic(std::move(poll_task), interval_ms, &task_id, SYSTEM_GUI_TASK_GROUP)) {
        return false;
    }
    live_preview_poll_task_id_ = task_id;
    return true;
}


std::expected<void, std::string> System::Impl::ensure_gui_loaded(AppRecord &record)
{
    const auto &descriptor = record.gui_descriptor;
    if (descriptor.root_kind == GuiRootKind::None) {
        return {};
    }
    if (!gui_runtime_) {
        return std::unexpected("GUI runtime is not available");
    }
    if (!record.document_id.has_value()) {
        std::expected<gui::DocumentId, std::string> load_result = std::unexpected("Invalid GUI root");
        if (descriptor.root_kind == GuiRootKind::File) {
            load_result = gui_runtime_->load_file(
                              resolve_app_resource_path(record.info.manifest, descriptor.root),
                              environment_
                          );
        } else if (descriptor.root_kind == GuiRootKind::JsonString) {
            load_result = gui_runtime_->load_json(
                              record.info.manifest.id,
                              descriptor.root,
                              resolve_app_resource_dir(record.info.manifest),
                              environment_
                          );
        }
        if (!load_result) {
            return std::unexpected(load_result.error());
        }
        record.document_id = *load_result;
        if (config_.enable_gui_live_preview && descriptor.root_kind == GuiRootKind::File) {
            auto preview_result = enable_live_preview_for_document(
                                      *record.document_id,
                                      config_.gui_live_preview_options
                                  );
            if (!preview_result) {
                if (!gui_runtime_->unload(*record.document_id)) {
                    BROOKESIA_LOGW("Failed to rollback app GUI document load: document_id(%1%)", record.document_id->value());
                }
                record.document_id.reset();
                return std::unexpected(preview_result.error());
            }
        }
    }
#if defined(__EMSCRIPTEN__)
    if (record.info.manifest.kind != AppKind::Native && record.document_id.has_value() &&
            record.action_connections.empty()) {
        std::string root_json;
        if (descriptor.root_kind == GuiRootKind::File) {
            auto root_path = resolve_app_resource_path(record.info.manifest, descriptor.root);
            auto read_result = read_text_file(root_path);
            if (!read_result) {
                BROOKESIA_LOGW(
                    "Failed to auto-subscribe runtime GUI actions: app_id(%1%), error(%2%)",
                    record.info.app_id, read_result.error()
                );
                return {};
            }
            root_json = std::move(*read_result);
        } else if (descriptor.root_kind == GuiRootKind::JsonString) {
            root_json = descriptor.root;
        }

        if (!root_json.empty()) {
            boost::system::error_code error_code;
            auto root = boost::json::parse(root_json, error_code);
            if (error_code) {
                BROOKESIA_LOGW(
                    "Failed to parse GUI root for action auto-subscribe: app_id(%1%), error(%2%)",
                    record.info.app_id, error_code.message()
                );
                return {};
            }

            std::set<std::string> actions;
            collect_gui_action_strings(root, actions);
            for (auto &action : actions) {
                auto subscribe_result = subscribe_gui_action_now(record.info.app_id, std::move(action));
                if (!subscribe_result) {
                    BROOKESIA_LOGW(
                        "Failed to auto-subscribe runtime GUI action: app_id(%1%), error(%2%)",
                        record.info.app_id, subscribe_result.error()
                    );
                }
            }
        }
    }
#endif
    return {};
}


void System::Impl::unload_gui(AppRecord &record)
{
    record.action_connections.clear();
    record.action_connection_keys.clear();
    if (gui_runtime_ && record.document_id.has_value()) {
        remove_live_preview_document(*record.document_id);
        if (!gui_runtime_->unload(*record.document_id)) {
            BROOKESIA_LOGW("Failed to unload app GUI document: document_id(%1%)", record.document_id->value());
        }
    }
    record.document_id.reset();
}


std::expected<void, std::string> System::gui_set_binding_value(
    AppId app_id,
    std::string_view absolute_path,
    std::string_view key,
    std::string value
)
{
    return impl_->enqueue_gui_binding_update(app_id, absolute_path, key, std::move(value));
}

std::expected<void, std::string> System::gui_set_binding_values(
    AppId app_id,
    const std::vector<gui::BindingValueUpdate> &updates
)
{
    return impl_->enqueue_gui_binding_updates(app_id, updates);
}

std::optional<std::string> System::gui_get_binding_value(
    AppId app_id,
    std::string_view absolute_path,
    std::string_view key
) const
{
    return impl_->run_task_sync<std::optional<std::string>>(
               SYSTEM_GUI_TASK_GROUP,
    [this, app_id, absolute_path = std::string(absolute_path), key = std::string(key)]() -> std::optional<std::string> {
        auto record_result = impl_->get_record(app_id);
        if (!record_result || !impl_->gui_runtime_ || !record_result.value()->document_id.has_value())
        {
            return std::nullopt;
        }
        return impl_->gui_runtime_->get_binding_value(*record_result.value()->document_id, absolute_path, key);
    },
    std::nullopt
           );
}

std::expected<boost::json::value, std::string> System::gui_get_constant_value(
    AppId app_id,
    std::string_view path
) const
{
    return impl_->run_task_sync<std::expected<boost::json::value, std::string>>(
               SYSTEM_GUI_TASK_GROUP,
    [this, app_id, path = std::string(path)]() -> std::expected<boost::json::value, std::string> {
        auto record_result = impl_->get_record(app_id);
        if (!record_result || !impl_->gui_runtime_ || !record_result.value()->document_id.has_value())
        {
            return std::unexpected("App GUI document is not loaded");
        }
        return impl_->gui_runtime_->get_constant_value(*record_result.value()->document_id, path);
    },
    std::unexpected("Failed to post GUI constant query task")
           );
}

std::expected<void, std::string> System::gui_set_text(
    AppId app_id,
    std::string_view absolute_path,
    std::string_view text
)
{
    return impl_->run_task_sync<std::expected<void, std::string>>(
               SYSTEM_GUI_TASK_GROUP,
               [this, app_id, absolute_path = std::string(absolute_path), text = std::string(text)]()
    -> std::expected<void, std::string> {
        auto record_result = impl_->get_record(app_id);
        if (!record_result || !impl_->gui_runtime_ || !record_result.value()->document_id.has_value())
        {
            return std::unexpected("App GUI document is not loaded");
        }
        impl_->flush_pending_gui_bindings(app_id);
        auto view = impl_->gui_runtime_->find_view(*record_result.value()->document_id, absolute_path);
        if (!view.valid())
        {
            return std::unexpected("GUI text view is missing: " + absolute_path);
        }
        auto label = view.as_label();
        if (!label.valid() || !label.set_text(text))
        {
            return std::unexpected("GUI view is not a label: " + absolute_path);
        }
        return {};
    },
    std::unexpected("Failed to post GUI text task")
           );
}

std::expected<void, std::string> System::gui_set_value(AppId app_id, std::string_view absolute_path, int32_t value)
{
    return impl_->post_gui_task(
    [this, app_id, absolute_path = std::string(absolute_path), value]() {
        auto record_result = impl_->get_record(app_id);
        if (!record_result || !impl_->gui_runtime_ || !record_result.value()->document_id.has_value()) {
            BROOKESIA_LOGW("Skip GUI value update: app_id(%1%), path(%2%)", app_id, absolute_path);
            return;
        }
        auto view = impl_->gui_runtime_->find_view(*record_result.value()->document_id, absolute_path);
        if (!view.valid()) {
            BROOKESIA_LOGW("Skip GUI value update because view is missing: app_id(%1%), path(%2%)", app_id, absolute_path);
            return;
        }
        if (auto slider = view.as_slider(); slider.valid()) {
            (void)slider.set_value(value);
            return;
        }
        if (auto progress_bar = view.as_progress_bar(); progress_bar.valid()) {
            (void)progress_bar.set_value(value);
            return;
        }
        BROOKESIA_LOGW("Skip GUI value update because view does not support numeric values: app_id(%1%), path(%2%)",
                       app_id, absolute_path);
    }
           );
}

std::expected<void, std::string> System::gui_set_checked(AppId app_id, std::string_view absolute_path, bool checked)
{
    return impl_->post_gui_task(
    [this, app_id, absolute_path = std::string(absolute_path), checked]() {
        auto record_result = impl_->get_record(app_id);
        if (!record_result || !impl_->gui_runtime_ || !record_result.value()->document_id.has_value()) {
            BROOKESIA_LOGW("Skip GUI checked update: app_id(%1%), path(%2%)", app_id, absolute_path);
            return;
        }
        auto view = impl_->gui_runtime_->find_view(*record_result.value()->document_id, absolute_path);
        if (!view.valid()) {
            BROOKESIA_LOGW("Skip GUI checked update because view is missing: app_id(%1%), path(%2%)", app_id, absolute_path);
            return;
        }
        if (auto sw = view.as_switch(); sw.valid()) {
            (void)sw.set_checked(checked);
            return;
        }
        if (auto checkbox = view.as_checkbox(); checkbox.valid()) {
            (void)checkbox.set_checked(checked);
            return;
        }
        BROOKESIA_LOGW("Skip GUI checked update because view does not support checked values: app_id(%1%), path(%2%)",
                       app_id, absolute_path);
    }
           );
}

std::expected<gui::View, std::string> System::gui_create_view(
    AppId app_id,
    std::string_view template_id,
    std::string_view parent_absolute_path,
    std::string_view instance_id
)
{
    return impl_->run_task_sync<std::expected<gui::View, std::string>>(
               SYSTEM_GUI_TASK_GROUP,
               [this,
                app_id,
                template_id = std::string(template_id),
                parent_absolute_path = std::string(parent_absolute_path),
    instance_id = std::string(instance_id)]() -> std::expected<gui::View, std::string> {
        auto record_result = impl_->get_record(app_id);
        if (!record_result)
        {
            return std::unexpected(record_result.error());
        }
        auto &record = *record_result.value();
        if (!impl_->gui_runtime_ || !record.document_id.has_value())
        {
            return std::unexpected("App GUI document is not loaded");
        }
        impl_->flush_pending_gui_bindings(app_id);
        return impl_->gui_runtime_->create_view(*record.document_id, template_id, parent_absolute_path, instance_id);
    },
    std::unexpected("Failed to post GUI create view task")
           );
}

bool System::gui_destroy_view(AppId app_id, std::string_view absolute_path)
{
    return impl_->run_task_sync<bool>(
               SYSTEM_GUI_TASK_GROUP,
    [this, app_id, absolute_path = std::string(absolute_path)]() {
        auto record_result = impl_->get_record(app_id);
        if (!record_result || !impl_->gui_runtime_ || !record_result.value()->document_id.has_value()) {
            BROOKESIA_LOGW("Skip GUI destroy view: app_id(%1%), path(%2%)", app_id, absolute_path);
            return false;
        }
        return impl_->gui_runtime_->destroy_view(*record_result.value()->document_id, absolute_path);
    },
    false
           );
}

std::expected<void, std::string> System::Impl::subscribe_gui_action_now(AppId app_id, std::string action)
{
    auto heap_before_task = heap_trace::capture();
    if constexpr (heap_trace::enabled) {
        const auto app_id_string = std::to_string(app_id);
        const auto details = "app_id=" + app_id_string + " action=" + action;
        heap_trace::log_detail(
            "system.gui.action",
            "subscribe before GUI task",
            app_id_string,
            details,
            heap_before_task
        );
    }
    auto record_result = get_record(app_id);
    if (!record_result) {
        return std::unexpected(record_result.error());
    }
    auto &record = *record_result.value();
    if (!gui_runtime_ || !record.document_id.has_value()) {
        return std::unexpected("App GUI document is not loaded");
    }
    if (record.action_connection_keys.contains(action)) {
        BROOKESIA_LOGD(
            "Skip duplicate GUI action subscription: app_id(%1%), manifest_id(%2%), action(%3%)",
            app_id, record.info.manifest.id, action
        );
        return {};
    }
    auto heap_before_connect = heap_trace::capture();
    if constexpr (heap_trace::enabled) {
        const auto details = "app_id=" + std::to_string(app_id) +
                             " manifest_id=" + record.info.manifest.id +
                             " action=" + action +
                             " action_connections=" + std::to_string(record.action_connections.size());
        heap_trace::log_detail(
            "system.gui.action",
            "subscribe before GUI runtime connect",
            record.info.manifest.id,
            details,
            heap_before_connect,
            &heap_before_task
        );
    }
    auto connection = gui_runtime_->subscribe_event_action(
                          *record.document_id,
                          action,
    [this, app_id, action](const gui::Event & event) {
        auto event_path = event.path;
        auto payload_json = boost::json::serialize(event.payload);
        auto post_result = post_task(
                               SYSTEM_APP_INPUT_TASK_GROUP,
                               [
                                   this,
                                   app_id,
                                   action,
                                   event_path = std::move(event_path),
                                   payload_json = std::move(payload_json)
        ]() {
            auto result = dispatch_action(app_id, action, event_path, payload_json);
            if (!result) {
                BROOKESIA_LOGW(
                    "Dispatch app action failed: app_id(%1%), action(%2%), error(%3%)",
                    app_id, action, result.error()
                );
            }
        }
                           );
        if (!post_result) {
            BROOKESIA_LOGW(
                "Post app action failed: app_id(%1%), action(%2%), error(%3%)",
                app_id, action, post_result.error()
            );
        }
    }
                      );
    if constexpr (heap_trace::enabled) {
        const auto details = "app_id=" + std::to_string(app_id) +
                             " manifest_id=" + record.info.manifest.id +
                             " action=" + action +
                             " connected=" + std::to_string(connection.connected());
        heap_trace::log_detail(
            "system.gui.action",
            "subscribe after GUI runtime connect",
            record.info.manifest.id,
            details,
            heap_trace::capture(),
            &heap_before_connect
        );
    }
    if (!connection.connected()) {
        return std::unexpected("Failed to subscribe GUI action");
    }
    record.action_connections.push_back(std::move(connection));
    record.action_connection_keys.insert(action);
    if constexpr (heap_trace::enabled) {
        const auto details = "app_id=" + std::to_string(app_id) +
                             " manifest_id=" + record.info.manifest.id +
                             " action=" + action +
                             " action_connections=" + std::to_string(record.action_connections.size());
        heap_trace::log_detail(
            "system.gui.action",
            "subscribe after action_connections push_back",
            record.info.manifest.id,
            details,
            heap_trace::capture(),
            &heap_before_connect
        );
    }
    return {};
}

std::expected<void, std::string> System::gui_subscribe_action(AppId app_id, std::string_view action)
{
#if defined(__EMSCRIPTEN__)
    auto action_string = std::string(action);
    if (esp_brookesia::gui::wasm::post_gui_task([this, app_id, action = std::move(action_string)]() mutable {
    std::expected<void, std::string> result;
    impl_->execute_task_with_group_context(
        SYSTEM_GUI_TASK_GROUP,
    [this, app_id, action = std::move(action), &result]() mutable {
        result = impl_->subscribe_gui_action_now(app_id, std::move(action));
        }
        );
        if (!result)
        {
            BROOKESIA_LOGW(
                "Deferred GUI action subscribe failed: app_id(%1%), error(%2%)",
                app_id, result.error()
            );
        }
    })) {
        return {};
    }
    return std::unexpected("Failed to post GUI subscribe action task");
#else
    return impl_->run_task_sync<std::expected<void, std::string>>(
               SYSTEM_GUI_TASK_GROUP,
    [this, app_id, action = std::string(action)]() mutable {
        return impl_->subscribe_gui_action_now(app_id, std::move(action));
    },
    std::unexpected("Failed to post GUI subscribe action task")
           );
#endif
}

gui::ScopedConnection System::gui_subscribe_action(
    AppId app_id,
    std::string_view action,
    std::function<void(const gui::Event &)> handler
)
{
    return impl_->run_task_sync<gui::ScopedConnection>(
               SYSTEM_GUI_TASK_GROUP,
    [this, app_id, action = std::string(action), handler = std::move(handler)]() mutable {
        auto record_result = impl_->get_record(app_id);
        if (!record_result)
        {
            return gui::ScopedConnection {};
        }
        auto &record = *record_result.value();
        if (!impl_->gui_runtime_ || !record.document_id.has_value())
        {
            return gui::ScopedConnection {};
        }
        auto scheduled_handler = [this, handler = std::move(handler)](const gui::Event & event)
        {
            auto event_copy = event;
            auto post_result = impl_->post_task(
                                   SYSTEM_APP_INPUT_TASK_GROUP,
            [handler, event_copy = std::move(event_copy)]() mutable {
                handler(event_copy);
            }
                               );
            if (!post_result) {
                BROOKESIA_LOGW("Post app action handler failed: %1%", post_result.error());
            }
        };
        return impl_->gui_runtime_->subscribe_event_action(*record.document_id, action, std::move(scheduled_handler));
    },
    gui::ScopedConnection {}
           );
}

std::expected<void, std::string> System::gui_trigger_screen_flow(
    AppId app_id,
    std::string_view screen_flow,
    std::string_view action
)
{
    return impl_->run_task_sync<std::expected<void, std::string>>(
               SYSTEM_GUI_INPUT_TASK_GROUP,
               [this, app_id, screen_flow = std::string(screen_flow), action = std::string(action)]()
    -> std::expected<void, std::string> {
        auto record_result = impl_->get_record(app_id);
        if (!record_result)
        {
            return std::unexpected(record_result.error());
        }
        auto &record = *record_result.value();
        if (!impl_->gui_runtime_ || !record.document_id.has_value())
        {
            return std::unexpected("App GUI document is not loaded");
        }
        return impl_->gui_runtime_->trigger_screen_flow(*record.document_id, screen_flow, action);
    },
    std::unexpected("Failed to post GUI screen flow trigger task")
           );
}

std::optional<std::string> System::gui_get_screen_flow_state(AppId app_id, std::string_view screen_flow) const
{
    return impl_->run_task_sync<std::optional<std::string>>(
               SYSTEM_GUI_TASK_GROUP,
    [this, app_id, screen_flow = std::string(screen_flow)]() -> std::optional<std::string> {
        auto record_result = impl_->get_record(app_id);
        if (!record_result || !impl_->gui_runtime_ || !record_result.value()->document_id.has_value())
        {
            return std::nullopt;
        }
        return impl_->gui_runtime_->get_screen_flow_state(*record_result.value()->document_id, screen_flow);
    },
    std::nullopt
           );
}

std::optional<gui::ViewFrame> System::gui_get_view_frame(AppId app_id, std::string_view absolute_path) const
{
    return impl_->run_task_sync<std::optional<gui::ViewFrame>>(
               SYSTEM_GUI_TASK_GROUP,
    [this, app_id, absolute_path = std::string(absolute_path)]() -> std::optional<gui::ViewFrame> {
        auto record_result = impl_->get_record(app_id);
        if (!record_result || !impl_->gui_runtime_ || !record_result.value()->document_id.has_value())
        {
            return std::nullopt;
        }
        auto view = impl_->gui_runtime_->find_view(*record_result.value()->document_id, absolute_path);
        if (!view.valid())
        {
            return std::nullopt;
        }
        auto state = impl_->gui_runtime_->get_view_state(view, gui::ViewStateKind::Frame);
        if (!state.has_value())
        {
            return std::nullopt;
        }
        const auto *frame = std::get_if<gui::ViewFrame>(&state.value());
        if (frame == nullptr)
        {
            return std::nullopt;
        }
        return *frame;
    },
    std::nullopt
           );
}

std::expected<void, std::string> System::gui_set_view_src(
    AppId app_id,
    std::string_view absolute_path,
    std::string_view src
)
{
    return impl_->post_gui_input_task(
    [this, app_id, absolute_path = std::string(absolute_path), src = std::string(src)]() {
        auto record_result = impl_->get_record(app_id);
        if (!record_result || !impl_->gui_runtime_ || !record_result.value()->document_id.has_value()) {
            BROOKESIA_LOGW("Skip GUI image source update: app_id(%1%), path(%2%)", app_id, absolute_path);
            return;
        }
        impl_->flush_pending_gui_bindings(app_id);
        if (!impl_->gui_runtime_->set_view_src(*record_result.value()->document_id, absolute_path, src)) {
            BROOKESIA_LOGW("Failed to set GUI image source: app_id(%1%), path(%2%)", app_id, absolute_path);
        }
    }
           );
}

std::expected<gui::SubscriptionId, std::string> System::gui_start_view_animation_with_id(
    AppId app_id,
    std::string_view absolute_path,
    const gui::Animation &animation,
    gui::Runtime::AnimationCompletedHandler completed_handler
)
{
    auto start_result = gui_start_view_animation_with_result(
                            app_id, absolute_path, animation, std::move(completed_handler)
                        );
    if (!start_result) {
        return std::unexpected(start_result.error());
    }
    if (start_result->subscription_id == 0) {
        return std::unexpected("Failed to start view animation");
    }
    return start_result->subscription_id;
}

std::expected<gui::RuntimeAnimationStartResult, std::string> System::gui_start_view_animation_with_result(
    AppId app_id,
    std::string_view absolute_path,
    const gui::Animation &animation,
    gui::Runtime::AnimationCompletedHandler completed_handler
)
{
    return impl_->run_task_sync<std::expected<gui::RuntimeAnimationStartResult, std::string>>(
               SYSTEM_GUI_INPUT_TASK_GROUP,
               [this,
                app_id,
                absolute_path = std::string(absolute_path),
                animation,
    completed_handler = std::move(completed_handler)]() mutable -> std::expected<gui::RuntimeAnimationStartResult, std::string> {
        auto record_result = impl_->get_record(app_id);
        if (!record_result)
        {
            return std::unexpected(record_result.error());
        }
        auto &record = *record_result.value();
        if (!impl_->gui_runtime_ || !record.document_id.has_value())
        {
            return std::unexpected("App GUI document is not loaded");
        }
        auto scheduled_completed_handler = [this, completed_handler = std::move(completed_handler)]() mutable {
            if (!completed_handler)
            {
                return;
            }
            auto post_result = impl_->post_task(
                SYSTEM_APP_INPUT_TASK_GROUP,
            [completed_handler = std::move(completed_handler)]() mutable {
                completed_handler();
            }
            );
            if (!post_result)
            {
                BROOKESIA_LOGW("Post animation completed handler failed: %1%", post_result.error());
            }
        };
        auto start_result = impl_->gui_runtime_->start_view_animation_with_result(
                                *record.document_id,
                                absolute_path,
                                animation,
                                std::move(scheduled_completed_handler)
                            );
        if (start_result.subscription_id == 0)
        {
            return std::unexpected("Failed to start view animation");
        }
        return start_result;
    },
    std::unexpected("Failed to post GUI animation task")
           );
}

bool System::gui_stop_animation(AppId, gui::SubscriptionId subscription_id)
{
    auto result = impl_->post_gui_input_task([this, subscription_id]() {
        if (impl_->gui_runtime_) {
            (void)impl_->gui_runtime_->unsubscribe_subscription(subscription_id);
        }
    });
    return result.has_value();
}

std::expected<void, std::string> System::gui_scroll_to_view(
    AppId app_id,
    std::string_view absolute_path,
    bool animated
)
{
    return impl_->run_task_sync<std::expected<void, std::string>>(
               SYSTEM_GUI_INPUT_TASK_GROUP,
    [this, app_id, absolute_path = std::string(absolute_path), animated]() -> std::expected<void, std::string> {
        auto record_result = impl_->get_record(app_id);
        if (!record_result)
        {
            return std::unexpected(record_result.error());
        }
        auto &record = *record_result.value();
        if (!impl_->gui_runtime_ || !record.document_id.has_value())
        {
            return std::unexpected("App GUI document is not loaded");
        }
        impl_->flush_pending_gui_bindings(app_id);
        if (!impl_->gui_runtime_->scroll_view_to_visible(*record.document_id, absolute_path, animated))
        {
            return std::unexpected("Failed to scroll view to visible");
        }
        return {};
    },
    std::unexpected("Failed to post GUI scroll task")
           );
}

std::expected<GuiBatchResult, std::string> System::gui_execute_batch(
    AppId app_id,
    const std::vector<GuiBatchCommand> &commands
)
{
    if (impl_->should_defer_gui_task(SYSTEM_GUI_INPUT_TASK_GROUP)) {
        if (!can_defer_gui_batch_commands(commands)) {
            return std::unexpected("Cannot defer GUI batch with synchronous result commands while LVGL is rendering");
        }
        auto post_result = impl_->post_gui_input_task([this, app_id, commands]() {
            auto result = impl_->execute_gui_batch_now(app_id, commands);
            if (!result) {
                BROOKESIA_LOGW("Deferred GUI batch failed: app_id(%1%), error(%2%)", app_id, result.error());
            } else if (!result->success) {
                BROOKESIA_LOGW("Deferred GUI batch completed with failed command: app_id(%1%)", app_id);
            }
        });
        if (!post_result) {
            return std::unexpected(post_result.error());
        }
        return make_deferred_gui_batch_result(commands);
    }

    return impl_->run_task_sync<std::expected<GuiBatchResult, std::string>>(
               SYSTEM_GUI_INPUT_TASK_GROUP,
    [this, app_id, commands]() -> std::expected<GuiBatchResult, std::string> {
        return impl_->execute_gui_batch_now(app_id, commands);
    },
    std::unexpected("Failed to post GUI batch task")
           );
}

void System::set_gui_view_debug_enabled(bool enabled)
{
    (void)impl_->run_task_sync<bool>(
        SYSTEM_GUI_TASK_GROUP,
    [this, enabled]() {
        if (!impl_->gui_runtime_) {
            return false;
        }
        impl_->gui_runtime_->set_view_debug_enabled(enabled);
        return true;
    },
    false
    );
}

bool System::is_gui_view_debug_enabled() const
{
    return impl_->run_task_sync<bool>(
               SYSTEM_GUI_TASK_GROUP,
    [this]() {
        return impl_->gui_runtime_ && impl_->gui_runtime_->is_view_debug_enabled();
    },
    false
           );
}

std::expected<void, std::string> System::enable_app_gui_live_preview(
    AppId app_id,
    const gui::LivePreviewOptions &options
)
{
    return impl_->run_task_sync<std::expected<void, std::string>>(
               SYSTEM_GUI_TASK_GROUP,
    [this, app_id, options]() -> std::expected<void, std::string> {
        auto record_result = impl_->get_record(app_id);
        if (!record_result)
        {
            return std::unexpected(record_result.error());
        }
        auto &record = *record_result.value();
        if (!record.document_id.has_value())
        {
            return std::unexpected("App GUI document is not loaded");
        }
        return impl_->enable_live_preview_for_document(*record.document_id, options);
    },
    std::unexpected("Failed to post GUI live preview enable task")
           );
}

bool System::disable_app_gui_live_preview(AppId app_id)
{
    return impl_->run_task_sync<bool>(
               SYSTEM_GUI_TASK_GROUP,
    [this, app_id]() {
        auto record_result = impl_->get_record(app_id);
        if (!record_result || !record_result.value()->document_id.has_value()) {
            return false;
        }
        return impl_->disable_live_preview_for_document(*record_result.value()->document_id);
    },
    false
           );
}

void System::poll_gui_live_preview()
{
    (void)impl_->run_task_sync<bool>(
        SYSTEM_GUI_TASK_GROUP,
    [this]() {
        impl_->poll_live_preview_documents();
        return true;
    },
    false
    );
}

std::expected<gui::DocumentId, std::string> System::system_gui_load_file(
    std::string_view resource_dir,
    std::string_view path
)
{
    return system_gui().load_file(resource_dir, path);
}

std::expected<gui::DocumentId, std::string> System::system_gui_load_json(
    std::string_view root_path,
    std::string_view json,
    std::string_view resource_dir
)
{
    return system_gui().load_json(root_path, json, resource_dir);
}

std::expected<gui::View, std::string> System::system_gui_mount_screen(
    gui::DocumentId document_id,
    std::string_view absolute_path,
    const gui::MountTarget &target
)
{
    return system_gui().mount_screen(document_id, absolute_path, target);
}

bool System::system_gui_unmount_screen(gui::DocumentId document_id, std::string_view absolute_path)
{
    return system_gui().unmount_screen(document_id, absolute_path);
}

bool System::system_gui_unload(gui::DocumentId document_id)
{
    return system_gui().unload(document_id);
}

std::expected<void, std::string> System::system_gui_enable_live_preview(
    gui::DocumentId document_id,
    const gui::LivePreviewOptions &options
)
{
    return system_gui().enable_live_preview(document_id, options);
}

bool System::system_gui_disable_live_preview(gui::DocumentId document_id)
{
    return system_gui().disable_live_preview(document_id);
}

gui::ScopedConnection System::system_gui_subscribe_action(
    gui::DocumentId document_id,
    std::string_view action,
    gui::Runtime::ActionHandler handler
)
{
    return system_gui().subscribe_action(document_id, action, std::move(handler));
}

} // namespace esp_brookesia::system::core
