/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "private/parser_impl.hpp"
#include "private/gui_json_storage.hpp"

namespace esp_brookesia::gui::parser_detail {



std::expected<std::vector<ImageFontGlyph>, std::string> parse_image_font_glyphs(
    const boost::json::object &object,
    const std::filesystem::path &asset_dir,
    std::string_view owner)
{
    const auto *glyphs_value = find_child_value(object, "glyphs");
    if (glyphs_value == nullptr || !glyphs_value->is_array()) {
        return std::unexpected(std::string(owner) + " must contain array field 'glyphs'");
    }
    if (glyphs_value->as_array().empty()) {
        return std::unexpected(std::string(owner) + " field 'glyphs' must not be empty");
    }

    std::vector<ImageFontGlyph> glyphs;
    boost::unordered_flat_set<uint32_t> codepoints;
    for (const auto &glyph_value : glyphs_value->as_array()) {
        if (!glyph_value.is_object()) {
            return std::unexpected(std::string(owner) + " glyph entries must be objects");
        }
        const auto &glyph_object = glyph_value.as_object();

        auto codepoint_text = parse_string_field(glyph_object, "codepoint");
        if (!codepoint_text) {
            return std::unexpected(codepoint_text.error());
        }
        auto codepoint = parse_image_font_codepoint(*codepoint_text);
        if (!codepoint) {
            return std::unexpected(codepoint.error());
        }
        if (!codepoints.insert(*codepoint).second) {
            return std::unexpected("Duplicate imageFont glyph codepoint: " + *codepoint_text);
        }

        auto glyph_src = parse_string_field(glyph_object, "src");
        if (!glyph_src) {
            return std::unexpected(glyph_src.error());
        }
        if (glyph_src->empty()) {
            return std::unexpected(std::string(owner) + " glyph field 'src' must not be empty");
        }

        glyphs.push_back(ImageFontGlyph {
            .codepoint = *codepoint,
            .src = resolve_path(asset_dir, *glyph_src).string(),
        });
    }
    return glyphs;
}

std::vector<uint32_t> sorted_image_font_codepoints(const std::vector<ImageFontGlyph> &glyphs)
{
    std::vector<uint32_t> codepoints;
    codepoints.reserve(glyphs.size());
    for (const auto &glyph : glyphs) {
        codepoints.push_back(glyph.codepoint);
    }
    std::sort(codepoints.begin(), codepoints.end());
    return codepoints;
}

std::expected<FontAsset, std::string> parse_font_asset(
    const boost::json::object &object, const std::filesystem::path &asset_dir)
{
    FontAsset font;

    auto id = parse_string_field(object, "id");
    if (!id) {
        return std::unexpected(id.error());
    }
    font.id = *id;

    auto kind = parse_string_field(object, "kind", "file");
    if (!kind) {
        return std::unexpected(kind.error());
    }
    if (*kind != "file" && *kind != "imageFont") {
        return std::unexpected("Field 'kind' must be 'file' or 'imageFont'");
    }
    font.kind = *kind;

    auto languages = parse_string_array(object, "languages");
    if (!languages) {
        return std::unexpected(languages.error());
    }
    if (font.kind == "file" && languages->empty()) {
        return std::unexpected("Field 'languages' must not be empty");
    }
    boost::unordered_flat_set<std::string> unique_languages;
    for (const auto &language : *languages) {
        if (language.empty()) {
            return std::unexpected("Field 'languages' must not contain empty strings");
        }
        if (!unique_languages.insert(language).second) {
            return std::unexpected("Field 'languages' must not contain duplicate values: " + language);
        }
    }
    font.languages = std::move(*languages);

    auto fallbacks = parse_resource_reference_array_field(object, "fallbacks", "font", "'${font.<id>}'");
    if (!fallbacks) {
        return std::unexpected(fallbacks.error());
    }
    font.fallbacks = std::move(*fallbacks);

    if (font.kind == "file") {
        auto src = parse_string_field(object, "src");
        if (!src) {
            return std::unexpected(src.error());
        }
        if (src->empty()) {
            return std::unexpected("File font field 'src' must not be empty");
        }
        font.src = resolve_path(asset_dir, *src).string();
    } else {
        auto src = parse_string_field(object, "src");
        if (!src) {
            return std::unexpected(src.error());
        }
        if (!src->empty()) {
            return std::unexpected("imageFont entries must not use field 'src'; use glyphs[].src");
        }

        const auto *sizes_value = find_child_value(object, "sizes");
        if (sizes_value != nullptr) {
            if (find_child_value(object, "height") != nullptr || find_child_value(object, "glyphs") != nullptr) {
                return std::unexpected("imageFont must use either 'sizes' or 'height' + 'glyphs', not both");
            }
            if (!sizes_value->is_array()) {
                return std::unexpected("imageFont field 'sizes' must be an array");
            }
            if (sizes_value->as_array().empty()) {
                return std::unexpected("imageFont field 'sizes' must not be empty");
            }

            boost::unordered_flat_set<int32_t> heights;
            std::optional<std::vector<uint32_t>> reference_codepoints;
            size_t size_index = 0;
            for (const auto &size_value : sizes_value->as_array()) {
                if (!size_value.is_object()) {
                    return std::unexpected("imageFont sizes entries must be objects");
                }
                const auto &size_object = size_value.as_object();

                auto height = parse_int_field(size_object, "height");
                if (!height) {
                    return std::unexpected(height.error());
                }
                if (*height <= 0) {
                    return std::unexpected("imageFont sizes[].height must be positive");
                }
                if (!heights.insert(*height).second) {
                    return std::unexpected("imageFont sizes[].height must not contain duplicates");
                }

                auto glyphs = parse_image_font_glyphs(
                                  size_object,
                                  asset_dir,
                                  "imageFont sizes[" + std::to_string(size_index) + "]"
                              );
                if (!glyphs) {
                    return std::unexpected(glyphs.error());
                }

                auto codepoints = sorted_image_font_codepoints(*glyphs);
                if (!reference_codepoints.has_value()) {
                    reference_codepoints = std::move(codepoints);
                } else if (*reference_codepoints != codepoints) {
                    return std::unexpected("imageFont sizes[] entries must contain the same glyph codepoints");
                }

                font.sizes.push_back(ImageFontSize {
                    .height = *height,
                    .glyphs = std::move(*glyphs),
                });
                ++size_index;
            }
            std::sort(
                font.sizes.begin(),
                font.sizes.end(),
            [](const ImageFontSize & lhs, const ImageFontSize & rhs) {
                return lhs.height < rhs.height;
            }
            );
            font.height = font.sizes.front().height;
            font.glyphs = font.sizes.front().glyphs;
        } else {
            auto height = parse_int_field(object, "height");
            if (!height) {
                return std::unexpected(height.error());
            }
            if (*height <= 0) {
                return std::unexpected("imageFont field 'height' must be positive");
            }
            font.height = *height;

            auto glyphs = parse_image_font_glyphs(object, asset_dir, "imageFont");
            if (!glyphs) {
                return std::unexpected(glyphs.error());
            }
            font.glyphs = std::move(*glyphs);
            font.sizes.push_back(ImageFontSize {
                .height = font.height,
                .glyphs = font.glyphs,
            });
        }
    }

    BROOKESIA_LOGD(
        "Parsed font asset: id='%1%', kind='%2%', src='%3%', language_count=%4%, fallback_count=%5%",
        font.id,
        font.kind,
        font.src,
        font.languages.size(),
        font.fallbacks.size()
    );

    return font;
}

std::expected<ImageAsset, std::string> parse_image_asset(
    const boost::json::object &object, const std::filesystem::path &asset_dir)
{
    ImageAsset image;

    auto id = parse_string_field(object, "id");
    if (!id) {
        return std::unexpected(id.error());
    }
    image.id = *id;

    auto src = parse_string_field(object, "src");
    if (!src) {
        return std::unexpected(src.error());
    }
    image.src = resolve_path(asset_dir, *src).string();

    auto width = parse_int_field(object, "width");
    if (!width) {
        return std::unexpected(width.error());
    }
    image.width = *width;

    auto height = parse_int_field(object, "height");
    if (!height) {
        return std::unexpected(height.error());
    }
    image.height = *height;

    auto preload = parse_bool_field(object, "preload", image.preload);
    if (!preload) {
        return std::unexpected(preload.error());
    }
    image.preload = *preload;

    BROOKESIA_LOGD(
        "Parsed image asset: id='%1%', src='%2%', width=%3%, height=%4%, preload=%5%",
        image.id,
        image.src,
        image.width,
        image.height,
        image.preload
    );

    return image;
}

std::expected<std::vector<FontAsset>, std::string> parse_font_asset_set(
    const boost::json::object &object,
    const std::filesystem::path &asset_dir)
{
    auto type = parse_string_field(object, "type");
    if (!type) {
        return std::unexpected(type.error());
    }
    if (*type == "font") {
        return std::unexpected("Font descriptor type 'font' is no longer supported; use type='fontSet' with fonts[]");
    }
    if (*type != "fontSet") {
        return std::unexpected("Font descriptor must use type='fontSet'");
    }

    const auto *fonts_value = find_child_value(object, "fonts");
    if (fonts_value == nullptr || !fonts_value->is_array()) {
        return std::unexpected("fontSet must contain array field 'fonts'");
    }
    if (fonts_value->as_array().empty()) {
        return std::unexpected("fontSet field 'fonts' must not be empty");
    }

    std::vector<FontAsset> fonts;
    boost::unordered_flat_set<std::string> ids;
    for (const auto &font_value : fonts_value->as_array()) {
        if (!font_value.is_object()) {
            return std::unexpected("fontSet entries must be objects");
        }
        const auto &font_object = font_value.as_object();
        if (find_child_value(font_object, "type") != nullptr) {
            return std::unexpected("fontSet entries must not contain field 'type'");
        }
        auto font = parse_font_asset(font_object, asset_dir);
        if (!font) {
            return std::unexpected(font.error());
        }
        if (!ids.insert(font->id).second) {
            return std::unexpected("Duplicate font id in fontSet: " + font->id);
        }
        fonts.push_back(std::move(*font));
    }
    return fonts;
}

std::expected<std::vector<ImageAsset>, std::string> parse_image_asset_set(
    const boost::json::object &object,
    const std::filesystem::path &asset_dir)
{
    auto type = parse_string_field(object, "type");
    if (!type) {
        return std::unexpected(type.error());
    }
    if (*type == "image") {
        return std::unexpected(
                   "Image descriptor type 'image' is no longer supported; use type='imageSet' with images[]"
               );
    }
    if (*type != "imageSet") {
        return std::unexpected("Image descriptor must use type='imageSet'");
    }

    const auto *images_value = find_child_value(object, "images");
    if (images_value == nullptr || !images_value->is_array()) {
        return std::unexpected("imageSet must contain array field 'images'");
    }
    if (images_value->as_array().empty()) {
        return std::unexpected("imageSet field 'images' must not be empty");
    }

    std::vector<ImageAsset> images;
    boost::unordered_flat_set<std::string> ids;
    for (const auto &image_value : images_value->as_array()) {
        if (!image_value.is_object()) {
            return std::unexpected("imageSet entries must be objects");
        }
        const auto &image_object = image_value.as_object();
        if (find_child_value(image_object, "type") != nullptr) {
            return std::unexpected("imageSet entries must not contain field 'type'");
        }
        auto image = parse_image_asset(image_object, asset_dir);
        if (!image) {
            return std::unexpected(image.error());
        }
        if (!ids.insert(image->id).second) {
            return std::unexpected("Duplicate image id in imageSet: " + image->id);
        }
        images.push_back(std::move(*image));
    }
    return images;
}

std::expected<void, std::string> merge_theme_constant_asset(
    const RootAssetEntry &asset_entry,
    boost::json::value &constants)
{
    if (!asset_entry.value.is_object()) {
        return std::unexpected("Theme asset must be an object: " + asset_entry.source_label);
    }

    const auto &asset_object = asset_entry.value.as_object();
    auto asset_type = parse_string_field(asset_object, "type");
    if (!asset_type) {
        return std::unexpected(
                   "Failed to parse theme asset '" + asset_entry.source_label + "': " + asset_type.error()
               );
    }
    if (*asset_type != "constant") {
        return std::unexpected(
                   "Theme assets must be type='constant': " + asset_entry.source_label
               );
    }

    const auto *data_value = find_child_value(asset_object, "data");
    if (data_value == nullptr || !data_value->is_object()) {
        return std::unexpected(
                   "Theme constant asset '" + asset_entry.source_label + "' must contain object field 'data'"
               );
    }
    merge_json(constants, *data_value);
    return {};
}

std::expected<void, std::string> append_and_merge_theme_constant_assets(
    const boost::json::object &object,
    std::string_view key,
    const std::filesystem::path &base_dir,
    boost::unordered_flat_set<std::string> &loaded_paths,
    boost::json::value &constants,
    std::string_view inline_entry_label)
{
    std::vector<RootAssetEntry> entries;
    auto append_result = append_root_asset_entries(
                             object,
                             key,
                             base_dir,
                             loaded_paths,
                             nullptr,
                             entries,
                             inline_entry_label
                         );
    if (!append_result) {
        return append_result;
    }

    for (const auto &entry : entries) {
        auto merge_result = merge_theme_constant_asset(entry, constants);
        if (!merge_result) {
            return merge_result;
        }
    }
    return {};
}

std::expected<ThemeAsset, std::string> parse_theme_asset(
    const boost::json::object &object,
    const std::filesystem::path &base_dir,
    const Environment &environment)
{
    ThemeAsset theme;

    auto type = parse_string_field(object, "type", "theme");
    if (!type) {
        return std::unexpected(type.error());
    }
    if (*type != "theme") {
        return std::unexpected("Theme asset must use type='theme'");
    }

    auto id = parse_string_field(object, "id");
    if (!id) {
        return std::unexpected(id.error());
    }
    theme.id = *id;

    if (find_child_value(object, "colors") != nullptr) {
        return std::unexpected("Theme asset field 'colors' is no longer supported; use constant data.colors");
    }

    boost::json::value constants(boost::json::object(), make_gui_json_storage());
    boost::unordered_flat_set<std::string> loaded_paths;
    auto merge_result = append_and_merge_theme_constant_assets(
                            object,
                            "assets",
                            base_dir,
                            loaded_paths,
                            constants,
                            "inline theme asset"
                        );
    if (!merge_result) {
        return std::unexpected(merge_result.error());
    }

    const auto *variants_value = find_child_value(object, "variants");
    if (variants_value != nullptr) {
        if (!variants_value->is_array()) {
            return std::unexpected("Theme field 'variants' must be an array");
        }
        for (const auto &variant_value : variants_value->as_array()) {
            if (!variant_value.is_object()) {
                return std::unexpected("Theme variant entries must be objects");
            }
            const auto &variant_object = variant_value.as_object();
            auto when = parse_string_field(variant_object, "when", "${expr(true)}");
            if (!when) {
                return std::unexpected(when.error());
            }
            if (!is_expression_string(*when)) {
                return std::unexpected("Theme variant field 'when' must use '${expr(...)}'");
            }

            auto matched_value = evaluate_expression_string(*when, constants, environment);
            if (!matched_value) {
                return std::unexpected(matched_value.error());
            }
            if (!matched_value->is_bool()) {
                return std::unexpected("Theme variant field 'when' expression must evaluate to boolean");
            }
            if (!matched_value->as_bool()) {
                continue;
            }

            merge_result = append_and_merge_theme_constant_assets(
                               variant_object,
                               "assets",
                               base_dir,
                               loaded_paths,
                               constants,
                               "inline theme variant asset"
                           );
            if (!merge_result) {
                return std::unexpected(merge_result.error());
            }
        }
    }

    if (const auto *colors_value = resolve_constant(constants, "colors"); colors_value != nullptr) {
        auto colors_result = flatten_color_constants(*colors_value, "", theme.colors);
        if (!colors_result) {
            return std::unexpected(colors_result.error());
        }
    }

    Environment theme_environment = environment;
    theme_environment.colors = theme.colors;

    if (const auto *constant_styles_value = resolve_constant(constants, "styles"); constant_styles_value != nullptr) {
        if (!constant_styles_value->is_object()) {
            return std::unexpected("Theme constant 'styles' must be an object");
        }
        auto styles_result = parse_named_style_map(constant_styles_value->as_object(), constants, theme_environment, true);
        if (!styles_result) {
            return std::unexpected(styles_result.error());
        }
        theme.styles.insert(styles_result->begin(), styles_result->end());
    }

    const auto *styles_value = find_child_value(object, "styles");
    if (styles_value != nullptr) {
        if (!styles_value->is_object()) {
            return std::unexpected("Theme asset field 'styles' must be an object");
        }
        auto styles_result = parse_named_style_map(styles_value->as_object(), constants, theme_environment, true);
        if (!styles_result) {
            return std::unexpected(styles_result.error());
        }
        for (auto &[key, style] : *styles_result) {
            theme.styles.insert_or_assign(std::move(key), std::move(style));
        }
    }

    BROOKESIA_LOGD(
        "Parsed theme asset: id='%1%', color_count=%2%, style_count=%3%",
        theme.id,
        theme.colors.size(),
        theme.styles.size()
    );

    return theme;
}

std::expected<std::vector<std::string>, std::string> parse_screen_flow_from_array(
    const boost::json::object &object)
{
    const auto *from_value = find_child_value(object, "from");
    if (from_value == nullptr) {
        return std::vector<std::string>();
    }
    if (!from_value->is_array()) {
        return std::unexpected("ScreenFlow transition field 'from' must be a string array");
    }

    std::vector<std::string> from;
    for (const auto &state_value : from_value->as_array()) {
        if (!state_value.is_string()) {
            return std::unexpected("ScreenFlow transition field 'from' must contain only strings");
        }
        from.emplace_back(state_value.as_string().c_str());
    }
    return from;
}

std::expected<ScreenFlow, std::string> parse_screen_flow(const boost::json::object &object)
{
    ScreenFlow flow;

    if (find_child_value(object, "states") != nullptr) {
        return std::unexpected(
                   "ScreenFlow field 'states' is no longer supported; use top-level screen ids as states"
               );
    }
    auto type = parse_string_field(object, "type");
    if (!type) {
        return std::unexpected(type.error());
    }
    if (*type != "screenFlow") {
        return std::unexpected("ScreenFlow asset must use type='screenFlow'");
    }
    auto id = parse_string_field(object, "id");
    if (!id) {
        return std::unexpected(id.error());
    }
    flow.id = *id;

    auto screens = parse_string_array(object, "screens");
    if (!screens) {
        return std::unexpected(screens.error());
    }
    flow.screens = std::move(*screens);

    auto initial = parse_string_field(object, "initial");
    if (!initial) {
        return std::unexpected(initial.error());
    }
    flow.initial = *initial;

    const auto *transitions_value = find_child_value(object, "transitions");
    if (transitions_value != nullptr) {
        if (!transitions_value->is_array()) {
            return std::unexpected("ScreenFlow field 'transitions' must be an array");
        }
        for (const auto &transition_value : transitions_value->as_array()) {
            if (!transition_value.is_object()) {
                return std::unexpected("ScreenFlow transitions must be objects");
            }
            const auto &transition_object = transition_value.as_object();
            auto from = parse_screen_flow_from_array(transition_object);
            auto action = parse_string_field(transition_object, "action");
            auto to = parse_string_field(transition_object, "to");
            if (!from || !action || !to) {
                return std::unexpected(!from ? from.error() : (!action ? action.error() : to.error()));
            }
            flow.transitions.push_back(ScreenFlowTransition{
                .from = *from,
                .action = *action,
                .to = *to,
            });
        }
    }

    BROOKESIA_LOGD(
        "Parsed screen flow: id='%1%', initial='%2%', screen_count=%3%, transition_count=%4%",
        flow.id,
        flow.initial,
        flow.screens.size(),
        flow.transitions.size()
    );

    return flow;
}
}
