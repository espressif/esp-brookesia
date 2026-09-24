/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "private/parser_impl.hpp"
#include "private/gui_json_storage.hpp"

#include <map>

#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
#include "brookesia/lib_utils/memory_profiler.hpp"
#endif

namespace esp_brookesia::gui {
using namespace parser_detail;

#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
static void log_parser_internal_heap(std::string_view base_dir, std::string_view stage)
{
    const auto snap = ::esp_brookesia::lib_utils::MemoryProfiler::take_raw_heap_snapshot();
    if (!snap.valid) {
        return;
    }
    BROOKESIA_LOGI(
        "[HeapTrace][gui.parser] base_dir(%1%) stage(%2%) internal_free(%3%) internal_largest(%4%) "
        "psram_free(%5%) psram_largest(%6%)",
        base_dir,
        stage,
        snap.internal_free,
        snap.internal_largest,
        snap.external_free,
        snap.external_largest
    );
}
#else
static void log_parser_internal_heap(std::string_view, std::string_view) {}
#endif


static std::expected<ParsedDocument, std::string> parse_document_impl(
    std::string_view json,
    std::string_view base_dir,
    const Environment &environment,
    std::vector<std::string> dependency_files,
    bool allow_defer)
{
    BROOKESIA_LOG_TRACE_GUARD();

    BROOKESIA_LOGD(
        "Params: json(size=%1%), base_dir(%2%), environment(%3%)",
        json.size(), base_dir, environment
    );

    const auto total_start = ParserProfileClock::now();
    auto stage_start = total_start;
    boost::system::error_code error_code;
    boost::json::value json_value = boost::json::parse(json, error_code, make_gui_json_storage());
    auto stage_end = ParserProfileClock::now();
    if (error_code) {
        BROOKESIA_LOGE("Failed to parse GUI JSON: %1%", error_code.message());
        return std::unexpected("Failed to parse GUI JSON: " + error_code.message());
    }
    if (!json_value.is_object()) {
        return std::unexpected("GUI root document must be a JSON object");
    }
    GUI_INTERFACE_PROFILE_LOGI(
        "GUI parser profile: base_dir(%1%), stage(parse_root_json), bytes(%2%), elapsed_ms(%3%), total_ms(%4%)",
        base_dir,
        json.size(),
        parser_profile_elapsed_ms(stage_start, stage_end),
        parser_profile_elapsed_ms(total_start, stage_end)
    );
    log_parser_internal_heap(base_dir, "after parse_root_json");

    const auto &root_object = json_value.as_object();
    const auto version = parse_string_field(root_object, "version", std::string(CURRENT_DOCUMENT_VERSION));
    if (!version) {
        return std::unexpected(version.error());
    }

    if (find_child_value(root_object, "constants") != nullptr || find_child_value(root_object, "nodes") != nullptr) {
        return std::unexpected("Legacy root fields 'constants'/'nodes' are no longer supported; use 'assets'");
    }

    ParsedDocument parsed_document;
    auto &document = parsed_document.document;
    document.version = *version;
    parsed_document.dependency_files = std::move(dependency_files);

    const std::filesystem::path resolved_base_dir = std::filesystem::path(base_dir).lexically_normal();
    boost::json::value constants(boost::json::object(), make_gui_json_storage());
    boost::unordered_flat_set<std::string> loaded_asset_paths;
    std::vector<RootAssetEntry> asset_entries;

    if (const auto *root_screen_flow_value = find_child_value(root_object, "screenFlow");
            root_screen_flow_value != nullptr) {
        (void)root_screen_flow_value;
        return std::unexpected("Root field 'screenFlow' is no longer supported; add a screenFlow asset to assets[]");
    }

    stage_start = ParserProfileClock::now();
    auto append_result = append_root_asset_entries(
                             root_object,
                             "assets",
                             resolved_base_dir,
                             loaded_asset_paths,
                             &parsed_document.dependency_files,
                             asset_entries,
                             "inline asset"
                         );
    if (!append_result) {
        return std::unexpected(append_result.error());
    }

    const auto *variants_value = find_child_value(root_object, "variants");
    if (variants_value != nullptr) {
        if (!variants_value->is_array()) {
            return std::unexpected("Field 'variants' must be an array");
        }

        for (const auto &variant_value : variants_value->as_array()) {
            if (!variant_value.is_object()) {
                return std::unexpected("Variant entries must be objects");
            }

            const auto &variant_object = variant_value.as_object();
            if (find_child_value(variant_object, "constants") != nullptr ||
                    find_child_value(variant_object, "nodes") != nullptr) {
                return std::unexpected(
                           "Legacy variant fields 'constants'/'nodes' are no longer supported; use 'assets'"
                       );
            }

            auto when = parse_string_field(variant_object, "when", "${expr(true)}");
            if (!when) {
                return std::unexpected(when.error());
            }
            if (!is_expression_string(*when)) {
                return std::unexpected(
                           "Variant field 'when' must use '${expr(...)}'; migrate old expressions such as "
                           "'WidthDp == 1024' to '${expr(${env.widthDp} == 1024dp)}'"
                       );
            }
            if (when->find("${env.theme}") != std::string::npos) {
                document.environment_dependencies.theme = true;
            }
            if (when->find("${env.language}") != std::string::npos) {
                document.environment_dependencies.language = true;
            }
            if (when->find("${env.widthPx}") != std::string::npos ||
                    when->find("${env.heightPx}") != std::string::npos ||
                    when->find("${env.widthDp}") != std::string::npos ||
                    when->find("${env.heightDp}") != std::string::npos ||
                    when->find("${env.density}") != std::string::npos ||
                    when->find("${env.fontScale}") != std::string::npos) {
                document.environment_dependencies.metrics = true;
            }
            document.theme_sensitive = document.environment_dependencies.any();

            auto matched_value = evaluate_expression_string(*when, constants, environment);
            if (!matched_value) {
                return std::unexpected(matched_value.error());
            }
            if (!matched_value->is_bool()) {
                return std::unexpected("Variant field 'when' expression must evaluate to boolean");
            }
            const bool matched = matched_value->as_bool();
            if (!matched) {
                continue;
            }

            if (const auto *variant_screen_flow_value = find_child_value(variant_object, "screenFlow");
                    variant_screen_flow_value != nullptr) {
                (void)variant_screen_flow_value;
                return std::unexpected(
                           "Variant field 'screenFlow' is no longer supported; add a screenFlow asset to assets[]"
                       );
            }

            append_result = append_root_asset_entries(
                                variant_object,
                                "assets",
                                resolved_base_dir,
                                loaded_asset_paths,
                                &parsed_document.dependency_files,
                                asset_entries,
                                "inline variant asset"
                            );
            if (!append_result) {
                return std::unexpected(append_result.error());
            }
        }
    }
    stage_end = ParserProfileClock::now();
    GUI_INTERFACE_PROFILE_LOGI(
        "GUI parser profile: base_dir(%1%), stage(load_asset_entries), assets(%2%), dependencies(%3%), "
        "elapsed_ms(%4%), total_ms(%5%)",
        base_dir,
        asset_entries.size(),
        parsed_document.dependency_files.size(),
        parser_profile_elapsed_ms(stage_start, stage_end),
        parser_profile_elapsed_ms(total_start, stage_end)
    );

    log_parser_internal_heap(base_dir, "after load_asset_entries");

    stage_start = ParserProfileClock::now();
    for (auto &asset_entry : asset_entries) {
        const auto &asset_object = asset_entry.value.as_object();
        auto asset_type = parse_string_field(asset_object, "type");
        if (!asset_type) {
            return std::unexpected(
                       "Failed to parse asset '" + asset_entry.source_label + "': " + asset_type.error()
                   );
        }
        if (*asset_type != "constant") {
            continue;
        }

        auto replace_result = substitute_references(asset_entry.value, constants, environment);
        if (!replace_result) {
            return std::unexpected(
                       "Failed to resolve asset '" + asset_entry.source_label + "': " + replace_result.error()
                   );
        }

        auto &constant_object = asset_entry.value.as_object();
        auto data_it = find_key(constant_object, "data");
        if (data_it == constant_object.end() || !data_it->value().is_object()) {
            return std::unexpected(
                       "Failed to parse constant asset '" + asset_entry.source_label +
                       "': Constant asset must contain an object field 'data'"
                   );
        }
        merge_json(constants, std::move(data_it->value()));
        asset_entry.value = boost::json::value(nullptr);
    }

    document.constants = std::move(constants);
    stage_end = ParserProfileClock::now();
    GUI_INTERFACE_PROFILE_LOGI(
        "GUI parser profile: base_dir(%1%), stage(resolve_constants), assets(%2%), elapsed_ms(%3%), total_ms(%4%)",
        base_dir,
        asset_entries.size(),
        parser_profile_elapsed_ms(stage_start, stage_end),
        parser_profile_elapsed_ms(total_start, stage_end)
    );
    log_parser_internal_heap(base_dir, "after resolve_constants");

    stage_start = ParserProfileClock::now();
    std::vector<ResolvedAssetEntry> resolved_assets;
    resolved_assets.reserve(asset_entries.size());
    for (auto &asset_entry : asset_entries) {
        if (asset_entry.value.is_null()) {
            continue;
        }
        const auto &asset_object = asset_entry.value.as_object();
        auto asset_type = parse_string_field(asset_object, "type");
        if (!asset_type) {
            return std::unexpected(
                       "Failed to parse asset '" + asset_entry.source_label + "': " + asset_type.error()
                   );
        }
        if (*asset_type == "constant") {
            asset_entry.value = boost::json::value(nullptr);
            continue;
        }

        auto replace_result = substitute_references(asset_entry.value, document.constants, environment);
        if (!replace_result) {
            return std::unexpected(
                       "Failed to resolve asset '" + asset_entry.source_label + "': " + replace_result.error()
                   );
        }

        resolved_assets.push_back(ResolvedAssetEntry{
            .value = std::move(asset_entry.value),
            .base_dir = std::move(asset_entry.base_dir),
            .source_label = std::move(asset_entry.source_label),
            .type = *asset_type,
            .deferred_view_screen = asset_entry.deferred_view_screen,
            .deferred_path = std::move(asset_entry.deferred_path),
            .peeked_mount_mode = asset_entry.peeked_mount_mode,
        });
        asset_entry.value = boost::json::value(nullptr);
    }
    asset_entries.clear();
    asset_entries.shrink_to_fit();
    stage_end = ParserProfileClock::now();
    GUI_INTERFACE_PROFILE_LOGI(
        "GUI parser profile: base_dir(%1%), stage(resolve_assets), resolved_assets(%2%), "
        "elapsed_ms(%3%), total_ms(%4%)",
        base_dir,
        resolved_assets.size(),
        parser_profile_elapsed_ms(stage_start, stage_end),
        parser_profile_elapsed_ms(total_start, stage_end)
    );
    log_parser_internal_heap(base_dir, "after resolve_assets");

    stage_start = ParserProfileClock::now();
    for (auto &asset_entry : resolved_assets) {
        if (asset_entry.value.is_null()) {
            continue;
        }
        const auto &resolved_asset_object = asset_entry.value.as_object();

        if (asset_entry.type == "font" || asset_entry.type == "fontSet") {
            return std::unexpected(
                       "Document asset type '" + asset_entry.type +
                       "' is not supported; register fonts globally via Runtime"
                   );
        }

        if (asset_entry.type == "image") {
            return std::unexpected(
                       "Document asset type 'image' is no longer supported; use type='imageSet' with images[]"
                   );
        }

        if (asset_entry.type == "imageSet") {
            auto images = parse_image_asset_set(resolved_asset_object, asset_entry.base_dir);
            if (!images) {
                return std::unexpected(
                           "Failed to parse imageSet asset '" + asset_entry.source_label + "': " + images.error()
                       );
            }
            document.images.reserve(document.images.size() + images->size());
            std::move(images->begin(), images->end(), std::back_inserter(document.images));
            asset_entry.value = boost::json::value(nullptr);
            continue;
        }

        if (asset_entry.type == "theme") {
            return std::unexpected(
                       "Document asset type 'theme' is no longer supported; load themes globally via Runtime"
                   );
        }
    }
    stage_end = ParserProfileClock::now();
    GUI_INTERFACE_PROFILE_LOGI(
        "GUI parser profile: base_dir(%1%), stage(parse_image_assets), images(%2%), elapsed_ms(%3%), "
        "total_ms(%4%)",
        base_dir,
        document.images.size(),
        parser_profile_elapsed_ms(stage_start, stage_end),
        parser_profile_elapsed_ms(total_start, stage_end)
    );

    stage_start = ParserProfileClock::now();
    for (auto &asset_entry : resolved_assets) {
        if (asset_entry.value.is_null() || asset_entry.type != "styleSet") {
            continue;
        }
        const auto &resolved_asset_object = asset_entry.value.as_object();
        const auto *styles_value = find_child_value(resolved_asset_object, "styles");
        if (styles_value == nullptr || !styles_value->is_object()) {
            return std::unexpected(
                       "Failed to parse styleSet asset '" + asset_entry.source_label +
                       "': styleSet must contain object field 'styles'"
                   );
        }
        auto styles = parse_named_style_map(styles_value->as_object(), document.constants, environment, false);
        if (!styles) {
            return std::unexpected(
                       "Failed to parse styleSet asset '" + asset_entry.source_label + "': " + styles.error()
                   );
        }
        for (auto &[key, style] : *styles) {
            document.styles.insert_or_assign(std::move(key), std::move(style));
        }
        asset_entry.value = boost::json::value(nullptr);
    }
    stage_end = ParserProfileClock::now();
    GUI_INTERFACE_PROFILE_LOGI(
        "GUI parser profile: base_dir(%1%), stage(parse_style_assets), styles(%2%), elapsed_ms(%3%), total_ms(%4%)",
        base_dir,
        document.styles.size(),
        parser_profile_elapsed_ms(stage_start, stage_end),
        parser_profile_elapsed_ms(total_start, stage_end)
    );
    log_parser_internal_heap(base_dir, "after parse_style_assets");

    stage_start = ParserProfileClock::now();
    InteractionTemplateRawMap raw_interactions;
    for (auto &asset_entry : resolved_assets) {
        if (asset_entry.value.is_null() || asset_entry.type != "interactionTemplate") {
            continue;
        }
        auto &resolved_asset_object = asset_entry.value.as_object();
        auto id = parse_string_field(resolved_asset_object, "id");
        if (!id) {
            return std::unexpected(
                       "Failed to parse interactionTemplate asset '" + asset_entry.source_label + "': " + id.error()
                   );
        }
        auto interaction_object = std::move(resolved_asset_object);
        asset_entry.value = boost::json::value(nullptr);
        for (const auto &entry : interaction_object) {
            const auto normalized_key = normalize_object_key(entry.key());
            if (normalized_key == "type" || normalized_key == "id" || normalized_key == "common_props" ||
                    normalized_key == "events" || normalized_key == "animations" || normalized_key == "state_styles") {
                continue;
            }
            return std::unexpected(
                       "Failed to parse interactionTemplate asset '" + asset_entry.source_label +
                       "': unsupported field '" + std::string(entry.key()) + "'"
                   );
        }
        interaction_object.erase("type");
        interaction_object.erase("id");
        auto [unused_it, inserted] = raw_interactions.emplace(*id, std::move(interaction_object));
        if (!inserted) {
            return std::unexpected("Duplicate interactionTemplate id: " + *id);
        }
    }
    stage_end = ParserProfileClock::now();
    GUI_INTERFACE_PROFILE_LOGI(
        "GUI parser profile: base_dir(%1%), stage(parse_interaction_templates), interactions(%2%), "
        "elapsed_ms(%3%), total_ms(%4%)",
        base_dir,
        raw_interactions.size(),
        parser_profile_elapsed_ms(stage_start, stage_end),
        parser_profile_elapsed_ms(total_start, stage_end)
    );

    stage_start = ParserProfileClock::now();
    TemplateRawMap raw_templates;
    for (auto &asset_entry : resolved_assets) {
        if (asset_entry.value.is_null()) {
            continue;
        }
        if (asset_entry.type == "imageSet" || asset_entry.type == "interactionTemplate" ||
                asset_entry.type == "styleSet") {
            continue;
        }
        if (asset_entry.type == "view") {
            return std::unexpected(
                       "Document asset type 'view' is no longer supported; use viewScreen or viewTemplate"
                   );
        }
        if (asset_entry.type != "viewTemplate") {
            continue;
        }
        const auto &resolved_asset_object = asset_entry.value.as_object();
        auto id = parse_string_field(resolved_asset_object, "id");
        if (!id) {
            return std::unexpected(
                       "Failed to parse viewTemplate asset '" + asset_entry.source_label + "': " + id.error()
                   );
        }
        const auto *node_value = find_child_value(resolved_asset_object, "node");
        if (node_value == nullptr || !node_value->is_object()) {
            return std::unexpected(
                       "Failed to parse viewTemplate asset '" + asset_entry.source_label +
                       "': viewTemplate must contain object field 'node'"
                   );
        }
        auto node_object = node_value->as_object();
        if (find_child_value(node_object, "id") != nullptr) {
            return std::unexpected(
                       "Failed to parse viewTemplate asset '" + asset_entry.source_label +
                       "': viewTemplate.node must not contain field 'id'"
                   );
        }
        node_object.insert_or_assign("id", *id);
        auto [unused_it, inserted] = raw_templates.emplace(*id, std::move(node_object));
        if (!inserted) {
            return std::unexpected("Duplicate viewTemplate id: " + *id);
        }
        asset_entry.value = boost::json::value(nullptr);
    }

    document.templates.reserve(raw_templates.size());
    for (auto &raw_template_entry : raw_templates) {
        std::vector<std::string> template_stack;
        auto template_object = raw_template_entry.second;
        auto slot_result = apply_template_slots(template_object, nullptr);
        if (!slot_result) {
            return std::unexpected(
                       "Failed to parse viewTemplate asset '" + raw_template_entry.first + "': " +
                       slot_result.error()
                   );
        }
        auto node = parse_view_node(template_object, environment, raw_templates, raw_interactions, template_stack);
        if (!node) {
            return std::unexpected(
                       "Failed to parse viewTemplate asset '" + raw_template_entry.first + "': " + node.error()
                   );
        }
        if (node->type == NodeType::Screen) {
            return std::unexpected("viewTemplate root node must not be a screen");
        }
        document.templates.push_back(std::move(*node));
    }
    stage_end = ParserProfileClock::now();
    GUI_INTERFACE_PROFILE_LOGI(
        "GUI parser profile: base_dir(%1%), stage(parse_templates), raw_templates(%2%), templates(%3%), "
        "elapsed_ms(%4%), total_ms(%5%)",
        base_dir,
        raw_templates.size(),
        document.templates.size(),
        parser_profile_elapsed_ms(stage_start, stage_end),
        parser_profile_elapsed_ms(total_start, stage_end)
    );

    stage_start = ParserProfileClock::now();
    document.screens.reserve(resolved_assets.size());
    std::vector<ResolvedAssetEntry *> view_screen_assets;
    for (auto &asset_entry : resolved_assets) {
        if (asset_entry.value.is_null()) {
            continue;
        }
        const auto &resolved_asset_object = asset_entry.value.as_object();
        if (asset_entry.type == "imageSet" || asset_entry.type == "interactionTemplate" ||
                asset_entry.type == "styleSet" || asset_entry.type == "viewTemplate") {
            continue;
        }
        if (asset_entry.type == "screenFlow") {
            auto flow = parse_screen_flow(resolved_asset_object);
            if (!flow) {
                return std::unexpected(
                           "Failed to parse screenFlow asset '" + asset_entry.source_label + "': " + flow.error()
                       );
            }
            document.screen_flows.push_back(std::move(*flow));
            asset_entry.value = boost::json::value(nullptr);
            continue;
        }
        if (asset_entry.type != "viewScreen") {
            return std::unexpected(
                       "Unsupported asset type in '" + asset_entry.source_label + "': " + asset_entry.type
                   );
        }
        view_screen_assets.push_back(&asset_entry);
    }

    boost::unordered_flat_set<std::string> entry_screen_ids;
    for (const auto &flow : document.screen_flows) {
        if (!flow.initial.empty()) {
            entry_screen_ids.insert(flow.initial);
        }
    }

    auto parse_one_view_screen = [&](ResolvedAssetEntry & asset_entry) -> std::expected<Node, std::string> {
        boost::json::value loaded_value;
        const boost::json::object *screen_object = &asset_entry.value.as_object();
        if (asset_entry.deferred_view_screen)
        {
            auto text = read_text_file(asset_entry.deferred_path);
            if (!text) {
                return std::unexpected(text.error());
            }
            auto parsed_json = parse_json_text(asset_entry.deferred_path, *text);
            if (!parsed_json) {
                return std::unexpected(parsed_json.error());
            }
            auto replace_result = substitute_references(*parsed_json, document.constants, environment);
            if (!replace_result) {
                return std::unexpected(replace_result.error());
            }
            loaded_value = std::move(*parsed_json);
            if (!loaded_value.is_object()) {
                return std::unexpected(
                           "Asset file must contain a JSON object: " + asset_entry.deferred_path.generic_string()
                       );
            }
            screen_object = &loaded_value.as_object();
        }
        std::vector<std::string> template_stack;
        return parse_view_node(
                   *screen_object,
                   environment,
                   raw_templates,
                   raw_interactions,
                   template_stack,
                   NodeType::Screen
               );
    };

    size_t eager_parsed_screens = 0;
    size_t deferred_screens = 0;
    for (auto *asset_entry : view_screen_assets) {
        auto id = parse_string_field(asset_entry->value.as_object(), "id");
        if (!id) {
            return std::unexpected(
                       "Failed to parse viewScreen asset '" + asset_entry->source_label + "': " + id.error()
                   );
        }
        const bool is_entry = entry_screen_ids.empty() ? eager_parsed_screens == 0 : entry_screen_ids.contains(*id);
        const bool defer_non_entry = allow_defer && !is_entry;
        const bool keep_deferred = asset_entry->deferred_view_screen &&
                                   (defer_non_entry || asset_entry->peeked_mount_mode == MountMode::Dynamic);
        if (keep_deferred) {
            auto node = parse_one_view_screen(*asset_entry);
            if (!node) {
                return std::unexpected(
                           "Failed to parse viewScreen asset '" + asset_entry->source_label + "': " + node.error()
                       );
            }
            parsed_document.deferred_screen_files.emplace(
                node->id, asset_entry->deferred_path.generic_string()
            );
            document.screens.push_back(std::move(*node));
            ++deferred_screens;
            asset_entry->value = boost::json::value(nullptr);
            continue;
        }

        auto node = parse_one_view_screen(*asset_entry);
        if (!node) {
            return std::unexpected(
                       "Failed to parse viewScreen asset '" + asset_entry->source_label + "': " + node.error()
                   );
        }
        document.screens.push_back(std::move(*node));
        ++eager_parsed_screens;
        asset_entry->value = boost::json::value(nullptr);
    }
    if (!parsed_document.deferred_screen_files.empty()) {
        for (const auto &[template_id, template_object] : raw_templates) {
            parsed_document.view_template_json.emplace(template_id, boost::json::serialize(template_object));
        }
        for (const auto &[interaction_id, interaction_object] : raw_interactions) {
            parsed_document.interaction_template_json.emplace(
                interaction_id, boost::json::serialize(interaction_object));
        }
    }
    resolved_assets.clear();
    resolved_assets.shrink_to_fit();
    raw_interactions.clear();
    raw_templates.clear();
    stage_end = ParserProfileClock::now();
    GUI_INTERFACE_PROFILE_LOGI(
        "GUI parser profile: base_dir(%1%), stage(parse_flows_screens), flows(%2%), screens(%3%), "
        "eager_parsed(%4%), deferred_files(%5%), elapsed_ms(%6%), total_ms(%7%)",
        base_dir,
        document.screen_flows.size(),
        document.screens.size(),
        eager_parsed_screens,
        deferred_screens,
        parser_profile_elapsed_ms(stage_start, stage_end),
        parser_profile_elapsed_ms(total_start, stage_end)
    );
    log_parser_internal_heap(base_dir, "after parse_flows_screens");

    stage_start = ParserProfileClock::now();
    auto validation = validate_document(document);
    stage_end = ParserProfileClock::now();
    GUI_INTERFACE_PROFILE_LOGI(
        "GUI parser profile: base_dir(%1%), stage(validate_document), elapsed_ms(%2%), total_ms(%3%)",
        base_dir,
        parser_profile_elapsed_ms(stage_start, stage_end),
        parser_profile_elapsed_ms(total_start, stage_end)
    );
    if (!validation.success) {
        return std::unexpected(validation.errors.empty() ? "Invalid GUI document" : validation.errors.front());
    }
    for (auto &screen : document.screens) {
        if (!parsed_document.deferred_screen_files.contains(screen.id)) {
            continue;
        }
        const auto mount_mode = screen.mount_mode;
        const auto screen_id = screen.id;
        screen = Node {};
        screen.type = NodeType::Screen;
        screen.id = screen_id;
        screen.mount_mode = mount_mode;
    }

    stage_end = ParserProfileClock::now();
    GUI_INTERFACE_PROFILE_LOGI(
        "GUI parser profile: base_dir(%1%), stage(total), dependencies(%2%), images(%3%), styles(%4%), "
        "templates(%5%), flows(%6%), screens(%7%), elapsed_ms(%8%), total_ms(%9%)",
        base_dir,
        parsed_document.dependency_files.size(),
        document.images.size(),
        document.styles.size(),
        document.templates.size(),
        document.screen_flows.size(),
        document.screens.size(),
        parser_profile_elapsed_ms(total_start, stage_end),
        parser_profile_elapsed_ms(total_start, stage_end)
    );
    return parsed_document;
}

std::expected<Document, std::string> parse_document(
    std::string_view json, std::string_view base_dir, const Environment &environment)
{
    auto parsed_document = parse_document_impl(json, base_dir, environment, {}, false);
    if (!parsed_document) {
        return std::unexpected(parsed_document.error());
    }
    return parsed_document->document;
}

std::expected<std::vector<FontAsset>, std::string> parse_font_asset_set_json(
    std::string_view json,
    std::string_view base_dir)
{
    boost::system::error_code error_code;
    auto value = boost::json::parse(json, error_code, make_gui_json_storage());
    if (error_code) {
        return std::unexpected("Failed to parse font asset set JSON: " + error_code.message());
    }
    if (!value.is_object()) {
        return std::unexpected("Font asset set JSON must be an object");
    }
    return parse_font_asset_set(value.as_object(), std::filesystem::path(base_dir));
}

std::expected<std::vector<FontAsset>, std::string> parse_font_asset_set_file(std::string_view path)
{
    const std::filesystem::path file_path = std::filesystem::path(path).lexically_normal();
    auto text = read_text_file(file_path);
    if (!text) {
        return std::unexpected(text.error());
    }
    return parse_font_asset_set_json(*text, file_path.parent_path().string());
}

std::expected<std::vector<ImageAsset>, std::string> parse_image_asset_set_json(
    std::string_view json,
    std::string_view base_dir)
{
    boost::system::error_code error_code;
    auto value = boost::json::parse(json, error_code, make_gui_json_storage());
    if (error_code) {
        return std::unexpected("Failed to parse image asset set JSON: " + error_code.message());
    }
    if (!value.is_object()) {
        return std::unexpected("Image asset set JSON must be an object");
    }
    return parse_image_asset_set(value.as_object(), std::filesystem::path(base_dir));
}

std::expected<std::vector<ImageAsset>, std::string> parse_image_asset_set_file(std::string_view path)
{
    const std::filesystem::path file_path = std::filesystem::path(path).lexically_normal();
    auto text = read_text_file(file_path);
    if (!text) {
        return std::unexpected(text.error());
    }
    return parse_image_asset_set_json(*text, file_path.parent_path().string());
}

std::expected<ThemeAsset, std::string> parse_theme_asset_json(
    std::string_view json,
    std::string_view base_dir,
    const Environment &environment)
{
    boost::system::error_code error_code;
    auto value = boost::json::parse(json, error_code, make_gui_json_storage());
    if (error_code) {
        return std::unexpected("Failed to parse theme asset JSON: " + error_code.message());
    }
    if (!value.is_object()) {
        return std::unexpected("Theme asset JSON must be an object");
    }
    const auto &object = value.as_object();
    auto type = parse_string_field(object, "type");
    if (!type) {
        return std::unexpected(type.error());
    }
    if (*type != "theme") {
        return std::unexpected("Theme asset must use type='theme'");
    }
    return parse_theme_asset(object, std::filesystem::path(base_dir).lexically_normal(), environment);
}

std::expected<ThemeAsset, std::string> parse_theme_asset_file(
    std::string_view path,
    const Environment &environment)
{
    const std::filesystem::path file_path = std::filesystem::path(path).lexically_normal();
    auto text = read_text_file(file_path);
    if (!text) {
        return std::unexpected(text.error());
    }
    return parse_theme_asset_json(*text, file_path.parent_path().string(), environment);
}

std::expected<ParsedDocument, std::string> parse_document_file_with_metadata(
    std::string_view path,
    const Environment &environment,
    bool allow_defer)
{
    BROOKESIA_LOG_TRACE_GUARD();

    BROOKESIA_LOGD("Params: path(%1%), environment(%2%)", path, environment);

    const std::filesystem::path file_path = std::filesystem::path(path).lexically_normal();
    auto text = read_text_file(file_path);
    if (!text) {
        return std::unexpected(text.error());
    }
    GUI_INTERFACE_PROFILE_LOGI("Parsing GUI root file '%1%'", file_path.string());
    std::vector<std::string> dependency_files;
    dependency_files.push_back(file_path.string());
    return parse_document_impl(
               *text, file_path.parent_path().string(), environment, std::move(dependency_files), allow_defer);
}

std::expected<Document, std::string> parse_document_file(std::string_view path, const Environment &environment)
{
    const std::filesystem::path file_path = std::filesystem::path(path).lexically_normal();
    auto text = read_text_file(file_path);
    if (!text) {
        return std::unexpected(text.error());
    }
    std::vector<std::string> dependency_files;
    dependency_files.push_back(file_path.string());
    auto parsed_document = parse_document_impl(
                               *text, file_path.parent_path().string(), environment, std::move(dependency_files), false);
    if (!parsed_document) {
        return std::unexpected(parsed_document.error());
    }
    return parsed_document->document;
}

static std::expected<TemplateRawMap, std::string> restore_template_map(
    const std::map<std::string, std::string> &json_by_id)
{
    TemplateRawMap restored;
    for (const auto &[id, text] : json_by_id) {
        boost::system::error_code error_code;
        auto value = boost::json::parse(text, error_code, make_gui_json_storage());
        if (error_code || !value.is_object()) {
            return std::unexpected("Failed to restore viewTemplate '" + id + "'");
        }
        restored.emplace(id, value.as_object());
    }
    return restored;
}

static std::expected<InteractionTemplateRawMap, std::string> restore_interaction_map(
    const std::map<std::string, std::string> &json_by_id)
{
    InteractionTemplateRawMap restored;
    for (const auto &[id, text] : json_by_id) {
        boost::system::error_code error_code;
        auto value = boost::json::parse(text, error_code, make_gui_json_storage());
        if (error_code || !value.is_object()) {
            return std::unexpected("Failed to restore interactionTemplate '" + id + "'");
        }
        restored.emplace(id, value.as_object());
    }
    return restored;
}

std::expected<Node, std::string> parse_view_screen_file(
    std::string_view path,
    const boost::json::value &constants,
    const Environment &environment,
    const std::map<std::string, std::string> &view_template_json,
    const std::map<std::string, std::string> &interaction_template_json)
{
    const std::filesystem::path file_path = std::filesystem::path(path).lexically_normal();
    auto text = parser_detail::read_text_file(file_path);
    if (!text) {
        return std::unexpected(text.error());
    }
    auto parsed_json = parser_detail::parse_json_text(file_path, *text);
    if (!parsed_json) {
        return std::unexpected(parsed_json.error());
    }
    auto replace_result = parser_detail::substitute_references(*parsed_json, constants, environment);
    if (!replace_result) {
        return std::unexpected(replace_result.error());
    }
    if (!parsed_json->is_object()) {
        return std::unexpected("ViewScreen file must contain a JSON object: " + file_path.generic_string());
    }
    auto templates = restore_template_map(view_template_json);
    if (!templates) {
        return std::unexpected(templates.error());
    }
    auto interactions = restore_interaction_map(interaction_template_json);
    if (!interactions) {
        return std::unexpected(interactions.error());
    }
    std::vector<std::string> template_stack;
    return parser_detail::parse_view_node(
               parsed_json->as_object(),
               environment,
               *templates,
               *interactions,
               template_stack,
               NodeType::Screen
           );
}

} // namespace esp_brookesia::gui
