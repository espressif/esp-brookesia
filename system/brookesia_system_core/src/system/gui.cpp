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
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <set>
#include <span>
#include <string_view>
#include <thread>
#include <vector>

#if BROOKESIA_SYSTEM_CORE_GUI_BINDING_PROBE
#include "esp_timer.h"
#endif
#include "boost/json.hpp"

#include "brookesia/service_helper/system/storage.hpp"
#include "private/utils.hpp"
#include "private/heap_trace.hpp"
#include "private/system/impl.hpp"
#if CONFIG_BROOKESIA_GUI_BACKEND_GSP
#   include "brookesia/gui_gsp/bundle.hpp"
#endif

namespace esp_brookesia::system::core {

namespace {

constexpr uint32_t STORAGE_FS_TIMEOUT_MS = 5000;

#if CONFIG_BROOKESIA_GUI_BACKEND_GSP
uint32_t gsp_name_key(std::string_view name)
{
    uint32_t key = 2166136261U;
    for (const char character : name) {
        key = (key ^ static_cast<uint8_t>(character)) * 16777619U;
    }
    return key;
}

// A burst of updates, such as a whole page being filled in, can fill the GSP command queue. The queue
// reports that as a timeout while it drains, so wait briefly and offer the update again.
template <typename Submit>
esp_gsp_err_t submit_gsp_update(Submit &&submit)
{
    constexpr int ATTEMPTS = 20;
    constexpr auto PAUSE = std::chrono::milliseconds(5);
    auto result = submit();
    for (int attempt = 1; (result == ESP_GSP_ERR_TIMEOUT) && (attempt < ATTEMPTS); ++attempt) {
        std::this_thread::sleep_for(PAUSE);
        result = submit();
    }
    return result;
}

int64_t steady_us()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::steady_clock::now().time_since_epoch()
           ).count();
}

std::atomic<int64_t> gsp_page_requested_us{0};
std::atomic<uint32_t> gsp_page_delivery_seq{0};
#endif

#if BROOKESIA_SYSTEM_CORE_USE_JSON_GUI
std::string make_binding_update_key(std::string_view absolute_path, std::string_view key)
{
    std::string update_key(absolute_path);
    update_key.push_back('\x1f');
    update_key.append(key);
    return update_key;
}
#endif

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
    case GuiBatchCommand::Type::ScrollTo:
        return "ScrollTo";
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
    auto result = service::helper::Storage::fs_read_text(std::string(path), STORAGE_FS_TIMEOUT_MS);
    if (!result) {
        return std::unexpected("Failed to read GUI root file: " + std::string(path) + ", error: " + result.error());
    }
    return result.value();
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
        case GuiBatchCommand::Type::ScrollTo:
            flush_pending_gui_bindings(app_id);
            command_result.success = gui_runtime_->scroll_view_to(
                                         *record.document_id,
                                         command.path,
                                         command.scroll_x,
                                         command.scroll_y,
                                         command.animated
                                     );
            if (!command_result.success) {
                command_result.error_message = "Failed to scroll view";
            }
            break;
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
#if !BROOKESIA_SYSTEM_CORE_USE_JSON_GUI
    (void)app_id;
    (void)updates;
    return std::unexpected("JSON GUI is unavailable with the GSP backend");
