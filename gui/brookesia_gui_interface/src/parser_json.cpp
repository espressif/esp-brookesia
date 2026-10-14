/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "private/parser_impl.hpp"
#include "private/gui_json_storage.hpp"

#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <system_error>

namespace esp_brookesia::gui::parser_detail {

namespace {

// Bound how many root-asset files one Storage batch holds. Do not also cap by
// byte size: that needs file_size() on the caller, and on ESP without PSRAM XIP
// flash metadata I/O from a PSRAM stack asserts esp_task_stack_is_sane_cache_disabled.
constexpr size_t kRootAssetReadBatchMaxFiles = 8;

bool can_read_text_on_caller()
{
#if !defined(ESP_PLATFORM)
    return true;
#elif defined(CONFIG_SPIRAM_XIP_FROM_PSRAM) && CONFIG_SPIRAM_XIP_FROM_PSRAM
    return true;
#else
    return false;
#endif
}

std::expected<std::string, std::string> read_text_file_local(const std::filesystem::path &path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return std::unexpected("Failed to open file: " + path.generic_string());
    }

    std::string text;
    std::error_code size_error;
    const auto expected_size = std::filesystem::file_size(path, size_error);
    if (!size_error) {
        text.reserve(static_cast<size_t>(expected_size));
    }
    text.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    if (input.bad()) {
        return std::unexpected("Failed to read file: " + path.generic_string());
    }
    return text;
}

} // namespace

std::expected<std::string, std::string> read_text_file(const std::filesystem::path &path)
{
    BROOKESIA_LOG_TRACE_GUARD();

    BROOKESIA_LOGD("Params: path(%1%)", path.string());

    const auto start = ParserProfileClock::now();
    std::expected<std::string, std::string> result = std::unexpected("Failed to read file");
    const char *via = "storage";
    if (can_read_text_on_caller()) {
        via = "local";
        result = read_text_file_local(path);
        if (!result) {
            via = "storage";
            result = StorageHelper::fs_read_text(path.generic_string());
        }
    } else {
        result = StorageHelper::fs_read_text(path.generic_string());
    }
    const auto end = ParserProfileClock::now();
    if (!result) {
        BROOKESIA_LOGE("Failed to read file: '%1%', error: %2%", path.string(), result.error());
        return std::unexpected("Failed to read file: " + path.string() + ", error: " + result.error());
    }

    GUI_INTERFACE_PROFILE_LOGI(
        "GUI parser file profile: file(%1%), stage(read_text), bytes(%2%), elapsed_ms(%3%), via(%4%)",
        path.string(),
        result->size(),
        parser_profile_elapsed_ms(start, end),
        via
    );
    return std::move(result).value();
}

std::expected<std::vector<std::string>, std::string> read_text_files(const std::vector<std::filesystem::path> &paths)
{
    std::vector<std::string> contents(paths.size());
    if (paths.empty()) {
        return contents;
    }

    const auto start = ParserProfileClock::now();
    const char *via = "storage_batch";
    std::vector<size_t> missing;
    missing.reserve(paths.size());
    if (can_read_text_on_caller()) {
        via = "local";
        for (size_t i = 0; i < paths.size(); ++i) {
            auto one = read_text_file_local(paths[i]);
            if (!one) {
                missing.push_back(i);
                continue;
            }
            contents[i] = std::move(*one);
        }
    } else {
        for (size_t i = 0; i < paths.size(); ++i) {
            missing.push_back(i);
        }
    }
    if (!missing.empty()) {
        std::vector<std::string> path_strings;
        path_strings.reserve(missing.size());
        for (const size_t index : missing) {
            path_strings.push_back(paths[index].generic_string());
        }
        // fs_read_text_many binds Storage itself; only retry paths that failed locally.
        auto batch = StorageHelper::fs_read_text_many(path_strings);
        if (!batch) {
            return std::unexpected(batch.error());
        }
        if (batch->size() != missing.size()) {
            return std::unexpected("GUI parser read_text_batch size mismatch");
        }
        for (size_t i = 0; i < missing.size(); ++i) {
            contents[missing[i]] = std::move((*batch)[i]);
        }
        via = (missing.size() == paths.size()) ? "storage_batch" : "local+storage";
    }
    const auto end = ParserProfileClock::now();

    size_t total_bytes = 0;
    for (const auto &text : contents) {
        total_bytes += text.size();
    }
    GUI_INTERFACE_PROFILE_LOGI(
        "GUI parser file profile: stage(read_text_batch), files(%1%), bytes(%2%), elapsed_ms(%3%), via(%4%)",
        paths.size(),
        total_bytes,
        parser_profile_elapsed_ms(start, end),
        via
    );
    return contents;
}

