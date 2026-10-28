/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "private/runtime_impl.hpp"

namespace esp_brookesia::gui {

void Runtime::Impl::merge_style_layer(SparseStyle &destination, const SparseStyle &source)
{
    destination.merge_from(source);
}
void Runtime::Impl::merge_style_set_layer(SparseStyleSet &destination, const SparseStyleSet &source)
{
    merge_style_layer(destination.style, source.style);
    for (const auto &[state_name, state_style] : source.state_styles) {
        merge_style_layer(destination.state_styles[state_name], state_style);
    }
    for (const auto &[part_name, part_style] : source.part_styles) {
        merge_style_layer(destination.part_styles[part_name].style, part_style.style);
        for (const auto &[state_name, state_style] : part_style.state_styles) {
            merge_style_layer(destination.part_styles[part_name].state_styles[state_name], state_style);
        }
    }
}
SparseStyleSet Runtime::Impl::compose_style_set(const TreeRecord &tree, const SparseNode &node) const
{
    SparseStyleSet composed_style;

    const auto &builtin_theme = get_builtin_default_theme();
    if (auto all_style_it = builtin_theme.styles.find("all"); all_style_it != builtin_theme.styles.end()) {
        merge_style_set_layer(composed_style, all_style_it->second);
    }
    const auto builtin_style_key = get_theme_style_key(node.type);
    if (auto subtype_style_it = builtin_theme.styles.find(builtin_style_key);
            subtype_style_it != builtin_theme.styles.end()) {
        merge_style_set_layer(composed_style, subtype_style_it->second);
    }

    const auto &theme_id = tree.environment.theme_id;
    auto theme_it = global_themes.find(theme_id);
    if (theme_it != global_themes.end()) {
        auto all_style_it = theme_it->second.styles.find("all");
        if (all_style_it != theme_it->second.styles.end()) {
            merge_style_set_layer(composed_style, all_style_it->second);
        }
        const auto style_key = get_theme_style_key(node.type);
        auto subtype_style_it = theme_it->second.styles.find(style_key);
        if (subtype_style_it != theme_it->second.styles.end()) {
            merge_style_set_layer(composed_style, subtype_style_it->second);
        }
    }

    for (const auto &style_ref : node.style_refs) {
        auto local_style_it = tree.styles.find(style_ref);
        if (local_style_it != tree.styles.end()) {
            merge_style_set_layer(composed_style, local_style_it->second);
            continue;
        }
        if (theme_it != global_themes.end()) {
            auto named_style_it = theme_it->second.styles.find(style_ref);
            if (named_style_it != theme_it->second.styles.end()) {
                merge_style_set_layer(composed_style, named_style_it->second);
                continue;
            }
        }
        BROOKESIA_LOGW(
            "Document %1% and theme '%2%' do not contain named style ref: %3%",
            tree.document_id,
            theme_id,
            style_ref
        );
    }

    merge_style_layer(composed_style.style, node.style);
    for (const auto &[state_name, state_style] : node.state_styles) {
        merge_style_layer(composed_style.state_styles[state_name], state_style);
    }
    for (const auto &[part_name, part_style] : node.part_styles) {
        merge_style_layer(composed_style.part_styles[part_name].style, part_style.style);
        for (const auto &[state_name, state_style] : part_style.state_styles) {
            merge_style_layer(composed_style.part_styles[part_name].state_styles[state_name], state_style);
        }
    }
    return composed_style;
}
std::string Runtime::Impl::resolve_keyboard_key_color(
    const TreeRecord &tree,
    const std::string &color,
    std::string_view field_name) const
{
    auto resolved = resolve_color_binding_value(tree, color);
    if (!resolved) {
        BROOKESIA_LOGW(
            "Failed to resolve keyboard key color field '%1%' value '%2%': %3%",
            field_name,
            color,
            resolved.error()
        );
        return color;
    }
    return *resolved;
}
std::optional<KeyboardKeyStyle> Runtime::Impl::resolve_keyboard_key_style_ref(
    const TreeRecord &tree,
    std::string_view style_ref) const
{
    const SparseStyleSet *style_set = nullptr;
    auto local_style_it = tree.styles.find(std::string(style_ref));
    if (local_style_it != tree.styles.end()) {
        style_set = &local_style_it->second;
    }

    const auto &theme_id = tree.environment.theme_id;
    auto theme_it = global_themes.find(theme_id);
    if (style_set == nullptr && theme_it == global_themes.end()) {
        BROOKESIA_LOGD(
            "Skip keyboard key style ref '%1%' until current theme '%2%' is registered",
            style_ref,
            theme_id
        );
        return std::nullopt;
    }

    if (style_set == nullptr) {
        auto style_it = theme_it->second.styles.find(std::string(style_ref));
        if (style_it == theme_it->second.styles.end()) {
            BROOKESIA_LOGW(
                "Document %1% and theme '%2%' do not contain keyboard key style ref: %3%",
                tree.document_id,
                theme_id,
                style_ref
            );
            return std::nullopt;
        }
        style_set = &style_it->second;
    }

    const auto pressed_it = style_set->state_styles.find("pressed");
    const auto *pressed_style = pressed_it != style_set->state_styles.end() ? &pressed_it->second : nullptr;

    KeyboardKeyStyle key_style;
    if (style_set->style.bg_color().has_value()) {
        key_style.bg_color = resolve_keyboard_key_color(tree, *style_set->style.bg_color(), "bgColor");
    }
    if (style_set->style.text_color().has_value()) {
        key_style.text_color = resolve_keyboard_key_color(tree, *style_set->style.text_color(), "textColor");
    }
    if (pressed_style != nullptr) {
        if (pressed_style->bg_color().has_value()) {
            key_style.pressed_bg_color =
                resolve_keyboard_key_color(tree, *pressed_style->bg_color(), "pressed.bgColor");
        }
        if (pressed_style->text_color().has_value()) {
            key_style.pressed_text_color =
                resolve_keyboard_key_color(tree, *pressed_style->text_color(), "pressed.textColor");
        }
    }
    if (style_set->style.radius().has_value()) {
        key_style.radius = std::max<int32_t>(0, *style_set->style.radius());
    }
    return key_style;
}
void Runtime::Impl::resolve_keyboard_key_style_refs(const TreeRecord &tree, KeyboardProps &props) const
{
    auto resolved_styles = props.key_styles;
    for (const auto &[style_class, style_ref] : props.key_style_refs) {
        auto resolved = resolve_keyboard_key_style_ref(tree, style_ref);
        if (resolved.has_value()) {
            resolved_styles.insert_or_assign(style_class, std::move(*resolved));
        }
    }
    props.resolved_key_styles = std::move(resolved_styles);
}
void Runtime::Impl::resolve_keyboard_key_style_refs(const TreeRecord &tree, SparseNode &node) const
{
    if (node.type == NodeType::Keyboard) {
        resolve_keyboard_key_style_refs(tree, node.keyboard_props);
    }
}
std::expected<void, std::string> Runtime::Impl::validate_node_resource_references(
    const TreeRecord &tree, const SparseNode &node, const std::string &absolute_path) const
{
    if (node.style.font().has_value() && !node.style.font()->empty() &&
            !has_font_resource(tree, *node.style.font()) &&
            !is_builtin_default_font_id(*node.style.font())) {
        return std::unexpected(
                   "Node '" + absolute_path + "' references missing font resource: " + *node.style.font()
               );
    }
    if (node.type == NodeType::Image && !node.image_props.src.empty() &&
            !has_image_resource(tree, node.image_props.src)) {
        return std::unexpected(
                   "Node '" + absolute_path + "' references missing image resource: " + node.image_props.src
               );
    }
    if (node.type == NodeType::Keyboard) {
        for (const auto &[unused_mode, layout] : node.keyboard_props.layouts) {
            (void)unused_mode;
            for (const auto &row : layout.rows) {
                for (const auto &key : row) {
                    if (!key.image.empty() && !has_image_resource(tree, key.image)) {
                        return std::unexpected(
                                   "Node '" + absolute_path +
                                   "' references missing keyboard key image resource: " + key.image
                               );
                    }
                }
            }
        }
    }

    for (const auto &child : node.children) {
        auto child_path = absolute_path + "/" + child.id;
        auto child_validation = validate_node_resource_references(tree, child, child_path);
        if (!child_validation) {
            return child_validation;
        }
    }
    return {};
}
std::expected<void, std::string> Runtime::Impl::validate_resource_references(const TreeRecord &tree) const
{
    const auto &builtin_theme = get_builtin_default_theme();
    for (const auto &[style_key, style_set] : builtin_theme.styles) {
        if (style_set.style.font().has_value() && !style_set.style.font()->empty() &&
                !has_font_resource(tree, *style_set.style.font()) &&
                !is_builtin_default_font_id(*style_set.style.font())) {
            return std::unexpected(
                       "Built-in default theme style '" + style_key +
                       "' references missing font resource: " + *style_set.style.font()
                   );
        }
    }
    for (const auto &[theme_id, theme] : global_themes) {
        for (const auto &[style_key, style_set] : theme.styles) {
            if (style_set.style.font().has_value() && !style_set.style.font()->empty() &&
                    !has_font_resource(tree, *style_set.style.font()) &&
                    !is_builtin_default_font_id(*style_set.style.font())) {
                std::string message = "Theme '";
                message += theme_id;
                message += "' style '";
                message += style_key;
                message += "' references missing font resource: ";
                message += *style_set.style.font();
                return std::unexpected(message);
            }
        }
    }
    for (const auto &[screen_id, screen] : tree.screens) {
        auto validation = validate_node_resource_references(tree, screen, "/" + screen_id);
        if (!validation) {
            return validation;
        }
    }
    for (const auto &[template_id, template_node] : tree.templates) {
        auto validation = validate_node_resource_references(tree, template_node, "<template:" + template_id + ">");
        if (!validation) {
            return validation;
        }
    }
    return {};
}
SparseResolvedStyle Runtime::Impl::resolve_style(const TreeRecord &tree, const SparseNode &node) const
{
    SparseResolvedStyle resolved_style;
    auto style_set = compose_style_set(tree, node);
    resolve_style_set_color_fields(tree, style_set);
    resolved_style.style = style_set.style;
    resolved_style.state_styles = style_set.state_styles;
    resolved_style.part_styles = style_set.part_styles;

    struct FontDescriptor {
        std::string id;
        std::string kind;
        std::string primary_src;
        std::vector<std::string> fallback_ids;
        std::vector<NativeFontVariant> native_fonts;
        int32_t image_font_height = 0;
        std::vector<ImageFontGlyph> image_font_glyphs;
        std::vector<ImageFontSize> image_font_sizes;
    };

    auto find_font_descriptor = [&](std::string_view font_id, bool log_missing = true) -> std::optional<FontDescriptor> {
        auto dynamic_font_it = global_fonts.find(std::string(font_id));
        if (dynamic_font_it != global_fonts.end())
        {
            BROOKESIA_LOGD("Resolved font from dynamic backend resource: id='%1%', primary_src='%2%', fallback_count=%3%",
                           dynamic_font_it->second.id,
                           dynamic_font_it->second.primary_src,
                           dynamic_font_it->second.fallback_ids.size());
            return FontDescriptor {
                .id = dynamic_font_it->second.id,
                .kind = dynamic_font_it->second.kind,
                .primary_src = dynamic_font_it->second.primary_src,
                .fallback_ids = dynamic_font_it->second.fallback_ids,
                .native_fonts = dynamic_font_it->second.native_fonts,
                .image_font_height = dynamic_font_it->second.image_font_height,
                .image_font_glyphs = dynamic_font_it->second.image_font_glyphs,
                .image_font_sizes = dynamic_font_it->second.image_font_sizes,
            };
        }

        if (log_missing)
        {
            BROOKESIA_LOGW("Unable to resolve font id: '%1%'", font_id);
        }
        return std::nullopt;
    };

    auto resolve_font_chain = [&](std::string_view requested_font_id, bool allow_builtin_fallback = false) {
        const std::string font_id = requested_font_id.empty() ? std::string("default") : std::string(requested_font_id);
        auto font = find_font_descriptor(font_id, !allow_builtin_fallback || font_id != "default");
        if (!font.has_value()) {
            if (allow_builtin_fallback && font_id == "default") {
                BROOKESIA_LOGD(
                    "No runtime/json 'default' font resource found; fallback to backend built-in font"
                );
                return;
            }
            BROOKESIA_LOGW("Font chain resolution failed: requested_font_id='%1%'", font_id);
            return;
        }

        resolved_style.resolved_font.font_id = font->id;
        resolved_style.resolved_font.kind = font->kind;
        resolved_style.resolved_font.primary_src = font->primary_src;
        resolved_style.resolved_font.native_fonts = font->native_fonts;
        resolved_style.resolved_font.image_font_height = font->image_font_height;
        resolved_style.resolved_font.image_font_glyphs = font->image_font_glyphs;
        resolved_style.resolved_font.image_font_sizes = font->image_font_sizes;

        boost::unordered_flat_set<std::string> visited_font_ids;
        std::vector<std::string> pending_font_ids(font->fallback_ids.begin(), font->fallback_ids.end());
        while (!pending_font_ids.empty()) {
            const auto current_font_id = pending_font_ids.front();
            pending_font_ids.erase(pending_font_ids.begin());
            if (!visited_font_ids.insert(current_font_id).second) {
                continue;
            }
            auto fallback_font = find_font_descriptor(current_font_id);
            if (!fallback_font.has_value()) {
                BROOKESIA_LOGW("Skipping unresolved fallback font id: '%1%'", current_font_id);
                continue;
            }
            resolved_style.resolved_font.fallback_srcs.push_back(fallback_font->primary_src);
            for (const auto &nested_fallback : fallback_font->fallback_ids) {
                pending_font_ids.push_back(nested_fallback);
            }
        }

        BROOKESIA_LOGD("Resolved font chain: requested_font_id='%1%', resolved_font_id='%2%', primary_src='%3%', fallback_count=%4%, native_count=%5%",
                       font_id,
                       resolved_style.resolved_font.font_id,
                       resolved_style.resolved_font.primary_src,
                       resolved_style.resolved_font.fallback_srcs.size(),
                       resolved_style.resolved_font.native_fonts.size());
    };

    if (resolved_style.style.font().has_value() && !resolved_style.style.font()->empty()) {
        resolve_font_chain(
            *resolved_style.style.font(),
            is_builtin_default_font_id(*resolved_style.style.font())
        );
        return resolved_style;
    }

    auto default_font_it = default_fonts_by_language.find(tree.environment.language);
    if (default_font_it != default_fonts_by_language.end() && !default_font_it->second.empty()) {
        resolve_font_chain(default_font_it->second, true);
        if (!resolved_style.resolved_font.font_id.empty() || !resolved_style.resolved_font.primary_src.empty() ||
                !resolved_style.resolved_font.native_fonts.empty()) {
            return resolved_style;
        }
    }

    resolve_font_chain("default", true);
    return resolved_style;
}
std::shared_ptr<const SparseResolvedStyle> Runtime::Impl::intern_resolved_style(
    TreeRecord &tree,
    SparseResolvedStyle resolved_style)
{
    bool reused = false;
    auto result = intern_sparse_resolved_style(tree.resolved_style_intern_cache, std::move(resolved_style), &reused);
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
    if (reused) {
        ++dbg_style_intern_hits_;
    } else {
        ++dbg_style_intern_misses_;
    }
#endif
    return result;
}
std::shared_ptr<const SparseResolvedStyle> Runtime::Impl::resolve_style_shared(
    TreeRecord &tree,
    const SparseNode &node,
    const SparseNode *canonical_definition )
{
    if (style_cache_revision_ != current_style_revision_) {
        for (auto &[unused_document_id, cached_tree] : trees) {
            (void)unused_document_id;
            cached_tree.definition_style_cache.clear();
            cached_tree.resolved_style_intern_cache.clear();
        }
        // During load, `tree` has not necessarily been inserted into `trees` yet.
        tree.definition_style_cache.clear();
        tree.resolved_style_intern_cache.clear();
        style_cache_revision_ = current_style_revision_;
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
        dbg_style_cache_hits_ = 0;
        dbg_style_cache_misses_ = 0;
        dbg_style_intern_hits_ = 0;
        dbg_style_intern_misses_ = 0;
#endif
    }

    if (canonical_definition != nullptr) {
        auto cached_it = tree.definition_style_cache.find(canonical_definition);
        if (cached_it != tree.definition_style_cache.end()) {
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
            ++dbg_style_cache_hits_;
#endif
            return cached_it->second;
        }

        auto resolved = intern_resolved_style(tree, resolve_style(tree, node));
        tree.definition_style_cache.emplace(canonical_definition, resolved);
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
        ++dbg_style_cache_misses_;
#endif
        return resolved;
    }

    return intern_resolved_style(tree, resolve_style(tree, node));
}
void Runtime::Impl::refresh_global_fonts_from_backend()
{
    if (backend == nullptr) {
        return;
    }

    for (const auto &font_resource : backend->list_font_resources()) {
        if (unregistered_global_fonts.contains(font_resource.id)) {
            continue;
        }
        global_fonts.insert_or_assign(font_resource.id, font_resource);
        BROOKESIA_LOGD("Registered dynamic runtime font: id='%1%', primary_src='%2%', fallback_count=%3%, native_count=%4%",
                       font_resource.id,
                       font_resource.primary_src,
                       font_resource.fallback_ids.size(),
                       font_resource.native_fonts.size());
    }
}
void Runtime::Impl::dispatch_backend_event(const BackendEvent &event)
{
    for (auto &[unused_document_id, tree] : trees) {
        (void)unused_document_id;
        auto uid_it = tree.handle_to_uid.find(event.handle.value());
        if (uid_it == tree.handle_to_uid.end()) {
            continue;
        }

        auto *record = find_node_record(tree, uid_it->second);
        if (record == nullptr) {
            return;
        }
        const auto *root_record = find_root_node_record(tree, *record);

        Event action_event{
            .document_id = tree.document_id,
            .root_id = root_record == nullptr ? std::string() : std::string(root_record->node_id()),
            .node_id = std::string(record->node_id()),
            .path = record->absolute_path,
            .type = event.type,
            .action = event.action,
            .payload = event.payload,
        };
        if (event.type == EventType::Pressed) {
            record->press_lost_since_pressed = false;
        } else if (event.type == EventType::PressLost) {
            record->press_lost_since_pressed = true;
        }
        if (event.type == EventType::ValueChanged) {
            const auto node_type = record->node().type;
            if (node_type == NodeType::TextInput) {
                if (auto text = action_event.get_string("text"); text.has_value() &&
                        record->text_input_props().text != *text) {
                    record->mutable_text_input_props().text = std::string(*text);
                }
            } else if (node_type == NodeType::Slider || node_type == NodeType::ProgressBar ||
                       node_type == NodeType::Arc) {
                if (auto value = action_event.get_int64("value"); value.has_value() &&
                        record->range_props().value != static_cast<int32_t>(*value)) {
                    record->mutable_range_props().value = static_cast<int32_t>(*value);
                }
            } else if (node_type == NodeType::Switch || node_type == NodeType::Checkbox) {
                if (auto checked = action_event.get_bool("checked"); checked.has_value() &&
                        record->toggle_props().checked != *checked) {
                    record->mutable_toggle_props().checked = *checked;
                }
            } else if (node_type == NodeType::Dropdown) {
                if (auto selected_index = action_event.get_int64("selectedIndex"); selected_index.has_value() &&
                        record->dropdown_props().selected_index != static_cast<int32_t>(*selected_index)) {
                    record->mutable_dropdown_props().selected_index = static_cast<int32_t>(*selected_index);
                }
            }
        }
        auto interaction_state = find_interaction_state(tree, uid_it->second);
        const bool emit_click = record->node().type == NodeType::Button && event.type == EventType::Clicked;
        std::vector<Event> deferred_actions;
        execute_event_effects(tree, *record, action_event, deferred_actions);
        for (const auto &deferred_action : deferred_actions) {
            (void)dispatch_event_action_handlers(deferred_action);
        }
        (void)dispatch_event_action_handlers(action_event);

        auto source_is_loaded = [this, document_id = action_event.document_id, handle = event.handle]() {
            const auto *current_tree = resolve_tree_const(document_id);
            return current_tree != nullptr && current_tree->handle_to_uid.contains(handle.value());
        };
        if (interaction_state != nullptr && source_is_loaded()) {
            interaction_state->event_signal(action_event);
            if (emit_click && source_is_loaded()) {
                interaction_state->click_signal();
            }
        }
        return;
    }
}
std::optional<Runtime::Impl::NodeUid> Runtime::Impl::resolve_any_uid(const TreeRecord &tree, std::string_view id) const
{
    const auto query = normalize_absolute_path(id);
    return resolve_normalized_uid(tree, query);
}
std::optional<Runtime::Impl::NodeUid> Runtime::Impl::resolve_normalized_uid(
    const TreeRecord &tree,
    std::string_view normalized_absolute_path)
{
    if (normalized_absolute_path == "/") {
        return std::nullopt;
    }

    if (auto absolute_it = tree.absolute_path_to_uid.find(normalized_absolute_path);
            absolute_it != tree.absolute_path_to_uid.end()) {
        return absolute_it->second;
    }
    return std::nullopt;
}
Runtime::Impl::TreeRecord *Runtime::Impl::resolve_tree(DocumentId document_id)
{
    auto it = trees.find(document_id.value());
    return it == trees.end() ? nullptr : &it->second;
}
const Runtime::Impl::TreeRecord *Runtime::Impl::resolve_tree_const(DocumentId document_id) const
{
    auto it = trees.find(document_id.value());
    return it == trees.end() ? nullptr : &it->second;
}
Runtime::Impl::NodeRecord *Runtime::Impl::find_node_record(TreeRecord &tree, NodeUid uid)
{
    auto it = tree.nodes.find(uid);
    return it == tree.nodes.end() ? nullptr : &it->second;
}
const Runtime::Impl::NodeRecord *Runtime::Impl::find_node_record_const(const TreeRecord &tree, NodeUid uid)
{
    auto it = tree.nodes.find(uid);
    return it == tree.nodes.end() ? nullptr : &it->second;
}
std::optional<Runtime::Impl::NodeUid> Runtime::Impl::resolve_view_uid(const View &view) const
{
    const auto *tree = resolve_tree_const(view.document_id_);
    if (tree == nullptr) {
        return std::nullopt;
    }

    return resolve_any_uid(*tree, view.absolute_path_);
}
Runtime::Impl::NodeRecord *Runtime::Impl::resolve_view_record(const View &view)
{
    auto *tree = resolve_tree(view.document_id_);
    auto uid = resolve_view_uid(view);
    if (tree == nullptr || !uid.has_value()) {
        return nullptr;
    }
    return find_node_record(*tree, *uid);
}
Runtime::Impl::NodeRecord *Runtime::Impl::resolve_view_record(const View &view) const
{
    auto *tree = resolve_tree_const(view.document_id_);
    auto uid = resolve_view_uid(view);
    if (tree == nullptr || !uid.has_value()) {
        return nullptr;
    }
    return const_cast<NodeRecord *>(find_node_record_const(*tree, *uid));
}
}
