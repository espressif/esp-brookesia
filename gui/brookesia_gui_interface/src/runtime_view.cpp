/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "private/runtime_impl.hpp"

namespace esp_brookesia::gui {

View Runtime::Impl::find_view(Runtime *runtime, DocumentId document_id, std::string_view absolute_path)
{
    auto *tree = resolve_tree(document_id);
    if (tree == nullptr) {
        return View();
    }
    auto refresh_result = refresh_document_if_dirty(runtime, document_id);
    if (!refresh_result) {
        BROOKESIA_LOGW(
            "Failed to refresh dirty document before find_view: document_id=%1%, path='%2%', error=%3%",
            document_id,
            absolute_path,
            refresh_result.error()
        );
        return View();
    }
    tree = resolve_tree(document_id);
    if (tree == nullptr) {
        return View();
    }

    const auto query = normalize_absolute_path(absolute_path);
    if (query == "/") {
        return View();
    }

    if (auto uid = resolve_any_uid(*tree, query)) {
        auto *record = find_node_record_const(*tree, *uid);
        if (record != nullptr) {
            return View(runtime, document_id, record->absolute_path, record->node().type);
        }
    }

    return View();
}
std::expected<View, std::string> Runtime::Impl::create_view(
    Runtime *runtime,
    DocumentId document_id,
    std::string_view template_id,
    std::string_view parent_absolute_path,
    std::string_view instance_id)
{
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
    ++dbg_create_view_count_;
    auto hp_cv_before = ::esp_brookesia::lib_utils::MemoryProfiler::take_raw_heap_snapshot();
    dbg_step_create_node_ = 0;
    dbg_step_noderecord_ = 0;
    dbg_step_apply_props_ = 0;
    dbg_step_apply_layout_ = 0;
    dbg_step_apply_placement_ = 0;
    dbg_step_apply_style_ = 0;
    dbg_step_apply_anim_ = 0;
    dbg_step_events_ = 0;
    dbg_step_subscribe_ = 0;
    dbg_step_node_copy_ = 0;
    dbg_step_resolve_style_ = 0;
    dbg_step_store_ = 0;
    const size_t dbg_subtree_before = dbg_create_subtree_count_;
#endif
    auto *tree = resolve_tree(document_id);
    if (tree == nullptr) {
        return std::unexpected("GUI document not loaded");
    }
    auto refresh_result = refresh_document_if_dirty(runtime, document_id);
    if (!refresh_result) {
        return std::unexpected(refresh_result.error());
    }
    tree = resolve_tree(document_id);
    if (tree == nullptr) {
        return std::unexpected("GUI document not loaded");
    }

    auto template_it = tree->templates.find(std::string(template_id));
    if (template_it == tree->templates.end()) {
        return std::unexpected("Template not found: " + std::string(template_id));
    }

    const auto parent_query = normalize_absolute_path(parent_absolute_path);
    if (parent_query == "/") {
        return std::unexpected("Parent path must not be empty");
    }
    const auto instance_name = trim_slashes(instance_id);
    if (instance_name.empty() || instance_name.find('/') != std::string::npos) {
        return std::unexpected("Instance id must be a single path segment");
    }

    auto parent_uid = resolve_any_uid(*tree, parent_query);
    if (!parent_uid.has_value()) {
        return std::unexpected("Parent view not found: " + parent_query);
    }

    auto *parent_record = find_node_record(*tree, *parent_uid);
    if (parent_record == nullptr) {
        return std::unexpected("Parent record not found");
    }
    // Copy parent fields before create_subtree(); inserting child records can rehash tree.nodes.
    const BackendHandle parent_handle = parent_record->handle;
    const NodeUid parent_record_uid = *parent_uid;
    const auto *root_record = find_root_node_record(*tree, *parent_record);
    if (root_record == nullptr) {
        return std::unexpected("Parent view hierarchy is incomplete: " + parent_query);
    }
    const std::string parent_root_id(root_record->node_id());
    const Path parent_path = build_relative_node_path(*tree, *parent_record);

    const auto created_absolute_path = parent_query + "/" + instance_name;
    if (tree->absolute_path_to_uid.contains(created_absolute_path)) {
        return std::unexpected("View already exists: " + created_absolute_path);
    }
    refresh_global_fonts_from_backend();
    auto root_uid = create_subtree(
                        document_id,
                        *tree,
                        template_it->second,
                        parent_handle,
                        parent_record_uid,
                        parent_root_id,
                        append_path(parent_path, instance_name),
                        created_absolute_path,
                        instance_name,
                        true
                    );
    if (!root_uid) {
        return std::unexpected(root_uid.error());
    }

#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
    {
        auto hp_cv_after = ::esp_brookesia::lib_utils::MemoryProfiler::take_raw_heap_snapshot();
        const size_t dbg_nodes_built = dbg_create_subtree_count_ - dbg_subtree_before;
        BROOKESIA_LOGI(
            "[HeapTrace][gui.create_view] count(%1%) template(%2%) nodes_built(%3%) "
            "psram_before(%4%) psram_after(%5%) delta(%6%)",
            dbg_create_view_count_, std::string(template_id), dbg_nodes_built,
            hp_cv_before.external_free, hp_cv_after.external_free,
            static_cast<int64_t>(hp_cv_after.external_free) - static_cast<int64_t>(hp_cv_before.external_free)
        );
        BROOKESIA_LOGI(
            "[HeapTrace][gui.create_view.breakdown] count(%1%) nodes_built(%2%) create_node(%3%) noderecord(%4%) "
            "apply_props(%5%) apply_layout(%6%) apply_placement(%7%) apply_style(%8%) apply_anim(%9%) "
            "events(%10%) subscribe(%11%)",
            dbg_create_view_count_, dbg_nodes_built, dbg_step_create_node_, dbg_step_noderecord_,
            dbg_step_apply_props_, dbg_step_apply_layout_, dbg_step_apply_placement_, dbg_step_apply_style_,
            dbg_step_apply_anim_, dbg_step_events_, dbg_step_subscribe_
        );
        BROOKESIA_LOGI(
            "[HeapTrace][gui.create_view.noderecord] count(%1%) nodes_built(%2%) node_copy(%3%) "
            "resolve_style(%4%) store(%5%)",
            dbg_create_view_count_, dbg_nodes_built, dbg_step_node_copy_,
            dbg_step_resolve_style_, dbg_step_store_
        );
        size_t definition_style_entries = 0;
        size_t unique_style_results = 0;
        for (const auto &[unused_document_id, cached_tree] : trees) {
            (void)unused_document_id;
            definition_style_entries += cached_tree.definition_style_cache.size();
            for (const auto &entry : cached_tree.resolved_style_intern_cache) {
                if (!entry.expired()) {
                    ++unique_style_results;
                }
            }
        }
        BROOKESIA_LOGI(
            "[HeapTrace][gui.style_cache] count(%1%) hits(%2%) misses(%3%) entries(%4%) "
            "content_entries(%5%) definition_entries(%6%) unique_results(%7%) "
            "intern_hits(%8%) intern_misses(%9%)",
            dbg_create_view_count_, dbg_style_cache_hits_, dbg_style_cache_misses_,
            definition_style_entries,
            0, definition_style_entries, unique_style_results,
            dbg_style_intern_hits_, dbg_style_intern_misses_
        );
    }
#endif
    return View(runtime, document_id, created_absolute_path, template_it->second.type);
}
bool Runtime::Impl::destroy_view(DocumentId document_id, std::string_view absolute_path)
{
    auto *tree = resolve_tree(document_id);
    if (tree == nullptr) {
        return false;
    }

    const auto query = normalize_absolute_path(absolute_path);
    auto uid = resolve_any_uid(*tree, query);
    if (!uid.has_value()) {
        return false;
    }

    auto *record = find_node_record(*tree, *uid);
    if (record == nullptr || record->node().type == NodeType::Screen) {
        return false;
    }

    destroy_subtree(*tree, *uid);
    return true;
}
std::vector<Runtime::Impl::InstanceSnapshot> Runtime::Impl::capture_dynamic_instance_snapshots(const TreeRecord &tree)
{
    std::vector<InstanceSnapshot> snapshots;
    for (const auto &[uid, record] : tree.nodes) {
        if (!record.is_dynamic_template_root || record.definition == nullptr || record.parent_uid == 0) {
            continue;
        }
        auto parent_it = tree.nodes.find(record.parent_uid);
        if (parent_it == tree.nodes.end()) {
            continue;
        }

        size_t sibling_order = 0;
        if (parent_it->second.children != nullptr) {
            auto child_it = std::find(
                                parent_it->second.children->begin(),
                                parent_it->second.children->end(),
                                uid
                            );
            if (child_it != parent_it->second.children->end()) {
                sibling_order = static_cast<size_t>(
                                    std::distance(parent_it->second.children->begin(), child_it)
                                );
            }
        }
        snapshots.push_back(InstanceSnapshot{
            .template_id = record.definition->id,
            .parent_absolute_path = parent_it->second.absolute_path,
            .instance_id = std::string(record.node_id()),
            .sibling_order = sibling_order,
        });
    }
    return snapshots;
}
std::expected<void, std::string> Runtime::Impl::update(
    Runtime *runtime,
    DocumentId document_id,
    std::string_view file_path,
    const Environment &environment)
{
    auto effective_environment = environment;
    if (const auto *tree = resolve_tree_const(document_id); tree != nullptr) {
        if (effective_environment.theme_id.empty()) {
            effective_environment.theme_id = tree->environment.theme_id;
        }
        if (effective_environment.language.empty()) {
            effective_environment.language = tree->environment.language;
        }
    }
    effective_environment = make_effective_environment(std::move(effective_environment));
    const auto parse_environment = make_parse_environment(effective_environment);
    auto parsed_document = parse_document_file_with_metadata(file_path, parse_environment);
    if (!parsed_document) {
        return std::unexpected(parsed_document.error());
    }
    return update(runtime, document_id, file_path, effective_environment, *parsed_document);
}
std::expected<void, std::string> Runtime::Impl::update(
    Runtime *runtime,
    DocumentId document_id,
    std::string_view file_path,
    const Environment &environment,
    const ParsedDocument &parsed_document)
{
    auto *tree = resolve_tree(document_id);
    if (tree == nullptr) {
        return std::unexpected("GUI document not loaded");
    }

    auto validation = validate_document(parsed_document.document);
    if (!validation.success) {
        return std::unexpected(validation.errors.empty() ? "Invalid GUI document" : validation.errors.front());
    }

    std::vector<RunningScreenFlowSnapshot> running_flows;
    for (const auto &[unused_key, flow] : running_screen_flows_) {
        (void)unused_key;
        if (flow.document_id == document_id) {
            running_flows.push_back(RunningScreenFlowSnapshot{
                .document_id = flow.document_id,
                .flow_id = flow.flow_id,
                .current_state = flow.current_state,
                .current_screen = flow.current_screen,
                .target = flow.target,
                .was_mounted = is_screen_flow_screen_mounted(flow),
            });
        }
    }
    stop_screen_flows_for_document(document_id);

    std::vector<std::string> instantiated_dynamic_screens;
    std::vector<NodeStateSnapshot> state_snapshots;
    std::vector<MountedScreenRef> mounted_refs;
    for (const auto &[unused_target_key, mounted_ref] : mounted_screens_) {
        (void)unused_target_key;
        if (mounted_ref.document_id == document_id) {
            mounted_refs.push_back(mounted_ref);
        }
    }
    pop_transient_screens_for_document(document_id);

    for (const auto &[screen_id, uid] : tree->screen_roots) {
        auto screen_it = tree->screens.find(screen_id);
        if (screen_it != tree->screens.end() && screen_it->second.mount_mode == MountMode::Dynamic) {
            instantiated_dynamic_screens.push_back("/" + screen_id);
        }
        (void)uid;
    }

    for (const auto &[uid, record] : tree->nodes) {
        (void)uid;
        std::optional<Point> runtime_position;
        if (backend != nullptr && record.placement().mode == PlacementMode::Absolute) {
            auto frame = backend->get_node_frame(record.handle);
            if (frame && record.placement().x.mode == PlacementOffsetMode::Fixed &&
                    record.placement().y.mode == PlacementOffsetMode::Fixed &&
                    (frame->x != record.placement().x.value || frame->y != record.placement().y.value)) {
                runtime_position = Point{
                    .x = frame->x,
                    .y = frame->y,
                };
            }
        }
        state_snapshots.push_back({
            .absolute_path = record.absolute_path,
            .hidden = record.common_props().hidden,
            .runtime_position = runtime_position,
        });

    }

    auto instance_snapshots = capture_dynamic_instance_snapshots(*tree);
    std::sort(instance_snapshots.begin(), instance_snapshots.end(), [](const InstanceSnapshot & lhs, const InstanceSnapshot & rhs) {
        if (lhs.parent_absolute_path.size() != rhs.parent_absolute_path.size()) {
            return lhs.parent_absolute_path.size() < rhs.parent_absolute_path.size();
        }
        if (lhs.parent_absolute_path != rhs.parent_absolute_path) {
            return lhs.parent_absolute_path < rhs.parent_absolute_path;
        }
        if (lhs.sibling_order != rhs.sibling_order) {
            return lhs.sibling_order < rhs.sibling_order;
        }
        return lhs.instance_id < rhs.instance_id;
    });

    for (const auto &mounted_ref : mounted_refs) {
        (void)unmount_screen(mounted_ref.document_id, mounted_ref.absolute_path);
    }

    unload_partial_tree(*tree);
    release_tree_image_resources(*tree);
    tree->file_path = normalize_file_path(file_path);
    tree->file_backed = true;
    tree->environment_dependencies = parsed_document.document.environment_dependencies;
    tree->theme_sensitive = parsed_document.document.theme_sensitive;
    tree->constants = parsed_document.document.constants;
    tree->styles = parsed_document.document.styles;
    tree->deferred_screen_files = parsed_document.deferred_screen_files;
    tree->view_template_json = parsed_document.view_template_json;
    tree->interaction_template_json = parsed_document.interaction_template_json;
    advance_style_revision();
    tree->environment = make_effective_environment(environment);
    tree->dependency_files = normalize_dependency_files(parsed_document.dependency_files);
    if (tree->live_preview_enabled) {
        tree->dependency_mtimes = capture_dependency_mtimes(tree->dependency_files);
    } else {
        tree->dependency_mtimes.clear();
    }
    tree->environment_dirty = false;
    tree->styles_dirty = false;
    tree->images.clear();
    tree->screen_flows.clear();
    for (const auto &flow : parsed_document.document.screen_flows) {
        tree->screen_flows.emplace(flow.id, flow);
    }
    tree->definition_style_cache.clear();
    tree->resolved_style_intern_cache.clear();
    tree->screens.clear();
    tree->templates.clear();
    for (const auto &image : parsed_document.document.images) {
        tree->images.emplace(image.id, image);
        BROOKESIA_LOGD("Updated document image asset: id='%1%', src='%2%', width=%3%, height=%4%, preload=%5%",
                       image.id, image.src, image.width, image.height, image.preload);
    }
    for (const auto &node : parsed_document.document.screens) {
        tree->screens.emplace(node.id, node);
    }
    for (const auto &node : parsed_document.document.templates) {
        tree->templates.emplace(node.id, node);
    }
    auto resource_validation = validate_resource_references(*tree);
    if (!resource_validation) {
        return std::unexpected(resource_validation.error());
    }
    auto preload_result = preload_tree_image_resources(*tree);
    if (!preload_result) {
        release_tree_image_resources(*tree);
        return std::unexpected(preload_result.error());
    }

    // Preserve the document order while keeping every NodeRecord definition pointer anchored
    // in TreeRecord. parsed_document is temporary and cannot own runtime definitions.
    for (const auto &parsed_screen : parsed_document.document.screens) {
        const auto screen_it = tree->screens.find(parsed_screen.id);
        if (screen_it == tree->screens.end()) {
            return std::unexpected("Updated screen definition not found: " + parsed_screen.id);
        }
        const auto &screen = screen_it->second;
        if (screen.mount_mode == MountMode::Dynamic) {
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
            BROOKESIA_LOGI(
                "[HeapTrace][gui.eager_mount] screen(%1%) mode(dynamic) action(skipped)", screen.id);
#endif
            continue;
        }
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
        auto hp_eager_before = ::esp_brookesia::lib_utils::MemoryProfiler::take_raw_heap_snapshot();
#endif
        auto root_uid = create_subtree(
                            document_id,
                            *tree,
                            screen,
                            BackendHandle(),
                            0,
                            screen.id,
                            Path(),
                            "/" + screen.id,
                            std::nullopt
                        );
        if (!root_uid) {
            return std::unexpected(root_uid.error());
        }
        tree->screen_roots.emplace(screen.id, *root_uid);
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
        auto hp_eager_after = ::esp_brookesia::lib_utils::MemoryProfiler::take_raw_heap_snapshot();
        BROOKESIA_LOGI(
            "[HeapTrace][gui.eager_mount] screen(%1%) mode(eager) action(built) psram_delta(%2%)",
            screen.id,
            static_cast<int64_t>(hp_eager_after.external_free) -
            static_cast<int64_t>(hp_eager_before.external_free));
#endif
    }

    for (const auto &screen_path : instantiated_dynamic_screens) {
        const auto screen_id = trim_slashes(screen_path);
        if (tree->screen_roots.contains(screen_id) || !tree->screens.contains(screen_id)) {
            continue;
        }
        const auto &screen = tree->screens.at(screen_id);
        if (screen.mount_mode != MountMode::Dynamic) {
            continue;
        }
        auto root_uid = create_subtree(
                            document_id,
                            *tree,
                            screen,
                            BackendHandle(),
                            0,
                            screen.id,
                            Path(),
                            "/" + screen.id,
                            std::nullopt
                        );
        if (!root_uid) {
            return std::unexpected(root_uid.error());
        }
        tree->screen_roots.emplace(screen.id, *root_uid);
    }

    for (const auto &snapshot : instance_snapshots) {
        auto create_result = create_view(
                                 runtime,
                                 document_id,
                                 snapshot.template_id,
                                 snapshot.parent_absolute_path,
                                 snapshot.instance_id
                             );
        if (!create_result) {
            continue;
        }
    }

    for (const auto &snapshot : state_snapshots) {
        auto view = find_view(runtime, document_id, snapshot.absolute_path);
        if (!view.valid()) {
            continue;
        }
        set_view_hidden(view, snapshot.hidden);
        if (snapshot.runtime_position && backend != nullptr) {
            auto uid = resolve_any_uid(*tree, snapshot.absolute_path);
            if (!uid) {
                continue;
            }
            auto *record = find_node_record(*tree, *uid);
            if (record == nullptr) {
                continue;
            }
            auto placement = record->placement();
            placement.x = PlacementOffset(snapshot.runtime_position->x);
            placement.y = PlacementOffset(snapshot.runtime_position->y);
            backend->apply_placement(record->handle, placement, PlacementApplyMask::Position);
        }
    }

    for (const auto &mounted_ref : mounted_refs) {
        auto mount_result = mount_screen(runtime, document_id, mounted_ref.absolute_path, mounted_ref.target);
        if (!mount_result) {
            BROOKESIA_LOGW(
                "Failed to restore mounted screen after update: document_id=%1%, path='%2%', display_id='%3%', layer=%4%",
                document_id,
                mounted_ref.absolute_path,
                mounted_ref.target.display_id,
                BROOKESIA_DESCRIBE_ENUM_TO_STR(mounted_ref.target.layer)
            );
        }
    }

    for (const auto &flow : running_flows) {
        auto flow_it = tree->screen_flows.find(flow.flow_id);
        if (flow_it == tree->screen_flows.end()) {
            BROOKESIA_LOGW(
                "Failed to restore screen flow after update: document_id=%1%, flow='%2%', reason=flow missing",
                document_id,
                flow.flow_id
            );
            continue;
        }

        auto state = flow.current_state;
        if (!screen_flow_contains_state(flow_it->second, state)) {
            state = flow_it->second.initial;
        }
        auto screen_path = make_screen_flow_screen_path(state);
        if (!flow.was_mounted) {
            running_screen_flows_.insert_or_assign(
                build_screen_flow_key(document_id, flow.flow_id),
            RunningScreenFlow{
                .document_id = document_id,
                .flow_id = flow.flow_id,
                .current_state = state,
                .current_screen = screen_path,
                .target = flow.target,
            }
            );
            continue;
        }

        auto mount_result = mount_screen(runtime, document_id, screen_path, flow.target);
        if (!mount_result) {
            BROOKESIA_LOGW(
                "Failed to restore screen flow after update: document_id=%1%, flow='%2%', state='%3%', error=%4%",
                document_id,
                flow.flow_id,
                state,
                mount_result.error()
            );
            continue;
        }
        running_screen_flows_.insert_or_assign(
            build_screen_flow_key(document_id, flow.flow_id),
        RunningScreenFlow{
            .document_id = document_id,
            .flow_id = flow.flow_id,
            .current_state = state,
            .current_screen = screen_path,
            .target = flow.target,
        }
        );
    }

    return {};
}
size_t Runtime::Impl::reapply_style_record(TreeRecord &tree, NodeRecord &record)
{
    if (record.applied_style_revision == current_applied_style_revision()) {
        return 0;
    }
    if (record.node().type == NodeType::Keyboard && !record.keyboard_props().key_style_refs.empty()) {
        auto &props = record.mutable_keyboard_props();
        resolve_keyboard_key_style_refs(tree, props);
        if (backend != nullptr) {
            apply_record_props(record, PropsApplyMask::KeyboardConfig);
        }
    }
    const Node *style_definition = nullptr;
    if (!node_has_style_binding(*record.definition)) {
        style_definition = record.definition;
    }
    if (style_definition != nullptr) {
        record.resolved_style = resolve_style_shared(tree, *record.definition, style_definition);
    } else {
        const auto style_snapshot = make_style_snapshot(record);
        record.resolved_style = resolve_style_shared(tree, style_snapshot);
    }
    record.applied_style_revision = current_applied_style_revision();
    if (backend != nullptr) {
        backend->apply_style(record.handle, *record.resolved_style);
    }
    return 1;
}
size_t Runtime::Impl::reapply_subtree_styles(TreeRecord &tree, NodeUid uid)
{
    auto *record = find_node_record(tree, uid);
    if (record == nullptr) {
        return 0;
    }

    size_t applied_count = reapply_style_record(tree, *record);
    if (record->children != nullptr) {
        const auto children = *record->children;
        for (auto child_uid : children) {
            applied_count += reapply_subtree_styles(tree, child_uid);
        }
    }
    return applied_count;
}
size_t Runtime::Impl::reapply_styles(TreeRecord &tree)
{
    size_t applied_count = 0;
    for (auto &[unused_uid, record] : tree.nodes) {
        (void)unused_uid;
        record.applied_style_revision = 0;
        applied_count += reapply_style_record(tree, record);
    }
    return applied_count;
}
void Runtime::Impl::reapply_styles_for_all_trees()
{
    advance_style_revision();
    for (auto &[unused_document_id, tree] : trees) {
        (void)unused_document_id;
        reapply_styles(tree);
        tree.styles_dirty = false;
    }
}
size_t Runtime::Impl::reapply_mounted_styles_for_all_trees()
{
    advance_style_revision();
    size_t applied_count = 0;
    for (auto &[unused_document_id, tree] : trees) {
        (void)unused_document_id;
        // Keep preloaded but unmounted documents lazy. Applying a language font switch to every
        // retained DOM can instantiate large FreeType font caches for apps the user has not opened.
        tree.styles_dirty = true;
        applied_count += reapply_mounted_styles(tree);
    }
    return applied_count;
}
size_t Runtime::Impl::reapply_mounted_styles(TreeRecord &tree)
{
    boost::unordered_flat_set<uint64_t> refreshed_roots;
    size_t applied_count = 0;
    auto refresh_screen = [&](std::string_view absolute_path) {
        const auto screen_id = trim_slashes(absolute_path);
        auto screen_root_it = tree.screen_roots.find(screen_id);
        if (screen_root_it == tree.screen_roots.end() || !refreshed_roots.insert(screen_root_it->second).second) {
            return;
        }
        applied_count += reapply_subtree_styles(tree, screen_root_it->second);
    };

    for (const auto &[unused_key, mounted_ref] : mounted_screens_) {
        (void)unused_key;
        if (mounted_ref.document_id == tree.document_id) {
            refresh_screen(mounted_ref.absolute_path);
        }
    }
    for (const auto &[unused_id, transient_ref] : transient_screens_) {
        (void)unused_id;
        if (transient_ref.document_id == tree.document_id) {
            refresh_screen(transient_ref.absolute_path);
        }
    }
    return applied_count;
}
std::expected<void, std::string> Runtime::Impl::refresh_document_environment(Runtime *runtime, DocumentId document_id)
{
    auto *tree = resolve_tree(document_id);
    if (tree == nullptr) {
        return std::unexpected("Document not loaded: " + std::to_string(document_id.value()));
    }
    if (!tree->file_backed || tree->file_path.empty()) {
        return std::unexpected(
                   "Lazy environment refresh requires file-backed document: " + std::to_string(document_id.value())
               );
    }

    return update(runtime, document_id, tree->file_path, tree->environment);
}
std::expected<void, std::string> Runtime::Impl::refresh_document_styles(DocumentId document_id)
{
    auto *tree = resolve_tree(document_id);
    if (tree == nullptr) {
        return std::unexpected("Document not loaded: " + std::to_string(document_id.value()));
    }

    reapply_styles(*tree);
    tree->styles_dirty = false;
    return {};
}
std::expected<void, std::string> Runtime::Impl::refresh_document_if_dirty(Runtime *runtime, DocumentId document_id)
{
    auto *tree = resolve_tree(document_id);
    if (tree == nullptr) {
        return std::unexpected("Document not loaded: " + std::to_string(document_id.value()));
    }
    if (tree->environment_dirty) {
        return refresh_document_environment(runtime, document_id);
    }
    return {};
}
}