std::expected<boost::json::value, std::string> parse_json_text(
    const std::filesystem::path &path,
    const std::string &text)
{
    const auto start = ParserProfileClock::now();
    boost::system::error_code error_code;
    boost::json::value value = boost::json::parse(text, error_code, make_gui_json_storage());
    const auto end = ParserProfileClock::now();
    if (error_code) {
        BROOKESIA_LOGE("Failed to parse JSON file '%1%': %2%", path.string(), error_code.message());
        return std::unexpected("Failed to parse JSON file '" + path.string() + "': " + error_code.message());
    }

    GUI_INTERFACE_PROFILE_LOGI(
        "GUI parser file profile: file(%1%), stage(parse_json), bytes(%2%), elapsed_ms(%3%)",
        path.string(),
        text.size(),
        parser_profile_elapsed_ms(start, end)
    );
    return value;
}

std::filesystem::path resolve_path(
    const std::filesystem::path &base_dir, const std::string &path_string)
{
    if (path_string.size() >= 2 &&
            std::isalpha(static_cast<unsigned char>(path_string[0])) != 0 &&
            path_string[1] == ':') {
        return std::filesystem::path(path_string).lexically_normal();
    }
    return (base_dir / path_string).lexically_normal();
}

std::string normalize_object_key(std::string_view key)
{
    std::string normalized;
    normalized.reserve(key.size() + 4);
    for (char ch : key) {
        if (std::isupper(static_cast<unsigned char>(ch)) != 0) {
            if (!normalized.empty()) {
                normalized.push_back('_');
            }
            normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
        } else {
            normalized.push_back(ch);
        }
    }
    return normalized;
}

std::optional<std::string> make_camel_case_key(std::string_view key)
{
    if (key.find('_') == std::string_view::npos) {
        return std::nullopt;
    }

    std::string camel;
    camel.reserve(key.size());
    bool uppercase_next = false;
    for (const auto ch : key) {
        if (ch == '_') {
            uppercase_next = true;
            continue;
        }
        if (uppercase_next) {
            camel.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
            uppercase_next = false;
        } else {
            camel.push_back(ch);
        }
    }
    return camel;
}

bool is_valid_color_literal(std::string_view color)
{
    if (color.empty()) {
        return true;
    }
    if (color.size() != 7 || color.front() != '#') {
        return false;
    }
    for (size_t i = 1; i < color.size(); ++i) {
        if (std::isxdigit(static_cast<unsigned char>(color[i])) == 0) {
            return false;
        }
    }
    return true;
}

boost::json::object::const_iterator find_key(const boost::json::object &object, std::string_view key)
{
    auto it = object.find(key);
    if (it != object.end()) {
        return it;
    }
    if (auto camel_key = make_camel_case_key(key); camel_key.has_value()) {
        it = object.find(*camel_key);
        if (it != object.end()) {
            return it;
        }
    }

    for (auto current = object.begin(); current != object.end(); ++current) {
        if (normalize_object_key(current->key()) == key) {
            return current;
        }
    }

    return object.end();
}

boost::json::object::iterator find_key(boost::json::object &object, std::string_view key)
{
    auto it = object.find(key);
    if (it != object.end()) {
        return it;
    }
    if (auto camel_key = make_camel_case_key(key); camel_key.has_value()) {
        it = object.find(*camel_key);
        if (it != object.end()) {
            return it;
        }
    }

    for (auto current = object.begin(); current != object.end(); ++current) {
        if (normalize_object_key(current->key()) == key) {
            return current;
        }
    }

    return object.end();
}

const boost::json::value *find_child_value(const boost::json::object &object, std::string_view key)
{
    auto it = find_key(object, key);
    if (it == object.end()) {
        return nullptr;
    }
    return &it->value();
}