#else
    if (updates.empty()) {
        return {};
    }

    [[maybe_unused]] heap_trace::Snapshot heap_before_enqueue {};
    if constexpr (heap_trace::detail_enabled) {
        heap_before_enqueue = heap_trace::capture();
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
    if constexpr (heap_trace::detail_enabled) {
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
#endif
}


void System::Impl::flush_pending_gui_bindings(AppId app_id)
{
    [[maybe_unused]] heap_trace::Snapshot heap_before_flush {};
    if constexpr (heap_trace::detail_enabled) {
        heap_before_flush = heap_trace::capture();
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
        if constexpr (heap_trace::detail_enabled) {
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
#if BROOKESIA_SYSTEM_CORE_GUI_BINDING_PROBE
        const int64_t apply_started_us = esp_timer_get_time();
#endif
        auto record_result = get_record(app_id);
        if (!record_result || !gui_runtime_ || !record_result.value()->document_id.has_value()) {
            BROOKESIA_LOGW("Skip pending GUI binding flush: app_id(%1%), count(%2%)", app_id, updates.size());
        } else {
            gui_runtime_->set_binding_values(*record_result.value()->document_id, updates);
        }
#if BROOKESIA_SYSTEM_CORE_GUI_BINDING_PROBE
        gui_binding_flush_count_.fetch_add(1, std::memory_order_relaxed);
        gui_binding_applied_updates_.fetch_add(updates.size(), std::memory_order_relaxed);
        gui_binding_apply_us_.fetch_add(
            static_cast<uint64_t>(esp_timer_get_time() - apply_started_us), std::memory_order_relaxed);
#endif
        if constexpr (heap_trace::detail_enabled) {
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

    // This re-arm is a continuation ("run another pass later"), not an
    // affinity-sensitive call, so it must always take a real queue hop. A flush
    // always runs in the GUI task domain, so the post_task inline shortcut would
    // execute the next pass nested inside this one; under sustained binding
    // arrivals (v1.8 binding_stress) every level re-arms and the worker stack is
    // exhausted. allow_inline_execution=false keeps the pass ordered but queued.
    auto post_result = post_gui_task([this, app_id]() {
        flush_pending_gui_bindings(app_id);
    }, /*allow_inline_execution=*/false);
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


#if CONFIG_BROOKESIA_GUI_BACKEND_GSP
void System::Impl::forward_gsp_event(esp_gsp_handle_t gsp, const esp_gsp_event_t *event, void *user_ctx)
{
    (void)gsp;
    auto *target = static_cast<AppRecord::GspEventTarget *>(user_ctx);
    if ((target == nullptr) || (target->gate == nullptr) || (event == nullptr)) {
        return;
    }
    std::lock_guard lock(target->gate->mutex);
    if (target->gate->impl == nullptr) {
        return;
    }
    const uint64_t epoch = (target->epoch != nullptr)
                           ? target->epoch->load(std::memory_order_acquire)
                           : 0;
    target->gate->impl->dispatch_gsp_event(target->app_id, event, epoch);
}
#endif

std::expected<void, std::string> System::Impl::ensure_gsp_loaded(AppRecord &record)
{
#if !CONFIG_BROOKESIA_GUI_BACKEND_GSP
    (void)record;
    return std::unexpected("GSP GUI backend is not enabled");
#else
    if ((record.gsp_host != nullptr) && record.gsp_host->running()) {
        return std::unexpected("GSP session is still running; stop it before starting again");
    }
    for (const auto &[other_id, other] : apps_) {
        if ((other_id != record.info.app_id) && gsp_session_running(other)) {
            return std::unexpected("Another GSP session is already active on this panel");
        }
    }
    if ((gsp_panel_ == nullptr) || (gsp_touch_ == nullptr)) {
        return std::unexpected("GSP display devices are not bound");
    }
    const auto *panel = static_cast<esp_brookesia::hal::display::PanelIface *>(gsp_panel_);
    const auto *touch = static_cast<esp_brookesia::hal::display::TouchIface *>(gsp_touch_);
    if (!gsp_panel_owner_ || (gsp_panel_owner_.get().get() != panel) ||
            !gsp_touch_owner_ || (gsp_touch_owner_.get().get() != touch)) {
        auto held = hold_gsp_display_devices();
        if (!held) {
            return held;
        }
    }
    std::vector<std::byte> package_bytes;
    std::span<const std::byte> bytes;
    const esp_gsp_config_t *runtime_config = nullptr;
    if (record.native_app) {
        bytes = record.native_app->gui_bundle();
        runtime_config = static_cast<const esp_gsp_config_t *>(record.native_app->gui_runtime_config());
        if ((runtime_config == nullptr) && bytes.empty()) {
            return std::unexpected("GSP app has no compiled bundle");
        }
    } else if ((record.info.manifest.kind == AppKind::Runtime) &&
               (record.gui_descriptor.root_kind == GuiRootKind::GspBundle)) {
        const auto bundle_path = resolve_app_resource_path(record.info.manifest, record.gui_descriptor.root);
        auto info = service::helper::Storage::fs_stat(bundle_path, STORAGE_FS_TIMEOUT_MS);
        if (!info || !info->exists || (info->type != service::helper::Storage::FileType::File)) {
            return std::unexpected("GSP bundle not found: " + bundle_path);
        }
        constexpr uint64_t MAX_BUNDLE_BYTES = 8U * 1024U * 1024U;
        if ((info->size < 4) || (info->size > MAX_BUNDLE_BYTES)) {
            return std::unexpected("Damaged GSP bundle: unexpected size: " + bundle_path);
        }
        package_bytes.resize(static_cast<size_t>(info->size));
        auto read_result = service::helper::Storage::fs_read(
                               bundle_path,
                               service::RawBuffer(package_bytes.data(), package_bytes.size()),
                               STORAGE_FS_TIMEOUT_MS
                           );
        if (!read_result || (*read_result != package_bytes.size())) {
            return std::unexpected("Damaged GSP bundle: unable to read " + bundle_path);
        }
        bytes = std::span<const std::byte>(package_bytes.data(), package_bytes.size());
    } else {
        return std::unexpected("GSP bundle GUI requires a native app or a runtime GSP package");
    }

    if (record.gsp_host == nullptr) {
        record.gsp_host = std::make_unique<gui::gsp::AppHost>();
    }
    if (record.gsp_epoch == nullptr) {
        record.gsp_epoch = std::make_unique<std::atomic<uint64_t>>(0);
    }
    if (record.gsp_event_target.gate == nullptr) {
        record.gsp_event_target.gate = std::make_shared<AppRecord::GspCallbackGate>();
    }
    {
        std::lock_guard lock(record.gsp_event_target.gate->mutex);
        record.gsp_event_target.gate->impl = this;
    }
    record.gsp_event_target.app_id = record.info.app_id;
    record.gsp_event_target.epoch = record.gsp_epoch.get();
    const gui::gsp::DisplayLease display{
        .panel = static_cast<hal::display::PanelIface *>(gsp_panel_),
        .touch = static_cast<hal::display::TouchIface *>(gsp_touch_),
    };
    record.gsp_epoch->fetch_add(1, std::memory_order_release);
    auto started = (runtime_config != nullptr)
                   ? record.gsp_host->start(
                       *runtime_config, display, &System::Impl::forward_gsp_event, &record.gsp_event_target
                   )
                   : record.gsp_host->start(
                       bytes, display, &System::Impl::forward_gsp_event, &record.gsp_event_target
                   );
    if (!started) {
        if (!record.gsp_host->running()) {
            record.gsp_host.reset();
            record.gsp_event_target = {};
            record.gsp_epoch->fetch_add(1, std::memory_order_release);
        }
        return started;
    }
    if (!record.native_app) {
        if (auto handle = record.gsp_host->handle(); (handle != nullptr) &&
                (esp_gsp_set_swipe_enabled(handle, false) != ESP_GSP_OK)) {
            BROOKESIA_LOGW("Failed to disable GSP scene swipe for runtime app: %1%", record.info.manifest.id);
        }
        uint16_t target_scene = 0;
        if (record.gsp_scene_known) {
            target_scene = record.gsp_current_scene;
        } else {
            // A bundle with a copy of its pages per theme names the first page for each theme, so the app does not
            // flash the wrong theme while its JavaScript starts.
            const auto &start_scenes = record.gui_descriptor.start_scenes;
            if (auto start = start_scenes.find(environment_.theme_id); start != start_scenes.end()) {
                target_scene = start->second;
            }
            record.gsp_current_scene = target_scene;
            record.gsp_scene_known = true;
        }
        if (target_scene != 0) {
            if (esp_gsp_goto_scene(
                        record.gsp_host->handle(), target_scene, ESP_GSP_NO_TRANSITION
                    ) != ESP_GSP_OK) {
                BROOKESIA_LOGW(
                    "Failed to show scene %1% for theme %2%: %3%", target_scene, environment_.theme_id,
                    record.info.manifest.id
                );
            }
        }
    }
    BROOKESIA_LOGI("GSP app session started: manifest(%1%)", record.info.manifest.id);
    return {};
#endif
}

std::expected<void, std::string> System::Impl::stop_gsp_session(AppRecord &record)
{
#if !CONFIG_BROOKESIA_GUI_BACKEND_GSP
    (void)record;
    return {};
#else
    if (record.gsp_host == nullptr) {
        record.gsp_event_target = {};
        return {};
    }
    auto stopped = record.gsp_host->stop();
    if (!stopped) {
        BROOKESIA_LOGE("GSP stop failed; bundle, callback context, and HAL devices stay owned");
        return stopped;
    }
    record.gsp_host.reset();
    record.gsp_event_target = {};
    if (record.gsp_epoch != nullptr) {
        record.gsp_epoch->fetch_add(1, std::memory_order_release);
    }
    return {};
#endif
}

#if CONFIG_BROOKESIA_GUI_BACKEND_GSP
namespace {

void record_gsp_scene_update(
    std::vector<SystemHostBridge::GspUpdate> &updates,
    const SystemHostBridge::GspUpdate &update
)
{
    auto existing = std::find_if(
                        updates.begin(),
                        updates.end(),
    [&update](const SystemHostBridge::GspUpdate & item) {
        return (item.kind == update.kind) && (item.name == update.name) && (item.property == update.property);
    }
                    );
    if (existing != updates.end()) {
        *existing = update;
        return;
    }
    updates.push_back(update);
}

std::expected<void, std::string> submit_gsp_component_update(
    esp_gsp_handle_t handle,
    const SystemHostBridge::GspUpdate &update
)
{
    if (update.name.empty()) {
        return std::unexpected("GSP object name is empty");
    }
    const auto key = gsp_name_key(update.name);
    if (update.kind == SystemHostBridge::GspUpdate::Kind::SetProperty) {
        if (update.property.empty()) {
            return std::unexpected("GSP property name is empty");
        }
        const auto property_key = gsp_name_key(update.property);
        gsp_property_info_t info{};
        if (esp_gsp_component_get_property_info(handle, key, property_key, &info) != ESP_GSP_OK) {
            return std::unexpected("GSP object " + update.name + " has no property " + update.property);
        }
        using ValueKind = SystemHostBridge::GspUpdate::ValueKind;
        if (info.value_type == GSP_VALUE_COLOR) {
            if (update.value_kind != ValueKind::Text || update.text.size() != 7 || update.text.front() != '#') {
                return std::unexpected("GSP color property " + update.property + " needs a #RRGGBB string");
            }
            uint32_t rgb = 0;
            for (size_t index = 1; index < update.text.size(); ++index) {
                const char digit = update.text[index];
                uint32_t nibble = 0;
                if (digit >= '0' && digit <= '9') {
                    nibble = static_cast<uint32_t>(digit - '0');
                } else if (digit >= 'a' && digit <= 'f') {
                    nibble = static_cast<uint32_t>(digit - 'a' + 10);
                } else if (digit >= 'A' && digit <= 'F') {
                    nibble = static_cast<uint32_t>(digit - 'A' + 10);
                } else {
                    return std::unexpected("GSP color property " + update.property + " needs a #RRGGBB string");
                }
                rgb = (rgb << 4) | nibble;
            }
            const auto submitted = submit_gsp_update([&] {
                return esp_gsp_component_set_property_color_rgb888(handle, key, property_key, rgb);
            });
            if (submitted != ESP_GSP_OK) {
                return std::unexpected("Failed to update GSP color: " + update.name + "." + update.property);
            }
            return {};
        }
        if (update.value_kind == ValueKind::Text) {
            return std::unexpected("GSP property " + update.property + " does not take a string");
        }
        gsp_value_t value{};
        if (info.value_type == GSP_VALUE_BOOL) {
            value.type = GSP_VALUE_BOOL;
            value.data.boolean = update.value != 0;
        } else if (info.value_type == GSP_VALUE_U32) {
            if (update.value < 0) {
                return std::unexpected("GSP property " + update.property + " cannot be negative");
            }
            value.type = GSP_VALUE_U32;
            value.data.u32 = static_cast<uint32_t>(update.value);
        } else {
            value.type = GSP_VALUE_I32;
            value.data.i32 = update.value;
        }
        const auto submitted = submit_gsp_update([&] {
            return esp_gsp_component_set_property(handle, key, property_key, &value);
        });
        if (submitted != ESP_GSP_OK) {
            return std::unexpected("Failed to update GSP property: " + update.name + "." + update.property);
        }
        return {};
    }
    if (update.kind == SystemHostBridge::GspUpdate::Kind::SetText) {
        const auto submitted = submit_gsp_update([&] {
            return esp_gsp_component_set_text(handle, key, update.text.c_str());
        });
        if (submitted != ESP_GSP_OK) {
            return std::unexpected("Failed to update GSP text: " + update.name);
        }
        return {};
    }
    const auto submitted = submit_gsp_update([&] {
        return esp_gsp_component_set_value(handle, key, update.value);
    });
    if (submitted != ESP_GSP_OK) {
        return std::unexpected("Failed to update GSP value: " + update.name);
    }
    return {};
}

} // namespace
#endif

std::expected<void, std::string> System::Impl::apply_runtime_gsp_update(
    runtime::AppId runtime_app_id, const SystemHostBridge::GspUpdate &update
)
{
#if !CONFIG_BROOKESIA_GUI_BACKEND_GSP
    (void)runtime_app_id;
    (void)update;
    return std::unexpected("GSP GUI backend is not enabled");
#else
    auto owner = runtime_to_app_.find(runtime_app_id);
    if (owner == runtime_to_app_.end()) {
        return std::unexpected("Runtime app owner not found");
    }
    auto record_result = get_record(owner->second);
    if (!record_result) {
        return std::unexpected(record_result.error());
    }
    auto &record = *record_result.value();
    if (update.kind == SystemHostBridge::GspUpdate::Kind::ShowPage) {
        if ((update.value < 0) || (update.value > static_cast<int32_t>(UINT16_MAX))) {
            return std::unexpected("GSP page index is out of range");
        }
        const auto next_scene = static_cast<uint16_t>(update.value);
        if (!record.gsp_scene_known || (record.gsp_current_scene != next_scene)) {
            record.gsp_current_scene = next_scene;
            record.gsp_scene_known = true;
            record.gsp_scene_updates.clear();
        }
        if (record.gsp_suspended) {
            return {};
        }
        if ((record.gsp_host == nullptr) || !record.gsp_host->running() || (record.gsp_host->handle() == nullptr)) {
            return std::unexpected("GSP session is not running");
        }
        const auto handle = record.gsp_host->handle();
        gsp_page_requested_us.store(steady_us(), std::memory_order_release);
        if (esp_gsp_goto_scene(handle, next_scene, ESP_GSP_NO_TRANSITION) != ESP_GSP_OK) {
            gsp_page_requested_us.store(0, std::memory_order_release);
            return std::unexpected("Failed to show GSP page " + std::to_string(update.value));
        }
        BROOKESIA_LOGI("GSP page requested scene(%1%)", update.value);
        return {};
    }
    if (record.gsp_suspended) {
        if (update.name.empty()) {
            return std::unexpected("GSP object name is empty");
        }
        record_gsp_scene_update(record.gsp_scene_updates, update);
        return {};
    }
    if ((record.gsp_host == nullptr) || !record.gsp_host->running() || (record.gsp_host->handle() == nullptr)) {
        return std::unexpected("GSP session is not running");
    }
    auto result = submit_gsp_component_update(record.gsp_host->handle(), update);
    if (result) {
        record_gsp_scene_update(record.gsp_scene_updates, update);
    }
    return result;
#endif
}

std::expected<AppId, std::string> System::suspend_gsp_session_for_modal(AppId exclude_app_id)
{
#if !CONFIG_BROOKESIA_GUI_BACKEND_GSP
    (void)exclude_app_id;
    return INVALID_APP_ID;
#else
    AppId target_id = INVALID_APP_ID;
    for (auto &[app_id, record] : impl_->apps_) {
        if ((app_id != exclude_app_id) && impl_->gsp_session_running(record)) {
            target_id = app_id;
            break;
        }
    }
    if (target_id == INVALID_APP_ID) {
        return INVALID_APP_ID;
    }
    auto record_result = impl_->get_record(target_id);
    if (!record_result) {
        return std::unexpected(record_result.error());
    }
    auto &record = *record_result.value();
    if (record.native_app && record.context) {
        (void)record.native_app->on_pause(*record.context);
    }
    record.gsp_suspended = true;
    auto stopped = impl_->run_task_sync<std::expected<void, std::string>>(
                       SYSTEM_GUI_TASK_GROUP,
    [this, &record]() {
        return impl_->stop_gsp_session(record);
    },
    std::unexpected("Failed to post GSP modal suspend task")
                   );
    if (!stopped) {
        record.gsp_suspended = false;
        return std::unexpected(stopped.error());
    }
    return target_id;
#endif
}

std::expected<void, std::string> System::resume_gsp_session_after_modal(AppId app_id)
{
#if !CONFIG_BROOKESIA_GUI_BACKEND_GSP
    (void)app_id;
    return {};
#else
    if (app_id == INVALID_APP_ID) {
        return {};
    }
    auto record_result = impl_->get_record(app_id);
    if (!record_result) {
        return {};
    }
    auto &record = *record_result.value();
    if (!record.gsp_suspended || (record.info.state != AppState::Running)) {
        record.gsp_suspended = false;
        return {};
    }
    record.gsp_suspended = false;
    auto resumed = impl_->run_task_sync<std::expected<void, std::string>>(
                       SYSTEM_GUI_TASK_GROUP,
    [this, &record]() -> std::expected<void, std::string> {
        auto loaded = impl_->ensure_gsp_loaded(record);
        if (!loaded)
        {
            return loaded;
        }
        if (!record.native_app && (record.gsp_host != nullptr) && (record.gsp_host->handle() != nullptr))
        {
            const auto handle = record.gsp_host->handle();
            (void)esp_gsp_flush(handle, 500);
            for (const auto &cached : record.gsp_scene_updates) {
                (void)submit_gsp_component_update(handle, cached);
            }
        }
        return {};
    },
    std::unexpected("Failed to post GSP modal resume task")
                   );
    if (!resumed) {
        return resumed;
    }
    if (record.native_app && record.context) {
        return record.native_app->on_resume(*record.context);
    }
    if (record.runtime_app_id.has_value() && record.gsp_scene_known && (record.gsp_current_scene == 0)) {
        boost::json::object payload;
        payload["scene"] = 0;
        (void)impl_->call_runtime_lifecycle(
            record, LIFECYCLE_ON_ACTION, {"gsp.scene", "0", boost::json::serialize(payload)}
        );
    }
    return {};
#endif
}

bool System::Impl::gsp_session_running(const AppRecord &record) const
{
#if !CONFIG_BROOKESIA_GUI_BACKEND_GSP
    (void)record;
    return false;
#else
    return (record.gsp_host != nullptr) && record.gsp_host->running();
#endif
}

bool System::Impl::any_gsp_session_running() const
{
    if (gsp_shell_session_running()) {
        return true;
    }
    for (const auto &[_, record] : apps_) {
        if (gsp_session_running(record)) {
            return true;
        }
    }
    return false;
}

std::expected<void, std::string> System::Impl::hold_gsp_display_devices()
{
#if !CONFIG_BROOKESIA_GUI_BACKEND_GSP
    return {};
#else
    if ((gsp_panel_ == nullptr) && (gsp_touch_ == nullptr)) {
        return {};
    }
    if ((gsp_panel_ == nullptr) || (gsp_touch_ == nullptr)) {
        return std::unexpected("GSP display binding is incomplete");
    }
    const auto *panel = static_cast<esp_brookesia::hal::display::PanelIface *>(gsp_panel_);
    const auto *touch = static_cast<esp_brookesia::hal::display::TouchIface *>(gsp_touch_);
    gsp_panel_owner_.reset();
    gsp_touch_owner_.reset();
    for (auto &handle : esp_brookesia::hal::acquire_interfaces<esp_brookesia::hal::display::PanelIface>()) {
        if (handle.get().get() == panel) {
            gsp_panel_owner_ = std::move(handle);
            break;
        }
    }
    for (auto &handle : esp_brookesia::hal::acquire_interfaces<esp_brookesia::hal::display::TouchIface>()) {
        if (handle.get().get() == touch) {
            gsp_touch_owner_ = std::move(handle);
            break;
        }
    }
    if (!gsp_panel_owner_ || (gsp_panel_owner_.get().get() != panel) ||
            !gsp_touch_owner_ || (gsp_touch_owner_.get().get() != touch)) {
        gsp_panel_owner_.reset();
        gsp_touch_owner_.reset();
        return std::unexpected("GSP display devices are not held HAL panel and touch instances");
    }
    return {};
#endif
}

void System::Impl::release_gsp_display_devices()
{
#if !CONFIG_BROOKESIA_GUI_BACKEND_GSP
    return;
#else
    if (any_gsp_session_running()) {
        return;
    }
    gsp_panel_owner_.reset();
    gsp_touch_owner_.reset();
#endif
}

void System::Impl::detach_gsp_callbacks()
{
#if CONFIG_BROOKESIA_GUI_BACKEND_GSP
    for (auto &[_, record] : apps_) {
        if (record.gsp_event_target.gate == nullptr) {
            continue;
        }
        std::lock_guard lock(record.gsp_event_target.gate->mutex);
        record.gsp_event_target.gate->impl = nullptr;
    }
#endif
}

bool System::Impl::discard_app_record(AppId app_id)
{
    auto found = apps_.find(app_id);
    if (found == apps_.end()) {
        return true;
    }
    if (gsp_session_running(found->second)) {
        BROOKESIA_LOGE("Keeping app record because the GSP session is still running: app_id(%1%)", app_id);
        return false;
    }
    manifest_id_to_app_.erase(found->second.info.manifest.id);
    if (active_app_id_ == app_id) {
        active_app_id_ = INVALID_APP_ID;
    }
    apps_.erase(found);
    return true;
}

#if CONFIG_BROOKESIA_GUI_BACKEND_GSP
void System::Impl::dispatch_gsp_event(AppId app_id, const esp_gsp_event_t *event, uint64_t epoch)
{
    if (event == nullptr) {
        return;
    }
    char action[96];
    if (event->type == ESP_GSP_EVENT_SCENE_CHANGED) {
        std::snprintf(action, sizeof(action), "scene:%u", static_cast<unsigned>(event->scene_id));
    } else if (event->type == ESP_GSP_EVENT_CALL) {
        std::snprintf(
            action, sizeof(action), "%u:%u:%u", static_cast<unsigned>(event->scene_id),
            static_cast<unsigned>(event->action_id), static_cast<unsigned>(event->arg)
        );
    } else {
        return;
    }
    std::string action_text(action);
    const uint16_t scene_id = event->scene_id;
    const uint16_t action_id = event->action_id;
    const uint32_t arg = event->arg;
    const bool scene_changed = event->type == ESP_GSP_EVENT_SCENE_CHANGED;
    auto posted = post_task(
                      SYSTEM_APP_TASK_GROUP,
    [this, app_id, action_text, epoch, scene_id, action_id, arg, scene_changed]() {
        auto posted_record = get_record(app_id);
        if (!posted_record || !posted_record.value()->context) {
            return;
        }
        auto &record = *posted_record.value();
        const uint64_t current_epoch = (record.gsp_epoch != nullptr)
                                       ? record.gsp_epoch->load(std::memory_order_acquire)
                                       : 0;
        if ((record.info.state != AppState::Running) || (current_epoch != epoch) ||
                (record.gsp_host == nullptr) || !record.gsp_host->running()) {
            BROOKESIA_LOGI("Dropping GSP action after the session ended: app_id(%1%)", app_id);
            return;
        }
        if (record.native_app) {
            auto result = record.native_app->on_action(*record.context, action_text);
            if (!result) {
                BROOKESIA_LOGW("GSP app action failed: app_id(%1%), error(%2%)", app_id, result.error());
            }
            return;
        }
        if (!record.runtime_app_id.has_value()) {
            return;
        }

        if (scene_changed) {
            const auto requested_us = gsp_page_requested_us.exchange(0, std::memory_order_acq_rel);
            if (requested_us != 0) {
                const auto elapsed_ms = (steady_us() - requested_us) / 1000;
                BROOKESIA_LOGI("GSP page delivered scene(%1%) elapsed_ms(%2%)", scene_id, elapsed_ms);
            }
        }
        boost::json::object payload;
        payload["scene"] = scene_id;
        std::string action_name = scene_changed ? "gsp.scene" : "gsp.call";
        if (!scene_changed) {
            payload["action_id"] = action_id;
            payload["arg"] = arg;
            const auto bundle = record.gsp_host->bundle();
            if (bundle) {
                auto key = bundle->action_key(scene_id, action_id);
                if (key) {
                    payload["key"] = *key;
                } else {
                    payload["key_error"] = key.error();
                }
            }
        }
        auto result = call_runtime_lifecycle(
                          record, LIFECYCLE_ON_ACTION,
        {std::move(action_name), std::to_string(scene_id), boost::json::serialize(payload)}
                      );
        if (scene_changed) {
            gsp_page_delivery_seq.fetch_add(1, std::memory_order_release);
        }
        if (!result) {
            BROOKESIA_LOGW("GSP runtime action failed: app_id(%1%), error(%2%)", app_id, result.error());
        }
    });
    if (!posted) {
        BROOKESIA_LOGW("Failed to post GSP action: app_id(%1%), error(%2%)", app_id, posted.error());
    }
}

extern "C" uint32_t brookesia_gsp_page_delivery_seq()
{
    return gsp_page_delivery_seq.load(std::memory_order_acquire);
}
#endif

std::expected<void, std::string> System::Impl::ensure_gui_loaded(AppRecord &record, bool enable_idle_warm)
{
    const auto &descriptor = record.gui_descriptor;
    if (descriptor.root_kind == GuiRootKind::GspBundle) {
        return ensure_gsp_loaded(record);
    }
    if (descriptor.root_kind == GuiRootKind::None) {
        return {};
    }
    if (!gui_runtime_) {
        return std::unexpected("GUI runtime is not available");
    }
    if (!record.document_id.has_value()) {
        gui::DocumentLoadOptions load_options;
        // Non-entry Eager screens stay unbuilt until idle warm / first mount / path API access.
        // find_view and create_view materialize the owning Eager screen on demand.
        load_options.defer_non_entry_eager_screens = true;
        load_options.build_entry_screens = enable_idle_warm;
        load_options.schedule_idle_warm = enable_idle_warm;
        std::expected<gui::DocumentId, std::string> load_result = std::unexpected("Invalid GUI root");
        if (descriptor.root_kind == GuiRootKind::File) {
            load_result = gui_runtime_->load_file(
                              resolve_app_resource_path(record.info.manifest, descriptor.root),
                              environment_,
                              load_options
                          );
        } else if (descriptor.root_kind == GuiRootKind::JsonString) {
            load_result = gui_runtime_->load_json(
                              record.info.manifest.id,
                              descriptor.root,
                              resolve_app_resource_dir(record.info.manifest),
                              environment_,
                              load_options
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
    } else if (enable_idle_warm) {
        gui_runtime_->request_idle_screen_warm(*record.document_id);
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


std::expected<void, std::string> System::Impl::prepare_installed_app_gui(AppRecord &record)
{
    auto icon_result = register_app_icon_resource(record);
    if (!icon_result) {
        return icon_result;
    }
    if (!record.preload_dom) {
        return {};
    }

    auto resource_result = register_app_gui_resources(record);
    if (!resource_result) {
        return resource_result;
    }

    // preload_dom: parse + register resources only; defer all Eager screen builds until StartApp.
    return ensure_gui_loaded(record, false);
}


void System::Impl::release_app_gui_presentation(AppRecord &record)
{
    record.action_connections.clear();
    record.action_connection_keys.clear();
    if (!gui_runtime_ || !record.document_id.has_value()) {
        return;
    }

    // preload_dom keeps the parsed document and registered resources resident
    // between app runs; only detach the currently mounted presentation.
    for (const auto &flow_entry : record.gui_descriptor.screen_flows) {
        (void)gui_runtime_->stop_screen_flow(*record.document_id, flow_entry.screen_flow);
    }
}


void System::Impl::cleanup_stopped_app_gui(AppRecord &record)
{
    auto gsp_result = stop_gsp_session(record);
    if (!gsp_result) {
        BROOKESIA_LOGE("Keeping GSP resources after stop failure: %1%", gsp_result.error());
        return;
    }
    clear_pending_gui_bindings(record.info.app_id);
    if (record.preload_dom) {
        release_app_gui_presentation(record);
        return;
    }

    auto unloaded = unload_gui(record);
    if (!unloaded) {
        return;
    }
    unregister_app_gui_resources(record);
}


std::expected<void, std::string> System::Impl::rollback_installed_app_gui(AppRecord &record)
{
    auto unloaded = unload_gui(record);
    if (!unloaded) {
        return unloaded;
    }
    unregister_app_gui_resources(record);
    unregister_app_icon_resource(record);
    return {};
}


std::expected<void, std::string> System::Impl::unload_gui(AppRecord &record)
{
    auto stopped = stop_gsp_session(record);
    if (!stopped) {
        BROOKESIA_LOGE("GSP stop failed during GUI unload; session stays owned: %1%", stopped.error());
        return stopped;
    }
    record.action_connections.clear();
    record.action_connection_keys.clear();
    if (gui_runtime_ && record.document_id.has_value()) {
        remove_live_preview_document(*record.document_id);
        if (!gui_runtime_->unload(*record.document_id)) {
            BROOKESIA_LOGW("Failed to unload app GUI document: document_id(%1%)", record.document_id->value());
        }
    }
    record.document_id.reset();
    return {};
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

#if BROOKESIA_SYSTEM_CORE_GUI_BINDING_PROBE
System::GuiBindingFlushStats System::gui_binding_flush_stats() const
{
    return {
        .flush_count = impl_->gui_binding_flush_count_.load(std::memory_order_relaxed),
        .applied_updates = impl_->gui_binding_applied_updates_.load(std::memory_order_relaxed),
        .apply_us = impl_->gui_binding_apply_us_.load(std::memory_order_relaxed),
    };
}
#endif

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
#if !BROOKESIA_SYSTEM_CORE_USE_JSON_GUI
    (void)app_id;
    (void)absolute_path;
    (void)text;
    return std::unexpected("JSON GUI is unavailable with the GSP backend");
#else
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
#endif
}

std::expected<void, std::string> System::gui_set_value(AppId app_id, std::string_view absolute_path, int32_t value)
{
#if !BROOKESIA_SYSTEM_CORE_USE_JSON_GUI
    (void)app_id;
    (void)absolute_path;
    (void)value;
    return std::unexpected("JSON GUI is unavailable with the GSP backend");
#else
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
#endif
}

std::expected<void, std::string> System::gui_set_checked(AppId app_id, std::string_view absolute_path, bool checked)
{
#if !BROOKESIA_SYSTEM_CORE_USE_JSON_GUI
    (void)app_id;
    (void)absolute_path;
    (void)checked;
    return std::unexpected("JSON GUI is unavailable with the GSP backend");
#else
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
#endif
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
        impl_->flush_pending_gui_bindings(app_id);
        return impl_->gui_runtime_->destroy_view(*record_result.value()->document_id, absolute_path);
    },
    false
           );
}

std::expected<void, std::string> System::Impl::subscribe_gui_action_now(AppId app_id, std::string action)
{
    return subscribe_gui_actions_now(app_id, std::vector<std::string> {std::move(action)});
}

gui::Runtime::ActionHandler System::Impl::make_app_action_forwarder(AppId app_id)
{
    return [this, app_id](const gui::Event & event) {
        auto action = event.action;
        auto event_path = event.path;
        auto payload_json = boost::json::serialize(event.payload);
        auto post_action = [
                               this,
                               app_id,
                               action = std::move(action),
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
        };
        auto post_result = post_task(SYSTEM_APP_INPUT_TASK_GROUP, std::move(post_action));
        if (!post_result) {
            BROOKESIA_LOGW(
                "Post app action failed: app_id(%1%), action(%2%), error(%3%)",
                app_id, event.action, post_result.error()
            );
        }
    };
}

std::expected<void, std::string> System::Impl::subscribe_gui_actions_now(
    AppId app_id,
    const std::vector<std::string> &actions
)
{
    [[maybe_unused]] heap_trace::Snapshot heap_before_task {};
    if constexpr (heap_trace::detail_enabled) {
        heap_before_task = heap_trace::capture();
        const auto app_id_string = std::to_string(app_id);
        const auto details = "app_id=" + app_id_string + " actions=" + std::to_string(actions.size());
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
    [[maybe_unused]] heap_trace::Snapshot heap_before_connect {};
    if constexpr (heap_trace::detail_enabled) {
        heap_before_connect = heap_trace::capture();
        const auto details = "app_id=" + std::to_string(app_id) +
                             " manifest_id=" + record.info.manifest.id +
                             " actions=" + std::to_string(actions.size()) +
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

    std::vector<gui::ScopedConnection> new_connections;
    std::vector<std::string> new_connection_keys;
    new_connections.reserve(actions.size());
    new_connection_keys.reserve(actions.size());
    for (const auto &action : actions) {
        if (record.action_connection_keys.contains(action)) {
            BROOKESIA_LOGD(
                "Skip duplicate GUI action subscription: app_id(%1%), manifest_id(%2%), action(%3%)",
                app_id, record.info.manifest.id, action
            );
            continue;
        }

        auto connection = gui_runtime_->subscribe_event_action(
                              *record.document_id,
                              action,
                              make_app_action_forwarder(app_id)
                          );
        if (!connection.connected()) {
            return std::unexpected("Failed to subscribe GUI action");
        }
        new_connection_keys.push_back(action);
        new_connections.push_back(std::move(connection));
    }

    record.action_connections.reserve(record.action_connections.size() + new_connections.size());
    for (auto &connection : new_connections) {
        record.action_connections.push_back(std::move(connection));
    }
    for (auto &action : new_connection_keys) {
        record.action_connection_keys.insert(std::move(action));
    }

    if constexpr (heap_trace::detail_enabled) {
        const auto details = "app_id=" + std::to_string(app_id) +
                             " manifest_id=" + record.info.manifest.id +
                             " actions=" + std::to_string(actions.size()) +
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

std::expected<void, std::string> System::gui_subscribe_actions(AppId app_id, const std::vector<std::string> &actions)
{
#if defined(__EMSCRIPTEN__)
    auto action_list = actions;
    if (esp_brookesia::gui::wasm::post_gui_task([this, app_id, actions = std::move(action_list)]() mutable {
    std::expected<void, std::string> result;
    auto subscribe_actions_task = [this, app_id, actions = std::move(actions), &result]() mutable {
            result = impl_->subscribe_gui_actions_now(app_id, actions);
        };
        impl_->execute_task_with_group_context(
            SYSTEM_GUI_TASK_GROUP,
            std::move(subscribe_actions_task)
        );
        if (!result)
        {
            BROOKESIA_LOGW(
                "Deferred GUI actions subscribe failed: app_id(%1%), error(%2%)",
                app_id, result.error()
            );
        }
    })) {
        return {};
    }
    return std::unexpected("Failed to post GUI subscribe actions task");
#else
    auto subscribe_actions_task = [this, app_id, actions]() mutable -> std::expected<void, std::string> {
        return impl_->subscribe_gui_actions_now(app_id, actions);
    };
    return impl_->run_task_sync<std::expected<void, std::string>>(
               SYSTEM_GUI_TASK_GROUP,
               std::move(subscribe_actions_task),
               std::unexpected("Failed to post GUI subscribe actions task")
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

std::vector<gui::ScopedConnection> System::gui_subscribe_actions(
    AppId app_id,
    const std::vector<GuiActionHandlerSubscription> &subscriptions
)
{
    return impl_->run_task_sync<std::vector<gui::ScopedConnection>>(
               SYSTEM_GUI_TASK_GROUP,
    [this, app_id, subscriptions]() mutable {
        std::vector<gui::ScopedConnection> connections;
        connections.reserve(subscriptions.size());

        auto record_result = impl_->get_record(app_id);
        if (!record_result)
        {
            return connections;
        }
        auto &record = *record_result.value();
        if (!impl_->gui_runtime_ || !record.document_id.has_value())
        {
            return connections;
        }

        for (auto &subscription : subscriptions)
        {
            if (subscription.action.empty() || !subscription.handler) {
                connections.emplace_back();
                continue;
            }
            auto scheduled_handler = [this, handler = std::move(subscription.handler)](const gui::Event & event) {
                auto event_copy = event;
                auto post_handler = [handler, event_copy = std::move(event_copy)]() mutable {
                    handler(event_copy);
                };
                auto post_result = impl_->post_task(SYSTEM_APP_INPUT_TASK_GROUP, std::move(post_handler));
                if (!post_result) {
                    BROOKESIA_LOGW("Post app action handler failed: %1%", post_result.error());
                }
            };
            connections.push_back(
                impl_->gui_runtime_->subscribe_event_action(
                    *record.document_id,
                    subscription.action,
                    std::move(scheduled_handler)
                )
            );
        }
        return connections;
    },
    {}
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
#if !BROOKESIA_SYSTEM_CORE_USE_JSON_GUI
    (void)app_id;
    (void)absolute_path;
    return std::nullopt;
#else
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
#endif
}

std::expected<void, std::string> System::gui_set_view_src(
    AppId app_id,
    std::string_view absolute_path,
    std::string_view src
)
{
#if !BROOKESIA_SYSTEM_CORE_USE_JSON_GUI
    (void)app_id;
    (void)absolute_path;
    (void)src;
    return std::unexpected("JSON GUI is unavailable with the GSP backend");
#else
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
#endif
}

std::expected<void, std::string> System::gui_preload_images(
    AppId app_id,
    const std::vector<std::string> &image_ids
)
{
    return impl_->run_task_sync<std::expected<void, std::string>>(
               SYSTEM_GUI_INPUT_TASK_GROUP,
    [this, app_id, image_ids]() -> std::expected<void, std::string> {
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
        return impl_->gui_runtime_->preload_images(*record.document_id, image_ids);
    },
    std::unexpected("Failed to post GUI image preload task")
           );
}

std::expected<void, std::string> System::gui_release_images(
    AppId app_id,
    const std::vector<std::string> &image_ids
)
{
    return impl_->run_task_sync<std::expected<void, std::string>>(
               SYSTEM_GUI_INPUT_TASK_GROUP,
    [this, app_id, image_ids]() -> std::expected<void, std::string> {
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
        return impl_->gui_runtime_->release_preloaded_images(*record.document_id, image_ids);
    },
    std::unexpected("Failed to post GUI image release task")
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

std::expected<void, std::string> System::gui_scroll_to(
    AppId app_id,
    std::string_view absolute_path,
    int32_t x,
    int32_t y,
    bool animated
)
{
    return impl_->run_task_sync<std::expected<void, std::string>>(
               SYSTEM_GUI_INPUT_TASK_GROUP,
               [this,
                app_id,
                absolute_path = std::string(absolute_path),
                x,
                y,
    animated]() -> std::expected<void, std::string> {
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
        if (!impl_->gui_runtime_->scroll_view_to(*record.document_id, absolute_path, x, y, animated))
        {
            return std::unexpected("Failed to scroll view");
        }
        return {};
    },
    std::unexpected("Failed to post GUI scroll task")
           );
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

std::expected<void, std::string> System::set_gui_view_debug_enabled(bool enabled)
{
    return impl_->run_task_sync<std::expected<void, std::string>>(
               SYSTEM_GUI_TASK_GROUP,
    [this, enabled]() -> std::expected<void, std::string> {
        if (!impl_->gui_runtime_)
        {
            return std::unexpected("GUI runtime is not available");
        }
        impl_->gui_runtime_->set_view_debug_enabled(enabled);
        return {};
    },
    std::unexpected("Failed to post GUI view debug task")
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

std::expected<void, std::string> System::system_gui_load_theme_file(
    std::string_view resource_dir,
    std::string_view path
)
{
    return system_gui().load_theme_file(resource_dir, path);
}

std::expected<void, std::string> System::system_gui_set_theme(std::string_view theme_id)
{
    return system_gui().set_theme(theme_id);
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
