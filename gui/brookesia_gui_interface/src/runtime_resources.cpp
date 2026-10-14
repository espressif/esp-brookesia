/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "private/runtime_impl.hpp"

namespace esp_brookesia::gui {

std::expected<void, std::string> Runtime::Impl::load_theme(const ThemeAsset &theme)
{
    if (theme.id.empty()) {
        return std::unexpected("Theme id must not be empty");
    }
    const bool inserted = global_themes.emplace(theme.id, theme).second;
    if (!inserted) {
        global_themes.insert_or_assign(theme.id, theme);
    } else {
        theme_registration_order.push_back(theme.id);
    }
    if (theme.id == current_theme) {
        reapply_styles_for_all_trees();
    }
    return {};
}
std::vector<std::string> Runtime::Impl::list_supported_themes() const
{
    return theme_registration_order;
}
Environment Runtime::Impl::make_effective_environment(Environment environment) const
{
    if (environment.theme_id.empty()) {
        environment.theme_id = current_theme;
    }
    if (environment.language.empty()) {
        environment.language = current_language;
    }
    return environment;
}
Environment Runtime::Impl::make_parse_environment(Environment environment) const
{
    environment = make_effective_environment(std::move(environment));
    environment.colors.clear();
    auto theme_it = global_themes.find(environment.theme_id);
    if (theme_it != global_themes.end()) {
        environment.colors = theme_it->second.colors;
    }
    return environment;
}
std::expected<std::string, std::string> Runtime::Impl::resolve_color_binding_value(
    const TreeRecord &tree,
    std::string_view value) const
{
    auto color_path = parse_color_reference_path(value);
    if (!color_path.has_value()) {
        return std::string(value);
    }
    if (color_path->empty()) {
        return std::unexpected("Invalid color reference: " + std::string(value));
    }

    const auto &theme_id = tree.environment.theme_id;
    auto theme_it = global_themes.find(theme_id);
    if (theme_it == global_themes.end()) {
        return std::unexpected("Document theme is not registered: " + theme_id);
    }
    auto color_it = theme_it->second.colors.find(*color_path);
    if (color_it == theme_it->second.colors.end()) {
        return std::unexpected("Failed to resolve color reference: " + std::string(value));
    }
    return color_it->second;
}
void Runtime::Impl::resolve_style_color_field(
    const TreeRecord &tree,
    Style &style,
    std::optional<std::string> (Style::*getter)() const,
    void (Style::*setter)(std::optional<std::string>),
    std::string_view field_name) const
{
    auto field = (style.*getter)();
    if (!field.has_value()) {
        return;
    }
    auto resolved = resolve_color_binding_value(tree, *field);
    if (!resolved) {
        BROOKESIA_LOGW(
            "Failed to resolve style color field '%1%' value '%2%': %3%",
            field_name,
            *field,
            resolved.error()
        );
        return;
    }
    (style.*setter)(std::move(*resolved));
}
void Runtime::Impl::resolve_style_color_fields(const TreeRecord &tree, Style &style) const
{
    resolve_style_color_field(tree, style, &Style::bg_color, &Style::set_bg_color, "bgColor");
    resolve_style_color_field(
        tree, style, &Style::bg_gradient_color, &Style::set_bg_gradient_color, "bgGradientColor");
    resolve_style_color_field(tree, style, &Style::text_color, &Style::set_text_color, "textColor");
    resolve_style_color_field(tree, style, &Style::border_color, &Style::set_border_color, "borderColor");
    resolve_style_color_field(tree, style, &Style::line_color, &Style::set_line_color, "lineColor");
    resolve_style_color_field(tree, style, &Style::arc_color, &Style::set_arc_color, "arcColor");
    resolve_style_color_field(
        tree, style, &Style::arc_gradient_color, &Style::set_arc_gradient_color, "arcGradientColor");
    resolve_style_color_field(tree, style, &Style::shadow_color, &Style::set_shadow_color, "shadowColor");
    resolve_style_color_field(tree, style, &Style::image_recolor, &Style::set_image_recolor, "imageRecolor");
}
void Runtime::Impl::resolve_style_set_color_fields(const TreeRecord &tree, StyleSet &style_set) const
{
    resolve_style_color_fields(tree, style_set.style);
    for (auto &[unused_state, state_style] : style_set.state_styles) {
        (void)unused_state;
        resolve_style_color_fields(tree, state_style);
    }
    for (auto &[unused_part, part_style] : style_set.part_styles) {
        (void)unused_part;
        resolve_style_color_fields(tree, part_style.style);
        for (auto &[unused_state, state_style] : part_style.state_styles) {
            (void)unused_state;
            resolve_style_color_fields(tree, state_style);
        }
    }
}
std::expected<void, std::string> Runtime::Impl::set_theme(
    Runtime *runtime,
    std::string_view theme_id,
    bool reapply_loaded_documents)
{
    (void)runtime;
    (void)reapply_loaded_documents;
    const std::string normalized_theme_id(theme_id);
    if (normalized_theme_id.empty()) {
        return std::unexpected("Theme id must not be empty");
    }
    if (!global_themes.contains(normalized_theme_id)) {
        return std::unexpected("Theme not registered: " + normalized_theme_id);
    }
    current_theme = normalized_theme_id;
    BROOKESIA_LOGI(
        "Runtime theme configured: id='%1%', loaded_documents(%2%), applies_to(new documents only)",
        normalized_theme_id,
        trees.size()
    );
    return {};
}
std::expected<void, std::string> Runtime::Impl::set_theme(Runtime *runtime, std::string_view theme_id)
{
    return set_theme(runtime, theme_id, false);
}
std::expected<void, std::string> Runtime::Impl::reapply_styles(DocumentId id)
{
    return refresh_document_styles(id);
}
std::string Runtime::Impl::get_theme() const
{
    return current_theme;
}
std::expected<void, std::string> Runtime::Impl::register_font(const RuntimeFontResource &resource)
{
    if (resource.id.empty()) {
        return std::unexpected("Font id must not be empty");
    }
    if (resource.kind != "file" && resource.kind != "imageFont") {
        return std::unexpected("Font kind must be 'file' or 'imageFont': " + resource.id);
    }
    if (resource.kind == "file" && resource.primary_src.empty() && resource.native_fonts.empty()) {
        return std::unexpected("Font resource must define primary_src or native_fonts");
    }
    if (resource.kind == "file" && resource.languages.empty()) {
        return std::unexpected("Font languages must not be empty");
    }
    boost::unordered_flat_set<std::string> unique_languages;
    for (const auto &language : resource.languages) {
        if (language.empty()) {
            return std::unexpected("Font languages must not contain empty values");
        }
        if (!unique_languages.insert(language).second) {
            return std::unexpected("Font languages must not contain duplicates: " + language);
        }
    }
    if (resource.kind == "imageFont") {
        std::vector<ImageFontSize> image_font_sizes = resource.image_font_sizes;
        if (image_font_sizes.empty() && resource.image_font_height > 0 && !resource.image_font_glyphs.empty()) {
            image_font_sizes.push_back(ImageFontSize {
                .height = resource.image_font_height,
                .glyphs = resource.image_font_glyphs,
            });
        }
        if (image_font_sizes.empty()) {
            return std::unexpected("imageFont resource sizes must not be empty: " + resource.id);
        }
        boost::unordered_flat_set<int32_t> heights;
        std::optional<std::vector<uint32_t>> reference_codepoints;
        for (const auto &size : image_font_sizes) {
            if (size.height <= 0) {
                return std::unexpected("imageFont resource size height must be positive: " + resource.id);
            }
            if (size.glyphs.empty()) {
                return std::unexpected("imageFont resource size glyphs must not be empty: " + resource.id);
            }
            if (!heights.insert(size.height).second) {
                return std::unexpected("imageFont resource size heights must not contain duplicates: " + resource.id);
            }
            std::vector<uint32_t> size_codepoints;
            size_codepoints.reserve(size.glyphs.size());
            boost::unordered_flat_set<uint32_t> codepoints;
            for (const auto &glyph : size.glyphs) {
                if (glyph.codepoint == 0) {
                    return std::unexpected("imageFont resource glyph codepoint must not be zero: " + resource.id);
                }
                if (glyph.src.empty()) {
                    return std::unexpected("imageFont resource glyph src must not be empty: " + resource.id);
                }
                if (!codepoints.insert(glyph.codepoint).second) {
                    return std::unexpected("imageFont resource glyph codepoint duplicated: " + resource.id);
                }
                size_codepoints.push_back(glyph.codepoint);
            }
            std::sort(size_codepoints.begin(), size_codepoints.end());
            if (!reference_codepoints.has_value()) {
                reference_codepoints = std::move(size_codepoints);
            } else if (*reference_codepoints != size_codepoints) {
                return std::unexpected("imageFont resource sizes must contain the same glyph codepoints: " + resource.id);
            }
        }
        boost::unordered_flat_set<uint32_t> codepoints;
        for (const auto &glyph : resource.image_font_glyphs) {
            if (glyph.codepoint == 0) {
                return std::unexpected("imageFont resource glyph codepoint must not be zero: " + resource.id);
            }
            if (glyph.src.empty()) {
                return std::unexpected("imageFont resource glyph src must not be empty: " + resource.id);
            }
            if (!codepoints.insert(glyph.codepoint).second) {
                return std::unexpected("imageFont resource glyph codepoint duplicated: " + resource.id);
            }
        }
    }

    if (backend != nullptr && !resource.native_fonts.empty() && !backend->register_font_resource(resource)) {
        return std::unexpected("Failed to register native font resource in backend: " + resource.id);
    }

    unregistered_global_fonts.erase(resource.id);
    global_fonts.insert_or_assign(resource.id, resource);
    refresh_global_fonts_from_backend();
    if (std::find(font_registration_order.begin(), font_registration_order.end(), resource.id) == font_registration_order.end()) {
        font_registration_order.push_back(resource.id);
    }
    reapply_styles_for_all_trees();
    return {};
}
bool Runtime::Impl::unregister_font(std::string_view id)
{
    const std::string font_id(id);
    auto erased_count = global_fonts.erase(font_id);
    if (erased_count == 0) {
        return false;
    }
    unregistered_global_fonts.insert(font_id);
    std::erase(font_registration_order, font_id);
    for (auto it = default_fonts_by_language.begin(); it != default_fonts_by_language.end();) {
        if (it->second == font_id) {
            it = default_fonts_by_language.erase(it);
        } else {
            ++it;
        }
    }
    reapply_styles_for_all_trees();
    return true;
}
std::vector<std::string> Runtime::Impl::list_supported_fonts(std::string_view language ) const
{
    std::vector<std::string> font_ids;
    for (const auto &font_id : font_registration_order) {
        auto it = global_fonts.find(font_id);
        if (it == global_fonts.end()) {
            continue;
        }
        if (!language.empty() &&
                std::find(it->second.languages.begin(), it->second.languages.end(), language) == it->second.languages.end()) {
            continue;
        }
        font_ids.push_back(font_id);
    }
    return font_ids;
}
std::vector<std::string> Runtime::Impl::list_supported_languages() const
{
    std::vector<std::string> languages;
    boost::unordered_flat_set<std::string> seen;
    for (const auto &font_id : font_registration_order) {
        auto it = global_fonts.find(font_id);
        if (it == global_fonts.end()) {
            continue;
        }
        for (const auto &language : it->second.languages) {
            if (seen.insert(language).second) {
                languages.push_back(language);
            }
        }
    }
    return languages;
}
std::vector<std::string> Runtime::Impl::list_supported_languages(std::string_view font_id) const
{
    auto it = global_fonts.find(std::string(font_id));
    if (it == global_fonts.end()) {
        return {};
    }
    return it->second.languages;
}
std::expected<void, std::string> Runtime::Impl::set_language(
    Runtime *runtime,
    std::string_view language,
    bool reapply_loaded_documents)
{
    (void)runtime;
    (void)reapply_loaded_documents;
    const std::string normalized_language(language);
    if (normalized_language.empty()) {
        return std::unexpected("Language must not be empty");
    }
    const auto supported_languages = list_supported_languages();
    if (std::find(supported_languages.begin(), supported_languages.end(), normalized_language) == supported_languages.end()) {
        return std::unexpected("Language is not supported by any registered font: " + normalized_language);
    }
    current_language = normalized_language;
    BROOKESIA_LOGI(
        "Runtime language configured: language='%1%', loaded_documents(%2%), applies_to(new documents only)",
        normalized_language,
        trees.size()
    );
    return {};
}
std::expected<void, std::string> Runtime::Impl::set_language(Runtime *runtime, std::string_view language)
{
    return set_language(runtime, language, false);
}
std::string Runtime::Impl::get_language() const
{
    return current_language;
}
std::expected<void, std::string> Runtime::Impl::set_default_font_for_language(
    std::string_view language,
    std::string_view font_id)
{
    const std::string normalized_language(language);
    const std::string normalized_font_id(font_id);
    if (normalized_language.empty()) {
        return std::unexpected("Language must not be empty");
    }
    if (normalized_font_id.empty()) {
        return std::unexpected("Font id must not be empty");
    }
    auto font_it = global_fonts.find(normalized_font_id);
    if (font_it == global_fonts.end()) {
        return std::unexpected("Font not registered: " + normalized_font_id);
    }
    if (std::find(font_it->second.languages.begin(), font_it->second.languages.end(), normalized_language) ==
            font_it->second.languages.end()) {
        return std::unexpected(
                   "Font '" + normalized_font_id + "' does not support language '" + normalized_language + "'"
               );
    }
    default_fonts_by_language.insert_or_assign(normalized_language, normalized_font_id);
    if (normalized_language == current_language) {
        (void)reapply_mounted_styles_for_all_trees();
    }
    return {};
}
std::optional<std::string> Runtime::Impl::get_default_font_for_language(std::string_view language) const
{
    auto it = default_fonts_by_language.find(std::string(language));
    if (it == default_fonts_by_language.end()) {
        return std::nullopt;
    }
    return it->second;
}
void Runtime::Impl::refresh_image_references(TreeRecord &tree, std::string_view image_id)
{
    for (auto &[unused_uid, record] : tree.nodes) {
        (void)unused_uid;
        if (!node_references_image(record, image_id)) {
            continue;
        }
        if (record.node().type == NodeType::Image) {
            record.mutable_resolved_image() = resolve_image_spec(tree, record.image_props().src);
            const auto resource = make_runtime_image_resource(record);
            auto preload_result = ensure_image_resource_preloaded_for_tree(tree, resource);
            if (!preload_result) {
                BROOKESIA_LOGW("Failed to refresh image resource '%1%': %2%", image_id, preload_result.error());
                continue;
            }
            if (backend == nullptr) {
                continue;
            }
            Node apply_node;
            apply_node.type = NodeType::Image;
            apply_node.image_props = record.image_props();
            apply_node.resolved_image = record.resolved_image();
            apply_node.placement = record.placement();
            if (apply_node.resolved_image.primary_src.empty() && apply_node.resolved_image.native_src == 0) {
                apply_node.image_props.src.clear();
            }
            backend->apply_props(record.handle, apply_node, PropsApplyMask::ImageSource);
            backend->apply_placement(record.handle, record.placement(), PlacementApplyMask::Size);
            continue;
        }

        if (record.node().type == NodeType::Keyboard) {
            auto &props = record.mutable_keyboard_props();
            resolve_keyboard_key_style_refs(tree, props);
            resolve_keyboard_key_images(tree, props);
        }
        Node preload_snapshot;
        preload_snapshot.type = record.node().type;
        if (preload_snapshot.type == NodeType::Keyboard) {
            preload_snapshot.keyboard_props = record.keyboard_props();
        }
        auto preload_result = ensure_node_image_resources_preloaded(tree, preload_snapshot);
        if (!preload_result) {
            BROOKESIA_LOGW("Failed to refresh image resource '%1%': %2%", image_id, preload_result.error());
            continue;
        }
        if (backend == nullptr) {
            continue;
        }
        if (record.node().type == NodeType::Keyboard) {
            apply_record_props(record, PropsApplyMask::KeyboardConfig);
        }
    }
}
std::expected<void, std::string> Runtime::Impl::register_image(const RuntimeImageResource &resource)
{
    if (resource.id.empty()) {
        return std::unexpected("Image id must not be empty");
    }
    if (resource.primary_src.empty() && resource.native_src == 0) {
        return std::unexpected("Image resource must define primary_src or native_src");
    }
    RuntimeImageResource resolved_resource = resource;
    if ((resolved_resource.width <= 0 || resolved_resource.height <= 0) && backend != nullptr) {
        auto resolved = backend->resolve_image_resource(resolved_resource);
        if (!resolved) {
            return std::unexpected(
                       "Image resource size must be positive or resolvable by backend metadata: " +
                       resolved.error()
                   );
        }
        resolved_resource = std::move(*resolved);
    }
    if (resolved_resource.width <= 0 || resolved_resource.height <= 0) {
        return std::unexpected("Image resource size must be positive");
    }
    std::optional<RuntimeImageResource> old_resource;
    if (auto old_it = global_images.find(resolved_resource.id); old_it != global_images.end()) {
        old_resource = old_it->second;
    }
    std::vector<TreeRecord *> affected_trees;
    for (auto &[unused_document_id, tree] : trees) {
        (void)unused_document_id;
        bool affected = false;
        for (auto &[unused_uid, record] : tree.nodes) {
            (void)unused_uid;
            if (!node_references_image(record, resolved_resource.id)) {
                continue;
            }
            affected = true;
            break;
        }
        if (affected) {
            affected_trees.push_back(&tree);
        }
    }
    std::vector<TreeRecord *> preloaded_trees;
    const bool new_requires_auto_preload = should_preload_image_resource_automatically(resolved_resource);
    if (new_requires_auto_preload) {
        for (auto *tree : affected_trees) {
            if (has_automatic_preloaded_image_resource(*tree, resolved_resource)) {
                continue;
            }
            auto preload_result =
                preload_image_resource_for_tree(*tree, resolved_resource, ImagePreloadOwner::Automatic);
            if (!preload_result) {
                for (auto *preloaded_tree : preloaded_trees) {
                    release_image_resource_from_tree(
                        *preloaded_tree, resolved_resource, ImagePreloadOwner::Automatic);
                }
                return std::unexpected(preload_result.error());
            }
            preloaded_trees.push_back(tree);
        }
    }
    const bool replaced_resource = old_resource.has_value() &&
                                   !is_same_image_resource(*old_resource, resolved_resource);
    const bool old_requires_auto_preload =
        old_resource.has_value() && should_preload_image_resource_automatically(*old_resource);
    if (replaced_resource) {
        for (auto *tree : affected_trees) {
            release_image_resource_from_tree(*tree, *old_resource);
        }
    } else if (old_requires_auto_preload && !new_requires_auto_preload) {
        for (auto *tree : affected_trees) {
            release_image_resource_from_tree(*tree, *old_resource, ImagePreloadOwner::Automatic);
        }
    }
    global_images.insert_or_assign(resolved_resource.id, resolved_resource);
    for (auto *tree : affected_trees) {
        refresh_image_references(*tree, resolved_resource.id);
    }
    return {};
}
bool Runtime::Impl::unregister_image(std::string_view id)
{
    const std::string image_id(id);
    auto image_it = global_images.find(image_id);
    if (image_it == global_images.end()) {
        return false;
    }
    const auto old_resource = image_it->second;
    global_images.erase(image_it);
    for (auto &[unused_document_id, tree] : trees) {
        (void)unused_document_id;
        release_image_resource_from_tree(tree, old_resource);
        refresh_image_references(tree, image_id);
    }
    return true;
}
std::expected<void, std::string> Runtime::Impl::load_theme_json(std::string_view json, std::string_view base_dir)
{
    auto theme = parse_theme_asset_json(json, base_dir);
    if (!theme) {
        return std::unexpected(theme.error());
    }
    return load_theme(*theme);
}
std::expected<void, std::string> Runtime::Impl::load_theme_file(std::string_view path)
{
    auto theme = parse_theme_asset_file(path);
    if (!theme) {
        return std::unexpected(theme.error());
    }
    return load_theme(*theme);
}
std::expected<void, std::string> Runtime::Impl::register_font_json(std::string_view json, std::string_view base_dir)
{
    auto fonts = parse_font_asset_set_json(json, base_dir);
    if (!fonts) {
        return std::unexpected(fonts.error());
    }
    std::vector<std::pair<std::string, std::optional<RuntimeFontResource>>> previous_resources;
    for (const auto &font : *fonts) {
        std::optional<RuntimeFontResource> previous_resource;
        if (const auto old_it = global_fonts.find(font.id); old_it != global_fonts.end()) {
            previous_resource = old_it->second;
        }
        previous_resources.emplace_back(font.id, std::move(previous_resource));
        auto result = register_font(RuntimeFontResource{
            .id = font.id,
            .kind = font.kind,
            .primary_src = font.src,
            .languages = font.languages,
            .fallback_ids = font.fallbacks,
            .native_fonts = {},
            .image_font_height = font.height,
            .image_font_glyphs = font.glyphs,
            .image_font_sizes = font.sizes,
        });
        if (result) {
            continue;
        }
        for (auto it = previous_resources.rbegin(); it != previous_resources.rend(); ++it) {
            if (it->second.has_value()) {
                auto rollback_result = register_font(*it->second);
                if (!rollback_result) {
                    BROOKESIA_LOGW(
                        "Failed to rollback font resource '%1%': %2%",
                        it->first,
                        rollback_result.error()
                    );
                }
            } else {
                unregister_font(it->first);
            }
        }
        return std::unexpected("Failed to register font asset set: " + result.error());
    }
    return {};
}
std::expected<void, std::string> Runtime::Impl::register_font_file(std::string_view path)
{
    auto fonts = parse_font_asset_set_file(path);
    if (!fonts) {
        return std::unexpected(fonts.error());
    }
    std::vector<std::pair<std::string, std::optional<RuntimeFontResource>>> previous_resources;
    for (const auto &font : *fonts) {
        std::optional<RuntimeFontResource> previous_resource;
        if (const auto old_it = global_fonts.find(font.id); old_it != global_fonts.end()) {
            previous_resource = old_it->second;
        }
        previous_resources.emplace_back(font.id, std::move(previous_resource));
        auto result = register_font(RuntimeFontResource{
            .id = font.id,
            .kind = font.kind,
            .primary_src = font.src,
            .languages = font.languages,
            .fallback_ids = font.fallbacks,
            .native_fonts = {},
            .image_font_height = font.height,
            .image_font_glyphs = font.glyphs,
            .image_font_sizes = font.sizes,
        });
        if (result) {
            continue;
        }
        for (auto it = previous_resources.rbegin(); it != previous_resources.rend(); ++it) {
            if (it->second.has_value()) {
                auto rollback_result = register_font(*it->second);
                if (!rollback_result) {
                    BROOKESIA_LOGW(
                        "Failed to rollback font resource '%1%': %2%",
                        it->first,
                        rollback_result.error()
                    );
                }
            } else {
                unregister_font(it->first);
            }
        }
        return std::unexpected(
                   "Failed to register font asset set file '" + std::string(path) + "': " + result.error()
               );
    }
    return {};
}
std::expected<void, std::string> Runtime::Impl::register_image_json(std::string_view json, std::string_view base_dir)
{
    auto images = parse_image_asset_set_json(json, base_dir);
    if (!images) {
        return std::unexpected(images.error());
    }
    std::vector<std::pair<std::string, std::optional<RuntimeImageResource>>> previous_resources;
    for (const auto &image : *images) {
        std::optional<RuntimeImageResource> previous_resource;
        if (const auto old_it = global_images.find(image.id); old_it != global_images.end()) {
            previous_resource = old_it->second;
        }
        previous_resources.emplace_back(image.id, std::move(previous_resource));
        auto result = register_image(RuntimeImageResource{
            .id = image.id,
            .primary_src = image.src,
            .native_src = 0,
            .width = image.width,
            .height = image.height,
            .preload = image.preload,
        });
        if (result) {
            continue;
        }
        for (auto it = previous_resources.rbegin(); it != previous_resources.rend(); ++it) {
            if (it->second.has_value()) {
                auto rollback_result = register_image(*it->second);
                if (!rollback_result) {
                    BROOKESIA_LOGW(
                        "Failed to rollback image resource '%1%': %2%",
                        it->first,
                        rollback_result.error()
                    );
                }
            } else {
                unregister_image(it->first);
            }
        }
        return std::unexpected("Failed to register image asset set: " + result.error());
    }
    return {};
}
std::expected<void, std::string> Runtime::Impl::register_image_file(std::string_view path)
{
    auto images = parse_image_asset_set_file(path);
    if (!images) {
        return std::unexpected(images.error());
    }
    std::vector<std::pair<std::string, std::optional<RuntimeImageResource>>> previous_resources;
    for (const auto &image : *images) {
        std::optional<RuntimeImageResource> previous_resource;
        if (const auto old_it = global_images.find(image.id); old_it != global_images.end()) {
            previous_resource = old_it->second;
        }
        previous_resources.emplace_back(image.id, std::move(previous_resource));
        auto result = register_image(RuntimeImageResource{
            .id = image.id,
            .primary_src = image.src,
            .native_src = 0,
            .width = image.width,
            .height = image.height,
            .preload = image.preload,
        });
        if (result) {
            continue;
        }
        for (auto it = previous_resources.rbegin(); it != previous_resources.rend(); ++it) {
            if (it->second.has_value()) {
                auto rollback_result = register_image(*it->second);
                if (!rollback_result) {
                    BROOKESIA_LOGW(
                        "Failed to rollback image resource '%1%': %2%",
                        it->first,
                        rollback_result.error()
                    );
                }
            } else {
                unregister_image(it->first);
            }
        }
        return std::unexpected(
                   "Failed to register image asset set file '" + std::string(path) + "': " + result.error()
               );
    }
    return {};
}
std::expected<boost::json::value, std::string> Runtime::Impl::get_constant_value(
    DocumentId document_id,
    std::string_view path) const
{
    const auto *tree = resolve_tree_const(document_id);
    if (tree == nullptr) {
        return std::unexpected("GUI document not loaded");
    }
    return get_json_value_by_dot_path(tree->constants, path);
}
std::vector<RuntimeFontResource> Runtime::Impl::list_font_resources(DocumentId document_id) const
{
    const auto *tree = resolve_tree_const(document_id);
    if (tree == nullptr) {
        return {};
    }

    boost::unordered_flat_map<std::string, RuntimeFontResource> merged_fonts;
    for (const auto &[font_id, font] : global_fonts) {
        merged_fonts.insert_or_assign(font_id, font);
    }

    std::vector<RuntimeFontResource> resources;
    resources.reserve(merged_fonts.size());
    for (auto &[unused_font_id, font] : merged_fonts) {
        (void)unused_font_id;
        resources.push_back(std::move(font));
    }
    std::sort(resources.begin(), resources.end(), [](const RuntimeFontResource & lhs, const RuntimeFontResource & rhs) {
        return lhs.id < rhs.id;
    });
    return resources;
}
std::vector<RuntimeImageResource> Runtime::Impl::list_image_resources(DocumentId document_id) const
{
    const auto *tree = resolve_tree_const(document_id);
    if (tree == nullptr) {
        return {};
    }

    boost::unordered_flat_map<std::string, RuntimeImageResource> merged_images;
    for (const auto &[image_id, image] : global_images) {
        merged_images.insert_or_assign(image_id, image);
    }
    for (const auto &[image_id, image] : tree->images) {
        merged_images.insert_or_assign(image_id, RuntimeImageResource {
            .id = image.id,
            .primary_src = image.src,
            .native_src = 0,
            .width = image.width,
            .height = image.height,
            .preload = image.preload,
        });
    }

    std::vector<RuntimeImageResource> resources;
    resources.reserve(merged_images.size());
    for (auto &[unused_image_id, image] : merged_images) {
        (void)unused_image_id;
        resources.push_back(std::move(image));
    }
    std::sort(resources.begin(), resources.end(), [](const RuntimeImageResource & lhs, const RuntimeImageResource & rhs) {
        return lhs.id < rhs.id;
    });
    return resources;
}
}