std::expected<void, std::string> append_root_asset_entries(
    const boost::json::object &object,
    std::string_view key,
    const std::filesystem::path &base_dir,
    boost::unordered_flat_set<std::string> &loaded_paths,
    std::vector<std::string> *dependency_files,
    std::vector<RootAssetEntry> &ordered_entries,
    std::string_view inline_entry_label)
{
    const auto *value = find_child_value(object, key);
    if (value == nullptr) {
        return {};
    }
    if (!value->is_array()) {
        return std::unexpected("Field '" + std::string(key) + "' must be an array");
    }

    std::vector<PendingRootAssetEntry> pending_entries;
    size_t inline_index = 0;
    for (const auto &entry : value->as_array()) {
        if (entry.is_string()) {
            const auto path = resolve_path(base_dir, entry.as_string().c_str());
            const auto path_string = path.generic_string();
            if (!loaded_paths.insert(path_string).second) {
                continue;
            }
            if (dependency_files != nullptr) {
                dependency_files->push_back(path_string);
            }
            pending_entries.push_back(PendingRootAssetEntry{
                .value = {},
                .path = path,
                .base_dir = path.parent_path(),
                .source_label = path_string,
                .file_backed = true,
            });
            continue;
        }

        if (!entry.is_object()) {
            return std::unexpected(
                       "Field '" + std::string(key) + "' entries must be either file paths or asset objects"
                   );
        }

        pending_entries.push_back(PendingRootAssetEntry{
            .value = entry,
            .path = {},
            .base_dir = base_dir,
            .source_label = std::string(inline_entry_label) + " #" + std::to_string(inline_index),
            .file_backed = false,
        });
        ++inline_index;
    }

    // On Storage-worker paths (no caller-local FS), prefetch file assets in
    // file-count batches. Size/stat stays off the caller. On PC / PSRAM XIP,
    // keep one-file-at-a-time. Inline entries keep their original relative order.
    const bool use_storage_batches = !can_read_text_on_caller();
    size_t pending_index = 0;
    while (pending_index < pending_entries.size()) {
        auto &pending_entry = pending_entries[pending_index];
        if (!pending_entry.file_backed) {
            ordered_entries.push_back(RootAssetEntry{
                .value = std::move(pending_entry.value),
                .base_dir = std::move(pending_entry.base_dir),
                .source_label = std::move(pending_entry.source_label),
            });
            ++pending_index;
            continue;
        }

        if (!use_storage_batches) {
            auto one = read_text_file(pending_entry.path);
            if (!one) {
                return std::unexpected(one.error());
            }
            std::string text = std::move(*one);
            auto asset_value = parse_json_text(pending_entry.path, text);
            text.clear();
            text.shrink_to_fit();
            if (!asset_value) {
                return std::unexpected(asset_value.error());
            }
            if (!asset_value->is_object()) {
                return std::unexpected(
                           "Asset file must contain a JSON object: " + pending_entry.path.generic_string()
                       );
            }
            ordered_entries.push_back(RootAssetEntry{
                .value = std::move(*asset_value),
                .base_dir = std::move(pending_entry.base_dir),
                .source_label = std::move(pending_entry.source_label),
            });
            ++pending_index;
            continue;
        }

        std::vector<std::filesystem::path> batch_paths;
        std::vector<size_t> batch_indices;
        while (pending_index < pending_entries.size() && pending_entries[pending_index].file_backed) {
            if (batch_paths.size() >= kRootAssetReadBatchMaxFiles) {
                break;
            }
            batch_paths.push_back(pending_entries[pending_index].path);
            batch_indices.push_back(pending_index);
            ++pending_index;
        }

        // read_text_files uses StorageHelper::fs_read_text_many on this path.
        auto batch = read_text_files(batch_paths);
        if (!batch) {
            return std::unexpected(batch.error());
        }
        if (batch->size() != batch_paths.size()) {
            return std::unexpected("GUI parser read_text_batch size mismatch");
        }

        for (size_t batch_offset = 0; batch_offset < batch_indices.size(); ++batch_offset) {
            auto &file_entry = pending_entries[batch_indices[batch_offset]];
            std::string text = std::move((*batch)[batch_offset]);
            auto asset_value = parse_json_text(file_entry.path, text);
            text.clear();
            text.shrink_to_fit();
            if (!asset_value) {
                return std::unexpected(asset_value.error());
            }
            if (!asset_value->is_object()) {
                return std::unexpected(
                           "Asset file must contain a JSON object: " + file_entry.path.generic_string()
                       );
            }
            ordered_entries.push_back(RootAssetEntry{
                .value = std::move(*asset_value),
                .base_dir = std::move(file_entry.base_dir),
                .source_label = std::move(file_entry.source_label),
            });
        }
        batch->clear();
    }

    return {};
}

