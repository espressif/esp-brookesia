/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "private/runtime_impl.hpp"

namespace esp_brookesia::gui {

std::vector<GuiLayer> Runtime::Impl::list_layers() const
{
    return backend == nullptr ? std::vector<GuiLayer> {} : backend->list_layers();
}
std::vector<GuiDisplayInfo> Runtime::Impl::list_displays() const
{
    return backend == nullptr ? std::vector<GuiDisplayInfo> {} : backend->list_displays();
}
std::optional<std::string> Runtime::Impl::resolve_default_display_id() const
{
    const auto displays = list_displays();
    if (displays.empty()) {
        return std::nullopt;
    }

    for (const auto &display : displays) {
        if (display.is_default) {
            return display.id;
        }
    }

    return displays.front().id;
}
std::expected<MountTarget, std::string> Runtime::Impl::normalize_mount_target(const MountTarget &target) const
{
    auto normalized = target;
    if (normalized.display_id.empty()) {
        auto default_display_id = resolve_default_display_id();
        if (!default_display_id) {
            return std::unexpected("No GUI display registered");
        }
        normalized.display_id = *default_display_id;
        return normalized;
    }

    const auto displays = list_displays();
    const auto found = std::find_if(displays.begin(), displays.end(), [&](const GuiDisplayInfo & display) {
        return display.id == normalized.display_id;
    });
    if (found == displays.end()) {
        return std::unexpected("Display not found: " + normalized.display_id);
    }
    return normalized;
}
std::string Runtime::Impl::make_mounted_screen_key(
    DocumentId document_id,
    std::string_view absolute_path,
    const MountTarget &target) const
{
    if (target.mount_mode == MountStackMode::Stack) {
        return build_stack_mount_target_key(target.display_id, target.layer, document_id, absolute_path);
    }
    return build_replace_mount_target_key(target.display_id, target.layer);
}
void Runtime::Impl::reorder_mounted_screens(const MountTarget &target)
{
    if (backend == nullptr) {
        return;
    }

    std::vector<MountedScreenRef> layer_refs;
    for (const auto &[unused_key, mounted_ref] : mounted_screens_) {
        (void)unused_key;
        if (same_mount_layer(mounted_ref.target, target)) {
            layer_refs.push_back(mounted_ref);
        }
    }
    if (layer_refs.size() < 2) {
        return;
    }
    std::sort(layer_refs.begin(), layer_refs.end(), [](const MountedScreenRef & lhs, const MountedScreenRef & rhs) {
        if (lhs.target.z_order != rhs.target.z_order) {
            return lhs.target.z_order < rhs.target.z_order;
        }
        return lhs.sequence < rhs.sequence;
    });

    for (const auto &mounted_ref : layer_refs) {
        auto *tree = resolve_tree(mounted_ref.document_id);
        if (tree == nullptr) {
            continue;
        }
        const auto screen_id = trim_slashes(mounted_ref.absolute_path);
        auto screen_root_it = tree->screen_roots.find(screen_id);
        if (screen_root_it == tree->screen_roots.end()) {
            continue;
        }
        auto *record = find_node_record(*tree, screen_root_it->second);
        if (record == nullptr) {
            continue;
        }
        (void)backend->mount_screen(record->handle, mounted_ref.target);
    }
}
void Runtime::Impl::remove_replace_screen_on_layer(const MountTarget &target)
{
    const auto replace_key = build_replace_mount_target_key(target.display_id, target.layer);
    auto mounted_it = mounted_screens_.find(replace_key);
    if (mounted_it == mounted_screens_.end()) {
        return;
    }
    (void)unmount_screen(mounted_it->second.document_id, mounted_it->second.absolute_path);
}
void Runtime::Impl::remove_duplicate_stack_screen(DocumentId document_id, std::string_view absolute_path, const MountTarget &target)
{
    if (target.mount_mode != MountStackMode::Stack) {
        return;
    }
    std::vector<std::string> duplicate_keys;
    for (const auto &[target_key, mounted_ref] : mounted_screens_) {
        if (mounted_ref.target.mount_mode == MountStackMode::Stack &&
                mounted_ref.document_id == document_id &&
                mounted_ref.absolute_path == absolute_path &&
                same_mount_layer(mounted_ref.target, target)) {
            duplicate_keys.push_back(target_key);
        }
    }
    for (const auto &target_key : duplicate_keys) {
        auto mounted_it = mounted_screens_.find(target_key);
        if (mounted_it == mounted_screens_.end()) {
            continue;
        }
        (void)unmount_screen(mounted_it->second.document_id, mounted_it->second.absolute_path);
    }
}
std::expected<View, std::string> Runtime::Impl::mount_screen(
    Runtime *runtime,
    DocumentId document_id,
    std::string_view absolute_path,
    const MountTarget &target)
{
    const auto normalized_path = normalize_absolute_path(absolute_path);
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

    const auto screen_id = trim_slashes(normalized_path);
    if (screen_id.empty() || screen_id.find('/') != std::string::npos) {
        return std::unexpected("Screen absolute path must be in the form '/screen_id'");
    }

    auto screen_def_it = tree->screens.find(screen_id);
    if (screen_def_it == tree->screens.end()) {
        return std::unexpected("Screen not found: " + normalized_path);
    }

    auto root_uid = ensure_screen_built(document_id, *tree, screen_id);
    if (!root_uid) {
        return std::unexpected(root_uid.error());
    }
    (void)root_uid;
    auto screen_root_it = tree->screen_roots.find(screen_id);
    if (screen_root_it == tree->screen_roots.end()) {
        return std::unexpected("Screen root not found: " + normalized_path);
    }

    auto normalized_target = normalize_mount_target(target);
    if (!normalized_target) {
        return std::unexpected(normalized_target.error());
    }

    if (normalized_target->mount_mode == MountStackMode::Replace) {
        remove_replace_screen_on_layer(*normalized_target);
    } else {
        remove_duplicate_stack_screen(document_id, normalized_path, *normalized_target);
    }

    if (tree->styles_dirty) {
        (void)reapply_subtree_styles(*tree, screen_root_it->second);
    }

    auto *record = find_node_record(*tree, screen_root_it->second);
    if (record == nullptr || !backend->mount_screen(record->handle, *normalized_target)) {
        return std::unexpected("Failed to mount screen: " + normalized_path);
    }

    const auto target_key = make_mounted_screen_key(document_id, normalized_path, *normalized_target);
    mounted_screens_.insert_or_assign(target_key, MountedScreenRef{
        .document_id = document_id,
        .absolute_path = normalized_path,
        .target = *normalized_target,
        .sequence = next_mounted_screen_sequence_++,
    });
    reorder_mounted_screens(*normalized_target);
    return View(runtime, document_id, normalized_path, NodeType::Screen);
}
bool Runtime::Impl::unmount_screen(DocumentId document_id, std::string_view absolute_path)
{
    if (!document_id.is_valid()) {
        return false;
    }

    const auto normalized_path = normalize_absolute_path(absolute_path);
    std::vector<std::pair<std::string, MountTarget>> mounted_targets;
    for (const auto &[target_key, mounted_ref] : mounted_screens_) {
        if (mounted_ref.document_id == document_id && mounted_ref.absolute_path == normalized_path) {
            mounted_targets.emplace_back(target_key, mounted_ref.target);
        }
    }
    if (mounted_targets.empty()) {
        return false;
    }
    auto *tree = resolve_tree(document_id);
    if (tree == nullptr) {
        for (const auto &[target_key, unused_target] : mounted_targets) {
            (void)unused_target;
            mounted_screens_.erase(target_key);
        }
        return false;
    }
    const bool result = unmount_mounted_screen(*tree, normalized_path);
    for (const auto &[target_key, unused_target] : mounted_targets) {
        (void)unused_target;
        mounted_screens_.erase(target_key);
    }
    for (const auto &[unused_key, target] : mounted_targets) {
        (void)unused_key;
        reorder_mounted_screens(target);
    }
    return result;
}
std::expected<TransientMountId, std::string> Runtime::Impl::push_transient_screen(
    Runtime *runtime,
    DocumentId document_id,
    std::string_view absolute_path,
    const MountTarget &target)
{
    const auto normalized_path = normalize_absolute_path(absolute_path);
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

    const auto screen_id = trim_slashes(normalized_path);
    if (screen_id.empty() || screen_id.find('/') != std::string::npos) {
        return std::unexpected("Screen absolute path must be in the form '/screen_id'");
    }

    auto screen_def_it = tree->screens.find(screen_id);
    if (screen_def_it == tree->screens.end()) {
        return std::unexpected("Screen not found: " + normalized_path);
    }

    auto root_uid = ensure_screen_built(document_id, *tree, screen_id);
    if (!root_uid) {
        return std::unexpected(root_uid.error());
    }
    (void)root_uid;
    auto screen_root_it = tree->screen_roots.find(screen_id);
    if (screen_root_it == tree->screen_roots.end()) {
        return std::unexpected("Screen root not found: " + normalized_path);
    }

    auto normalized_target = normalize_mount_target(target);
    if (!normalized_target) {
        return std::unexpected(normalized_target.error());
    }

    if (tree->styles_dirty) {
        (void)reapply_subtree_styles(*tree, screen_root_it->second);
    }

    auto *record = find_node_record(*tree, screen_root_it->second);
    if (record == nullptr || !backend->mount_screen(record->handle, *normalized_target)) {
        return std::unexpected("Failed to push transient screen: " + normalized_path);
    }

    const auto transient_id = TransientMountId(next_transient_mount_id_++);
    transient_screens_.insert_or_assign(transient_id.value(), TransientScreenRef{
        .document_id = document_id,
        .absolute_path = normalized_path,
        .target = *normalized_target,
    });
    return transient_id;
}
bool Runtime::Impl::pop_transient_screen(TransientMountId id)
{
    auto transient_it = transient_screens_.find(id.value());
    if (transient_it == transient_screens_.end()) {
        return false;
    }

    auto *tree = resolve_tree(transient_it->second.document_id);
    const auto absolute_path = transient_it->second.absolute_path;
    transient_screens_.erase(transient_it);
    if (tree == nullptr) {
        return false;
    }
    return unmount_mounted_screen(*tree, absolute_path);
}
void Runtime::Impl::pop_transient_screens_for_document(DocumentId document_id)
{
    std::vector<TransientMountId> transient_ids;
    for (const auto &[id, transient] : transient_screens_) {
        if (transient.document_id == document_id) {
            transient_ids.push_back(TransientMountId(id));
        }
    }
    for (auto id : transient_ids) {
        (void)pop_transient_screen(id);
    }
}
bool Runtime::Impl::is_screen_flow_screen_mounted(const RunningScreenFlow &flow) const
{
    auto normalized_target = normalize_mount_target(flow.target);
    if (!normalized_target) {
        return false;
    }

    return std::any_of(mounted_screens_.begin(), mounted_screens_.end(), [&](const auto & entry) {
        const auto &mounted_ref = entry.second;
        return mounted_ref.document_id == flow.document_id &&
               mounted_ref.absolute_path == flow.current_screen &&
               same_mount_layer(mounted_ref.target, *normalized_target) &&
               mounted_ref.target.mount_mode == normalized_target->mount_mode;
    });
}
std::expected<void, std::string> Runtime::Impl::start_screen_flow(
    Runtime *runtime,
    DocumentId document_id,
    std::string_view flow_id,
    const MountTarget &target)
{
    auto *tree = resolve_tree(document_id);
    if (tree == nullptr) {
        return std::unexpected("GUI document not loaded");
    }
    auto flow_it = tree->screen_flows.find(std::string(flow_id));
    if (flow_it == tree->screen_flows.end()) {
        return std::unexpected("Screen flow not found: " + std::string(flow_id));
    }

    const auto flow_key = build_screen_flow_key(document_id, flow_id);
    if (running_screen_flows_.contains(flow_key)) {
        return {};
    }

    const auto initial_screen = make_screen_flow_screen_path(flow_it->second.initial);
    auto mount_result = mount_screen(runtime, document_id, initial_screen, target);
    if (!mount_result) {
        return std::unexpected(mount_result.error());
    }
    running_screen_flows_.insert_or_assign(flow_key, RunningScreenFlow{
        .document_id = document_id,
        .flow_id = std::string(flow_id),
        .current_state = flow_it->second.initial,
        .current_screen = initial_screen,
        .target = target,
    });
    return {};
}
std::expected<void, std::string> Runtime::Impl::trigger_screen_flow(
    Runtime *runtime,
    DocumentId document_id,
    std::string_view flow_id,
    std::string_view action)
{
    const auto flow_key = build_screen_flow_key(document_id, flow_id);
    auto running_it = running_screen_flows_.find(flow_key);
    if (running_it == running_screen_flows_.end()) {
        return std::unexpected("Screen flow is not running: " + std::string(flow_id));
    }
    auto *tree = resolve_tree(document_id);
    if (tree == nullptr) {
        running_screen_flows_.erase(running_it);
        return std::unexpected("GUI document not loaded");
    }
    auto flow_it = tree->screen_flows.find(std::string(flow_id));
    if (flow_it == tree->screen_flows.end()) {
        running_screen_flows_.erase(running_it);
        return std::unexpected("Screen flow not found: " + std::string(flow_id));
    }
    if (flow_it->second.transitions.empty()) {
        return std::unexpected("Screen flow has no transitions");
    }

    const auto *transition = find_screen_flow_transition(
                                 flow_it->second,
                                 running_it->second.current_state,
                                 action
                             );
    if (transition == nullptr) {
        return std::unexpected(
                   "Screen flow transition not found: state=" + running_it->second.current_state +
                   ", action=" + std::string(action)
               );
    }
    BROOKESIA_LOGI(
        "Screen flow transition: flow(%1%), from(%2%), action(%3%), to(%4%)",
        flow_id,
        running_it->second.current_state,
        action,
        transition->to
    );
    if (transition->to == running_it->second.current_state && is_screen_flow_screen_mounted(running_it->second)) {
        return {};
    }

    const auto old_screen = running_it->second.current_screen;
    const auto old_state = running_it->second.current_state;
    const auto target_screen = make_screen_flow_screen_path(transition->to);
    const auto target = running_it->second.target;
    (void)unmount_screen(document_id, old_screen);

    auto mount_result = mount_screen(runtime, document_id, target_screen, target);
    if (!mount_result) {
        auto restore_result = mount_screen(runtime, document_id, old_screen, target);
        if (restore_result) {
            running_it->second.current_state = old_state;
            running_it->second.current_screen = old_screen;
        }
        return std::unexpected(mount_result.error());
    }

    running_it->second.current_state = transition->to;
    running_it->second.current_screen = target_screen;
    return {};
}
bool Runtime::Impl::stop_screen_flow(DocumentId document_id, std::string_view flow_id)
{
    const auto flow_key = build_screen_flow_key(document_id, flow_id);
    auto running_it = running_screen_flows_.find(flow_key);
    if (running_it == running_screen_flows_.end()) {
        return false;
    }
    const auto current_screen = running_it->second.current_screen;
    running_screen_flows_.erase(running_it);
    (void)unmount_screen(document_id, current_screen);
    return true;
}
bool Runtime::Impl::has_screen_flow(DocumentId document_id, std::string_view flow_id) const
{
    const auto *tree = const_cast<Impl *>(this)->resolve_tree(document_id);
    return (tree != nullptr) && tree->screen_flows.contains(std::string(flow_id));
}
std::optional<std::string> Runtime::Impl::get_screen_flow_state(DocumentId document_id, std::string_view flow_id) const
{
    const auto flow_key = build_screen_flow_key(document_id, flow_id);
    auto running_it = running_screen_flows_.find(flow_key);
    if (running_it == running_screen_flows_.end()) {
        return std::nullopt;
    }
    return running_it->second.current_state;
}
void Runtime::Impl::stop_screen_flows_for_document(DocumentId document_id)
{
    std::vector<std::string> flow_ids;
    const auto prefix = std::to_string(document_id.value()) + '\x1f';
    for (const auto &[key, flow] : running_screen_flows_) {
        if (key.rfind(prefix, 0) == 0) {
            flow_ids.push_back(flow.flow_id);
        }
    }
    for (const auto &flow_id : flow_ids) {
        (void)stop_screen_flow(document_id, flow_id);
    }
}
}
