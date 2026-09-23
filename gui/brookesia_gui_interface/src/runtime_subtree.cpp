/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "private/runtime_impl.hpp"

namespace esp_brookesia::gui {

std::expected<Runtime::Impl::NodeUid, std::string> Runtime::Impl::create_subtree(
    DocumentId document_id,
    TreeRecord &tree,
    const Node &definition,
    BackendHandle parent_handle,
    NodeUid parent_uid,
    const std::string &root_id,
    const Path &current_path,
    const std::string &scope_root_absolute_path,
    const std::optional<std::string> &override_root_id,
    bool is_dynamic_template_root )
{
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
    ++dbg_create_subtree_count_;
#endif
#if BROOKESIA_GUI_INTERFACE_ENABLE_PROFILE_LOG
    ++subtree_build_profile_.nodes;
#endif
    auto step_start = subtree_profile_now();
    // root_id may originate from a NodeRecord string; keep it stable while descendants are inserted.
    const std::string stable_root_id = root_id;
    const bool needs_instance_node = override_root_id.has_value() || !definition.bindings.empty() ||
                                     definition.type == NodeType::Image ||
                                     definition.type == NodeType::Keyboard;
    std::unique_ptr<Node> instance_node;
    if (needs_instance_node) {
        instance_node = std::make_unique<Node>(clone_node_without_children(definition));
    }
    if (override_root_id.has_value()) {
        instance_node->id = *override_root_id;
    }
    add_subtree_profile_time(subtree_build_profile_.copy_definition_us, step_start);

    const auto absolute_node_path = absolute_node_path_to_string(stable_root_id, current_path);
    BindingApplyMasks initial_binding_masks;
    std::vector<InitialStyleBinding> initial_style_bindings;
    step_start = subtree_profile_now();
    if (!definition.bindings.empty()) {
        initial_binding_masks = apply_initial_bindings(
                                    tree, *instance_node, absolute_node_path, initial_style_bindings
                                );
    }
    add_subtree_profile_time(subtree_build_profile_.initial_bindings_us, step_start);
    step_start = subtree_profile_now();
    if (definition.type == NodeType::Image || definition.type == NodeType::Keyboard) {
        resolve_image_source(tree, *instance_node);
    }
    const Node &node = instance_node != nullptr ? *instance_node : definition;
    auto image_preload_result = ensure_node_image_resources_preloaded(tree, node);
    add_subtree_profile_time(subtree_build_profile_.image_resolve_us, step_start);
    if (!image_preload_result) {
        return std::unexpected(image_preload_result.error());
    }

    const auto parent_absolute_path = parent_absolute_path_to_string(stable_root_id, current_path);
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
    const size_t dbg_s0 = dbg_ext_free();
#endif
    step_start = subtree_profile_now();
    BackendHandle handle = backend->create_node(node, parent_handle, parent_absolute_path, scope_root_absolute_path);
    add_subtree_profile_time(subtree_build_profile_.create_node_us, step_start);
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
    const size_t dbg_s1 = dbg_ext_free();
    dbg_step_create_node_ += static_cast<int64_t>(dbg_s0) - static_cast<int64_t>(dbg_s1);
#endif
    if (!handle.is_valid()) {
        const auto node_path = path_to_string(current_path);
        return std::unexpected("Failed to create GUI node: " + (node_path.empty() ? node.id : node_path));
    }

#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
    const size_t dbg_s2 = dbg_ext_free();
#endif
    if (tree.next_uid == 0) {
        return std::unexpected("GUI node ID space exhausted; reload the document before creating more nodes");
    }
    const NodeUid uid = tree.next_uid++;
    NodeRecord record;
    record.absolute_path = absolute_node_path;
    record.definition = &definition;
    // resolve_style_shared() only reads the effective node and de-duplicates the result across
    // identically-styled instances (for example, repeated template list items).
    step_start = subtree_profile_now();
    const Node *style_definition = node_has_style_binding(definition) ? nullptr : &definition;
    record.resolved_style = resolve_style_shared(tree, node, style_definition);
    add_subtree_profile_time(subtree_build_profile_.resolve_style_us, step_start);
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
    const size_t dbg_s2b = dbg_ext_free();
    dbg_step_resolve_style_ += static_cast<int64_t>(dbg_s2) - static_cast<int64_t>(dbg_s2b);
#endif
    // ---------------------------------------------------------------------------------------
    // MEMORY-CRITICAL: static nodes retain only a stable definition pointer. A full Node is
    // materialized only while the backend constructs the object. Later setters allocate only
    // the affected compact instance-state domain.
    //
    // Drop descendant definitions. Each NodeRecord only needs this node's own definition; each
    //    descendant gets its own NodeRecord (via the recursion below), parent/child topology is
    //    tracked via record.children (uids) + tree.nodes, and any rebuild (hot-reload, dynamic
    //    mount, template instancing) is driven from tree.templates / tree.screens - never from
    //    record.node().children. Keeping children here stored the full subtree definition redundantly
    //    at every level (~O(sum of subtree sizes) instead of O(node count)).
    //
    // Mutable instance state is allocated only for domains that construction or a later setter
    // actually changes. The complete Node above is construction-scoped and is never retained by
    // a live record; bindings keep only their affected props/style/layout/placement payload.
    // ---------------------------------------------------------------------------------------
    step_start = subtree_profile_now();
    for (const auto &binding : initial_style_bindings) {
        auto apply_result = apply_binding_value(tree, record, binding.target, binding.value);
        if (!apply_result) {
            BROOKESIA_LOGW(
                "Failed to retain initial style binding: node='%1%', value='%2%', reason='%3%'",
                absolute_node_path,
                binding.value,
                apply_result.error()
            );
        }
    }
    if (instance_node != nullptr) {
        const auto common_props_mask = PropsApplyMask::CommonHidden |
                                       PropsApplyMask::CommonDisabled |
                                       PropsApplyMask::CommonClickable |
                                       PropsApplyMask::CommonScrollable |
                                       PropsApplyMask::CommonPressLock |
                                       PropsApplyMask::CommonTransform;
        if (has_mask(initial_binding_masks.props, common_props_mask)) {
            record.mutable_common_props() = instance_node->common_props;
        }
        if (has_mask(initial_binding_masks.props, PropsApplyMask::LabelText)) {
            record.mutable_label_props() = instance_node->label_props;
        }
        if (definition.type == NodeType::Image) {
            auto &state = record.ensure_compact_props_state();
            state.image_state = std::make_unique<NodeRecord::CompactPropsState::ImageState>(
            NodeRecord::CompactPropsState::ImageState{
                .props = instance_node->image_props,
                .resolved = instance_node->resolved_image,
            }
                                );
        }

        const auto typed_props_mask = [&definition]() {
            switch (definition.type) {
            case NodeType::FrameView:
                return PropsApplyMask::FrameViewConfig;
            case NodeType::TextInput:
                return PropsApplyMask::TextInputText |
                       PropsApplyMask::TextInputPlaceholder |
                       PropsApplyMask::TextInputPassword |
                       PropsApplyMask::TextInputMultiline |
                       PropsApplyMask::TextInputMaxLength;
            case NodeType::Slider:
            case NodeType::ProgressBar:
            case NodeType::Arc:
                return PropsApplyMask::RangeValue | PropsApplyMask::RangeRange;
            case NodeType::Switch:
            case NodeType::Checkbox:
                return PropsApplyMask::ToggleChecked;
            case NodeType::Dropdown:
                return PropsApplyMask::DropdownOptions | PropsApplyMask::DropdownSelectedIndex;
            case NodeType::Table:
                return PropsApplyMask::TableRows | PropsApplyMask::TableColumns | PropsApplyMask::TableCells;
            case NodeType::Line:
                return PropsApplyMask::LinePoints;
            case NodeType::Keyboard:
                return PropsApplyMask::KeyboardMode |
                       PropsApplyMask::KeyboardPopovers |
                       PropsApplyMask::KeyboardConfig;
            case NodeType::Canvas:
                return PropsApplyMask::CanvasCommands;
            case NodeType::Screen:
            case NodeType::Container:
            case NodeType::Label:
            case NodeType::Image:
            case NodeType::Button:
            case NodeType::Spinner:
            case NodeType::Max:
                return PropsApplyMask::None;
            }
            return PropsApplyMask::None;
        }();
        if (definition.type == NodeType::Keyboard ||
                (typed_props_mask != PropsApplyMask::None &&
                 has_mask(initial_binding_masks.props, typed_props_mask))) {
            switch (definition.type) {
            case NodeType::FrameView:
                record.mutable_frame_view_props() = instance_node->frame_view_props;
                break;
            case NodeType::TextInput:
                record.mutable_text_input_props() = instance_node->text_input_props;
                break;
            case NodeType::Slider:
            case NodeType::ProgressBar:
            case NodeType::Arc:
                record.mutable_range_props() = instance_node->range_props;
                break;
            case NodeType::Switch:
            case NodeType::Checkbox:
                record.mutable_toggle_props() = instance_node->toggle_props;
                break;
            case NodeType::Dropdown:
                record.mutable_dropdown_props() = instance_node->dropdown_props;
                break;
            case NodeType::Table:
                record.mutable_table_props() = instance_node->table_props;
                break;
            case NodeType::Line:
                record.mutable_line_props() = instance_node->line_props;
                break;
            case NodeType::Keyboard:
                record.mutable_keyboard_props() = instance_node->keyboard_props;
                break;
            case NodeType::Canvas:
                record.mutable_canvas_props() = instance_node->canvas_props;
                break;
            case NodeType::Screen:
            case NodeType::Container:
            case NodeType::Label:
            case NodeType::Image:
            case NodeType::Button:
            case NodeType::Spinner:
            case NodeType::Max:
                break;
            }
        }
        if (initial_binding_masks.layout != LayoutApplyMask::None) {
            record.mutable_layout() = instance_node->layout;
        }
        if (initial_binding_masks.placement != PlacementApplyMask::None) {
            record.mutable_placement() = instance_node->placement;
        }
    }
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
    const size_t dbg_s2a = dbg_ext_free();
    dbg_step_node_copy_ += static_cast<int64_t>(dbg_s2b) - static_cast<int64_t>(dbg_s2a);
#endif
    record.applied_style_revision = current_applied_style_revision();
    record.handle = handle;
    record.parent_uid = parent_uid;
    record.is_dynamic_template_root = is_dynamic_template_root;

    tree.handle_to_uid.emplace(handle.value(), uid);
    tree.nodes.emplace(uid, std::move(record));

    auto *stored_record = find_node_record(tree, uid);
    if (stored_record == nullptr) {
        return std::unexpected("Failed to store GUI node record");
    }
    tree.absolute_path_to_uid.emplace(stored_record->absolute_path, uid);
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
    const auto node_type_index = static_cast<size_t>(stored_record->node().type);
    if (node_type_index < dbg_live_nodes_by_type_.size()) {
        ++dbg_live_nodes_by_type_[node_type_index];
        const bool has_compact_props = stored_record->compact_props_state != nullptr;
        BROOKESIA_LOGI(
            "[HeapTrace][gui.node] action(create) type(%1%) live_type(%2%) node_record_size(%3%) node_size(%4%) "
            "compact_props(%5%) psram_free(%6%)",
            get_theme_style_key(stored_record->node().type),
            dbg_live_nodes_by_type_[node_type_index],
            sizeof(NodeRecord),
            sizeof(Node),
            has_compact_props,
            dbg_ext_free()
        );
    }
#endif
    add_subtree_profile_time(subtree_build_profile_.store_record_us, step_start);
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
    const size_t dbg_s3 = dbg_ext_free();
    dbg_step_store_ += static_cast<int64_t>(dbg_s2a) - static_cast<int64_t>(dbg_s3);
    dbg_step_noderecord_ += static_cast<int64_t>(dbg_s2) - static_cast<int64_t>(dbg_s3);
#endif

    step_start = subtree_profile_now();
    // Defer CommonTransform until after layout/placement so percent pivots use final size.
    // apply_props(All) would apply transform once here and again below.
    backend->apply_props(handle, node, PropsApplyMask::All & ~PropsApplyMask::CommonTransform);
    add_subtree_profile_time(subtree_build_profile_.apply_props_us, step_start);
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
    const size_t dbg_s4 = dbg_ext_free();
    dbg_step_apply_props_ += static_cast<int64_t>(dbg_s3) - static_cast<int64_t>(dbg_s4);
#endif
    step_start = subtree_profile_now();
    backend->apply_layout(handle, node.layout);
    add_subtree_profile_time(subtree_build_profile_.apply_layout_us, step_start);
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
    const size_t dbg_s5 = dbg_ext_free();
    dbg_step_apply_layout_ += static_cast<int64_t>(dbg_s4) - static_cast<int64_t>(dbg_s5);
#endif
    step_start = subtree_profile_now();
    backend->apply_placement(handle, node.placement);
    add_subtree_profile_time(subtree_build_profile_.apply_placement_us, step_start);
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
    const size_t dbg_s6 = dbg_ext_free();
    dbg_step_apply_placement_ += static_cast<int64_t>(dbg_s5) - static_cast<int64_t>(dbg_s6);
#endif
    step_start = subtree_profile_now();
    backend->apply_props(handle, node, PropsApplyMask::CommonTransform);
    add_subtree_profile_time(subtree_build_profile_.apply_transform_us, step_start);
    step_start = subtree_profile_now();
    backend->apply_style(handle, *stored_record->resolved_style);
    add_subtree_profile_time(subtree_build_profile_.apply_style_us, step_start);
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
    const size_t dbg_s7 = dbg_ext_free();
    dbg_step_apply_style_ += static_cast<int64_t>(dbg_s6) - static_cast<int64_t>(dbg_s7);
#endif
    step_start = subtree_profile_now();
    backend->apply_debug_visual(handle, view_debug_enabled_);
    backend->apply_animations(handle, node.animations);
    add_subtree_profile_time(subtree_build_profile_.apply_animations_us, step_start);
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
    const size_t dbg_s8 = dbg_ext_free();
    dbg_step_apply_anim_ += static_cast<int64_t>(dbg_s7) - static_cast<int64_t>(dbg_s8);
#endif
    step_start = subtree_profile_now();
    register_fast_action_routes(tree, *stored_record);
    backend->bind_events(handle, node.events);
    add_subtree_profile_time(subtree_build_profile_.events_us, step_start);
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
    const size_t dbg_s9 = dbg_ext_free();
    dbg_step_events_ += static_cast<int64_t>(dbg_s8) - static_cast<int64_t>(dbg_s9);
#endif
    if (parent_uid != 0) {
        auto *parent_record = find_node_record(tree, parent_uid);
        if (parent_record != nullptr) {
            if (parent_record->children == nullptr) {
                parent_record->children = std::make_unique<std::vector<NodeUid>>();
            }
            parent_record->children->push_back(uid);
        }
    }

    for (const auto &child : definition.children) {
        auto child_uid = create_subtree(
                             document_id,
                             tree,
                             child,
                             handle,
                             uid,
                             stable_root_id,
                             append_path(current_path, child.id),
                             scope_root_absolute_path,
                             std::nullopt
                         );
        if (!child_uid) {
            return std::unexpected(child_uid.error());
        }
    }

    return uid;
}
void Runtime::Impl::unload_partial_tree(TreeRecord &tree)
{
    std::vector<NodeUid> roots;
    roots.reserve(tree.nodes.size());
    for (const auto &[uid, record] : tree.nodes) {
        if (record.parent_uid == 0) {
            roots.push_back(uid);
        }
    }

    for (auto uid : roots) {
        destroy_subtree(tree, uid);
    }

    tree.handle_to_uid.clear();
    tree.absolute_path_to_uid.clear();
    tree.nodes.clear();
    tree.interaction_records.clear();
    tree.screen_roots.clear();
    tree.pending_warm_screens.clear();
    tree.idle_warm_scheduled = false;
}

void Runtime::Impl::remove_pending_warm_screen(TreeRecord &tree, std::string_view screen_id)
{
    tree.pending_warm_screens.erase(
        std::remove(tree.pending_warm_screens.begin(), tree.pending_warm_screens.end(), screen_id),
        tree.pending_warm_screens.end()
    );
}

std::expected<Runtime::Impl::NodeUid, std::string> Runtime::Impl::ensure_screen_built(
    DocumentId document_id,
    TreeRecord &tree,
    const std::string &screen_id)
{
    auto screen_root_it = tree.screen_roots.find(screen_id);
    if (screen_root_it != tree.screen_roots.end()) {
        const bool keep_alive_hit = std::any_of(
                                        dynamic_screen_keep_alive_.begin(),
                                        dynamic_screen_keep_alive_.end(),
        [document_id, &screen_id](const DynamicScreenKeepAliveEntry & entry) {
            return entry.document_id == document_id.value() && entry.screen_id == screen_id;
        }
                                    );
        forget_dynamic_screen_keep_alive(document_id, screen_id);
        if (keep_alive_hit) {
            GUI_INTERFACE_PROFILE_LOGI(
                "GUI runtime dynamic keep-alive hit: doc(%1%), screen(%2%), cache(%3%)",
                document_id.value(),
                screen_id,
                dynamic_screen_keep_alive_.size()
            );
        }
        return screen_root_it->second;
    }

    auto screen_def_it = tree.screens.find(screen_id);
    if (screen_def_it == tree.screens.end()) {
        return std::unexpected("Screen not found: /" + screen_id);
    }

    auto deferred_it = tree.deferred_screen_files.find(screen_id);
    if (deferred_it != tree.deferred_screen_files.end()) {
        const auto materialize_start = RuntimeProfileClock::now();
        auto parsed_screen = parse_view_screen_file(
                                 deferred_it->second,
                                 tree.constants,
                                 tree.environment,
                                 tree.view_template_json,
                                 tree.interaction_template_json
                             );
        if (!parsed_screen) {
            return std::unexpected(parsed_screen.error());
        }
        screen_def_it->second = std::move(*parsed_screen);
        tree.deferred_screen_files.erase(deferred_it);
        if (tree.deferred_screen_files.empty()) {
            tree.view_template_json.clear();
            tree.interaction_template_json.clear();
        }
        GUI_INTERFACE_PROFILE_LOGI(
            "GUI runtime screen build profile: doc(%1%), screen(%2%), stage(materialize_deferred), elapsed_ms(%3%)",
            document_id.value(),
            screen_id,
            runtime_profile_elapsed_ms(materialize_start, RuntimeProfileClock::now())
        );
    }

    refresh_global_fonts_from_backend();
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
    auto hp_build_before = ::esp_brookesia::lib_utils::MemoryProfiler::take_raw_heap_snapshot();
#endif
    const auto build_stage_start = RuntimeProfileClock::now();
    reset_subtree_build_profile();
    auto root_uid = create_subtree(
                        document_id,
                        tree,
                        screen_def_it->second,
                        BackendHandle(),
                        0,
                        screen_def_it->second.id,
                        Path(),
                        "/" + screen_def_it->second.id,
                        std::nullopt
                    );
    if (!root_uid) {
        return std::unexpected(root_uid.error());
    }
    tree.screen_roots.emplace(screen_id, *root_uid);
    remove_pending_warm_screen(tree, screen_id);
    forget_dynamic_screen_keep_alive(document_id, screen_id);
    const auto build_stage_end = RuntimeProfileClock::now();
    const auto build_ms = runtime_profile_elapsed_ms(build_stage_start, build_stage_end);
    const char *profile_stage =
        screen_def_it->second.mount_mode == MountMode::Dynamic ? "dynamic_screen_build" : "eager_screen_build";
    log_subtree_build_profile(profile_stage, document_id, screen_def_it->second.id, build_ms);
    GUI_INTERFACE_PROFILE_LOGI(
        "GUI runtime screen build profile: doc(%1%), screen(%2%), mode(%3%), elapsed_ms(%4%)",
        document_id.value(),
        screen_def_it->second.id,
        screen_def_it->second.mount_mode == MountMode::Dynamic ? "dynamic" : "eager",
        build_ms
    );
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
    auto hp_build_after = ::esp_brookesia::lib_utils::MemoryProfiler::take_raw_heap_snapshot();
    BROOKESIA_LOGI(
        "[HeapTrace][gui.screen_build] screen(%1%) mode(%2%) psram_delta(%3%)",
        screen_def_it->second.id,
        screen_def_it->second.mount_mode == MountMode::Dynamic ? "dynamic" : "eager",
        static_cast<int64_t>(hp_build_after.external_free) -
        static_cast<int64_t>(hp_build_before.external_free));
#endif
    return *root_uid;
}

void Runtime::Impl::request_idle_screen_warm(DocumentId document_id)
{
    auto *tree = resolve_tree(document_id);
    if (tree == nullptr || tree->pending_warm_screens.empty()) {
        return;
    }
    GUI_INTERFACE_PROFILE_LOGI(
        "GUI runtime idle warm request: doc(%1%), pending(%2%)",
        document_id.value(),
        tree->pending_warm_screens.size()
    );
    schedule_idle_screen_warm(document_id);
}

void Runtime::Impl::schedule_idle_screen_warm(DocumentId document_id)
{
    auto *tree = resolve_tree(document_id);
    if (tree == nullptr || tree->pending_warm_screens.empty()) {
        if (tree != nullptr) {
            tree->idle_warm_scheduled = false;
        }
        return;
    }
    if (tree->idle_warm_scheduled) {
        return;
    }
    if (!task_config.task_scheduler || !task_config.task_scheduler->is_running() ||
            task_config.gui_group.empty()) {
        GUI_INTERFACE_PROFILE_LOGI(
            "GUI runtime idle warm skip schedule: doc(%1%), pending(%2%), scheduler_ready(%3%), gui_group_empty(%4%)",
            document_id.value(),
            tree->pending_warm_screens.size(),
            (task_config.task_scheduler && task_config.task_scheduler->is_running()) ? 1 : 0,
            task_config.gui_group.empty() ? 1 : 0
        );
        return;
    }

    tree->idle_warm_scheduled = true;
    const bool posted = task_config.task_scheduler->post_delayed(
    [this, document_id]() {
        warm_one_pending_screen(document_id);
    },
    kIdleScreenWarmDelayMs,
    nullptr,
    task_config.gui_group
                        );
    if (!posted) {
        tree->idle_warm_scheduled = false;
        BROOKESIA_LOGW("Failed to schedule idle screen warm for doc(%1%)", document_id.value());
        return;
    }
    GUI_INTERFACE_PROFILE_LOGI(
        "GUI runtime idle warm scheduled: doc(%1%), pending(%2%), delay_ms(%3%)",
        document_id.value(),
        tree->pending_warm_screens.size(),
        kIdleScreenWarmDelayMs
    );
}

void Runtime::Impl::warm_one_pending_screen(DocumentId document_id)
{
    auto *tree = resolve_tree(document_id);
    if (tree == nullptr) {
        return;
    }
    tree->idle_warm_scheduled = false;
    while (!tree->pending_warm_screens.empty()) {
        const std::string screen_id = tree->pending_warm_screens.front();
        tree->pending_warm_screens.erase(tree->pending_warm_screens.begin());
        if (tree->screen_roots.contains(screen_id)) {
            continue;
        }
        auto build_result = ensure_screen_built(document_id, *tree, screen_id);
        if (!build_result) {
            BROOKESIA_LOGW(
                "Idle warm failed for screen(%1%): %2%",
                screen_id,
                build_result.error()
            );
        } else {
            GUI_INTERFACE_PROFILE_LOGI(
                "GUI runtime idle warm built: doc(%1%), screen(%2%), remaining(%3%)",
                document_id.value(),
                screen_id,
                tree->pending_warm_screens.size()
            );
        }
        break;
    }
    if (!tree->pending_warm_screens.empty()) {
        schedule_idle_screen_warm(document_id);
    } else {
        GUI_INTERFACE_PROFILE_LOGI(
            "GUI runtime idle warm done: doc(%1%)",
            document_id.value()
        );
    }
}

void Runtime::Impl::remember_dynamic_screen_keep_alive(DocumentId document_id, const std::string &screen_id)
{
    forget_dynamic_screen_keep_alive(document_id, screen_id);
    dynamic_screen_keep_alive_.push_back(DynamicScreenKeepAliveEntry{
        .document_id = document_id.value(),
        .screen_id = screen_id,
        .sequence = next_dynamic_keep_alive_sequence_++,
    });
    GUI_INTERFACE_PROFILE_LOGI(
        "GUI runtime dynamic keep-alive keep: doc(%1%), screen(%2%), cache(%3%)",
        document_id.value(),
        screen_id,
        dynamic_screen_keep_alive_.size()
    );
    evict_dynamic_screen_keep_alive_overflow();
}

void Runtime::Impl::forget_dynamic_screen_keep_alive(DocumentId document_id, std::string_view screen_id)
{
    dynamic_screen_keep_alive_.erase(
        std::remove_if(
            dynamic_screen_keep_alive_.begin(),
            dynamic_screen_keep_alive_.end(),
    [document_id, screen_id](const DynamicScreenKeepAliveEntry & entry) {
        return entry.document_id == document_id.value() && entry.screen_id == screen_id;
    }
        ),
    dynamic_screen_keep_alive_.end()
    );
}

void Runtime::Impl::evict_dynamic_screen_keep_alive_overflow()
{
    while (dynamic_screen_keep_alive_.size() > kDynamicScreenKeepAliveLimit) {
        auto oldest_it = std::min_element(
                             dynamic_screen_keep_alive_.begin(),
                             dynamic_screen_keep_alive_.end(),
        [](const DynamicScreenKeepAliveEntry & lhs, const DynamicScreenKeepAliveEntry & rhs) {
            return lhs.sequence < rhs.sequence;
        }
                         );
        if (oldest_it == dynamic_screen_keep_alive_.end()) {
            break;
        }
        const DocumentId document_id(oldest_it->document_id);
        const std::string screen_id = oldest_it->screen_id;
        dynamic_screen_keep_alive_.erase(oldest_it);
        auto *tree = resolve_tree(document_id);
        if (tree == nullptr) {
            continue;
        }
        auto screen_root_it = tree->screen_roots.find(screen_id);
        if (screen_root_it == tree->screen_roots.end()) {
            continue;
        }
        // Skip eviction while the screen is still mounted.
        bool still_mounted = false;
        const auto absolute_path = "/" + screen_id;
        for (const auto &[unused_key, mounted_ref] : mounted_screens_) {
            (void)unused_key;
            if (mounted_ref.document_id == document_id && mounted_ref.absolute_path == absolute_path) {
                still_mounted = true;
                break;
            }
        }
        if (still_mounted) {
            continue;
        }
        destroy_subtree(*tree, screen_root_it->second);
        tree->screen_roots.erase(screen_id);
        GUI_INTERFACE_PROFILE_LOGI(
            "GUI runtime dynamic keep-alive evict: doc(%1%), screen(%2%)",
            document_id.value(),
            screen_id
        );
    }
}

void Runtime::Impl::clear_dynamic_screen_keep_alive_for_document(DocumentId document_id)
{
    dynamic_screen_keep_alive_.erase(
        std::remove_if(
            dynamic_screen_keep_alive_.begin(),
            dynamic_screen_keep_alive_.end(),
    [document_id](const DynamicScreenKeepAliveEntry & entry) {
        return entry.document_id == document_id.value();
    }
        ),
    dynamic_screen_keep_alive_.end()
    );
}

bool Runtime::Impl::unmount_mounted_screen(TreeRecord &tree, std::string_view absolute_path)
{
    const auto screen_id = trim_slashes(absolute_path);
    auto mounted_it = tree.screen_roots.find(screen_id);
    if (mounted_it == tree.screen_roots.end()) {
        return false;
    }

    auto *record = find_node_record(tree, mounted_it->second);
    if (record == nullptr) {
        return false;
    }

    const bool result = backend->unmount_screen(record->handle);
    auto screen_def_it = tree.screens.find(screen_id);
    const bool is_dynamic =
        screen_def_it != tree.screens.end() && screen_def_it->second.mount_mode == MountMode::Dynamic;
    if (is_dynamic) {
        // Keep recently used Dynamic screens alive for fast remount (LRU capacity 2).
        remember_dynamic_screen_keep_alive(tree.document_id, screen_id);
    }
    return result;
}
void Runtime::Impl::destroy_subtree(TreeRecord &tree, NodeUid uid)
{
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
    ++dbg_destroy_subtree_count_;
#endif
    auto *record = find_node_record(tree, uid);
    if (record == nullptr) {
        return;
    }

    std::vector<NodeUid> subtree_uids;
    collect_subtree_uids(tree, uid, subtree_uids);

    const NodeUid parent_uid = record->parent_uid;
    const BackendHandle handle = record->handle;

    backend->destroy_node(handle);

    if (parent_uid != 0) {
        auto *parent_record = find_node_record(tree, parent_uid);
        if (parent_record != nullptr && parent_record->children != nullptr) {
            auto &siblings = *parent_record->children;
            siblings.erase(std::remove(siblings.begin(), siblings.end(), uid), siblings.end());
            if (siblings.empty()) {
                parent_record->children.reset();
            }
        }
    }

    for (auto current_uid : subtree_uids) {
        auto current_it = tree.nodes.find(current_uid);
        if (current_it == tree.nodes.end()) {
            continue;
        }

#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
        const auto node_type_index = static_cast<size_t>(current_it->second.node().type);
        if (node_type_index < dbg_live_nodes_by_type_.size() && dbg_live_nodes_by_type_[node_type_index] > 0) {
            const bool has_compact_props = current_it->second.compact_props_state != nullptr;
            --dbg_live_nodes_by_type_[node_type_index];
            BROOKESIA_LOGI(
                "[HeapTrace][gui.node] action(destroy) type(%1%) live_type(%2%) node_record_size(%3%) node_size(%4%) "
                "compact_props(%5%) psram_free(%6%)",
                get_theme_style_key(current_it->second.node().type),
                dbg_live_nodes_by_type_[node_type_index],
                sizeof(NodeRecord),
                sizeof(Node),
                has_compact_props,
                dbg_ext_free()
            );
        }
#endif
        unregister_fast_action_routes(current_it->second);
        tree.absolute_path_to_uid.erase(current_it->second.absolute_path);
        tree.handle_to_uid.erase(current_it->second.handle.value());
        erase_interaction_state(tree, current_uid);
        tree.nodes.erase(current_it);
    }
}
void Runtime::Impl::collect_subtree_uids(const TreeRecord &tree, NodeUid uid, std::vector<NodeUid> &uids) const
{
    auto *record = find_node_record_const(tree, uid);
    if (record == nullptr) {
        return;
    }
    uids.push_back(uid);
    if (record->children != nullptr) {
        for (auto child_uid : *record->children) {
            collect_subtree_uids(tree, child_uid, uids);
        }
    }
}
std::expected<std::vector<Dimension>, std::string> Runtime::Impl::parse_dimension_array_from_store_string(
    std::string_view value,
    const Environment &environment) const
{
    auto parsed = parse_json_fragment(value, "dimension array");
    if (!parsed) {
        return std::unexpected(parsed.error());
    }
    if (!parsed->is_array()) {
        return std::unexpected("dimension array must be a JSON array");
    }

    std::vector<Dimension> result;
    for (const auto &entry : parsed->as_array()) {
        if (entry.is_string()) {
            auto dimension = parse_dimension_from_store_string(entry.as_string().c_str(), environment);
            if (!dimension) {
                return std::unexpected(dimension.error());
            }
            result.push_back(*dimension);
        } else if (entry.is_int64()) {
            result.push_back(Dimension{.mode = SizeMode::Fixed, .value = static_cast<int32_t>(entry.as_int64())});
        } else {
            return std::unexpected("dimension array entries must be strings or integers");
        }
    }
    return result;
}
std::expected<std::vector<std::string>, std::string> Runtime::Impl::parse_string_array_from_store_string(std::string_view value) const
{
    auto parsed = parse_json_fragment(value, "string array");
    if (!parsed) {
        return std::unexpected(parsed.error());
    }
    if (!parsed->is_array()) {
        return std::unexpected("string array must be a JSON array");
    }

    std::vector<std::string> result;
    for (const auto &entry : parsed->as_array()) {
        if (!entry.is_string()) {
            return std::unexpected("string array entries must be strings");
        }
        result.emplace_back(entry.as_string().c_str());
    }
    return result;
}
std::expected<std::vector<Point>, std::string> Runtime::Impl::parse_points_from_store_string(std::string_view value) const
{
    auto parsed = parse_json_fragment(value, "points");
    if (!parsed) {
        return std::unexpected(parsed.error());
    }
    if (!parsed->is_array()) {
        return std::unexpected("points must be a JSON array");
    }

    std::vector<Point> result;
    for (const auto &entry : parsed->as_array()) {
        if (!entry.is_object()) {
            return std::unexpected("point entries must be objects");
        }
        const auto &point_object = entry.as_object();
        const auto *x_value = point_object.if_contains("x");
        const auto *y_value = point_object.if_contains("y");
        if (x_value == nullptr || y_value == nullptr || !x_value->is_int64() || !y_value->is_int64()) {
            return std::unexpected("point entries must contain integer x and y");
        }
        result.push_back(Point{
            .x = static_cast<int32_t>(x_value->as_int64()),
            .y = static_cast<int32_t>(y_value->as_int64()),
        });
    }
    return result;
}
std::expected<std::vector<TableCell>, std::string> Runtime::Impl::parse_table_cells_from_store_string(std::string_view value) const
{
    auto parsed = parse_json_fragment(value, "table cells");
    if (!parsed) {
        return std::unexpected(parsed.error());
    }
    if (!parsed->is_array()) {
        return std::unexpected("table cells must be a JSON array");
    }

    std::vector<TableCell> result;
    for (const auto &entry : parsed->as_array()) {
        if (!entry.is_object()) {
            return std::unexpected("table cell entries must be objects");
        }
        const auto &cell_object = entry.as_object();
        const auto *row_value = cell_object.if_contains("row");
        const auto *column_value = cell_object.if_contains("column");
        const auto *text_value = cell_object.if_contains("text");
        if (row_value == nullptr || column_value == nullptr || text_value == nullptr || !row_value->is_int64() ||
                !column_value->is_int64() || !text_value->is_string()) {
            return std::unexpected("table cell entries must contain integer row/column and string text");
        }
        result.push_back(TableCell{
            .row = static_cast<int32_t>(row_value->as_int64()),
            .column = static_cast<int32_t>(column_value->as_int64()),
            .text = text_value->as_string().c_str(),
        });
    }
    return result;
}
std::expected<std::vector<CanvasCommand>, std::string> Runtime::Impl::parse_canvas_commands_from_store_string(std::string_view value) const
{
    auto parsed = parse_json_fragment(value, "canvas commands");
    if (!parsed) {
        return std::unexpected(parsed.error());
    }
    if (!parsed->is_array()) {
        return std::unexpected("canvas commands must be a JSON array");
    }

    std::vector<CanvasCommand> result;
    for (const auto &entry : parsed->as_array()) {
        if (!entry.is_object()) {
            return std::unexpected("canvas command entries must be objects");
        }
        const auto &command_object = entry.as_object();
        const auto *type_value = command_object.if_contains("type");
        const auto *x_value = command_object.if_contains("x");
        const auto *y_value = command_object.if_contains("y");
        const auto *width_value = command_object.if_contains("width");
        const auto *height_value = command_object.if_contains("height");
        const auto *color_value = command_object.if_contains("color");
        if (type_value == nullptr || x_value == nullptr || y_value == nullptr || width_value == nullptr ||
                height_value == nullptr || color_value == nullptr || !type_value->is_string() || !x_value->is_int64() ||
                !y_value->is_int64() || !width_value->is_int64() || !height_value->is_int64() || !color_value->is_string()) {
            return std::unexpected("canvas command entries must contain type/x/y/width/height/color");
        }
        result.push_back(CanvasCommand{
            .type = type_value->as_string().c_str(),
            .x = static_cast<int32_t>(x_value->as_int64()),
            .y = static_cast<int32_t>(y_value->as_int64()),
            .width = static_cast<int32_t>(width_value->as_int64()),
            .height = static_cast<int32_t>(height_value->as_int64()),
            .color = color_value->as_string().c_str(),
        });
    }
    return result;
}

} // namespace esp_brookesia::gui