std::expected<std::vector<std::string>, std::string> parse_string_array(
    const boost::json::object &object, std::string_view key)
{
    std::vector<std::string> result;
    const auto *value = find_child_value(object, key);
    if (value == nullptr) {
        return result;
    }
    if (!value->is_array()) {
        return std::unexpected("Field '" + std::string(key) + "' must be an array");
    }

    for (const auto &entry : value->as_array()) {
        if (!entry.is_string()) {
            return std::unexpected("Field '" + std::string(key) + "' must only contain strings");
        }
        result.emplace_back(entry.as_string().c_str());
    }

    return result;
}

void merge_json(boost::json::value &destination, const boost::json::value &source)
{
    if (destination.is_object() && source.is_object()) {
        auto &destination_object = destination.as_object();
        for (const auto &[key, value] : source.as_object()) {
            auto it = destination_object.find(key);
            if (it == destination_object.end()) {
                destination_object.emplace(key, value);
            } else {
                merge_json(it->value(), value);
            }
        }
        return;
    }

    destination = source;
}

void merge_json(boost::json::value &destination, boost::json::value &&source)
{
    if (destination.is_object() && source.is_object()) {
        auto &destination_object = destination.as_object();
        auto &source_object = source.as_object();
        for (auto it = source_object.begin(); it != source_object.end();) {
            auto dest_it = destination_object.find(it->key());
            if (dest_it == destination_object.end()) {
                destination_object.emplace(it->key(), std::move(it->value()));
                it = source_object.erase(it);
            } else {
                merge_json(dest_it->value(), std::move(it->value()));
                it = source_object.erase(it);
            }
        }
        return;
    }

    destination = std::move(source);
}

std::string join_json_path(std::string_view prefix, std::string_view key)
{
    if (prefix.empty()) {
        return std::string(key);
    }
    return std::string(prefix) + "." + std::string(key);
}

std::expected<void, std::string> flatten_color_constants(
    const boost::json::value &value,
    std::string_view prefix,
    std::map<std::string, std::string> &colors)
{
    if (value.is_string()) {
        const std::string key(prefix);
        if (key.empty()) {
            return std::unexpected("Theme color key must not be empty");
        }
        const std::string color = value.as_string().c_str();
        if (is_reference_string(color)) {
            return std::unexpected("Theme color '" + key + "' must not reference another resource");
        }
        if (!is_valid_color_literal(color)) {
            return std::unexpected("Theme color '" + key + "' has invalid value: " + color);
        }
        colors.insert_or_assign(key, color);
        return {};
    }

    if (!value.is_object()) {
        return std::unexpected("Theme color '" + std::string(prefix) + "' must be a string or object");
    }

    for (const auto &[child_key, child_value] : value.as_object()) {
        const std::string key(child_key.begin(), child_key.end());
        if (key.empty()) {
            return std::unexpected("Theme color key must not be empty");
        }
        auto result = flatten_color_constants(child_value, join_json_path(prefix, key), colors);
        if (!result) {
            return result;
        }
    }
    return {};
}

const boost::json::value *resolve_constant(
    const boost::json::value &constants, std::string_view path)
{
    const boost::json::value *current = &constants;
    size_t begin = 0;
    while (begin <= path.size()) {
        const size_t end = path.find('.', begin);
        const std::string_view segment = path.substr(begin, end == std::string_view::npos ? path.size() - begin : end - begin);
        if (!current->is_object()) {
            return nullptr;
        }

        auto it = current->as_object().find(segment);
        if (it == current->as_object().end()) {
            return nullptr;
        }
        current = &it->value();
        if (end == std::string_view::npos) {
            break;
        }
        begin = end + 1;
    }

    return current;
}

bool is_reference_string(std::string_view text)
{
    return text.size() >= 4 && text.starts_with("${") && text.ends_with('}');
}

bool is_expression_string(std::string_view text)
{
    return text.size() >= 9 && text.starts_with("${expr(") && text.ends_with(")}");
}

std::string trim_expression_text(std::string_view text)
{
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) {
        text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) {
        text.remove_suffix(1);
    }
    return std::string(text);
}

