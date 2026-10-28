/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "private/runtime_impl.hpp"

#if defined(ESP_PLATFORM)
#include "esp_heap_caps.h"
#endif

namespace esp_brookesia::gui {

bool Runtime::Impl::is_view_valid(const View &view) const
{
    return resolve_view_uid(view).has_value();
}
std::string Runtime::Impl::get_view_id(const View &view) const
{
    auto *record = resolve_view_record(view);
    return record == nullptr ? std::string() : std::string(record->node_id());
}
SparseNode Runtime::Impl::make_style_snapshot(const NodeRecord &record) const
{
    SparseNode snapshot;
    snapshot.type = record.node().type;
    snapshot.style_refs = record.definition->style_refs;
    record.populate_style_snapshot(snapshot);
    return snapshot;
}
void Runtime::Impl::apply_record_props(NodeRecord &record, PropsApplyMask mask)
{
    if (backend == nullptr) {
        return;
    }
    if (record.transient_mutable_definition || record.compact_props_state == nullptr) {
        apply_backend_props(*backend, record.handle, record.node(), mask);
        return;
    }

    // IBackend consumes this snapshot during the call. Copy only the selected node type's
    // props into temporary stack storage; unrelated domains never become persistent state.
    SparseNode props_snapshot;
    props_snapshot.type = record.node().type;
    props_snapshot.common_props = record.common_props();
    props_snapshot.label_props = record.label_props();
    switch (props_snapshot.type) {
    case NodeType::Image:
        props_snapshot.image_props = record.image_props();
        props_snapshot.resolved_image = record.resolved_image();
        break;
    case NodeType::FrameView:
        props_snapshot.frame_view_props = record.frame_view_props();
        break;
    case NodeType::TextInput:
        props_snapshot.text_input_props = record.text_input_props();
        break;
    case NodeType::Slider:
    case NodeType::ProgressBar:
    case NodeType::Arc:
        props_snapshot.range_props = record.range_props();
        break;
    case NodeType::Switch:
    case NodeType::Checkbox:
        props_snapshot.toggle_props = record.toggle_props();
        break;
    case NodeType::Dropdown:
        props_snapshot.dropdown_props = record.dropdown_props();
        break;
    case NodeType::Table:
        props_snapshot.table_props = record.table_props();
        break;
    case NodeType::Line:
        props_snapshot.line_props = record.line_props();
        break;
    case NodeType::Keyboard:
        props_snapshot.keyboard_props = record.keyboard_props();
        break;
    case NodeType::Canvas:
        props_snapshot.canvas_props = record.canvas_props();
        break;
    case NodeType::Screen:
    case NodeType::Container:
    case NodeType::Label:
    case NodeType::Button:
    case NodeType::Spinner:
    case NodeType::Max:
        break;
    }
    props_snapshot.placement = record.placement();
    apply_backend_props(*backend, record.handle, props_snapshot, mask);
}
std::optional<ViewStateValue> Runtime::Impl::get_view_state_internal(const View &view, ViewStateKind kind) const
{
    auto *record = resolve_view_record(view);
    if (record == nullptr) {
        return std::nullopt;
    }

    switch (kind) {
    case ViewStateKind::CommonProps:
        return ViewStateValue(record->common_props());
    case ViewStateKind::TypedProps:
        switch (record->node().type) {
        case NodeType::Label:
            return ViewStateValue(TypedPropsVariant(record->label_props()));
        case NodeType::Image:
            return ViewStateValue(TypedPropsVariant(record->image_props()));
        case NodeType::FrameView:
            return ViewStateValue(TypedPropsVariant(record->frame_view_props()));
        case NodeType::TextInput:
            return ViewStateValue(TypedPropsVariant(record->text_input_props()));
        case NodeType::Slider:
        case NodeType::ProgressBar:
        case NodeType::Arc:
            return ViewStateValue(TypedPropsVariant(record->range_props()));
        case NodeType::Switch:
        case NodeType::Checkbox:
            return ViewStateValue(TypedPropsVariant(record->toggle_props()));
        case NodeType::Dropdown:
            return ViewStateValue(TypedPropsVariant(record->dropdown_props()));
        case NodeType::Table:
            return ViewStateValue(TypedPropsVariant(record->table_props()));
        case NodeType::Line:
            return ViewStateValue(TypedPropsVariant(record->line_props()));
        case NodeType::Keyboard:
            return ViewStateValue(TypedPropsVariant(record->keyboard_props()));
        case NodeType::Canvas:
            return ViewStateValue(TypedPropsVariant(record->canvas_props()));
        case NodeType::Screen:
        case NodeType::Container:
        case NodeType::Button:
        case NodeType::Spinner:
        case NodeType::Max:
            return std::nullopt;
        }
        return std::nullopt;
    case ViewStateKind::Layout:
        return ViewStateValue(record->layout());
    case ViewStateKind::Placement:
        return ViewStateValue(record->placement());
    case ViewStateKind::Style:
        return ViewStateValue(to_public(record->resolved_style->style));
    case ViewStateKind::ResolvedFont:
        return ViewStateValue(record->resolved_style->resolved_font);
    case ViewStateKind::ResolvedImage:
        if (record->node().type != NodeType::Image) {
            return std::nullopt;
        }
        return ViewStateValue(record->resolved_image());
    case ViewStateKind::Frame:
        if (backend == nullptr) {
            return std::nullopt;
        }
        if (auto frame = backend->get_node_frame(record->handle)) {
            return ViewStateValue(*frame);
        }
        return std::nullopt;
    case ViewStateKind::Max:
        return std::nullopt;
    }

    return std::nullopt;
}
bool Runtime::Impl::set_view_hidden(const View &view, bool hidden)
{
    auto *record = resolve_view_record(view);
    if (record == nullptr) {
        return false;
    }
    if (record->common_props().hidden == hidden) {
        apply_record_props(*record, PropsApplyMask::CommonHidden);
        return true;
    }
    record->mutable_common_props().hidden = hidden;
    apply_record_props(*record, PropsApplyMask::CommonHidden);
    return true;
}
bool Runtime::Impl::scroll_view_to(const View &view, int32_t x, int32_t y, bool animated)
{
    auto *record = resolve_view_record(view);
    if (record == nullptr || backend == nullptr) {
        return false;
    }
    return backend->scroll_node_to(record->handle, x, y, animated);
}
bool Runtime::Impl::scroll_view_to_visible(const View &view, bool animated)
{
    auto *record = resolve_view_record(view);
    if (record == nullptr || backend == nullptr) {
        return false;
    }
    return backend->scroll_node_to_visible(record->handle, animated);
}
bool Runtime::Impl::set_view_text(const View &view, std::string_view text)
{
    auto *record = resolve_view_record(view);
    if (record == nullptr || (record->node().type != NodeType::Label && record->node().type != NodeType::TextInput &&
                              record->node().type != NodeType::Checkbox)) {
        return false;
    }
    const auto &current_text = record->node().type == NodeType::TextInput ?
                               record->text_input_props().text : record->label_props().text;
    if (current_text == text) {
        if (record->node().type == NodeType::TextInput) {
            apply_record_props(*record, PropsApplyMask::TextInputText);
        } else {
            apply_record_props(*record, PropsApplyMask::LabelText);
        }
        return true;
    }
    if (record->node().type == NodeType::TextInput) {
        record->mutable_text_input_props().text = std::string(text);
    } else {
        record->mutable_label_props().text = std::string(text);
    }
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
    auto hp_text_before = ::esp_brookesia::lib_utils::MemoryProfiler::take_raw_heap_snapshot();
#endif
    if (record->node().type == NodeType::TextInput) {
        apply_record_props(*record, PropsApplyMask::TextInputText);
    } else {
        apply_record_props(*record, PropsApplyMask::LabelText);
    }
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
    {
        auto hp_text_after = ::esp_brookesia::lib_utils::MemoryProfiler::take_raw_heap_snapshot();
        ++dbg_set_view_text_count_;
        BROOKESIA_LOGI(
            "[HeapTrace][gui.set_view_text] count(%1%) text_len(%2%) psram_before(%3%) psram_after(%4%) delta(%5%)",
            dbg_set_view_text_count_, text.size(), hp_text_before.external_free, hp_text_after.external_free,
            static_cast<int64_t>(hp_text_after.external_free) - static_cast<int64_t>(hp_text_before.external_free)
        );
    }
#endif
    return true;
}
std::string Runtime::Impl::get_view_text(const View &view) const
{
    auto *record = resolve_view_record(view);
    if (record == nullptr) {
        return {};
    }
    if (record->node().type == NodeType::Label || record->node().type == NodeType::Checkbox) {
        return record->label_props().text;
    }
    if (record->node().type == NodeType::TextInput) {
        return record->text_input_props().text;
    }
    return {};
}
bool Runtime::Impl::set_view_src(const View &view, std::string_view src)
{
    auto *record = resolve_view_record(view);
    auto *tree = resolve_tree(view.document_id_);
    if (record == nullptr || tree == nullptr || record->node().type != NodeType::Image) {
        return false;
    }
    const auto previous_src = record->image_props().src;
    const auto previous_resolved_image = record->resolved_image();
    const auto previous_image_width = record->resolved_image().width;
    const auto previous_image_height = record->resolved_image().height;
    auto update_result = update_image_source(*tree, *record, src);
    if (!update_result) {
        BROOKESIA_LOGW("Reject image source: %1%", update_result.error());
        return false;
    }
    const auto resolved_src = record->resolved_image().primary_src.empty() ?
                              record->image_props().src :
                              record->resolved_image().primary_src;
    const RuntimeImageResource resource {
        .id = record->image_props().src,
        .primary_src = resolved_src,
        .native_src = record->resolved_image().native_src,
        .width = record->resolved_image().width,
        .height = record->resolved_image().height,
    };
    auto preload_result = ensure_image_resource_preloaded_for_tree(*tree, resource);
    if (!preload_result) {
        BROOKESIA_LOGW(
            "Reject image source because preload failed: path='%1%', error=%2%",
            resolved_src,
            preload_result.error()
        );
        record->mutable_image_props().src = previous_src;
        record->mutable_resolved_image() = previous_resolved_image;
        return false;
    }
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
    auto hp_src_before = ::esp_brookesia::lib_utils::MemoryProfiler::take_raw_heap_snapshot();
#endif
    apply_record_props(*record, PropsApplyMask::ImageSource);
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
    {
        auto hp_src_after = ::esp_brookesia::lib_utils::MemoryProfiler::take_raw_heap_snapshot();
        ++dbg_set_view_src_count_;
        BROOKESIA_LOGI(
            "[HeapTrace][gui.set_view_src] count(%1%) src(%2%) psram_before(%3%) psram_after(%4%) delta(%5%)",
            dbg_set_view_src_count_, resolved_src, hp_src_before.external_free, hp_src_after.external_free,
            static_cast<int64_t>(hp_src_after.external_free) - static_cast<int64_t>(hp_src_before.external_free)
        );
    }
#endif
    const bool needs_placement_reapply = (
            record->placement().width.mode != SizeMode::Fixed ||
            record->placement().height.mode != SizeMode::Fixed ||
            previous_image_width != record->resolved_image().width ||
            previous_image_height != record->resolved_image().height
                                         );
    if (needs_placement_reapply) {
        backend->apply_placement(record->handle, record->placement(), PlacementApplyMask::Size);
    }
    return true;
}
std::string Runtime::Impl::get_view_src(const View &view) const
{
    auto *record = resolve_view_record(view);
    return record == nullptr ? std::string() : record->image_props().src;
}
bool Runtime::Impl::set_view_value(const View &view, int32_t value)
{
    auto *record = resolve_view_record(view);
    if (record == nullptr || (record->node().type != NodeType::Slider && record->node().type != NodeType::ProgressBar &&
                              record->node().type != NodeType::Arc)) {
        return false;
    }
    if (record->range_props().value == value) {
        apply_record_props(*record, PropsApplyMask::RangeValue);
        return true;
    }
    record->mutable_range_props().value = value;
    apply_record_props(*record, PropsApplyMask::RangeValue);
    return true;
}
int32_t Runtime::Impl::get_view_value(const View &view) const
{
    auto *record = resolve_view_record(view);
    if (record == nullptr || (record->node().type != NodeType::Slider && record->node().type != NodeType::ProgressBar &&
                              record->node().type != NodeType::Arc)) {
        return 0;
    }
    return record->range_props().value;
}
bool Runtime::Impl::set_view_checked(const View &view, bool checked)
{
    auto *record = resolve_view_record(view);
    if (record == nullptr || (record->node().type != NodeType::Switch && record->node().type != NodeType::Checkbox)) {
        return false;
    }
    if (record->toggle_props().checked == checked) {
        apply_record_props(*record, PropsApplyMask::ToggleChecked);
        return true;
    }
    record->mutable_toggle_props().checked = checked;
    apply_record_props(*record, PropsApplyMask::ToggleChecked);
    return true;
}
bool Runtime::Impl::get_view_checked(const View &view) const
{
    auto *record = resolve_view_record(view);
    if (record == nullptr || (record->node().type != NodeType::Switch && record->node().type != NodeType::Checkbox)) {
        return false;
    }
    return record->toggle_props().checked;
}
bool Runtime::Impl::set_view_selected_index(const View &view, int32_t index)
{
    auto *record = resolve_view_record(view);
    if (record == nullptr || record->node().type != NodeType::Dropdown || index < 0) {
        return false;
    }
    if (record->dropdown_props().selected_index == index) {
        apply_record_props(*record, PropsApplyMask::DropdownSelectedIndex);
        return true;
    }
    record->mutable_dropdown_props().selected_index = index;
    apply_record_props(*record, PropsApplyMask::DropdownSelectedIndex);
    return true;
}
int32_t Runtime::Impl::get_view_selected_index(const View &view) const
{
    auto *record = resolve_view_record(view);
    return record != nullptr && record->node().type == NodeType::Dropdown ?
           record->dropdown_props().selected_index : 0;
}
bool Runtime::Impl::set_table_cell_text(const View &view, int32_t row, int32_t column, std::string_view text)
{
    auto *record = resolve_view_record(view);
    if (record == nullptr || record->node().type != NodeType::Table || row < 0 || column < 0) {
        return false;
    }
    const auto &current_cells = record->table_props().cells;
    const auto current_cell_it = std::find_if(
                                     current_cells.begin(), current_cells.end(),
    [row, column](const TableCell & cell) {
        return cell.row == row && cell.column == column;
    });
    if (current_cell_it != current_cells.end() && current_cell_it->text == text) {
        apply_record_props(*record, PropsApplyMask::TableCells);
        return true;
    }
    auto &cells = record->mutable_table_props().cells;
    auto cell_it = std::find_if(cells.begin(), cells.end(), [row, column](const TableCell & cell) {
        return cell.row == row && cell.column == column;
    });
    if (cell_it == cells.end()) {
        cells.push_back(TableCell{.row = row, .column = column, .text = std::string(text)});
    } else {
        cell_it->text = std::string(text);
    }
    apply_record_props(*record, PropsApplyMask::TableCells);
    return true;
}
esp_brookesia::lib_utils::connection Runtime::Impl::connect_view_event(const View &view, EventType type, View::EventHandler handler)
{
    auto *tree = resolve_tree(view.document_id_);
    auto uid = resolve_view_uid(view);
    auto *record = tree == nullptr || !uid.has_value() ? nullptr : find_node_record(*tree, *uid);
    if (record == nullptr || type == EventType::Max) {
        return esp_brookesia::lib_utils::connection();
    }
    auto &interaction_state = ensure_interaction_state(*tree, *uid);
    return interaction_state->event_signal.connect([type, handler = std::move(handler)](const Event & event) {
        if (handler && event.type == type) {
            handler(event);
        }
    });
}
esp_brookesia::lib_utils::connection Runtime::Impl::connect_button_click(const Button &button, Button::ClickHandler handler)
{
    auto *tree = resolve_tree(button.document_id_);
    auto uid = resolve_view_uid(button);
    auto *record = tree == nullptr || !uid.has_value() ? nullptr : find_node_record(*tree, *uid);
    if (record == nullptr || record->node().type != NodeType::Button) {
        return esp_brookesia::lib_utils::connection();
    }
    return ensure_interaction_state(*tree, *uid)->click_signal.connect(handler);
}
std::shared_ptr<std::function<void()>> Runtime::Impl::make_action_disconnect_handler(
                                        const std::shared_ptr<ActionRegistry> &registry,
                                        const std::shared_ptr<ActionSlot> &listener)
{
    auto disconnect_handler = std::make_shared<std::function<void()>>();
    *disconnect_handler = [weak_registry = std::weak_ptr<ActionRegistry>(registry),
                  weak_listener = std::weak_ptr<ActionSlot>(listener)]() {
        auto locked_listener = weak_listener.lock();
        if (locked_listener == nullptr ||
                !locked_listener->connected.exchange(false, std::memory_order_acq_rel)) {
            return;
        }

        auto locked_registry = weak_registry.lock();
        if (locked_registry == nullptr) {
            return;
        }

        std::lock_guard lock(locked_registry->mutex);
        for (auto route_it = locked_registry->routes.begin();
                route_it != locked_registry->routes.end(); ++route_it) {
            auto &bucket = route_it->bucket;
            if (!bucket.remove(locked_listener)) {
                continue;
            }
            if (bucket.empty()) {
                locked_registry->routes.erase(route_it);
                if (locked_registry->routes.empty()) {
                    std::vector<ActionRoute>().swap(locked_registry->routes);
                }
            }
            break;
        }
    };
    listener->disconnect_handler = disconnect_handler;
    return disconnect_handler;
}
std::expected<std::shared_ptr<std::function<void()>>, std::string> Runtime::Impl::connect_event_action_signal(
            Runtime *runtime,
            DocumentId document_id,
            std::string_view action,
            Runtime::ActionHandler handler
        )
{
#if defined(__EMSCRIPTEN__)
    (void)runtime;
#endif
    if (!document_id.is_valid() || !handler) {
        return std::unexpected("invalid subscription request");
    }

    auto *tree = resolve_tree(document_id);
    if (tree == nullptr) {
        const auto error = "document not loaded";
        BROOKESIA_LOGW(
            "Failed to connect event action handler: document not loaded (document_id=%1%, action='%2%')",
            document_id,
            action
        );
        return std::unexpected(error);
    }
#if !defined(__EMSCRIPTEN__)
    auto refresh_result = refresh_document_if_dirty(runtime, document_id);
    if (!refresh_result) {
        BROOKESIA_LOGW(
            "Failed to refresh dirty document before connecting event action handler: "
            "document_id=%1%, action='%2%', error=%3%",
            document_id,
            action,
            refresh_result.error()
        );
        return std::unexpected(refresh_result.error());
    }
#endif
    tree = resolve_tree(document_id);
    if (tree == nullptr) {
        return std::unexpected("document not loaded");
    }

    if (action.empty()) {
        const auto error = "invalid target";
        BROOKESIA_LOGW(
            "Failed to connect event action handler: invalid target (document_id=%1%, action='%2%')",
            document_id,
            action
        );
        return std::unexpected(error);
    }

    auto listener = std::make_shared<ActionSlot>();
    listener->handler = std::move(handler);
    {
        std::lock_guard lock(action_registry_->mutex);
        auto route_key = build_event_action_route_key(document_id, action);
        auto *route = action_registry_->find_route(route_key);
        if (route == nullptr) {
            action_registry_->routes.push_back(ActionRoute{
                .key = std::move(route_key),
                .bucket = {},
            });
            route = &action_registry_->routes.back();
        }
        route->bucket.add(listener);
    }
    return make_action_disconnect_handler(action_registry_, listener);
}
SubscriptionId Runtime::Impl::subscribe_event_action_with_id(
    Runtime *runtime,
    DocumentId document_id,
    std::string_view action,
    Runtime::ActionHandler handler
)
{
    auto disconnect_handler = connect_event_action_signal(runtime, document_id, action, std::move(handler));
    if (!disconnect_handler) {
        return 0;
    }
    const auto subscription_id = next_subscription_id_++;
    subscription_registry_->disconnect_handlers[subscription_id] = *disconnect_handler;
    register_document_subscription(subscription_id, document_id);
    return subscription_id;
}
RuntimeAnimationStartResult Runtime::Impl::start_view_animation_with_result(
    const View &view,
    const Animation &animation,
    Runtime::AnimationCompletedHandler completed_handler
)
{
    RuntimeAnimationStartResult start_result;
    if (backend == nullptr) {
        return start_result;
    }

    auto *record = resolve_view_record(view);
    if (record == nullptr) {
        return start_result;
    }

    const auto subscription_id = next_subscription_id_++;
    // Wrap completion handler so the registry/document maps are released automatically
    // when the animation finishes naturally. Many callers (e.g. launch transitions) discard
    // the returned subscription_id, so without this, every completed animation would leak an
    // entry in subscription_registry_->disconnect_handlers and subscription_document_ids_.
    auto self_releasing_handler = [registry = std::weak_ptr<SubscriptionRegistry>(subscription_registry_),
                                            doc_ids_self = this,
                                            subscription_id,
             user_handler = std::move(completed_handler)]() mutable {
        if (user_handler)
        {
            user_handler();
        }
        // Snapshot captures into locals before any erase. Erasing the entry from
        // disconnect_handlers below releases the ScopedConnection that owns this very
        // handler, destroying this closure mid-execution. After that point no capture
        // (doc_ids_self, subscription_id, registry) may be touched, so resolve them now
        // and perform the self-owning erase last.
        auto *const impl_self = doc_ids_self;
        const auto id = subscription_id;
        auto locked_registry = registry.lock();
        if (impl_self != nullptr)
        {
            impl_self->subscription_document_ids_.erase(id);
        }
        if (locked_registry != nullptr)
        {
            locked_registry->disconnect_handlers.erase(id);
        }
    };

    auto backend_result = backend->start_animation(record->handle, animation, std::move(self_releasing_handler));
    if (!backend_result || !backend_result->connection.connected()) {
        return start_result;
    }

    auto connection = std::make_shared<ScopedConnection>(std::move(backend_result->connection));
    auto disconnect_handler = std::make_shared<std::function<void()>>();
    *disconnect_handler = [registry = std::weak_ptr<SubscriptionRegistry>(subscription_registry_),
                                    subscription_id,
             connection = std::move(connection)]() mutable {
        connection->disconnect();
        if (auto locked_registry = registry.lock(); locked_registry != nullptr)
        {
            locked_registry->disconnect_handlers.erase(subscription_id);
        }
    };
    subscription_registry_->disconnect_handlers[subscription_id] = disconnect_handler;
    register_document_subscription(subscription_id, view.document_id_);
    start_result.subscription_id = subscription_id;
    start_result.resolved_from = backend_result->resolved_from;
    start_result.resolved_to = backend_result->resolved_to;
    return start_result;
}
SubscriptionId Runtime::Impl::start_view_animation_with_id(
    const View &view,
    const Animation &animation,
    Runtime::AnimationCompletedHandler completed_handler
)
{
    return start_view_animation_with_result(view, animation, std::move(completed_handler)).subscription_id;
}
ScopedConnection Runtime::Impl::start_view_animation(
    const View &view,
    const Animation &animation,
    Runtime::AnimationCompletedHandler completed_handler
)
{
    if (backend == nullptr) {
        return {};
    }

    auto *record = resolve_view_record(view);
    if (record == nullptr) {
        return {};
    }

    auto backend_result = backend->start_animation(record->handle, animation, std::move(completed_handler));
    if (!backend_result) {
        return {};
    }
    return std::move(backend_result->connection);
}
bool Runtime::Impl::dispatch_event_action_handlers(const Event &event)
{
    std::shared_ptr<ActionSlot> first_listener;
    std::vector<std::shared_ptr<ActionSlot>> extra_listeners;
    {
        std::lock_guard lock(action_registry_->mutex);
        const auto route_key = build_event_action_route_key(event.document_id, event.action);
        auto *route = action_registry_->find_route(route_key);
        if (route == nullptr) {
            return false;
        }
        first_listener = route->bucket.first_listener;
        extra_listeners = route->bucket.extra_listeners;
    }

    bool dispatched = false;
    if (first_listener != nullptr && first_listener->connected.load(std::memory_order_acquire)) {
        first_listener->handler(event);
        dispatched = true;
    }
    for (const auto &listener : extra_listeners) {
        if (!listener->connected.load(std::memory_order_acquire)) {
            continue;
        }
        listener->handler(event);
        dispatched = true;
    }
    return dispatched;
}
void Runtime::Impl::register_fast_action_routes(const TreeRecord &tree, const NodeRecord &record)
{
    if (!task_config.enable_fast_action_dispatch || !record.handle.is_valid()) {
        return;
    }

    const auto *root_record = find_root_node_record(tree, record);
    if (root_record == nullptr) {
        return;
    }
    std::lock_guard lock(fast_action_mutex_);
    for (const auto &event : record.node().events) {
        if (!is_fast_action_event_type(event.type) || event.action.empty() || !event.effects.empty()) {
            continue;
        }
        fast_action_events_[build_fast_action_route_key(record.handle, event.type, event.action)] = Event{
            .document_id = tree.document_id,
            .root_id = std::string(root_record->node_id()),
            .node_id = std::string(record.node_id()),
            .path = record.absolute_path,
            .type = event.type,
            .action = event.action,
            .payload = {},
        };
    }
}
void Runtime::Impl::unregister_fast_action_routes(const NodeRecord &record)
{
    if (!task_config.enable_fast_action_dispatch || !record.handle.is_valid()) {
        return;
    }

    std::lock_guard lock(fast_action_mutex_);
    for (const auto &event : record.node().events) {
        if (!is_fast_action_event_type(event.type) || event.action.empty() || !event.effects.empty()) {
            continue;
        }
        fast_action_events_.erase(build_fast_action_route_key(record.handle, event.type, event.action));
    }
}
bool Runtime::Impl::try_dispatch_fast_action_event(const BackendEvent &event)
{
    if (!task_config.enable_fast_action_dispatch || !is_fast_action_backend_event(event)) {
        return false;
    }

    Event action_event;
    {
        std::lock_guard lock(fast_action_mutex_);
        auto route_it = fast_action_events_.find(
                            build_fast_action_route_key(event.handle, event.type, event.action)
                        );
        if (route_it == fast_action_events_.end()) {
            return false;
        }
        action_event = route_it->second;
    }
    return dispatch_event_action_handlers(action_event);
}
void Runtime::Impl::apply_view_debug_to_all_nodes()
{
    if (backend == nullptr) {
        return;
    }
    for (auto &[unused_document_id, tree] : trees) {
        (void)unused_document_id;
        for (auto &[unused_uid, record] : tree.nodes) {
            (void)unused_uid;
            backend->apply_debug_visual(record.handle, view_debug_enabled_);
        }
    }
}
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
size_t Runtime::Impl::dbg_ext_free()
{
#if defined(ESP_PLATFORM) && defined(CONFIG_SPIRAM) && CONFIG_SPIRAM
    return heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
#else
    return ::esp_brookesia::lib_utils::MemoryProfiler::take_raw_heap_snapshot().external_free;
#endif
}
#endif

uint32_t Runtime::Impl::current_applied_style_revision() const
{
    return static_cast<uint32_t>(current_style_revision_);
}
void Runtime::Impl::advance_style_revision()
{
    ++current_style_revision_;
    if (current_applied_style_revision() != 0) {
        return;
    }

    // Zero is the per-record dirty sentinel. On the practically unreachable 32-bit tag wrap,
    // invalidate every record and skip zero so truncating the global revision remains exact.
    for (auto &[unused_document_id, tree] : trees) {
        (void)unused_document_id;
        for (auto &[unused_uid, record] : tree.nodes) {
            (void)unused_uid;
            record.applied_style_revision = 0;
        }
    }
    ++current_style_revision_;
}
RuntimeProfileClock::time_point Runtime::Impl::subtree_profile_now()
{
#if BROOKESIA_GUI_INTERFACE_ENABLE_PROFILE_LOG
    return RuntimeProfileClock::now();
#else
    return {};
#endif
}
void Runtime::Impl::add_subtree_profile_time(
    int64_t &bucket_us,
    const RuntimeProfileClock::time_point &start)
{
#if BROOKESIA_GUI_INTERFACE_ENABLE_PROFILE_LOG
    bucket_us += runtime_profile_elapsed_us(start, RuntimeProfileClock::now());
#else
    (void)bucket_us;
    (void)start;
#endif
}
void Runtime::Impl::reset_subtree_build_profile()
{
    subtree_build_profile_ = {};
}
void Runtime::Impl::log_subtree_build_profile(
    std::string_view stage,
    DocumentId document_id,
    std::string_view root_id,
    int64_t total_ms) const
{
    GUI_INTERFACE_PROFILE_LOGI(
        "GUI runtime subtree build profile: doc(%1%), stage(%2%), root(%3%), nodes(%4%), total_ms(%5%), "
        "copy_definition_ms(%6%), initial_bindings_ms(%7%), image_resolve_ms(%8%), create_node_ms(%9%), "
        "resolve_style_ms(%10%), store_record_ms(%11%)",
        document_id.value(),
        stage,
        root_id,
        subtree_build_profile_.nodes,
        total_ms,
        runtime_profile_us_to_ms(subtree_build_profile_.copy_definition_us),
        runtime_profile_us_to_ms(subtree_build_profile_.initial_bindings_us),
        runtime_profile_us_to_ms(subtree_build_profile_.image_resolve_us),
        runtime_profile_us_to_ms(subtree_build_profile_.create_node_us),
        runtime_profile_us_to_ms(subtree_build_profile_.resolve_style_us),
        runtime_profile_us_to_ms(subtree_build_profile_.store_record_us)
    );
    GUI_INTERFACE_PROFILE_LOGI(
        "GUI runtime subtree build profile: doc(%1%), stage(%2%), root(%3%), apply_props_ms(%4%), "
        "apply_layout_ms(%5%), apply_placement_ms(%6%), apply_transform_ms(%7%), apply_style_ms(%8%), "
        "apply_animations_ms(%9%), events_ms(%10%), subscribe_bindings_ms(%11%)",
        document_id.value(),
        stage,
        root_id,
        runtime_profile_us_to_ms(subtree_build_profile_.apply_props_us),
        runtime_profile_us_to_ms(subtree_build_profile_.apply_layout_us),
        runtime_profile_us_to_ms(subtree_build_profile_.apply_placement_us),
        runtime_profile_us_to_ms(subtree_build_profile_.apply_transform_us),
        runtime_profile_us_to_ms(subtree_build_profile_.apply_style_us),
        runtime_profile_us_to_ms(subtree_build_profile_.apply_animations_us),
        runtime_profile_us_to_ms(subtree_build_profile_.events_us),
        runtime_profile_us_to_ms(subtree_build_profile_.subscribe_bindings_us)
    );
}
}