std::string format_expression_number(double value)
{
    if (std::fabs(value) < 0.000001) {
        value = 0;
    }

    std::ostringstream stream;
    stream << value;
    auto text = stream.str();
    const auto dot = text.find('.');
    if (dot != std::string::npos) {
        while (!text.empty() && text.back() == '0') {
            text.pop_back();
        }
        if (!text.empty() && text.back() == '.') {
            text.pop_back();
        }
    }
    return text;
}

boost::json::value environment_reference_value(const Environment &environment, std::string_view path)
{
    const auto density = std::fabs(environment.density) < 0.000001F ? 1.0F : environment.density;
    if (path == "widthPx") {
        return boost::json::value(environment.width_px);
    }
    if (path == "heightPx") {
        return boost::json::value(environment.height_px);
    }
    if (path == "widthDp") {
        return boost::json::value(format_expression_number(
                                      static_cast<double>(environment.width_px) / static_cast<double>(density)
                                  ) + "dp");
    }
    if (path == "heightDp") {
        return boost::json::value(format_expression_number(
                                      static_cast<double>(environment.height_px) / static_cast<double>(density)
                                  ) + "dp");
    }
    if (path == "density") {
        return boost::json::value(environment.density);
    }
    if (path == "fontScale") {
        return boost::json::value(environment.font_scale);
    }
    if (path == "language") {
        return boost::json::value(environment.language);
    }
    if (path == "theme") {
        return boost::json::value(environment.theme_id);
    }
    return nullptr;
}

std::expected<ExpressionValue, std::string> parse_expression_numeric_text(std::string_view text)
{
    auto trimmed = trim_expression_text(text);
    ExpressionUnit unit = ExpressionUnit::None;
    if (trimmed.ends_with("dp")) {
        trimmed.resize(trimmed.size() - 2);
        unit = ExpressionUnit::Dp;
    }
    if (trimmed.empty()) {
        return std::unexpected("Expression numeric value is empty");
    }

    char *parse_end = nullptr;
    errno = 0;
    const double value = std::strtod(trimmed.c_str(), &parse_end);
    if (errno != 0 || parse_end == trimmed.c_str() || *parse_end != '\0') {
        return std::unexpected("Expression numeric value must be a number or dp value: " + std::string(text));
    }

    return ExpressionValue{.value = value, .unit = unit};
}

std::expected<ExpressionScalar, std::string> expression_value_from_json(
    const boost::json::value &value,
    std::string_view reference)
{
    if (value.is_int64()) {
        return ExpressionValue{.value = static_cast<double>(value.as_int64()), .unit = ExpressionUnit::None};
    }
    if (value.is_uint64()) {
        return ExpressionValue{.value = static_cast<double>(value.as_uint64()), .unit = ExpressionUnit::None};
    }
    if (value.is_double()) {
        return ExpressionValue{.value = value.as_double(), .unit = ExpressionUnit::None};
    }
    if (value.is_bool()) {
        return value.as_bool();
    }
    if (value.is_string()) {
        auto numeric = parse_expression_numeric_text(value.as_string().c_str());
        if (numeric) {
            return *numeric;
        }
        return std::string(value.as_string().c_str());
    }
    return std::unexpected(
               "Expression reference '" + std::string(reference) + "' must resolve to a number, boolean, or string"
           );
}

boost::json::value expression_value_to_json(const ExpressionScalar &scalar)
{
    if (std::holds_alternative<std::string>(scalar)) {
        return boost::json::value(std::get<std::string>(scalar));
    }
    if (std::holds_alternative<bool>(scalar)) {
        return boost::json::value(std::get<bool>(scalar));
    }

    const auto &value = std::get<ExpressionValue>(scalar);
    if (value.unit == ExpressionUnit::Dp) {
        return boost::json::value(format_expression_number(value.value) + "dp");
    }

    const double rounded = std::round(value.value);
    if (std::fabs(value.value - rounded) < 0.000001) {
        return boost::json::value(static_cast<int64_t>(rounded));
    }
    return boost::json::value(value.value);
}

std::expected<boost::json::value, std::string> evaluate_expression_string(
    std::string_view text,
    const boost::json::value &constants,
    const Environment &environment)
{
    const std::string_view expression(text.data() + 7, text.size() - 9);
    ConstantExpressionParser parser(expression, constants, environment);
    auto result = parser.parse();
    if (!result) {
        return std::unexpected("Failed to evaluate expression '" + std::string(text) + "': " + result.error());
    }
    return expression_value_to_json(*result);
}

std::expected<Reference, std::string> parse_reference_string(std::string_view text)
{
    if (!is_reference_string(text)) {
        return std::unexpected("String is not a resource reference");
    }

    const std::string_view key(text.data() + 2, text.size() - 3);
    const size_t separator = key.find('.');
    if (separator == std::string_view::npos) {
        return std::unexpected(
                   "Reference '" + std::string(text) + "' must use an explicit namespace; use '${constant." +
                   std::string(key) + "}' for constants"
               );
    }

    const std::string_view namespace_name = key.substr(0, separator);
    const std::string_view path = key.substr(separator + 1);
    if (namespace_name.empty() || path.empty()) {
        return std::unexpected("Reference '" + std::string(text) + "' must be in the form '${namespace.path}'");
    }
    if (namespace_name != "constant" && namespace_name != "env" && namespace_name != "color" &&
            namespace_name != "font" &&
            namespace_name != "image" && namespace_name != "view") {
        return std::unexpected(
                   "Unsupported reference namespace '" + std::string(namespace_name) +
                   "' in '" + std::string(text) +
                   "'; expected 'constant', 'env', 'color', 'font', 'image', or 'view'"
               );
    }

    return Reference {
        .namespace_name = std::string(namespace_name),
        .path = std::string(path),
    };
}

bool is_style_font_path(const std::vector<std::string> &path)
{
    return path.size() >= 2 && path[path.size() - 2] == "style" && path.back() == "font";
}

bool is_theme_style_font_path(const std::vector<std::string> &path)
{
    return path.size() >= 3 && path[path.size() - 3] == "styles" && path.back() == "font";
}

bool is_image_props_src_path(const std::vector<std::string> &path)
{
    return path.size() >= 2 && path[path.size() - 2] == "imageProps" && path.back() == "src";
}

bool is_keyboard_key_image_path(const std::vector<std::string> &path)
{
    return path.size() >= 2 && path[path.size() - 2] == "[]" && path.back() == "image";
}

bool is_placement_relative_to_path(const std::vector<std::string> &path)
{
    if (path.size() < 2 || path[path.size() - 2] != "placement") {
        return false;
    }
    return path.back() == "relativeTo" || path.back() == "relative_to";
}

bool is_font_fallback_path(const std::vector<std::string> &path)
{
    return path.size() >= 2 && path[path.size() - 2] == "fallbacks";
}

bool is_style_color_path(const std::vector<std::string> &path)
{
    if (path.empty()) {
        return false;
    }

    const auto field_name = normalize_object_key(path.back());
    return field_name == "bg_color" || field_name == "bg_gradient_color" || field_name == "text_color" ||
           field_name == "border_color" || field_name == "line_color" || field_name == "arc_color" ||
           field_name == "arc_gradient_color" || field_name == "shadow_color" || field_name == "image_recolor";
}

std::expected<void, std::string> substitute_references(
    boost::json::value &value,
    const boost::json::value &constants,
    const Environment &environment,
    std::vector<std::string> path)
{
    if (value.is_object()) {
        for (auto &[key, child] : value.as_object()) {
            path.emplace_back(key);
            auto result = substitute_references(child, constants, environment, path);
            path.pop_back();
            if (!result) {
                return result;
            }
        }
        return {};
    }

    if (value.is_array()) {
        for (auto &child : value.as_array()) {
            path.emplace_back("[]");
            auto result = substitute_references(child, constants, environment, path);
            path.pop_back();
            if (!result) {
                return result;
            }
        }
        return {};
    }

    if (!value.is_string()) {
        return {};
    }

    const std::string text = value.as_string().c_str();
    if (!is_reference_string(text)) {
        return {};
    }

    if (is_expression_string(text)) {
        auto expression_value = evaluate_expression_string(text, constants, environment);
        if (!expression_value) {
            return std::unexpected(expression_value.error());
        }
        value = *expression_value;
        return {};
    }

    auto reference = parse_reference_string(text);
    if (!reference) {
        return std::unexpected(reference.error());
    }

    if (reference->namespace_name == "font") {
        if (is_style_font_path(path) || is_theme_style_font_path(path) || is_font_fallback_path(path)) {
            return {};
        }
        return std::unexpected("Reference '" + text + "' is only allowed in style.font, theme styles, or font fallbacks");
    }
    if (reference->namespace_name == "image") {
        if (is_image_props_src_path(path) || is_keyboard_key_image_path(path)) {
            return {};
        }
        return std::unexpected("Reference '" + text + "' is only allowed in imageProps.src or keyboard key image");
    }
    if (reference->namespace_name == "view") {
        if (is_placement_relative_to_path(path)) {
            return {};
        }
        return std::unexpected("Reference '" + text + "' is only allowed in placement.relativeTo");
    }
    if (reference->namespace_name == "color") {
        if (!is_style_color_path(path)) {
            return std::unexpected("Reference '" + text + "' is only allowed in style color fields");
        }
        return {};
    }

    boost::json::value env_value;
    const boost::json::value *resolved = nullptr;
    if (reference->namespace_name == "constant") {
        resolved = resolve_constant(constants, reference->path);
        if (resolved == nullptr) {
            return std::unexpected("Failed to resolve constant reference: " + text);
        }
    } else if (reference->namespace_name == "env") {
        env_value = environment_reference_value(environment, reference->path);
        if (env_value.is_null()) {
            return std::unexpected("Failed to resolve environment reference: " + text);
        }
        resolved = &env_value;
    } else {
        return std::unexpected("Unsupported reference namespace '" + reference->namespace_name + "' in '" + text + "'");
    }

    if ((is_image_props_src_path(path) || is_keyboard_key_image_path(path)) && resolved->is_string()) {
        const std::string image_id = resolved->as_string().c_str();
        if (!is_reference_string(image_id)) {
            value = "${image." + image_id + "}";
            return {};
        }
    }

    value = *resolved;
    return {};
}

std::expected<std::string, std::string> parse_resource_reference_field(
    const boost::json::object &object,
    std::string_view key,
    std::string_view namespace_name,
    std::string_view migration_hint)
{
    const auto *value = find_child_value(object, key);
    if (value == nullptr) {
        return std::string();
    }
    if (!value->is_string()) {
        return std::unexpected("Field '" + std::string(key) + "' must be a string");
    }

    const std::string text = value->as_string().c_str();
    if (text.empty()) {
        return std::string();
    }
    if (!is_reference_string(text)) {
        return std::unexpected(
                   "Field '" + std::string(key) + "' must use " + std::string(migration_hint) +
                   "; direct resource ids are no longer supported"
               );
    }

    auto reference = parse_reference_string(text);
    if (!reference) {
        return std::unexpected(reference.error());
    }
    if (reference->namespace_name != namespace_name) {
        return std::unexpected(
                   "Field '" + std::string(key) + "' must use namespace '" + std::string(namespace_name) +
                   "', got '" + reference->namespace_name + "'"
               );
    }

    return reference->path;
}

std::expected<std::string, std::string> parse_view_reference_field(
    const boost::json::object &object,
    std::string_view key)
{
    auto path = parse_resource_reference_field(
                    object,
                    key,
                    "view",
                    "'${view.anchor}' syntax"
                );
    if (!path) {
        return std::unexpected(path.error());
    }
    if (path->find('/') != std::string::npos) {
        return std::unexpected(
                   "Field '" + std::string(key) + "' must use '.' to separate view path segments"
               );
    }
    return path;
}

std::expected<std::vector<std::string>, std::string> parse_resource_reference_array_field(
    const boost::json::object &object,
    std::string_view key,
    std::string_view namespace_name,
    std::string_view migration_hint)
{
    std::vector<std::string> result;
    const auto *value = find_child_value(object, key);
    if (value == nullptr) {
        return result;
    }
    if (!value->is_array()) {
        return std::unexpected("Field '" + std::string(key) + "' must be an array");
    }

    for (const auto &entry : value->as_array()) {
        if (!entry.is_string()) {
            return std::unexpected("Field '" + std::string(key) + "' must only contain strings");
        }
        const std::string text = entry.as_string().c_str();
        if (!is_reference_string(text)) {
            return std::unexpected(
                       "Field '" + std::string(key) + "' entries must use " + std::string(migration_hint) +
                       "; direct resource ids are no longer supported"
                   );
        }
        auto reference = parse_reference_string(text);
        if (!reference) {
            return std::unexpected(reference.error());
        }
        if (reference->namespace_name != namespace_name) {
            return std::unexpected(
                       "Field '" + std::string(key) + "' entries must use namespace '" +
                       std::string(namespace_name) + "', got '" + reference->namespace_name + "'"
                   );
        }
        result.push_back(reference->path);
    }

    return result;
}
}
