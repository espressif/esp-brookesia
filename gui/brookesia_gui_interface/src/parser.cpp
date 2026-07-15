/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/gui_interface/parser.hpp"
#include "brookesia/gui_interface/validator.hpp"
#include "brookesia/gui_interface/macro_configs.h"
#if !BROOKESIA_GUI_INTERFACE_PARSER_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iterator>
#include <map>
#include <optional>
#include <sstream>
#include <unordered_set>
#include <utility>
#include <variant>

#include "boost/json.hpp"
#include "boost/unordered/unordered_flat_map.hpp"
#include "boost/unordered/unordered_flat_set.hpp"
#include "brookesia/service_helper/system/storage.hpp"
#include "brookesia/service_manager/service/manager.hpp"

#if BROOKESIA_GUI_INTERFACE_ENABLE_PROFILE_LOG
#   define GUI_INTERFACE_PROFILE_LOGI(...) BROOKESIA_LOGI(__VA_ARGS__)
#else
#   define GUI_INTERFACE_PROFILE_LOGI(...) do { if (false) { BROOKESIA_LOGI(__VA_ARGS__); } } while (0)
#endif

namespace esp_brookesia::gui {

namespace {

using StorageHelper = service::helper::Storage;
using ParserProfileClock = std::chrono::steady_clock;
using FileTextMap = std::map<std::string, std::string>;

constexpr uint32_t STORAGE_BATCH_READ_TEXT_TIMEOUT_MS = 10000;

static int64_t parser_profile_elapsed_ms(
    const ParserProfileClock::time_point &start,
    const ParserProfileClock::time_point &end)
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
}

using VariantValue = std::variant<double, std::string, bool>;

struct Reference {
    std::string namespace_name;
    std::string path;
};

enum class ExpressionUnit {
    None,
    Dp,
};

struct ExpressionValue {
    double value = 0;
    ExpressionUnit unit = ExpressionUnit::None;
};

using ExpressionScalar = std::variant<ExpressionValue, std::string, bool>;

enum class ExpressionTokenType {
    Number,
    Constant,
    String,
    Bool,
    Plus,
    Minus,
    Multiply,
    Divide,
    LeftParen,
    RightParen,
    And,
    Or,
    Not,
    Equal,
    NotEqual,
    Greater,
    Less,
    GreaterEqual,
    LessEqual,
    End,
};

struct ExpressionToken {
    ExpressionTokenType type = ExpressionTokenType::End;
    std::string text;
    ExpressionScalar value;
};

struct RootAssetEntry {
    boost::json::value value;
    std::filesystem::path base_dir;
    std::string source_label;
};

struct PendingRootAssetEntry {
    boost::json::value value;
    std::filesystem::path path;
    std::filesystem::path base_dir;
    std::string source_label;
    bool file_backed = false;
};

struct ResolvedAssetEntry {
    boost::json::value value;
    std::filesystem::path base_dir;
    std::string source_label;
    std::string type;
};

using TemplateRawMap = boost::unordered_flat_map<std::string, boost::json::object>;
using InteractionTemplateRawMap = boost::unordered_flat_map<std::string, boost::json::object>;

enum class TokenType {
    Identifier,
    Number,
    String,
    LeftParen,
    RightParen,
    And,
    Or,
    Not,
    Equal,
    NotEqual,
    Greater,
    Less,
    GreaterEqual,
    LessEqual,
    End,
};

struct Token {
    TokenType type = TokenType::End;
    std::string text;
};

class Lexer {
public:
    explicit Lexer(std::string_view expression)
        : expression_(expression)
    {}

    std::expected<Token, std::string> next()
    {
        skip_spaces();
        if (position_ >= expression_.size()) {
            return Token{.type = TokenType::End, .text = ""};
        }

        const char ch = expression_[position_];
        if (std::isalpha(static_cast<unsigned char>(ch)) || ch == '_') {
            return lex_identifier();
        }
        if (std::isdigit(static_cast<unsigned char>(ch))) {
            return lex_number();
        }
        if (ch == '"') {
            return lex_string();
        }

        if (match("&&")) {
            return Token{.type = TokenType::And, .text = "&&"};
        }
        if (match("||")) {
            return Token{.type = TokenType::Or, .text = "||"};
        }
        if (match("==")) {
            return Token{.type = TokenType::Equal, .text = "=="};
        }
        if (match("!=")) {
            return Token{.type = TokenType::NotEqual, .text = "!="};
        }
        if (match(">=")) {
            return Token{.type = TokenType::GreaterEqual, .text = ">="};
        }
        if (match("<=")) {
            return Token{.type = TokenType::LessEqual, .text = "<="};
        }

        ++position_;
        switch (ch) {
        case '(':
            return Token{.type = TokenType::LeftParen, .text = "("};
        case ')':
            return Token{.type = TokenType::RightParen, .text = ")"};
        case '!':
            return Token{.type = TokenType::Not, .text = "!"};
        case '>':
            return Token{.type = TokenType::Greater, .text = ">"};
        case '<':
            return Token{.type = TokenType::Less, .text = "<"};
        default:
            return std::unexpected("Unsupported token in when expression");
        }
    }

private:
    std::expected<Token, std::string> lex_identifier()
    {
        const size_t begin = position_;
        while (position_ < expression_.size()) {
            const char ch = expression_[position_];
            if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '_') {
                break;
            }
            ++position_;
        }
        return Token{
            .type = TokenType::Identifier,
            .text = std::string(expression_.substr(begin, position_ - begin)),
        };
    }

    std::expected<Token, std::string> lex_number()
    {
        const size_t begin = position_;
        while (position_ < expression_.size()) {
            const char ch = expression_[position_];
            if (!std::isdigit(static_cast<unsigned char>(ch)) && ch != '.') {
                break;
            }
            ++position_;
        }
        return Token{
            .type = TokenType::Number,
            .text = std::string(expression_.substr(begin, position_ - begin)),
        };
    }

    std::expected<Token, std::string> lex_string()
    {
        ++position_;
        std::string value;
        while (position_ < expression_.size()) {
            const char ch = expression_[position_++];
            if (ch == '\\' && position_ < expression_.size()) {
                value.push_back(expression_[position_++]);
                continue;
            }
            if (ch == '"') {
                return Token{.type = TokenType::String, .text = std::move(value)};
            }
            value.push_back(ch);
        }

        return std::unexpected("Unterminated string literal in when expression");
    }

    bool match(std::string_view token)
    {
        if (expression_.substr(position_, token.size()) != token) {
            return false;
        }
        position_ += token.size();
        return true;
    }

    void skip_spaces()
    {
        while (position_ < expression_.size() &&
                std::isspace(static_cast<unsigned char>(expression_[position_])) != 0) {
            ++position_;
        }
    }

    std::string_view expression_;
    size_t position_ = 0;
};

class ExpressionParser {
public:
    ExpressionParser(std::string_view expression, const Environment &environment)
        : lexer_(expression)
        , environment_(environment)
    {}

    std::expected<bool, std::string> parse()
    {
        auto token = lexer_.next();
        if (!token) {
            return std::unexpected(token.error());
        }
        current_ = *token;

        auto value = parse_or();
        if (!value) {
            return value;
        }
        if (current_.type != TokenType::End) {
            return std::unexpected("Unexpected trailing token in when expression");
        }
        return *value;
    }

private:
    std::expected<bool, std::string> parse_or()
    {
        auto lhs = parse_and();
        if (!lhs) {
            return lhs;
        }

        while (current_.type == TokenType::Or) {
            auto advance_result = advance();
            if (!advance_result) {
                return std::unexpected(advance_result.error());
            }
            auto rhs = parse_and();
            if (!rhs) {
                return rhs;
            }
            *lhs = *lhs || *rhs;
        }
        return lhs;
    }

    std::expected<bool, std::string> parse_and()
    {
        auto lhs = parse_unary();
        if (!lhs) {
            return lhs;
        }

        while (current_.type == TokenType::And) {
            auto advance_result = advance();
            if (!advance_result) {
                return std::unexpected(advance_result.error());
            }
            auto rhs = parse_unary();
            if (!rhs) {
                return rhs;
            }
            *lhs = *lhs && *rhs;
        }
        return lhs;
    }

    std::expected<bool, std::string> parse_unary()
    {
        if (current_.type == TokenType::Not) {
            auto advance_result = advance();
            if (!advance_result) {
                return std::unexpected(advance_result.error());
            }
            auto value = parse_unary();
            if (!value) {
                return value;
            }
            return !*value;
        }
        return parse_primary();
    }

    std::expected<bool, std::string> parse_primary()
    {
        if (current_.type == TokenType::LeftParen) {
            auto advance_result = advance();
            if (!advance_result) {
                return std::unexpected(advance_result.error());
            }
            auto value = parse_or();
            if (!value) {
                return value;
            }
            if (current_.type != TokenType::RightParen) {
                return std::unexpected("Expected ')' in when expression");
            }
            advance_result = advance();
            if (!advance_result) {
                return std::unexpected(advance_result.error());
            }
            return value;
        }

        auto lhs = parse_operand();
        if (!lhs) {
            return std::unexpected(lhs.error());
        }

        if (!is_comparison(current_.type)) {
            if (std::holds_alternative<bool>(*lhs)) {
                return std::get<bool>(*lhs);
            }
            return std::unexpected("Expected comparison operator in when expression");
        }

        const TokenType op = current_.type;
        auto advance_result = advance();
        if (!advance_result) {
            return std::unexpected(advance_result.error());
        }

        auto rhs = parse_operand();
        if (!rhs) {
            return std::unexpected(rhs.error());
        }

        return compare(*lhs, *rhs, op);
    }

    std::expected<VariantValue, std::string> parse_operand()
    {
        switch (current_.type) {
        case TokenType::Identifier:
            return parse_identifier();
        case TokenType::Number: {
            const auto value = std::stod(current_.text);
            auto advance_result = advance();
            if (!advance_result) {
                return std::unexpected(advance_result.error());
            }
            return VariantValue(value);
        }
        case TokenType::String: {
            VariantValue value(current_.text);
            auto advance_result = advance();
            if (!advance_result) {
                return std::unexpected(advance_result.error());
            }
            return value;
        }
        default:
            return std::unexpected("Invalid operand in when expression");
        }
    }

    std::expected<VariantValue, std::string> parse_identifier()
    {
        const std::string identifier = current_.text;
        auto advance_result = advance();
        if (!advance_result) {
            return std::unexpected(advance_result.error());
        }

        if (identifier == "true") {
            return VariantValue(true);
        }
        if (identifier == "false") {
            return VariantValue(false);
        }
        if (identifier == "WidthPx") {
            return VariantValue(static_cast<double>(environment_.width_px));
        }
        if (identifier == "HeightPx") {
            return VariantValue(static_cast<double>(environment_.height_px));
        }
        if (identifier == "WidthDp") {
            return VariantValue(static_cast<double>(environment_.width_px) / environment_.density);
        }
        if (identifier == "HeightDp") {
            return VariantValue(static_cast<double>(environment_.height_px) / environment_.density);
        }
        if (identifier == "Language") {
            return VariantValue(environment_.language);
        }
        if (identifier == "Theme") {
            return VariantValue(environment_.theme_id);
        }

        return std::unexpected("Unsupported identifier in when expression: " + identifier);
    }

    static bool is_comparison(TokenType type)
    {
        return type == TokenType::Equal || type == TokenType::NotEqual ||
               type == TokenType::Greater || type == TokenType::Less ||
               type == TokenType::GreaterEqual || type == TokenType::LessEqual;
    }

    static std::expected<bool, std::string> compare(
        const VariantValue &lhs, const VariantValue &rhs, TokenType op
    )
    {
        if (lhs.index() != rhs.index()) {
            return std::unexpected("Mismatched operand types in when expression");
        }

        if (std::holds_alternative<double>(lhs)) {
            return compare_values(std::get<double>(lhs), std::get<double>(rhs), op);
        }
        if (std::holds_alternative<std::string>(lhs)) {
            return compare_values(std::get<std::string>(lhs), std::get<std::string>(rhs), op);
        }

        const bool left = std::get<bool>(lhs);
        const bool right = std::get<bool>(rhs);
        switch (op) {
        case TokenType::Equal:
            return left == right;
        case TokenType::NotEqual:
            return left != right;
        default:
            return std::unexpected("Unsupported bool comparison in when expression");
        }
    }

    template <typename T>
    static bool compare_values(const T &lhs, const T &rhs, TokenType op)
    {
        switch (op) {
        case TokenType::Equal:
            return lhs == rhs;
        case TokenType::NotEqual:
            return lhs != rhs;
        case TokenType::Greater:
            return lhs > rhs;
        case TokenType::Less:
            return lhs < rhs;
        case TokenType::GreaterEqual:
            return lhs >= rhs;
        case TokenType::LessEqual:
            return lhs <= rhs;
        case TokenType::Identifier:
        case TokenType::Number:
        case TokenType::String:
        case TokenType::LeftParen:
        case TokenType::RightParen:
        case TokenType::And:
        case TokenType::Or:
        case TokenType::Not:
        case TokenType::End:
        default:
            return false;
        }
    }

    std::expected<void, std::string> advance()
    {
        auto token = lexer_.next();
        if (!token) {
            return std::unexpected(token.error());
        }
        current_ = *token;
        return {};
    }

    Lexer lexer_;
    Environment environment_;
    Token current_;
};

static std::expected<std::string, std::string> read_text_file(const std::filesystem::path &path)
{
    BROOKESIA_LOG_TRACE_GUARD();

    BROOKESIA_LOGD("Params: path(%1%)", path.string());

    const auto start = ParserProfileClock::now();
    auto result = StorageHelper::fs_read_text(path.generic_string());
    const auto end = ParserProfileClock::now();
    if (!result) {
        BROOKESIA_LOGE("Failed to read file: '%1%', error: %2%", path.string(), result.error());
        return std::unexpected("Failed to read file: " + path.string() + ", error: " + result.error());
    }

    GUI_INTERFACE_PROFILE_LOGI(
        "GUI parser file profile: file(%1%), stage(read_text), bytes(%2%), elapsed_ms(%3%)",
        path.string(),
        result->size(),
        parser_profile_elapsed_ms(start, end)
    );
    return result.value();
}

static std::expected<FileTextMap, std::string> read_text_files(
    const std::vector<std::filesystem::path> &paths)
{
    BROOKESIA_LOG_TRACE_GUARD();

    if (paths.empty()) {
        return FileTextMap();
    }
    if (paths.size() == 1) {
        auto text = read_text_file(paths.front());
        if (!text) {
            return std::unexpected(text.error());
        }
        return FileTextMap({{paths.front().generic_string(), std::move(text.value())}});
    }

    // Keep all flash file I/O on the Storage service worker while avoiding a dedicated batch FS API.
    auto storage_service = service::ServiceManager::get_instance().get_service(StorageHelper::get_name().data());
    if (!storage_service) {
        return std::unexpected("Storage service not found");
    }

    std::vector<std::string> path_strings;
    path_strings.reserve(paths.size());
    std::vector<service::FunctionCall> calls;
    calls.reserve(paths.size());
    const std::string read_text_function_name = BROOKESIA_DESCRIBE_ENUM_TO_STR(StorageHelper::FunctionId::FSReadText);
    const std::string path_param_name = BROOKESIA_DESCRIBE_TO_STR(StorageHelper::FunctionFSPathParam::Path);
    for (const auto &path : paths) {
        auto path_string = path.generic_string();
        calls.push_back({
            .name = read_text_function_name,
            .parameters = {
                {path_param_name, path_string},
            },
        });
        path_strings.push_back(std::move(path_string));
    }

    const auto start = ParserProfileClock::now();
    auto result = storage_service->call_functions_sync(std::move(calls), STORAGE_BATCH_READ_TEXT_TIMEOUT_MS);
    const auto end = ParserProfileClock::now();

    if (result.results.size() > path_strings.size()) {
        return std::unexpected(
                   "Storage FS batch read returned " + std::to_string(result.results.size()) +
                   " result(s) for " + std::to_string(path_strings.size()) + " file(s)"
               );
    }

    FileTextMap files;
    size_t total_bytes = 0;
    for (size_t i = 0; i < result.results.size(); i++) {
        auto &call_result = result.results[i];
        const auto &path_string = path_strings[i];
        if (!call_result.success) {
            return std::unexpected(
                       "Failed to read file: " + path_string + ", error: " + call_result.error_message
                   );
        }
        if (!call_result.data.has_value()) {
            return std::unexpected("Storage FS batch read returned no data for path '" + path_string + "'");
        }
        auto *text = std::get_if<std::string>(&call_result.data.value());
        if (text == nullptr) {
            return std::unexpected("Storage FS batch read returned non-string data for path '" + path_string + "'");
        }
        total_bytes += text->size();
        files.emplace(path_string, std::move(*text));
    }

    if (!result.success) {
        return std::unexpected("Storage FS batch read failed without per-file error");
    }
    if (files.size() != path_strings.size()) {
        return std::unexpected(
                   "Storage FS batch read returned " + std::to_string(files.size()) +
                   " result(s) for " + std::to_string(path_strings.size()) + " file(s)"
               );
    }
    GUI_INTERFACE_PROFILE_LOGI(
        "GUI parser file batch profile: files(%1%), bytes(%2%), elapsed_ms(%3%)",
        files.size(),
        total_bytes,
        parser_profile_elapsed_ms(start, end)
    );
    return files;
}

static std::expected<boost::json::value, std::string> parse_json_text(
    const std::filesystem::path &path,
    const std::string &text)
{
    const auto start = ParserProfileClock::now();
    boost::system::error_code error_code;
    boost::json::value value = boost::json::parse(text, error_code);
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

static std::filesystem::path resolve_path(
    const std::filesystem::path &base_dir, const std::string &path_string)
{
    if (path_string.size() >= 2 &&
            std::isalpha(static_cast<unsigned char>(path_string[0])) != 0 &&
            path_string[1] == ':') {
        return std::filesystem::path(path_string).lexically_normal();
    }
    return (base_dir / path_string).lexically_normal();
}

static std::string normalize_object_key(std::string_view key)
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

static std::optional<std::string> make_camel_case_key(std::string_view key)
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

static bool is_valid_color_literal(std::string_view color)
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

static boost::json::object::const_iterator find_key(const boost::json::object &object, std::string_view key)
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

static boost::json::object::iterator find_key(boost::json::object &object, std::string_view key)
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

static const boost::json::value *find_child_value(const boost::json::object &object, std::string_view key)
{
    auto it = find_key(object, key);
    if (it == object.end()) {
        return nullptr;
    }
    return &it->value();
}

static std::expected<void, std::string> append_root_asset_entries(
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
    std::vector<std::filesystem::path> file_paths;
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
            file_paths.push_back(path);
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

    auto file_texts = read_text_files(file_paths);
    if (!file_texts) {
        return std::unexpected(file_texts.error());
    }

    for (auto &pending_entry : pending_entries) {
        if (!pending_entry.file_backed) {
            ordered_entries.push_back(RootAssetEntry{
                .value = std::move(pending_entry.value),
                .base_dir = std::move(pending_entry.base_dir),
                .source_label = std::move(pending_entry.source_label),
            });
            continue;
        }

        const auto path_string = pending_entry.path.generic_string();
        auto text_it = file_texts->find(path_string);
        if (text_it == file_texts->end()) {
            return std::unexpected("Storage FS batch read did not return asset file: " + path_string);
        }

        auto asset_value = parse_json_text(pending_entry.path, text_it->second);
        if (!asset_value) {
            return std::unexpected(asset_value.error());
        }
        if (!asset_value->is_object()) {
            return std::unexpected("Asset file must contain a JSON object: " + path_string);
        }

        ordered_entries.push_back(RootAssetEntry{
            .value = std::move(*asset_value),
            .base_dir = std::move(pending_entry.base_dir),
            .source_label = std::move(pending_entry.source_label),
        });
    }

    return {};
}

static std::expected<std::vector<std::string>, std::string> parse_string_array(
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

static void merge_json(boost::json::value &destination, const boost::json::value &source)
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

static bool is_reference_string(std::string_view text);

static std::string join_json_path(std::string_view prefix, std::string_view key)
{
    if (prefix.empty()) {
        return std::string(key);
    }
    return std::string(prefix) + "." + std::string(key);
}

static std::expected<void, std::string> flatten_color_constants(
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

static const boost::json::value *resolve_constant(
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

static bool is_reference_string(std::string_view text)
{
    return text.size() >= 4 && text.starts_with("${") && text.ends_with('}');
}

static bool is_expression_string(std::string_view text)
{
    return text.size() >= 9 && text.starts_with("${expr(") && text.ends_with(")}");
}

static std::string trim_expression_text(std::string_view text)
{
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) {
        text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) {
        text.remove_suffix(1);
    }
    return std::string(text);
}

static std::string format_expression_number(double value)
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

static boost::json::value environment_reference_value(const Environment &environment, std::string_view path)
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

static std::expected<ExpressionValue, std::string> parse_expression_numeric_text(std::string_view text)
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

static std::expected<ExpressionScalar, std::string> expression_value_from_json(
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

class ExpressionLexer {
public:
    ExpressionLexer(std::string_view expression, const boost::json::value &constants, const Environment &environment)
        : expression_(expression)
        , constants_(constants)
        , environment_(environment)
    {}

    std::expected<ExpressionToken, std::string> next()
    {
        skip_spaces();
        if (position_ >= expression_.size()) {
            return ExpressionToken{.type = ExpressionTokenType::End, .text = "", .value = {}};
        }

        const char ch = expression_[position_];
        if (match("&&")) {
            return ExpressionToken{.type = ExpressionTokenType::And, .text = "&&", .value = {}};
        }
        if (match("||")) {
            return ExpressionToken{.type = ExpressionTokenType::Or, .text = "||", .value = {}};
        }
        if (match("==")) {
            return ExpressionToken{.type = ExpressionTokenType::Equal, .text = "==", .value = {}};
        }
        if (match("!=")) {
            return ExpressionToken{.type = ExpressionTokenType::NotEqual, .text = "!=", .value = {}};
        }
        if (match(">=")) {
            return ExpressionToken{.type = ExpressionTokenType::GreaterEqual, .text = ">=", .value = {}};
        }
        if (match("<=")) {
            return ExpressionToken{.type = ExpressionTokenType::LessEqual, .text = "<=", .value = {}};
        }
        if (ch == '+') {
            ++position_;
            return ExpressionToken{.type = ExpressionTokenType::Plus, .text = "+", .value = {}};
        }
        if (ch == '-') {
            ++position_;
            return ExpressionToken{.type = ExpressionTokenType::Minus, .text = "-", .value = {}};
        }
        if (ch == '*') {
            ++position_;
            return ExpressionToken{.type = ExpressionTokenType::Multiply, .text = "*", .value = {}};
        }
        if (ch == '/') {
            ++position_;
            return ExpressionToken{.type = ExpressionTokenType::Divide, .text = "/", .value = {}};
        }
        if (ch == '(') {
            ++position_;
            return ExpressionToken{.type = ExpressionTokenType::LeftParen, .text = "(", .value = {}};
        }
        if (ch == ')') {
            ++position_;
            return ExpressionToken{.type = ExpressionTokenType::RightParen, .text = ")", .value = {}};
        }
        if (ch == '!') {
            ++position_;
            return ExpressionToken{.type = ExpressionTokenType::Not, .text = "!", .value = {}};
        }
        if (ch == '>') {
            ++position_;
            return ExpressionToken{.type = ExpressionTokenType::Greater, .text = ">", .value = {}};
        }
        if (ch == '<') {
            ++position_;
            return ExpressionToken{.type = ExpressionTokenType::Less, .text = "<", .value = {}};
        }
        if (ch == '"') {
            return parse_string();
        }
        if (std::isalpha(static_cast<unsigned char>(ch)) || ch == '_') {
            return parse_identifier();
        }
        if (ch == '$') {
            return parse_reference();
        }
        if (std::isdigit(static_cast<unsigned char>(ch)) || ch == '.') {
            return parse_number();
        }

        return std::unexpected(
                   "Unexpected character in expression at position " + std::to_string(position_) + ": " +
                   std::string(1, ch)
               );
    }

private:
    bool match(std::string_view token)
    {
        if (expression_.substr(position_, token.size()) != token) {
            return false;
        }
        position_ += token.size();
        return true;
    }

    void skip_spaces()
    {
        while (position_ < expression_.size() &&
                std::isspace(static_cast<unsigned char>(expression_[position_]))) {
            ++position_;
        }
    }

    std::expected<ExpressionToken, std::string> parse_identifier()
    {
        const size_t begin = position_;
        while (position_ < expression_.size()) {
            const char ch = expression_[position_];
            if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '_') {
                break;
            }
            ++position_;
        }

        const auto text = std::string(expression_.substr(begin, position_ - begin));
        if (text == "true") {
            return ExpressionToken{.type = ExpressionTokenType::Bool, .text = text, .value = true};
        }
        if (text == "false") {
            return ExpressionToken{.type = ExpressionTokenType::Bool, .text = text, .value = false};
        }
        return std::unexpected(
                   "Unsupported identifier in expression: " + text +
                   "; use '${env.<field>}' or '${constant.<path>}' references"
               );
    }

    std::expected<ExpressionToken, std::string> parse_string()
    {
        ++position_;
        std::string value;
        while (position_ < expression_.size()) {
            const char ch = expression_[position_++];
            if (ch == '\\' && position_ < expression_.size()) {
                value.push_back(expression_[position_++]);
                continue;
            }
            if (ch == '"') {
                return ExpressionToken{
                    .type = ExpressionTokenType::String,
                    .text = value,
                    .value = value,
                };
            }
            value.push_back(ch);
        }

        return std::unexpected("Unterminated string literal in expression");
    }

    std::expected<ExpressionToken, std::string> parse_number()
    {
        const size_t begin = position_;
        bool has_dot = false;
        while (position_ < expression_.size()) {
            const char ch = expression_[position_];
            if (std::isdigit(static_cast<unsigned char>(ch))) {
                ++position_;
                continue;
            }
            if (ch == '.' && !has_dot) {
                has_dot = true;
                ++position_;
                continue;
            }
            break;
        }
        if (position_ + 2 <= expression_.size() && expression_.substr(position_, 2) == "dp") {
            position_ += 2;
        }

        const auto text = expression_.substr(begin, position_ - begin);
        auto value = parse_expression_numeric_text(text);
        if (!value) {
            return std::unexpected(value.error());
        }
        return ExpressionToken{
            .type = ExpressionTokenType::Number,
            .text = std::string(text),
            .value = *value,
        };
    }

    std::expected<ExpressionToken, std::string> parse_reference()
    {
        const auto rest = expression_.substr(position_);
        constexpr std::string_view CONSTANT_PREFIX = "${constant.";
        constexpr std::string_view ENV_PREFIX = "${env.";
        const bool is_constant = rest.starts_with(CONSTANT_PREFIX);
        const bool is_env = rest.starts_with(ENV_PREFIX);
        if (!is_constant && !is_env) {
            return std::unexpected(
                       "Expression references must use '${constant.<path>}' or '${env.<field>}' at position " +
                       std::to_string(position_)
                   );
        }
        const auto prefix = is_constant ? CONSTANT_PREFIX : ENV_PREFIX;
        const size_t path_begin = position_ + prefix.size();
        const size_t end = expression_.find('}', path_begin);
        if (end == std::string_view::npos) {
            return std::unexpected("Unterminated reference in expression");
        }

        const auto path = expression_.substr(path_begin, end - path_begin);
        if (path.empty()) {
            return std::unexpected("Expression reference path must not be empty");
        }
        boost::json::value env_value;
        const boost::json::value *resolved = nullptr;
        if (is_constant) {
            resolved = resolve_constant(constants_, path);
            if (resolved == nullptr) {
                return std::unexpected(
                           "Failed to resolve expression constant reference: ${constant." + std::string(path) + "}"
                       );
            }
        } else {
            env_value = environment_reference_value(environment_, path);
            if (env_value.is_null()) {
                return std::unexpected(
                           "Failed to resolve expression environment reference: ${env." + std::string(path) + "}"
                       );
            }
            resolved = &env_value;
        }

        const auto reference_text = std::string("${") + (is_constant ? "constant." : "env.") + std::string(path) + "}";
        auto value = expression_value_from_json(*resolved, reference_text);
        if (!value) {
            return std::unexpected(value.error());
        }

        position_ = end + 1;
        return ExpressionToken{
            .type = ExpressionTokenType::Constant,
            .text = reference_text,
            .value = *value,
        };
    }

    std::string_view expression_;
    const boost::json::value &constants_;
    const Environment &environment_;
    size_t position_ = 0;
};

class ConstantExpressionParser {
public:
    ConstantExpressionParser(
        std::string_view expression, const boost::json::value &constants, const Environment &environment)
        : lexer_(expression, constants, environment)
    {}

    std::expected<ExpressionScalar, std::string> parse()
    {
        auto first = lexer_.next();
        if (!first) {
            return std::unexpected(first.error());
        }
        current_ = *first;

        auto result = parse_or();
        if (!result) {
            return result;
        }
        if (current_.type != ExpressionTokenType::End) {
            return std::unexpected("Unexpected token at end of expression: " + current_.text);
        }
        return result;
    }

private:
    std::expected<void, std::string> advance()
    {
        auto next_token = lexer_.next();
        if (!next_token) {
            return std::unexpected(next_token.error());
        }
        current_ = *next_token;
        return {};
    }

    std::expected<ExpressionScalar, std::string> parse_or()
    {
        auto left = parse_and();
        if (!left) {
            return left;
        }

        while (current_.type == ExpressionTokenType::Or) {
            auto advance_result = advance();
            if (!advance_result) {
                return std::unexpected(advance_result.error());
            }
            auto right = parse_and();
            if (!right) {
                return right;
            }
            auto left_bool = scalar_to_bool(*left);
            auto right_bool = scalar_to_bool(*right);
            if (!left_bool || !right_bool) {
                return std::unexpected("Expression '||' operands must be boolean");
            }
            left = *left_bool || *right_bool;
        }

        return left;
    }

    std::expected<ExpressionScalar, std::string> parse_and()
    {
        auto left = parse_comparison();
        if (!left) {
            return left;
        }

        while (current_.type == ExpressionTokenType::And) {
            auto advance_result = advance();
            if (!advance_result) {
                return std::unexpected(advance_result.error());
            }
            auto right = parse_comparison();
            if (!right) {
                return right;
            }
            auto left_bool = scalar_to_bool(*left);
            auto right_bool = scalar_to_bool(*right);
            if (!left_bool || !right_bool) {
                return std::unexpected("Expression '&&' operands must be boolean");
            }
            left = *left_bool && *right_bool;
        }

        return left;
    }

    std::expected<ExpressionScalar, std::string> parse_comparison()
    {
        auto left = parse_additive();
        if (!left) {
            return left;
        }
        if (!is_comparison(current_.type)) {
            return left;
        }

        const auto op = current_.type;
        auto advance_result = advance();
        if (!advance_result) {
            return std::unexpected(advance_result.error());
        }
        auto right = parse_additive();
        if (!right) {
            return right;
        }
        return compare_scalars(*left, *right, op);
    }

    std::expected<ExpressionScalar, std::string> parse_additive()
    {
        auto left = parse_multiplicative();
        if (!left) {
            return left;
        }

        while (current_.type == ExpressionTokenType::Plus || current_.type == ExpressionTokenType::Minus) {
            const auto op = current_.type;
            auto advance_result = advance();
            if (!advance_result) {
                return std::unexpected(advance_result.error());
            }
            auto right = parse_multiplicative();
            if (!right) {
                return right;
            }
            auto left_number = scalar_to_number(*left);
            auto right_number = scalar_to_number(*right);
            if (!left_number || !right_number) {
                return std::unexpected("Expression '+' and '-' operands must be numeric");
            }
            auto next = apply_additive(*left_number, *right_number, op);
            if (!next) {
                return std::unexpected(next.error());
            }
            left = *next;
        }

        return left;
    }

    std::expected<ExpressionScalar, std::string> parse_multiplicative()
    {
        auto left = parse_unary();
        if (!left) {
            return left;
        }

        while (current_.type == ExpressionTokenType::Multiply || current_.type == ExpressionTokenType::Divide) {
            const auto op = current_.type;
            auto advance_result = advance();
            if (!advance_result) {
                return std::unexpected(advance_result.error());
            }
            auto right = parse_unary();
            if (!right) {
                return right;
            }
            auto left_number = scalar_to_number(*left);
            auto right_number = scalar_to_number(*right);
            if (!left_number || !right_number) {
                return std::unexpected("Expression '*' and '/' operands must be numeric");
            }
            auto next = apply_multiplicative(*left_number, *right_number, op);
            if (!next) {
                return std::unexpected(next.error());
            }
            left = *next;
        }

        return left;
    }

    std::expected<ExpressionScalar, std::string> parse_unary()
    {
        if (current_.type == ExpressionTokenType::Not) {
            auto advance_result = advance();
            if (!advance_result) {
                return std::unexpected(advance_result.error());
            }
            auto value = parse_unary();
            if (!value) {
                return value;
            }
            auto bool_value = scalar_to_bool(*value);
            if (!bool_value) {
                return std::unexpected("Expression '!' operand must be boolean");
            }
            return !*bool_value;
        }

        if (current_.type == ExpressionTokenType::Plus || current_.type == ExpressionTokenType::Minus) {
            const bool negative = current_.type == ExpressionTokenType::Minus;
            auto advance_result = advance();
            if (!advance_result) {
                return std::unexpected(advance_result.error());
            }
            auto value = parse_unary();
            if (!value) {
                return value;
            }
            auto number = scalar_to_number(*value);
            if (!number) {
                return std::unexpected("Expression unary '+' and '-' operands must be numeric");
            }
            if (negative) {
                number->value = -number->value;
            }
            return *number;
        }

        return parse_primary();
    }

    std::expected<ExpressionScalar, std::string> parse_primary()
    {
        if (current_.type == ExpressionTokenType::Number || current_.type == ExpressionTokenType::Constant ||
                current_.type == ExpressionTokenType::String || current_.type == ExpressionTokenType::Bool) {
            const auto value = current_.value;
            auto advance_result = advance();
            if (!advance_result) {
                return std::unexpected(advance_result.error());
            }
            return value;
        }
        if (current_.type == ExpressionTokenType::LeftParen) {
            auto advance_result = advance();
            if (!advance_result) {
                return std::unexpected(advance_result.error());
            }
            auto value = parse_additive();
            if (!value) {
                return value;
            }
            if (current_.type != ExpressionTokenType::RightParen) {
                return std::unexpected("Missing ')' in expression");
            }
            advance_result = advance();
            if (!advance_result) {
                return std::unexpected(advance_result.error());
            }
            return value;
        }

        return std::unexpected("Expected number, reference, or '(' in expression");
    }

    static bool is_comparison(ExpressionTokenType type)
    {
        return type == ExpressionTokenType::Equal || type == ExpressionTokenType::NotEqual ||
               type == ExpressionTokenType::Greater || type == ExpressionTokenType::Less ||
               type == ExpressionTokenType::GreaterEqual || type == ExpressionTokenType::LessEqual;
    }

    static std::optional<ExpressionValue> scalar_to_number(const ExpressionScalar &scalar)
    {
        if (!std::holds_alternative<ExpressionValue>(scalar)) {
            return std::nullopt;
        }
        return std::get<ExpressionValue>(scalar);
    }

    static std::optional<bool> scalar_to_bool(const ExpressionScalar &scalar)
    {
        if (!std::holds_alternative<bool>(scalar)) {
            return std::nullopt;
        }
        return std::get<bool>(scalar);
    }

    static std::expected<bool, std::string> compare_scalars(
        const ExpressionScalar &left,
        const ExpressionScalar &right,
        ExpressionTokenType op)
    {
        if (std::holds_alternative<ExpressionValue>(left) && std::holds_alternative<ExpressionValue>(right)) {
            return compare_numbers(std::get<ExpressionValue>(left), std::get<ExpressionValue>(right), op);
        }
        if (std::holds_alternative<std::string>(left) && std::holds_alternative<std::string>(right)) {
            return compare_values(std::get<std::string>(left), std::get<std::string>(right), op);
        }
        if (std::holds_alternative<bool>(left) && std::holds_alternative<bool>(right)) {
            const bool left_bool = std::get<bool>(left);
            const bool right_bool = std::get<bool>(right);
            if (op == ExpressionTokenType::Equal) {
                return left_bool == right_bool;
            }
            if (op == ExpressionTokenType::NotEqual) {
                return left_bool != right_bool;
            }
            return std::unexpected("Expression boolean comparison only supports '==' and '!='");
        }
        return std::unexpected("Expression comparison operands must have matching types");
    }

    static std::expected<bool, std::string> compare_numbers(
        const ExpressionValue &left,
        const ExpressionValue &right,
        ExpressionTokenType op)
    {
        if (left.unit != right.unit) {
            return std::unexpected("Expression numeric comparison requires matching units");
        }
        return compare_values(left.value, right.value, op);
    }

    template <typename T>
    static std::expected<bool, std::string> compare_values(const T &left, const T &right, ExpressionTokenType op)
    {
        switch (op) {
        case ExpressionTokenType::Equal:
            return left == right;
        case ExpressionTokenType::NotEqual:
            return left != right;
        case ExpressionTokenType::Greater:
            return left > right;
        case ExpressionTokenType::Less:
            return left < right;
        case ExpressionTokenType::GreaterEqual:
            return left >= right;
        case ExpressionTokenType::LessEqual:
            return left <= right;
        default:
            return std::unexpected("Unsupported expression comparison operator");
        }
    }

    static std::expected<ExpressionValue, std::string> apply_additive(
        ExpressionValue left,
        ExpressionValue right,
        ExpressionTokenType op)
    {
        if (left.unit != right.unit) {
            return std::unexpected("Expression '+' and '-' require matching units");
        }

        if (op == ExpressionTokenType::Plus) {
            left.value += right.value;
        } else {
            left.value -= right.value;
        }
        return left;
    }

    static std::expected<ExpressionValue, std::string> apply_multiplicative(
        ExpressionValue left,
        ExpressionValue right,
        ExpressionTokenType op)
    {
        if (op == ExpressionTokenType::Divide && std::fabs(right.value) < 0.000001) {
            return std::unexpected("Expression division by zero");
        }

        if (op == ExpressionTokenType::Multiply) {
            if (left.unit != ExpressionUnit::None && right.unit != ExpressionUnit::None) {
                return std::unexpected("Expression '*' does not allow both operands to have units");
            }
            return ExpressionValue{
                .value = left.value * right.value,
                .unit = left.unit != ExpressionUnit::None ? left.unit : right.unit,
            };
        }

        if (right.unit != ExpressionUnit::None) {
            return std::unexpected("Expression '/' divisor must be unitless");
        }
        return ExpressionValue{
            .value = left.value / right.value,
            .unit = left.unit,
        };
    }

    ExpressionLexer lexer_;
    ExpressionToken current_;
};

static boost::json::value expression_value_to_json(const ExpressionScalar &scalar)
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

static std::expected<boost::json::value, std::string> evaluate_expression_string(
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

static std::expected<Reference, std::string> parse_reference_string(std::string_view text)
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

static bool is_style_font_path(const std::vector<std::string> &path)
{
    return path.size() >= 2 && path[path.size() - 2] == "style" && path.back() == "font";
}

static bool is_theme_style_font_path(const std::vector<std::string> &path)
{
    return path.size() >= 3 && path[path.size() - 3] == "styles" && path.back() == "font";
}

static bool is_image_props_src_path(const std::vector<std::string> &path)
{
    return path.size() >= 2 && path[path.size() - 2] == "imageProps" && path.back() == "src";
}

static bool is_keyboard_key_image_path(const std::vector<std::string> &path)
{
    return path.size() >= 2 && path[path.size() - 2] == "[]" && path.back() == "image";
}

static bool is_placement_relative_to_path(const std::vector<std::string> &path)
{
    if (path.size() < 2 || path[path.size() - 2] != "placement") {
        return false;
    }
    return path.back() == "relativeTo" || path.back() == "relative_to";
}

static bool is_font_fallback_path(const std::vector<std::string> &path)
{
    return path.size() >= 2 && path[path.size() - 2] == "fallbacks";
}

static bool is_style_color_path(const std::vector<std::string> &path)
{
    if (path.empty()) {
        return false;
    }

    const auto field_name = normalize_object_key(path.back());
    return field_name == "bg_color" || field_name == "bg_gradient_color" || field_name == "text_color" ||
           field_name == "border_color" || field_name == "line_color" || field_name == "arc_color" ||
           field_name == "arc_gradient_color" || field_name == "shadow_color" || field_name == "image_recolor";
}

static std::expected<void, std::string> substitute_references(
    boost::json::value &value,
    const boost::json::value &constants,
    const Environment &environment,
    std::vector<std::string> path = {})
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

static std::expected<std::string, std::string> parse_resource_reference_field(
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

static std::expected<std::string, std::string> parse_view_reference_field(
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

static std::expected<std::vector<std::string>, std::string> parse_resource_reference_array_field(
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

static std::expected<std::string, std::string> parse_string_field(
    const boost::json::object &object, std::string_view key, std::string default_value
);
static std::expected<int32_t, std::string> parse_scaled_value(
    const boost::json::value *value, std::string_view field_name, std::string_view unit, float scale
);
static std::expected<int32_t, std::string> parse_int_field(
    const boost::json::object &object, std::string_view key, int32_t default_value
);

static std::expected<Style, std::string> parse_style_object(
    const boost::json::object &style_object,
    const Environment &environment,
    bool allow_font = true,
    bool allow_font_size = true)
{
    Style style;

    auto parse_optional_string = [&](std::string_view key, std::optional<std::string> &field) -> std::expected<void, std::string> {
        const auto *value = find_child_value(style_object, key);
        if (value == nullptr)
        {
            return {};
        }
        auto parsed = parse_string_field(style_object, key, "");
        if (!parsed)
        {
            return std::unexpected(parsed.error());
        }
        field = *parsed;
        return {};
    };

    auto parse_optional_scaled = [&](std::string_view key, std::optional<int32_t> &field, std::string_view unit, float scale) -> std::expected<void, std::string> {
        const auto *value = find_child_value(style_object, key);
        if (value == nullptr)
        {
            return {};
        }
        auto parsed = parse_scaled_value(value, key, unit, scale);
        if (!parsed)
        {
            return std::unexpected(parsed.error());
        }
        field = *parsed;
        return {};
    };

    auto parse_optional_int = [&](std::string_view key, std::optional<int32_t> &field) -> std::expected<void, std::string> {
        const auto *value = find_child_value(style_object, key);
        if (value == nullptr)
        {
            return {};
        }
        auto parsed = parse_int_field(style_object, key, 0);
        if (!parsed)
        {
            return std::unexpected(parsed.error());
        }
        field = *parsed;
        return {};
    };

    auto bg_color = parse_optional_string("bg_color", style.bg_color);
    if (!bg_color) {
        return std::unexpected(bg_color.error());
    }
    auto bg_gradient_color = parse_optional_string("bg_gradient_color", style.bg_gradient_color);
    if (!bg_gradient_color) {
        return std::unexpected(bg_gradient_color.error());
    }
    auto bg_gradient_direction = parse_optional_string("bg_gradient_direction", style.bg_gradient_direction);
    if (!bg_gradient_direction) {
        return std::unexpected(bg_gradient_direction.error());
    }
    auto text_color = parse_optional_string("text_color", style.text_color);
    if (!text_color) {
        return std::unexpected(text_color.error());
    }
    auto border_color = parse_optional_string("border_color", style.border_color);
    if (!border_color) {
        return std::unexpected(border_color.error());
    }
    auto line_color = parse_optional_string("line_color", style.line_color);
    if (!line_color) {
        return std::unexpected(line_color.error());
    }
    auto arc_color = parse_optional_string("arc_color", style.arc_color);
    if (!arc_color) {
        return std::unexpected(arc_color.error());
    }
    auto arc_gradient_color = parse_optional_string("arc_gradient_color", style.arc_gradient_color);
    if (!arc_gradient_color) {
        return std::unexpected(arc_gradient_color.error());
    }
    auto shadow_color = parse_optional_string("shadow_color", style.shadow_color);
    if (!shadow_color) {
        return std::unexpected(shadow_color.error());
    }

    const auto *font_value = find_child_value(style_object, "font");
    if (font_value != nullptr) {
        if (!allow_font) {
            return std::unexpected("State styles do not support field 'font'");
        }
        auto font = parse_resource_reference_field(style_object, "font", "font", "'${font.<id>}'");
        if (!font) {
            return std::unexpected(font.error());
        }
        style.font = *font;
    }

    auto bg_main_stop = parse_optional_int("bg_main_stop", style.bg_main_stop);
    if (!bg_main_stop) {
        return std::unexpected(bg_main_stop.error());
    }
    auto bg_gradient_stop = parse_optional_int("bg_gradient_stop", style.bg_gradient_stop);
    if (!bg_gradient_stop) {
        return std::unexpected(bg_gradient_stop.error());
    }
    auto bg_gradient_opacity = parse_optional_int("bg_gradient_opacity", style.bg_gradient_opacity);
    if (!bg_gradient_opacity) {
        return std::unexpected(bg_gradient_opacity.error());
    }
    auto border_width = parse_optional_scaled("border_width", style.border_width, "dp", environment.density);
    if (!border_width) {
        return std::unexpected(border_width.error());
    }
    auto radius = parse_optional_scaled("radius", style.radius, "dp", environment.density);
    if (!radius) {
        return std::unexpected(radius.error());
    }
    auto padding = parse_optional_scaled("padding", style.padding, "dp", environment.density);
    if (!padding) {
        return std::unexpected(padding.error());
    }
    auto padding_left = parse_optional_scaled("padding_left", style.padding_left, "dp", environment.density);
    if (!padding_left) {
        return std::unexpected(padding_left.error());
    }
    auto padding_right = parse_optional_scaled("padding_right", style.padding_right, "dp", environment.density);
    if (!padding_right) {
        return std::unexpected(padding_right.error());
    }
    auto padding_top = parse_optional_scaled("padding_top", style.padding_top, "dp", environment.density);
    if (!padding_top) {
        return std::unexpected(padding_top.error());
    }
    auto padding_bottom = parse_optional_scaled("padding_bottom", style.padding_bottom, "dp", environment.density);
    if (!padding_bottom) {
        return std::unexpected(padding_bottom.error());
    }
    auto margin = parse_optional_scaled("margin", style.margin, "dp", environment.density);
    if (!margin) {
        return std::unexpected(margin.error());
    }
    auto margin_left = parse_optional_scaled("margin_left", style.margin_left, "dp", environment.density);
    if (!margin_left) {
        return std::unexpected(margin_left.error());
    }
    auto margin_right = parse_optional_scaled("margin_right", style.margin_right, "dp", environment.density);
    if (!margin_right) {
        return std::unexpected(margin_right.error());
    }
    auto margin_top = parse_optional_scaled("margin_top", style.margin_top, "dp", environment.density);
    if (!margin_top) {
        return std::unexpected(margin_top.error());
    }
    auto margin_bottom = parse_optional_scaled("margin_bottom", style.margin_bottom, "dp", environment.density);
    if (!margin_bottom) {
        return std::unexpected(margin_bottom.error());
    }
    auto shadow_width = parse_optional_scaled("shadow_width", style.shadow_width, "dp", environment.density);
    if (!shadow_width) {
        return std::unexpected(shadow_width.error());
    }
    auto shadow_offset_x = parse_optional_scaled("shadow_offset_x", style.shadow_offset_x, "dp", environment.density);
    if (!shadow_offset_x) {
        return std::unexpected(shadow_offset_x.error());
    }
    auto shadow_offset_y = parse_optional_scaled("shadow_offset_y", style.shadow_offset_y, "dp", environment.density);
    if (!shadow_offset_y) {
        return std::unexpected(shadow_offset_y.error());
    }
    auto opacity = parse_optional_int("opacity", style.opacity);
    if (!opacity) {
        return std::unexpected(opacity.error());
    }
    auto line_width = parse_optional_scaled("line_width", style.line_width, "dp", environment.density);
    if (!line_width) {
        return std::unexpected(line_width.error());
    }
    auto image_opacity = parse_optional_int("image_opacity", style.image_opacity);
    if (!image_opacity) {
        return std::unexpected(image_opacity.error());
    }
    auto image_recolor = parse_optional_string("image_recolor", style.image_recolor);
    if (!image_recolor) {
        return std::unexpected(image_recolor.error());
    }
    auto image_recolor_opacity = parse_optional_int("image_recolor_opacity", style.image_recolor_opacity);
    if (!image_recolor_opacity) {
        return std::unexpected(image_recolor_opacity.error());
    }
    if (find_child_value(style_object, "font_size") != nullptr && !allow_font_size) {
        return std::unexpected("State styles do not support field 'fontSize'");
    }
    if (allow_font_size) {
        auto font_size = parse_optional_scaled("font_size", style.font_size, "sp", environment.density * environment.font_scale);
        if (!font_size) {
            return std::unexpected(font_size.error());
        }
    }
    if (find_child_value(style_object, "image_font_size") != nullptr && !allow_font_size) {
        return std::unexpected("State styles do not support field 'imageFontSize'");
    }
    if (allow_font_size) {
        auto image_font_size =
            parse_optional_scaled("image_font_size", style.image_font_size, "sp", environment.density * environment.font_scale);
        if (!image_font_size) {
            return std::unexpected(image_font_size.error());
        }
    }
    const auto *text_align_value = find_child_value(style_object, "text_align");
    if (text_align_value != nullptr) {
        auto text_align_text = parse_string_field(style_object, "text_align", "");
        if (!text_align_text) {
            return std::unexpected(text_align_text.error());
        }
        TextAlign text_align = TextAlign::Auto;
        if (!BROOKESIA_DESCRIBE_STR_TO_ENUM_FLEXIBLE(*text_align_text, text_align)) {
            return std::unexpected("Field 'textAlign' has invalid enum value");
        }
        style.text_align = text_align;
    }

    auto arc_width = parse_optional_scaled("arc_width", style.arc_width, "dp", environment.density);
    if (!arc_width) {
        return std::unexpected(arc_width.error());
    }
    auto arc_opacity = parse_optional_int("arc_opacity", style.arc_opacity);
    if (!arc_opacity) {
        return std::unexpected(arc_opacity.error());
    }
    auto arc_gradient_segments = parse_optional_int("arc_gradient_segments", style.arc_gradient_segments);
    if (!arc_gradient_segments) {
        return std::unexpected(arc_gradient_segments.error());
    }
    const auto *arc_rounded_value = find_child_value(style_object, "arc_rounded");
    if (arc_rounded_value != nullptr) {
        if (!arc_rounded_value->is_bool()) {
            return std::unexpected("Field 'arcRounded' must be boolean");
        }
        style.arc_rounded = arc_rounded_value->as_bool();
    }
    const auto *clip_corner_value = find_child_value(style_object, "clip_corner");
    if (clip_corner_value != nullptr) {
        if (!clip_corner_value->is_bool()) {
            return std::unexpected("Field 'clipCorner' must be boolean");
        }
        style.clip_corner = clip_corner_value->as_bool();
    }

    return style;
}

static std::expected<StateStyleMap, std::string> parse_state_styles_object(
    const boost::json::object &state_styles_object,
    const Environment &environment)
{
    StateStyleMap state_styles;

    for (const auto &[state_key, style_value] : state_styles_object) {
        const std::string state_name(state_key.begin(), state_key.end());
        if (!is_supported_style_state_name(state_name)) {
            return std::unexpected("Unsupported stateStyles state: " + state_name);
        }
        if (!style_value.is_object()) {
            return std::unexpected("stateStyles." + state_name + " must be an object");
        }
        auto style = parse_style_object(style_value.as_object(), environment, false, false);
        if (!style) {
            return std::unexpected("stateStyles." + state_name + " parse failed: " + style.error());
        }
        state_styles.insert_or_assign(state_name, std::move(*style));
    }

    return state_styles;
}

static std::expected<PartStyleSet, std::string> parse_part_style_set_object(
    const boost::json::object &part_style_object,
    const Environment &environment)
{
    PartStyleSet part_style_set;

    const auto *style_value = find_child_value(part_style_object, "style");
    if (style_value != nullptr) {
        if (!style_value->is_object()) {
            return std::unexpected("partStyles style field must be an object");
        }
        auto style = parse_style_object(style_value->as_object(), environment, false, false);
        if (!style) {
            return std::unexpected(style.error());
        }
        part_style_set.style = std::move(*style);
    } else {
        auto style = parse_style_object(part_style_object, environment, false, false);
        if (!style) {
            return std::unexpected(style.error());
        }
        part_style_set.style = std::move(*style);
    }

    const auto *state_styles_value = find_child_value(part_style_object, "state_styles");
    if (state_styles_value != nullptr) {
        if (!state_styles_value->is_object()) {
            return std::unexpected("partStyles stateStyles field must be an object");
        }
        auto state_styles = parse_state_styles_object(state_styles_value->as_object(), environment);
        if (!state_styles) {
            return std::unexpected(state_styles.error());
        }
        part_style_set.state_styles = std::move(*state_styles);
    }

    return part_style_set;
}

static std::expected<PartStyleMap, std::string> parse_part_styles_object(
    const boost::json::object &part_styles_object,
    const Environment &environment)
{
    PartStyleMap part_styles;

    for (const auto &[part_key, part_style_value] : part_styles_object) {
        const std::string part_name(part_key.begin(), part_key.end());
        if (!is_supported_style_part_name(part_name)) {
            return std::unexpected("Unsupported partStyles part: " + part_name);
        }
        if (!part_style_value.is_object()) {
            return std::unexpected("partStyles." + part_name + " must be an object");
        }
        auto part_style = parse_part_style_set_object(part_style_value.as_object(), environment);
        if (!part_style) {
            return std::unexpected("partStyles." + part_name + " parse failed: " + part_style.error());
        }
        part_styles.insert_or_assign(part_name, std::move(*part_style));
    }

    return part_styles;
}

static std::expected<StyleSet, std::string> parse_style_set_object(
    const boost::json::object &style_object,
    const Environment &environment)
{
    StyleSet style_set;

    auto style = parse_style_object(style_object, environment);
    if (!style) {
        return std::unexpected(style.error());
    }
    style_set.style = std::move(*style);

    const auto *state_styles_value = find_child_value(style_object, "state_styles");
    if (state_styles_value != nullptr) {
        if (!state_styles_value->is_object()) {
            return std::unexpected("Field 'stateStyles' must be an object");
        }
        auto state_styles = parse_state_styles_object(state_styles_value->as_object(), environment);
        if (!state_styles) {
            return std::unexpected(state_styles.error());
        }
        style_set.state_styles = std::move(*state_styles);
    }

    const auto *part_styles_value = find_child_value(style_object, "part_styles");
    if (part_styles_value != nullptr) {
        if (!part_styles_value->is_object()) {
            return std::unexpected("Field 'partStyles' must be an object");
        }
        auto part_styles = parse_part_styles_object(part_styles_value->as_object(), environment);
        if (!part_styles) {
            return std::unexpected(part_styles.error());
        }
        style_set.part_styles = std::move(*part_styles);
    }

    return style_set;
}

static const boost::unordered_flat_set<std::string> &get_theme_subtype_style_keys()
{
    static const boost::unordered_flat_set<std::string> keys = {
        "all",
        "screen",
        "container",
        "label",
        "button",
        "image",
        "textInput",
        "slider",
        "switch",
        "checkbox",
        "dropdown",
        "progressBar",
        "spinner",
        "arc",
        "line",
        "table",
        "keyboard",
        "canvas",
    };
    return keys;
}

static std::expected<std::map<std::string, StyleSet>, std::string> parse_named_style_map(
    const boost::json::object &styles_object,
    const boost::json::value &constants,
    const Environment &environment,
    bool allow_subtype_keys)
{
    std::map<std::string, StyleSet> styles;
    const auto &subtype_keys = get_theme_subtype_style_keys();

    for (const auto &[style_key, style_value] : styles_object) {
        const std::string key(style_key.begin(), style_key.end());
        if (!allow_subtype_keys && key.find('.') == std::string::npos) {
            return std::unexpected(
                       "Unsupported local style key: " + key +
                       "; local named styles must contain '.'"
                   );
        }
        if (allow_subtype_keys && !subtype_keys.contains(key) && key.find('.') == std::string::npos) {
            return std::unexpected(
                       "Unsupported theme style key: " + key +
                       "; custom named styles must contain '.'"
                   );
        }
        if (!style_value.is_object()) {
            return std::unexpected("Style '" + key + "' must be an object");
        }

        boost::json::value resolved_style_value = style_value;
        auto replace_result = substitute_references(
                                  resolved_style_value,
                                  constants,
                                  environment,
        {"styles", key}
                              );
        if (!replace_result) {
            return std::unexpected("Style '" + key + "' resolve failed: " + replace_result.error());
        }

        auto style_set = parse_style_set_object(resolved_style_value.as_object(), environment);
        if (!style_set) {
            return std::unexpected("Style '" + key + "' parse failed: " + style_set.error());
        }
        styles.insert_or_assign(key, std::move(*style_set));
    }

    return styles;
}

static std::expected<int32_t, std::string> parse_scaled_value(
    const boost::json::value *value, std::string_view field_name, std::string_view unit, float scale)
{
    if (value == nullptr) {
        return 0;
    }
    if (value->is_int64()) {
        return static_cast<int32_t>(value->as_int64());
    }
    if (!value->is_string()) {
        return std::unexpected("Field '" + std::string(field_name) + "' must be a string");
    }

    const std::string text = value->as_string().c_str();
    if (!text.ends_with(unit)) {
        return std::unexpected(
                   "Field '" + std::string(field_name) + "' must use " + std::string(unit) + " units"
               );
    }

    const std::string number_text = text.substr(0, text.size() - unit.size());
    const float number = std::stof(number_text);
    return static_cast<int32_t>(std::lround(number * scale));
}

static std::expected<PlacementOffset, std::string> parse_placement_offset(
    const boost::json::value *value, std::string_view field_name, const Environment &environment)
{
    if (value == nullptr) {
        return PlacementOffset {};
    }
    if (value->is_int64()) {
        return PlacementOffset(static_cast<int32_t>(value->as_int64()));
    }
    if (!value->is_string()) {
        return std::unexpected("Field '" + std::string(field_name) + "' must be a string");
    }

    const std::string text = value->as_string().c_str();
    if (text.ends_with("%")) {
        const std::string number_text = text.substr(0, text.size() - 1);
        const float number = std::stof(number_text);
        if (!std::isfinite(number) || number < 0.0F) {
            return std::unexpected("Field '" + std::string(field_name) + "' percent value must be >= 0");
        }
        PlacementOffset offset;
        offset.mode = PlacementOffsetMode::Percent;
        offset.value = static_cast<int32_t>(std::lround(number));
        return offset;
    }

    auto fixed = parse_scaled_value(value, field_name, "dp", environment.density);
    if (!fixed) {
        return std::unexpected(fixed.error());
    }
    return PlacementOffset(*fixed);
}

static std::expected<Dimension, std::string> parse_dimension(
    const boost::json::value *value, std::string_view field_name, const Environment &environment)
{
    Dimension dimension;
    if (value == nullptr) {
        return dimension;
    }

    if (value->is_int64()) {
        dimension.mode = SizeMode::Fixed;
        dimension.value = static_cast<int32_t>(value->as_int64());
        return dimension;
    }

    if (!value->is_string()) {
        return std::unexpected("Field '" + std::string(field_name) + "' must be a string");
    }

    const std::string text = value->as_string().c_str();
    if (text == "match") {
        dimension.mode = SizeMode::Match;
        return dimension;
    }
    if (text == "wrap") {
        dimension.mode = SizeMode::Wrap;
        return dimension;
    }
    if (text.ends_with("%")) {
        const std::string number_text = text.substr(0, text.size() - 1);
        const float number = std::stof(number_text);
        if (!std::isfinite(number) || number < 0.0F) {
            return std::unexpected("Field '" + std::string(field_name) + "' percent value must be >= 0");
        }
        dimension.mode = SizeMode::Percent;
        dimension.value = static_cast<int32_t>(std::lround(number));
        return dimension;
    }
    if (!text.ends_with("dp")) {
        return std::unexpected("Field '" + std::string(field_name) + "' must be dp/%/match/wrap");
    }

    dimension.mode = SizeMode::Fixed;
    dimension.value = static_cast<int32_t>(
                          std::lround(std::stof(text.substr(0, text.size() - 2)) * environment.density)
                      );
    return dimension;
}

static std::expected<float, std::string> parse_positive_float_text(
    std::string_view text, std::string_view field_name)
{
    std::string owned(text);
    char *parse_end = nullptr;
    errno = 0;
    const float parsed = std::strtof(owned.c_str(), &parse_end);
    if (errno != 0 || parse_end == owned.c_str() || *parse_end != '\0' || !std::isfinite(parsed) ||
            parsed <= 0.0F) {
        return std::unexpected("Field '" + std::string(field_name) + "' must be a positive number");
    }
    return parsed;
}

static std::expected<float, std::string> parse_aspect_ratio(
    const boost::json::value *value, std::string_view field_name)
{
    if (value == nullptr) {
        return 0.0F;
    }

    if (value->is_int64()) {
        const auto integer_value = value->as_int64();
        if (integer_value <= 0) {
            return std::unexpected("Field '" + std::string(field_name) + "' must be positive");
        }
        return static_cast<float>(integer_value);
    }
    if (value->is_uint64()) {
        const auto unsigned_value = value->as_uint64();
        if (unsigned_value == 0) {
            return std::unexpected("Field '" + std::string(field_name) + "' must be positive");
        }
        return static_cast<float>(unsigned_value);
    }
    if (value->is_double()) {
        const auto double_value = value->as_double();
        if (!std::isfinite(double_value) || double_value <= 0.0) {
            return std::unexpected("Field '" + std::string(field_name) + "' must be positive");
        }
        return static_cast<float>(double_value);
    }
    if (!value->is_string()) {
        return std::unexpected("Field '" + std::string(field_name) + "' must be a string or number");
    }

    const std::string text = value->as_string().c_str();
    const auto separator = text.find(':');
    if (separator == std::string::npos) {
        return parse_positive_float_text(text, field_name);
    }

    auto width = parse_positive_float_text(std::string_view(text).substr(0, separator), field_name);
    if (!width) {
        return std::unexpected(width.error());
    }
    auto height = parse_positive_float_text(std::string_view(text).substr(separator + 1), field_name);
    if (!height) {
        return std::unexpected(height.error());
    }
    return *width / *height;
}

template <typename T>
static std::expected<T, std::string> parse_enum_field(
    const boost::json::object &object, std::string_view key, T default_value)
{
    const auto *value = find_child_value(object, key);
    if (value == nullptr) {
        return default_value;
    }
    if (!value->is_string()) {
        return std::unexpected("Field '" + std::string(key) + "' must be a string");
    }

    T parsed_value {};
    const std::string text = value->as_string().c_str();
    if (!BROOKESIA_DESCRIBE_STR_TO_ENUM_FLEXIBLE(text, parsed_value)) {
        return std::unexpected("Invalid enum value for field '" + std::string(key) + "': " + text);
    }
    return parsed_value;
}

static std::expected<std::string, std::string> parse_string_field(
    const boost::json::object &object, std::string_view key, std::string default_value = {})
{
    const auto *value = find_child_value(object, key);
    if (value == nullptr) {
        return default_value;
    }
    if (!value->is_string()) {
        return std::unexpected("Field '" + std::string(key) + "' must be a string");
    }
    return std::string(value->as_string().c_str());
}

static std::expected<bool, std::string> parse_bool_field(
    const boost::json::object &object, std::string_view key, bool default_value = false)
{
    const auto *value = find_child_value(object, key);
    if (value == nullptr) {
        return default_value;
    }
    if (!value->is_bool()) {
        return std::unexpected("Field '" + std::string(key) + "' must be a bool");
    }
    return value->as_bool();
}

static std::expected<int32_t, std::string> parse_int_field(
    const boost::json::object &object, std::string_view key, int32_t default_value = 0)
{
    const auto *value = find_child_value(object, key);
    if (value == nullptr) {
        return default_value;
    }
    if (!value->is_int64()) {
        return std::unexpected("Field '" + std::string(key) + "' must be an integer");
    }
    return static_cast<int32_t>(value->as_int64());
}

static bool is_supported_keyboard_mode(std::string_view mode)
{
    return mode == "text" || mode == "upper" || mode == "number" || mode == "special";
}

static std::expected<KeyboardKey, std::string> parse_keyboard_key(const boost::json::value &value)
{
    KeyboardKey key;
    if (value.is_string()) {
        key.text = value.as_string().c_str();
        return key;
    }
    if (!value.is_object()) {
        return std::unexpected("Keyboard layout key must be a string or object");
    }

    const auto &object = value.as_object();
    auto text = parse_string_field(object, "text");
    if (!text) {
        return std::unexpected(text.error());
    }
    auto width = parse_int_field(object, "width", 1);
    if (!width) {
        return std::unexpected(width.error());
    }
    auto role = parse_string_field(object, "role");
    if (!role) {
        return std::unexpected(role.error());
    }
    auto mode = parse_string_field(object, "mode");
    if (!mode) {
        return std::unexpected(mode.error());
    }
    auto style_class = parse_string_field(object, "styleClass");
    if (!style_class) {
        return std::unexpected(style_class.error());
    }
    auto image = parse_resource_reference_field(
                     object,
                     "image",
                     "image",
                     "'${image.<id>}' syntax"
                 );
    if (!image) {
        return std::unexpected(image.error());
    }
    if (*width <= 0 || *width > 15) {
        return std::unexpected("Keyboard layout key width must be in range 1..15");
    }
    if (*role == "mode") {
        if (mode->empty()) {
            return std::unexpected("Keyboard layout mode key requires field 'mode'");
        }
        if (!is_supported_keyboard_mode(*mode)) {
            return std::unexpected("Unsupported keyboard mode target: " + *mode);
        }
    } else if (!mode->empty()) {
        return std::unexpected("Keyboard layout key field 'mode' is only valid when role is 'mode'");
    }

    key.text = std::move(*text);
    key.width = *width;
    key.role = std::move(*role);
    key.mode = std::move(*mode);
    key.style_class = std::move(*style_class);
    key.image = std::move(*image);
    return key;
}

static std::expected<KeyboardLayout, std::string> parse_keyboard_layout(const boost::json::value &value)
{
    if (!value.is_array()) {
        return std::unexpected("Keyboard layout must be an array of rows");
    }

    KeyboardLayout layout;
    for (const auto &row_value : value.as_array()) {
        if (!row_value.is_array()) {
            return std::unexpected("Keyboard layout row must be an array");
        }
        std::vector<KeyboardKey> row;
        for (const auto &key_value : row_value.as_array()) {
            auto key = parse_keyboard_key(key_value);
            if (!key) {
                return std::unexpected(key.error());
            }
            if (key->text.empty() && key->role.empty()) {
                return std::unexpected("Keyboard layout key requires text or role");
            }
            row.push_back(std::move(*key));
        }
        if (row.empty()) {
            return std::unexpected("Keyboard layout row must not be empty");
        }
        layout.rows.push_back(std::move(row));
    }
    if (layout.rows.empty()) {
        return std::unexpected("Keyboard layout must not be empty");
    }
    return layout;
}

static std::expected<std::map<std::string, KeyboardLayout>, std::string> parse_keyboard_layouts_field(
    const boost::json::object &object)
{
    std::map<std::string, KeyboardLayout> layouts;
    const auto *value = find_child_value(object, "layouts");
    if (value == nullptr) {
        return layouts;
    }
    if (!value->is_object()) {
        return std::unexpected("Field 'keyboardProps.layouts' must be an object");
    }

    for (const auto &[mode_key, layout_value] : value->as_object()) {
        std::string mode(mode_key.begin(), mode_key.end());
        if (!is_supported_keyboard_mode(mode)) {
            return std::unexpected("Unsupported keyboard layout mode: " + mode);
        }
        auto layout = parse_keyboard_layout(layout_value);
        if (!layout) {
            return std::unexpected("keyboardProps.layouts." + mode + " parse failed: " + layout.error());
        }
        layouts.insert_or_assign(std::move(mode), std::move(*layout));
    }

    return layouts;
}

static std::expected<std::vector<std::string>, std::string> parse_keyboard_allowed_modes_field(
    const boost::json::object &object)
{
    auto modes = parse_string_array(object, "allowedModes");
    if (!modes) {
        return std::unexpected(modes.error());
    }
    if (modes->empty()) {
        return *modes;
    }

    std::unordered_set<std::string> seen_modes;
    for (const auto &mode : *modes) {
        if (!is_supported_keyboard_mode(mode)) {
            return std::unexpected("Unsupported keyboard allowed mode: " + mode);
        }
        if (!seen_modes.insert(mode).second) {
            return std::unexpected("Duplicate keyboard allowed mode: " + mode);
        }
    }
    return *modes;
}

static std::expected<KeyboardKeyStyle, std::string> parse_keyboard_key_style(
    const boost::json::value &value, std::string_view class_name, const Environment &environment)
{
    if (!value.is_object()) {
        return std::unexpected("keyboardProps.keyStyles." + std::string(class_name) + " must be an object");
    }

    const auto &object = value.as_object();
    auto bg_color = parse_string_field(object, "bgColor");
    if (!bg_color) {
        return std::unexpected(bg_color.error());
    }
    auto text_color = parse_string_field(object, "textColor");
    if (!text_color) {
        return std::unexpected(text_color.error());
    }
    auto pressed_bg_color = parse_string_field(object, "pressedBgColor");
    if (!pressed_bg_color) {
        return std::unexpected(pressed_bg_color.error());
    }
    auto pressed_text_color = parse_string_field(object, "pressedTextColor");
    if (!pressed_text_color) {
        return std::unexpected(pressed_text_color.error());
    }
    int32_t radius = 0;
    if (const auto *radius_value = find_child_value(object, "radius"); radius_value != nullptr) {
        auto parsed_radius = parse_scaled_value(
                                 radius_value,
                                 "keyboardProps.keyStyles." + std::string(class_name) + ".radius",
                                 "dp",
                                 environment.density
                             );
        if (!parsed_radius) {
            return std::unexpected(parsed_radius.error());
        }
        radius = std::max<int32_t>(0, *parsed_radius);
    }
    return KeyboardKeyStyle{
        .bg_color = std::move(*bg_color),
        .text_color = std::move(*text_color),
        .pressed_bg_color = std::move(*pressed_bg_color),
        .pressed_text_color = std::move(*pressed_text_color),
        .radius = radius,
    };
}

static bool is_supported_keyboard_key_style_class(std::string_view class_name)
{
    return class_name == "default" || class_name == "special" || class_name == "mode" || class_name == "action" ||
           class_name == "disabled";
}

static std::expected<std::map<std::string, KeyboardKeyStyle>, std::string> parse_keyboard_key_styles_field(
    const boost::json::object &object, const Environment &environment)
{
    std::map<std::string, KeyboardKeyStyle> styles;
    const auto *value = find_child_value(object, "keyStyles");
    if (value == nullptr) {
        return styles;
    }
    if (!value->is_object()) {
        return std::unexpected("Field 'keyboardProps.keyStyles' must be an object");
    }

    for (const auto &[class_key, style_value] : value->as_object()) {
        std::string class_name(class_key.begin(), class_key.end());
        if (!is_supported_keyboard_key_style_class(class_name)) {
            return std::unexpected("Unsupported keyboard key style class: " + class_name);
        }
        auto style = parse_keyboard_key_style(style_value, class_name, environment);
        if (!style) {
            return std::unexpected(style.error());
        }
        styles.insert_or_assign(std::move(class_name), std::move(*style));
    }
    return styles;
}

static std::expected<std::map<std::string, std::string>, std::string> parse_keyboard_key_style_refs_field(
    const boost::json::object &object)
{
    std::map<std::string, std::string> style_refs;
    const auto *value = find_child_value(object, "keyStyleRefs");
    if (value == nullptr) {
        return style_refs;
    }
    if (!value->is_object()) {
        return std::unexpected("Field 'keyboardProps.keyStyleRefs' must be an object");
    }

    for (const auto &[class_key, ref_value] : value->as_object()) {
        std::string class_name(class_key.begin(), class_key.end());
        if (!is_supported_keyboard_key_style_class(class_name)) {
            return std::unexpected("Unsupported keyboard key style ref class: " + class_name);
        }
        if (!ref_value.is_string()) {
            return std::unexpected("keyboardProps.keyStyleRefs." + class_name + " must be a string");
        }
        style_refs.insert_or_assign(std::move(class_name), ref_value.as_string().c_str());
    }
    return style_refs;
}

static std::expected<PivotValue, std::string> parse_pivot_value(
    const boost::json::value *value,
    std::string_view field_name,
    PivotValue default_value = {})
{
    if (value == nullptr) {
        return default_value;
    }
    if (value->is_int64()) {
        return PivotValue{
            .percent = false,
            .value = static_cast<int32_t>(value->as_int64()),
        };
    }
    if (!value->is_string()) {
        return std::unexpected("Field '" + std::string(field_name) + "' must be an integer or percent string");
    }

    std::string text(value->as_string().c_str());
    if (!text.ends_with('%')) {
        return std::unexpected("Field '" + std::string(field_name) + "' percent value must end with '%'");
    }
    text.pop_back();
    char *parse_end = nullptr;
    errno = 0;
    const float parsed = std::strtof(text.c_str(), &parse_end);
    if (errno != 0 || parse_end == text.c_str() || *parse_end != '\0') {
        return std::unexpected("Field '" + std::string(field_name) + "' percent value must be numeric");
    }
    return PivotValue{
        .percent = true,
        .value = static_cast<int32_t>(std::lround(parsed)),
    };
}

static std::expected<PivotValue, std::string> parse_pivot_field(
    const boost::json::object &object,
    std::string_view key,
    PivotValue default_value = {})
{
    return parse_pivot_value(find_child_value(object, key), key, default_value);
}

static std::expected<std::vector<Dimension>, std::string> parse_dimension_array(
    const boost::json::object &object,
    std::string_view key,
    const Environment &environment)
{
    std::vector<Dimension> result;
    const auto *value = find_child_value(object, key);
    if (value == nullptr) {
        return result;
    }
    if (!value->is_array()) {
        return std::unexpected("Field '" + std::string(key) + "' must be an array");
    }
    for (const auto &entry : value->as_array()) {
        auto dimension = parse_dimension(&entry, key, environment);
        if (!dimension) {
            return std::unexpected(dimension.error());
        }
        result.push_back(*dimension);
    }
    return result;
}

static std::expected<void, std::string> reject_fields(
    const boost::json::object &object, std::initializer_list<std::string_view> fields, std::string_view target)
{
    for (auto field : fields) {
        if (find_child_value(object, field) != nullptr) {
            return std::unexpected(
                       "Field '" + std::string(field) + "' must be moved to '" + std::string(target) + "'"
                   );
        }
    }
    return {};
}

static std::expected<std::vector<Point>, std::string> parse_points_field(const boost::json::object &object)
{
    std::vector<Point> points;
    const auto *value = find_child_value(object, "points");
    if (value == nullptr) {
        return points;
    }
    if (!value->is_array()) {
        return std::unexpected("Field 'points' must be an array");
    }
    for (const auto &entry : value->as_array()) {
        if (!entry.is_object()) {
            return std::unexpected("Field 'points' entries must be objects");
        }
        const auto &point_object = entry.as_object();
        auto x = parse_int_field(point_object, "x");
        if (!x) {
            return std::unexpected(x.error());
        }
        auto y = parse_int_field(point_object, "y");
        if (!y) {
            return std::unexpected(y.error());
        }
        points.push_back(Point{.x = *x, .y = *y});
    }
    return points;
}

static std::expected<std::vector<TableCell>, std::string> parse_table_cells_field(const boost::json::object &object)
{
    std::vector<TableCell> cells;
    const auto *value = find_child_value(object, "cells");
    if (value == nullptr) {
        return cells;
    }
    if (!value->is_array()) {
        return std::unexpected("Field 'cells' must be an array");
    }
    for (const auto &entry : value->as_array()) {
        if (!entry.is_object()) {
            return std::unexpected("Field 'cells' entries must be objects");
        }
        const auto &cell_object = entry.as_object();
        auto row = parse_int_field(cell_object, "row");
        if (!row) {
            return std::unexpected(row.error());
        }
        auto column = parse_int_field(cell_object, "column");
        if (!column) {
            return std::unexpected(column.error());
        }
        auto text = parse_string_field(cell_object, "text");
        if (!text) {
            return std::unexpected(text.error());
        }
        cells.push_back(TableCell{.row = *row, .column = *column, .text = *text});
    }
    return cells;
}

static std::expected<std::vector<CanvasCommand>, std::string> parse_canvas_commands_field(
    const boost::json::object &object)
{
    std::vector<CanvasCommand> commands;
    const auto *value = find_child_value(object, "commands");
    if (value == nullptr) {
        return commands;
    }
    if (!value->is_array()) {
        return std::unexpected("Field 'commands' must be an array");
    }
    for (const auto &entry : value->as_array()) {
        if (!entry.is_object()) {
            return std::unexpected("Field 'commands' entries must be objects");
        }
        const auto &command_object = entry.as_object();
        auto type = parse_string_field(command_object, "type");
        if (!type) {
            return std::unexpected(type.error());
        }
        auto x = parse_int_field(command_object, "x");
        if (!x) {
            return std::unexpected(x.error());
        }
        auto y = parse_int_field(command_object, "y");
        if (!y) {
            return std::unexpected(y.error());
        }
        auto width = parse_int_field(command_object, "width");
        if (!width) {
            return std::unexpected(width.error());
        }
        auto height = parse_int_field(command_object, "height");
        if (!height) {
            return std::unexpected(height.error());
        }
        auto color = parse_string_field(command_object, "color");
        if (!color) {
            return std::unexpected(color.error());
        }
        commands.push_back(CanvasCommand{
            .type = *type,
            .x = *x,
            .y = *y,
            .width = *width,
            .height = *height,
            .color = *color,
        });
    }
    return commands;
}

static std::expected<Animation, std::string> parse_animation(const boost::json::object &object);

static std::expected<std::string, std::string> parse_event_effect_value(
    const boost::json::object &object,
    std::string_view key)
{
    const auto *value = find_child_value(object, key);
    if (value == nullptr) {
        return std::unexpected("Field '" + std::string(key) + "' is required");
    }
    if (value->is_string()) {
        return std::string(value->as_string().c_str());
    }
    if (value->is_bool()) {
        return value->as_bool() ? "true" : "false";
    }
    if (value->is_int64()) {
        return std::to_string(value->as_int64());
    }
    if (value->is_uint64()) {
        return std::to_string(value->as_uint64());
    }
    if (value->is_double()) {
        return boost::json::serialize(*value);
    }
    if (value->is_array() || value->is_object()) {
        return boost::json::serialize(*value);
    }
    return std::string();
}

static std::expected<EventPropertyUpdate, std::string> parse_event_property_update(
    const boost::json::object &object,
    std::string_view default_target = "self")
{
    EventPropertyUpdate update;
    auto target = parse_string_field(object, "target", std::string(default_target));
    if (!target) {
        return std::unexpected(target.error());
    }
    update.target = *target;

    auto field = parse_string_field(object, "field");
    if (!field || field->empty()) {
        return std::unexpected(!field ? field.error() : "Event property update field must not be empty");
    }
    update.field = *field;

    auto value = parse_event_effect_value(object, "value");
    if (!value) {
        return std::unexpected(value.error());
    }
    update.value = *value;
    return update;
}

static std::expected<EventEffect, std::string> parse_event_effect(const boost::json::object &object)
{
    EventEffect effect;
    auto type = parse_enum_field<EventEffectType>(object, "type", EventEffectType::EmitAction);
    if (!type) {
        return std::unexpected(type.error());
    }
    effect.type = *type;

    auto target = parse_string_field(object, "target", effect.target);
    if (!target) {
        return std::unexpected(target.error());
    }
    effect.target = *target;

    auto action = parse_string_field(object, "action");
    if (!action) {
        return std::unexpected(action.error());
    }
    effect.action = *action;

    auto require_valid_press = parse_bool_field(object, "requireValidPress");
    if (!require_valid_press) {
        return std::unexpected(require_valid_press.error());
    }
    effect.require_valid_press = *require_valid_press;

    auto animation_id = parse_string_field(object, "animationId");
    if (!animation_id) {
        return std::unexpected(animation_id.error());
    }
    effect.animation_id = *animation_id;

    switch (effect.type) {
    case EventEffectType::EmitAction:
        if (effect.action.empty()) {
            return std::unexpected("emitAction effect requires non-empty field 'action'");
        }
        break;
    case EventEffectType::SetProperty: {
        auto update = parse_event_property_update(object, effect.target);
        if (!update) {
            return std::unexpected(update.error());
        }
        effect.updates.push_back(std::move(*update));
        break;
    }
    case EventEffectType::SetProperties: {
        const auto *updates_value = find_child_value(object, "updates");
        if (updates_value == nullptr || !updates_value->is_array()) {
            return std::unexpected("setProperties effect requires array field 'updates'");
        }
        for (const auto &entry : updates_value->as_array()) {
            if (!entry.is_object()) {
                return std::unexpected("setProperties updates entries must be objects");
            }
            auto update = parse_event_property_update(entry.as_object(), effect.target);
            if (!update) {
                return std::unexpected(update.error());
            }
            effect.updates.push_back(std::move(*update));
        }
        if (effect.updates.empty()) {
            return std::unexpected("setProperties effect updates must not be empty");
        }
        break;
    }
    case EventEffectType::StartAnimation: {
        const auto *animation_value = find_child_value(object, "animation");
        if (animation_value != nullptr) {
            if (!animation_value->is_object()) {
                return std::unexpected("startAnimation field 'animation' must be an object");
            }
            auto animation = parse_animation(animation_value->as_object());
            if (!animation) {
                return std::unexpected(animation.error());
            }
            effect.animation = *animation;
        }
        if (effect.animation_id.empty() && animation_value == nullptr) {
            return std::unexpected("startAnimation effect requires 'animationId' or 'animation'");
        }
        break;
    }
    case EventEffectType::StopAnimation:
        if (effect.animation_id.empty()) {
            return std::unexpected("stopAnimation effect requires non-empty field 'animationId'");
        }
        break;
    case EventEffectType::Max:
    default:
        return std::unexpected("Invalid event effect type");
    }
    return effect;
}

static std::expected<EventBinding, std::string> parse_event_binding(const boost::json::object &object)
{
    EventBinding binding;
    auto type = parse_enum_field<EventType>(object, "type", EventType::Clicked);
    if (!type) {
        return std::unexpected(type.error());
    }
    binding.type = *type;

    auto action = parse_string_field(object, "action");
    if (!action) {
        return std::unexpected(action.error());
    }
    binding.action = *action;

    const auto *effects_value = find_child_value(object, "effects");
    if (effects_value != nullptr) {
        if (!effects_value->is_array()) {
            return std::unexpected("Field 'effects' must be an array");
        }
        for (const auto &effect_value : effects_value->as_array()) {
            if (!effect_value.is_object()) {
                return std::unexpected("Event effects entries must be objects");
            }
            auto effect = parse_event_effect(effect_value.as_object());
            if (!effect) {
                return std::unexpected(effect.error());
            }
            binding.effects.push_back(std::move(*effect));
        }
    }
    return binding;
}

static std::expected<Animation, std::string> parse_animation(const boost::json::object &object)
{
    Animation animation;
    auto id = parse_string_field(object, "id");
    if (!id) {
        return std::unexpected(id.error());
    }
    animation.id = *id;

    auto trigger = parse_enum_field<AnimationTrigger>(object, "trigger", AnimationTrigger::Mount);
    if (!trigger) {
        return std::unexpected(trigger.error());
    }
    animation.trigger = *trigger;

    auto property = parse_enum_field<AnimationProperty>(object, "property", AnimationProperty::Opacity);
    if (!property) {
        return std::unexpected(property.error());
    }
    animation.property = *property;

    auto from_mode = parse_enum_field<AnimationValueMode>(object, "fromMode", AnimationValueMode::Absolute);
    if (!from_mode) {
        return std::unexpected(from_mode.error());
    }
    animation.from_mode = *from_mode;

    auto from = parse_int_field(object, "from");
    if (!from) {
        return std::unexpected(from.error());
    }
    animation.from = *from;

    auto to_mode = parse_enum_field<AnimationValueMode>(object, "toMode", AnimationValueMode::Absolute);
    if (!to_mode) {
        return std::unexpected(to_mode.error());
    }
    animation.to_mode = *to_mode;

    auto to = parse_int_field(object, "to");
    if (!to) {
        return std::unexpected(to.error());
    }
    animation.to = *to;

    auto duration = parse_int_field(object, "duration", 150);
    if (!duration) {
        return std::unexpected(duration.error());
    }
    animation.duration = *duration;

    auto delay = parse_int_field(object, "delay");
    if (!delay) {
        return std::unexpected(delay.error());
    }
    animation.delay = *delay;

    auto easing = parse_enum_field<AnimationEasing>(object, "easing", AnimationEasing::Linear);
    if (!easing) {
        return std::unexpected(easing.error());
    }
    animation.easing = *easing;

    auto repeat = parse_int_field(object, "repeat");
    if (!repeat) {
        return std::unexpected(repeat.error());
    }
    animation.repeat = *repeat;

    auto playback = parse_bool_field(object, "playback");
    if (!playback) {
        return std::unexpected(playback.error());
    }
    animation.playback = *playback;
    return animation;
}

static std::expected<Node, std::string> parse_view_node(
    const boost::json::object &object,
    const Environment &environment,
    const TemplateRawMap &templates,
    const InteractionTemplateRawMap &interactions,
    std::vector<std::string> &template_stack,
    std::optional<NodeType> forced_type = std::nullopt);
static std::expected<FontAsset, std::string> parse_font_asset(
    const boost::json::object &object, const std::filesystem::path &asset_dir);

static std::vector<std::string> split_template_path(std::string_view path)
{
    std::vector<std::string> parts;
    std::string current;
    for (const auto ch : path) {
        if (ch == '/') {
            if (!current.empty()) {
                parts.push_back(std::move(current));
                current.clear();
            }
            continue;
        }
        current.push_back(ch);
    }
    if (!current.empty()) {
        parts.push_back(std::move(current));
    }
    return parts;
}

static boost::json::object *find_template_override_target(
    boost::json::object &root,
    std::string_view relative_path)
{
    if (relative_path.empty() || relative_path == ".") {
        return &root;
    }
    if (relative_path.front() == '/') {
        return nullptr;
    }

    auto *current = &root;
    for (const auto &part : split_template_path(relative_path)) {
        auto children_it = current->find("children");
        if (children_it == current->end() || !children_it->value().is_array()) {
            return nullptr;
        }
        boost::json::object *next = nullptr;
        for (auto &child_value : children_it->value().as_array()) {
            if (!child_value.is_object()) {
                continue;
            }
            auto &child_object = child_value.as_object();
            auto id = parse_string_field(child_object, "id");
            if (id && *id == part) {
                next = &child_object;
                break;
            }
        }
        if (next == nullptr) {
            return nullptr;
        }
        current = next;
    }
    return current;
}

static std::expected<void, std::string> apply_template_overrides(
    boost::json::object &root,
    const boost::json::object &overrides)
{
    static const boost::unordered_flat_set<std::string> forbidden_fields = {
        "id",
        "type",
        "children",
        "node",
        "templateId",
        "template_id",
        "overrides",
        "slots",
    };

    for (const auto &[path_key, override_value] : overrides) {
        const std::string path(path_key.begin(), path_key.end());
        if (!override_value.is_object()) {
            return std::unexpected("Template override '" + path + "' must be an object");
        }
        auto *target = find_template_override_target(root, path);
        if (target == nullptr) {
            return std::unexpected("Template override target not found: " + path);
        }
        for (const auto &[field_key, field_value] : override_value.as_object()) {
            const std::string field(field_key.begin(), field_key.end());
            if (forbidden_fields.contains(field)) {
                return std::unexpected("Template override cannot replace field: " + field);
            }
            auto it = target->find(field_key);
            if (it != target->end() && it->value().is_object() && field_value.is_object()) {
                merge_json(it->value(), field_value);
            } else {
                target->insert_or_assign(field_key, field_value);
            }
        }
    }

    return {};
}

static std::expected<void, std::string> expand_template_slots_in_children(
    boost::json::array &children,
    const boost::json::object *slots,
    boost::unordered_flat_set<std::string> &declared_slots)
{
    boost::json::array expanded_children;
    for (auto &child_value : children) {
        if (!child_value.is_object()) {
            expanded_children.emplace_back(std::move(child_value));
            continue;
        }

        auto &child_object = child_value.as_object();
        auto type = parse_string_field(child_object, "type");
        if (type && *type == "slot") {
            auto id = parse_string_field(child_object, "id");
            if (!id) {
                return std::unexpected("Template slot must contain string field 'id'");
            }
            if (id->empty()) {
                return std::unexpected("Template slot id must not be empty");
            }
            if (!declared_slots.emplace(*id).second) {
                return std::unexpected("Duplicate template slot id: " + *id);
            }

            const auto *replacement = find_child_value(child_object, "children");
            if (slots != nullptr) {
                auto slot_it = slots->find(*id);
                if (slot_it != slots->end()) {
                    replacement = &slot_it->value();
                }
            }
            if (replacement == nullptr) {
                continue;
            }
            if (!replacement->is_array()) {
                return std::unexpected("Template slot '" + *id + "' replacement must be an array");
            }
            for (const auto &replacement_child : replacement->as_array()) {
                expanded_children.emplace_back(replacement_child);
            }
            continue;
        }

        if (auto children_it = child_object.find("children");
                children_it != child_object.end() && children_it->value().is_array()) {
            auto result = expand_template_slots_in_children(children_it->value().as_array(), slots, declared_slots);
            if (!result) {
                return std::unexpected(result.error());
            }
        }
        expanded_children.emplace_back(std::move(child_value));
    }

    children = std::move(expanded_children);
    return {};
}

static std::expected<void, std::string> apply_template_slots(
    boost::json::object &root,
    const boost::json::object *slots)
{
    boost::unordered_flat_set<std::string> declared_slots;
    if (auto children_it = root.find("children"); children_it != root.end() && children_it->value().is_array()) {
        auto result = expand_template_slots_in_children(children_it->value().as_array(), slots, declared_slots);
        if (!result) {
            return std::unexpected(result.error());
        }
    }

    if (slots != nullptr) {
        for (const auto &[slot_key, slot_value] : *slots) {
            (void)slot_value;
            const std::string slot_id(slot_key.begin(), slot_key.end());
            if (!declared_slots.contains(slot_id)) {
                return std::unexpected("TemplateRef provides unknown slot: " + slot_id);
            }
        }
    }

    return {};
}

static std::expected<Node, std::string> parse_template_ref(
    const boost::json::object &object,
    const Environment &environment,
    const TemplateRawMap &templates,
    const InteractionTemplateRawMap &interactions,
    std::vector<std::string> &template_stack)
{
    auto id = parse_string_field(object, "id");
    auto template_id = parse_string_field(object, "templateId");
    if (!id || !template_id) {
        return std::unexpected(!id ? id.error() : template_id.error());
    }
    const auto template_it = templates.find(*template_id);
    if (template_it == templates.end()) {
        return std::unexpected("TemplateRef references missing templateId: " + *template_id);
    }
    if (std::find(template_stack.begin(), template_stack.end(), *template_id) != template_stack.end()) {
        return std::unexpected("TemplateRef cycle detected for templateId: " + *template_id);
    }

    auto node_object = template_it->second;
    node_object.insert_or_assign("id", *id);
    const boost::json::object *slots = nullptr;
    if (const auto *slots_value = find_child_value(object, "slots"); slots_value != nullptr) {
        if (!slots_value->is_object()) {
            return std::unexpected("TemplateRef field 'slots' must be an object");
        }
        slots = &slots_value->as_object();
    }
    auto slot_result = apply_template_slots(node_object, slots);
    if (!slot_result) {
        return std::unexpected(slot_result.error());
    }
    if (const auto *overrides_value = find_child_value(object, "overrides"); overrides_value != nullptr) {
        if (!overrides_value->is_object()) {
            return std::unexpected("TemplateRef field 'overrides' must be an object");
        }
        auto override_result = apply_template_overrides(node_object, overrides_value->as_object());
        if (!override_result) {
            return std::unexpected(override_result.error());
        }
    }

    template_stack.push_back(*template_id);
    auto parsed = parse_view_node(node_object, environment, templates, interactions, template_stack);
    template_stack.pop_back();
    return parsed;
}

static std::expected<std::vector<Node>, std::string> parse_children(
    const boost::json::object &object,
    const Environment &environment,
    const TemplateRawMap &templates,
    const InteractionTemplateRawMap &interactions,
    std::vector<std::string> &template_stack)
{
    std::vector<Node> children;
    const auto *value = find_child_value(object, "children");
    if (value == nullptr) {
        return children;
    }
    if (!value->is_array()) {
        return std::unexpected("Field 'children' must be an array");
    }

    for (const auto &child : value->as_array()) {
        if (!child.is_object()) {
            return std::unexpected("Child nodes must be objects");
        }
        const auto &child_object = child.as_object();
        auto child_type = parse_string_field(child_object, "type");
        if (!child_type) {
            return std::unexpected(child_type.error());
        }
        if (*child_type == "slot") {
            return std::unexpected("Node type 'slot' is only allowed inside viewTemplate assets");
        }
        auto parsed_child = (*child_type == "templateRef") ?
                            parse_template_ref(child_object, environment, templates, interactions, template_stack) :
                            parse_view_node(child_object, environment, templates, interactions, template_stack);
        if (!parsed_child) {
            return std::unexpected(parsed_child.error());
        }
        if (parsed_child->type == NodeType::Screen) {
            return std::unexpected("Node type 'screen' is only allowed at the top level");
        }
        children.push_back(std::move(*parsed_child));
    }

    return children;
}

static std::expected<NodeType, std::string> parse_node_type(std::string_view type)
{
    if (type == "screen") {
        return NodeType::Screen;
    }
    if (type == "container") {
        return NodeType::Container;
    }
    if (type == "label") {
        return NodeType::Label;
    }
    if (type == "button") {
        return NodeType::Button;
    }
    if (type == "image") {
        return NodeType::Image;
    }
    if (type == "frameView" || type == "frame_view") {
        return NodeType::FrameView;
    }
    if (type == "textInput" || type == "text_input" || type == "textarea" || type == "textArea") {
        return NodeType::TextInput;
    }
    if (type == "slider") {
        return NodeType::Slider;
    }
    if (type == "switch") {
        return NodeType::Switch;
    }
    if (type == "checkbox") {
        return NodeType::Checkbox;
    }
    if (type == "dropdown") {
        return NodeType::Dropdown;
    }
    if (type == "progressBar" || type == "progress_bar") {
        return NodeType::ProgressBar;
    }
    if (type == "spinner") {
        return NodeType::Spinner;
    }
    if (type == "arc") {
        return NodeType::Arc;
    }
    if (type == "line") {
        return NodeType::Line;
    }
    if (type == "table") {
        return NodeType::Table;
    }
    if (type == "keyboard") {
        return NodeType::Keyboard;
    }
    if (type == "canvas") {
        return NodeType::Canvas;
    }

    return std::unexpected("Unsupported view node type: " + std::string(type));
}

static bool default_clickable_for_node_type(NodeType type)
{
    switch (type) {
    case NodeType::Label:
        return false;
    case NodeType::Button:
    case NodeType::TextInput:
    case NodeType::Slider:
    case NodeType::Switch:
    case NodeType::Checkbox:
    case NodeType::Dropdown:
    case NodeType::Arc:
    case NodeType::Keyboard:
    case NodeType::Screen:
    case NodeType::Container:
    case NodeType::Image:
    case NodeType::FrameView:
    case NodeType::ProgressBar:
    case NodeType::Spinner:
    case NodeType::Line:
    case NodeType::Table:
    case NodeType::Canvas:
    case NodeType::Max:
    default:
        return true;
    }
}

static bool default_scrollable_for_node_type(NodeType type)
{
    switch (type) {
    case NodeType::Screen:
    case NodeType::Container:
        return true;
    case NodeType::Label:
    case NodeType::Button:
    case NodeType::Image:
    case NodeType::FrameView:
    case NodeType::TextInput:
    case NodeType::Slider:
    case NodeType::Switch:
    case NodeType::Checkbox:
    case NodeType::Dropdown:
    case NodeType::ProgressBar:
    case NodeType::Spinner:
    case NodeType::Arc:
    case NodeType::Line:
    case NodeType::Table:
    case NodeType::Keyboard:
    case NodeType::Canvas:
    case NodeType::Max:
    default:
        return false;
    }
}

static CommonProps get_builtin_default_common_props(NodeType type)
{
    CommonProps props;
    props.clickable = default_clickable_for_node_type(type);
    props.scrollable = default_scrollable_for_node_type(type);
    return props;
}

static Layout get_builtin_default_layout(NodeType type)
{
    (void)type;
    return Layout{};
}

static Placement get_builtin_default_placement(NodeType type)
{
    Placement placement;
    if (type == NodeType::Screen) {
        placement.width = Dimension{.mode = SizeMode::Match, .value = 0};
        placement.height = Dimension{.mode = SizeMode::Match, .value = 0};
    }
    return placement;
}

static void merge_object_defaults(boost::json::object &target, const boost::json::object &defaults)
{
    for (const auto &entry : defaults) {
        auto target_it = find_key(target, normalize_object_key(entry.key()));
        if (target_it == target.end()) {
            target.insert_or_assign(std::string(entry.key()), entry.value());
            continue;
        }
        if (target_it->value().is_object() && entry.value().is_object()) {
            merge_object_defaults(target_it->value().as_object(), entry.value().as_object());
        }
    }
}

static std::expected<void, std::string> prepend_array_defaults(
    boost::json::object &target,
    const boost::json::object &defaults,
    std::string_view field)
{
    auto defaults_it = find_key(defaults, field);
    if (defaults_it == defaults.end()) {
        return {};
    }
    if (!defaults_it->value().is_array()) {
        return std::unexpected("interactionTemplate field '" + std::string(field) + "' must be an array");
    }

    boost::json::array merged;
    for (const auto &value : defaults_it->value().as_array()) {
        merged.push_back(value);
    }

    auto target_it = find_key(target, field);
    if (target_it != target.end()) {
        if (!target_it->value().is_array()) {
            return std::unexpected("Node field '" + std::string(field) + "' must be an array");
        }
        for (const auto &value : target_it->value().as_array()) {
            merged.push_back(value);
        }
        target.erase(target_it);
    }
    target.insert_or_assign(std::string(field), std::move(merged));
    return {};
}

static std::expected<void, std::string> apply_interaction_template_object(
    boost::json::object &target,
    const boost::json::object &interaction,
    std::string_view id)
{
    for (const auto &field : {
                "common_props", "state_styles"
            }) {
        auto interaction_it = find_key(interaction, field);
        if (interaction_it == interaction.end()) {
            continue;
        }
        if (!interaction_it->value().is_object()) {
            return std::unexpected(
                       "interactionTemplate '" + std::string(id) + "' field '" + field + "' must be an object"
                   );
        }

        auto target_it = find_key(target, field);
        if (target_it == target.end()) {
            target.insert_or_assign(std::string(interaction_it->key()), interaction_it->value());
            continue;
        }
        if (!target_it->value().is_object()) {
            return std::unexpected("Node field '" + std::string(field) + "' must be an object");
        }
        merge_object_defaults(target_it->value().as_object(), interaction_it->value().as_object());
    }

    auto events = prepend_array_defaults(target, interaction, "events");
    if (!events) {
        return events;
    }
    return prepend_array_defaults(target, interaction, "animations");
}

static std::expected<boost::json::object, std::string> apply_interaction_templates(
    const boost::json::object &source,
    const InteractionTemplateRawMap &interactions)
{
    auto refs = parse_string_array(source, "interactionRefs");
    if (!refs) {
        return std::unexpected(refs.error());
    }
    boost::json::object target = source;
    for (auto it = refs->rbegin(); it != refs->rend(); ++it) {
        auto interaction_it = interactions.find(*it);
        if (interaction_it == interactions.end()) {
            return std::unexpected("Node references missing interactionTemplate: " + *it);
        }
        auto apply_result = apply_interaction_template_object(target, interaction_it->second, *it);
        if (!apply_result) {
            return std::unexpected(apply_result.error());
        }
    }
    return target;
}

static std::expected<Node, std::string> parse_view_node(
    const boost::json::object &source_object,
    const Environment &environment,
    const TemplateRawMap &templates,
    const InteractionTemplateRawMap &interactions,
    std::vector<std::string> &template_stack,
    std::optional<NodeType> forced_type)
{
    BROOKESIA_LOG_TRACE_GUARD();

    auto interaction_object = apply_interaction_templates(source_object, interactions);
    if (!interaction_object) {
        return std::unexpected(interaction_object.error());
    }
    const auto &object = *interaction_object;

    BROOKESIA_LOGD("Params: object(keys=%1%), environment(%2%)", object.size(), environment);

    Node node;

    if (find_child_value(object, "subtype") != nullptr) {
        return std::unexpected("Field 'subtype' is no longer supported; use node field 'type'");
    }
    if (find_child_value(object, "type") != nullptr &&
            find_child_value(object, "type")->is_string() &&
            find_child_value(object, "type")->as_string() == "view") {
        return std::unexpected("View asset type 'view' is no longer supported; use viewScreen/viewTemplate assets");
    }
    if (forced_type.has_value()) {
        node.type = *forced_type;
    } else {
        auto type_name = parse_string_field(object, "type");
        if (!type_name) {
            return std::unexpected(type_name.error());
        }
        auto type = parse_node_type(*type_name);
        if (!type) {
            return std::unexpected(type.error());
        }
        node.type = *type;
    }
    node.common_props = get_builtin_default_common_props(node.type);
    node.layout = get_builtin_default_layout(node.type);
    node.placement = get_builtin_default_placement(node.type);

    const auto *mount_mode_value = find_child_value(object, "mount_mode");
    if (mount_mode_value != nullptr) {
        if (node.type != NodeType::Screen) {
            return std::unexpected("Field 'mount_mode' is only allowed on top-level screen nodes");
        }

        auto mount_mode = parse_enum_field<MountMode>(object, "mount_mode", MountMode::Eager);
        if (!mount_mode) {
            return std::unexpected(mount_mode.error());
        }
        node.mount_mode = *mount_mode;
    }

    auto id = parse_string_field(object, "id");
    if (!id) {
        return std::unexpected(id.error());
    }
    node.id = *id;

    if (find_child_value(object, "props") != nullptr) {
        return std::unexpected(
                   "Field 'props' is no longer supported; move fields into 'commonProps' or the corresponding "
                   "typed props object"
               );
    }

    const auto *common_props_value = find_child_value(object, "common_props");
    if (common_props_value != nullptr) {
        if (!common_props_value->is_object()) {
            return std::unexpected("Field 'commonProps' must be an object");
        }
        const auto &common_props_object = common_props_value->as_object();
        auto hidden = parse_bool_field(common_props_object, "hidden");
        if (!hidden) {
            return std::unexpected(hidden.error());
        }
        node.common_props.hidden = *hidden;

        auto disabled = parse_bool_field(common_props_object, "disabled");
        if (!disabled) {
            return std::unexpected(disabled.error());
        }
        node.common_props.disabled = *disabled;

        auto clickable = parse_bool_field(common_props_object, "clickable", node.common_props.clickable);
        if (!clickable) {
            return std::unexpected(clickable.error());
        }
        node.common_props.clickable = *clickable;

        auto scrollable = parse_bool_field(common_props_object, "scrollable", node.common_props.scrollable);
        if (!scrollable) {
            return std::unexpected(scrollable.error());
        }
        node.common_props.scrollable = *scrollable;

        auto press_lock = parse_bool_field(common_props_object, "pressLock", node.common_props.press_lock);
        if (!press_lock) {
            return std::unexpected(press_lock.error());
        }
        node.common_props.press_lock = *press_lock;

        auto angle = parse_int_field(common_props_object, "angle", node.common_props.angle);
        if (!angle) {
            return std::unexpected(angle.error());
        }
        node.common_props.angle = *angle;

        auto zoom = parse_int_field(common_props_object, "zoom", node.common_props.zoom);
        if (!zoom) {
            return std::unexpected(zoom.error());
        }
        node.common_props.zoom = *zoom;

        auto pivot_x = parse_pivot_field(common_props_object, "pivotX", node.common_props.pivot_x);
        if (!pivot_x) {
            return std::unexpected(pivot_x.error());
        }
        node.common_props.pivot_x = *pivot_x;

        auto pivot_y = parse_pivot_field(common_props_object, "pivotY", node.common_props.pivot_y);
        if (!pivot_y) {
            return std::unexpected(pivot_y.error());
        }
        node.common_props.pivot_y = *pivot_y;
    }

    const auto *label_props_value = find_child_value(object, "label_props");
    if (label_props_value != nullptr) {
        if (node.type != NodeType::Label && node.type != NodeType::Checkbox) {
            return std::unexpected("Field 'labelProps' is only allowed on label and checkbox nodes");
        }
        if (!label_props_value->is_object()) {
            return std::unexpected("Field 'labelProps' must be an object");
        }
        const auto &label_props_object = label_props_value->as_object();
        auto text = parse_string_field(label_props_object, "text");
        if (!text) {
            return std::unexpected(text.error());
        }
        node.label_props.text = *text;
    }

    const auto *image_props_value = find_child_value(object, "image_props");
    if (image_props_value != nullptr) {
        if (node.type != NodeType::Image) {
            return std::unexpected("Field 'imageProps' is only allowed on image nodes");
        }
        if (!image_props_value->is_object()) {
            return std::unexpected("Field 'imageProps' must be an object");
        }
        const auto &image_props_object = image_props_value->as_object();
        auto src = parse_resource_reference_field(image_props_object, "src", "image", "'${image.<id>}'");
        if (!src) {
            return std::unexpected(src.error());
        }
        node.image_props.src = *src;
        auto inner_align = parse_string_field(image_props_object, "innerAlign", node.image_props.inner_align);
        if (!inner_align) {
            return std::unexpected(inner_align.error());
        }
        node.image_props.inner_align = *inner_align;
        auto recolor = parse_string_field(image_props_object, "recolor", node.image_props.recolor);
        if (!recolor) {
            return std::unexpected(recolor.error());
        }
        node.image_props.recolor = *recolor;
        auto recolor_opacity = parse_int_field(
                                   image_props_object, "recolorOpacity", node.image_props.recolor_opacity
                               );
        if (!recolor_opacity) {
            return std::unexpected(recolor_opacity.error());
        }
        node.image_props.recolor_opacity = *recolor_opacity;
        auto angle = parse_int_field(image_props_object, "angle", node.image_props.angle);
        if (!angle) {
            return std::unexpected(angle.error());
        }
        node.image_props.angle = *angle;
        auto offset_x = parse_int_field(image_props_object, "offsetX", node.image_props.offset_x);
        if (!offset_x) {
            return std::unexpected(offset_x.error());
        }
        node.image_props.offset_x = *offset_x;
        auto offset_y = parse_int_field(image_props_object, "offsetY", node.image_props.offset_y);
        if (!offset_y) {
            return std::unexpected(offset_y.error());
        }
        node.image_props.offset_y = *offset_y;
        auto zoom = parse_int_field(image_props_object, "zoom", node.image_props.zoom);
        if (!zoom) {
            return std::unexpected(zoom.error());
        }
        node.image_props.zoom = *zoom;
        auto pivot_x = parse_pivot_field(image_props_object, "pivotX", node.image_props.pivot_x);
        if (!pivot_x) {
            return std::unexpected(pivot_x.error());
        }
        node.image_props.pivot_x = *pivot_x;
        auto pivot_y = parse_pivot_field(image_props_object, "pivotY", node.image_props.pivot_y);
        if (!pivot_y) {
            return std::unexpected(pivot_y.error());
        }
        node.image_props.pivot_y = *pivot_y;
    }

    const auto *frame_view_props_value = find_child_value(object, "frame_view_props");
    if (frame_view_props_value != nullptr) {
        if (node.type != NodeType::FrameView) {
            return std::unexpected("Field 'frameViewProps' is only allowed on frameView nodes");
        }
        if (!frame_view_props_value->is_object()) {
            return std::unexpected("Field 'frameViewProps' must be an object");
        }
        const auto &frame_view_props_object = frame_view_props_value->as_object();
        auto auto_register_output = parse_bool_field(
                                        frame_view_props_object, "auto_register_output",
                                        node.frame_view_props.auto_register_output
                                    );
        if (!auto_register_output) {
            return std::unexpected(auto_register_output.error());
        }
        node.frame_view_props.auto_register_output = *auto_register_output;

        auto output_name = parse_string_field(
                               frame_view_props_object, "output_name", node.frame_view_props.output_name
                           );
        if (!output_name) {
            return std::unexpected(output_name.error());
        }
        node.frame_view_props.output_name = *output_name;

        auto color_format = parse_enum_field<FrameColorFormat>(
                                frame_view_props_object, "color_format", node.frame_view_props.color_format
                            );
        if (!color_format) {
            return std::unexpected(color_format.error());
        }
        node.frame_view_props.color_format = *color_format;
    }

    const auto *text_input_props_value = find_child_value(object, "text_input_props");
    if (text_input_props_value != nullptr) {
        if (node.type != NodeType::TextInput) {
            return std::unexpected("Field 'textInputProps' is only allowed on textInput nodes");
        }
        if (!text_input_props_value->is_object()) {
            return std::unexpected("Field 'textInputProps' must be an object");
        }
        const auto &text_input_props_object = text_input_props_value->as_object();
        auto text = parse_string_field(text_input_props_object, "text");
        if (!text) {
            return std::unexpected(text.error());
        }
        node.text_input_props.text = *text;
        auto placeholder = parse_string_field(text_input_props_object, "placeholder");
        if (!placeholder) {
            return std::unexpected(placeholder.error());
        }
        node.text_input_props.placeholder = *placeholder;
        auto password = parse_bool_field(text_input_props_object, "password");
        if (!password) {
            return std::unexpected(password.error());
        }
        node.text_input_props.password = *password;
        auto multiline = parse_bool_field(text_input_props_object, "multiline");
        if (!multiline) {
            return std::unexpected(multiline.error());
        }
        node.text_input_props.multiline = *multiline;
        auto max_length = parse_int_field(text_input_props_object, "max_length");
        if (!max_length) {
            return std::unexpected(max_length.error());
        }
        node.text_input_props.max_length = *max_length;
    }

    const auto *range_props_value = find_child_value(object, "range_props");
    if (range_props_value != nullptr) {
        if (node.type != NodeType::Slider && node.type != NodeType::ProgressBar && node.type != NodeType::Arc) {
            return std::unexpected("Field 'rangeProps' is only allowed on slider, progressBar, and arc nodes");
        }
        if (!range_props_value->is_object()) {
            return std::unexpected("Field 'rangeProps' must be an object");
        }
        const auto &range_props_object = range_props_value->as_object();
        auto value = parse_int_field(range_props_object, "value");
        if (!value) {
            return std::unexpected(value.error());
        }
        auto min = parse_int_field(range_props_object, "min");
        if (!min) {
            return std::unexpected(min.error());
        }
        auto max = parse_int_field(range_props_object, "max", 100);
        if (!max) {
            return std::unexpected(max.error());
        }
        auto step = parse_int_field(range_props_object, "step", 1);
        if (!step) {
            return std::unexpected(step.error());
        }
        node.range_props = RangeProps{.value = *value, .min = *min, .max = *max, .step = *step};
    }

    const auto *toggle_props_value = find_child_value(object, "toggle_props");
    if (toggle_props_value != nullptr) {
        if (node.type != NodeType::Switch && node.type != NodeType::Checkbox) {
            return std::unexpected("Field 'toggleProps' is only allowed on switch and checkbox nodes");
        }
        if (!toggle_props_value->is_object()) {
            return std::unexpected("Field 'toggleProps' must be an object");
        }
        const auto &toggle_props_object = toggle_props_value->as_object();
        auto checked = parse_bool_field(toggle_props_object, "checked");
        if (!checked) {
            return std::unexpected(checked.error());
        }
        node.toggle_props.checked = *checked;
    }

    const auto *dropdown_props_value = find_child_value(object, "dropdown_props");
    if (dropdown_props_value != nullptr) {
        if (node.type != NodeType::Dropdown) {
            return std::unexpected("Field 'dropdownProps' is only allowed on dropdown nodes");
        }
        if (!dropdown_props_value->is_object()) {
            return std::unexpected("Field 'dropdownProps' must be an object");
        }
        const auto &dropdown_props_object = dropdown_props_value->as_object();
        auto options = parse_string_array(dropdown_props_object, "options");
        if (!options) {
            return std::unexpected(options.error());
        }
        auto selected_index = parse_int_field(dropdown_props_object, "selected_index");
        if (!selected_index) {
            return std::unexpected(selected_index.error());
        }
        node.dropdown_props.options = std::move(*options);
        node.dropdown_props.selected_index = *selected_index;
    }

    const auto *table_props_value = find_child_value(object, "table_props");
    if (table_props_value != nullptr) {
        if (node.type != NodeType::Table) {
            return std::unexpected("Field 'tableProps' is only allowed on table nodes");
        }
        if (!table_props_value->is_object()) {
            return std::unexpected("Field 'tableProps' must be an object");
        }
        const auto &table_props_object = table_props_value->as_object();
        auto rows = parse_int_field(table_props_object, "rows");
        if (!rows) {
            return std::unexpected(rows.error());
        }
        auto columns = parse_int_field(table_props_object, "columns");
        if (!columns) {
            return std::unexpected(columns.error());
        }
        auto cells = parse_table_cells_field(table_props_object);
        if (!cells) {
            return std::unexpected(cells.error());
        }
        node.table_props = TableProps{.rows = *rows, .columns = *columns, .cells = std::move(*cells)};
    }

    const auto *line_props_value = find_child_value(object, "line_props");
    if (line_props_value != nullptr) {
        if (node.type != NodeType::Line) {
            return std::unexpected("Field 'lineProps' is only allowed on line nodes");
        }
        if (!line_props_value->is_object()) {
            return std::unexpected("Field 'lineProps' must be an object");
        }
        const auto &line_props_object = line_props_value->as_object();
        auto points = parse_points_field(line_props_object);
        if (!points) {
            return std::unexpected(points.error());
        }
        node.line_props.points = std::move(*points);
    }

    const auto *keyboard_props_value = find_child_value(object, "keyboard_props");
    if (keyboard_props_value != nullptr) {
        if (node.type != NodeType::Keyboard) {
            return std::unexpected("Field 'keyboardProps' is only allowed on keyboard nodes");
        }
        if (!keyboard_props_value->is_object()) {
            return std::unexpected("Field 'keyboardProps' must be an object");
        }
        const auto &keyboard_props_object = keyboard_props_value->as_object();
        auto mode = parse_string_field(keyboard_props_object, "mode", "text");
        if (!mode) {
            return std::unexpected(mode.error());
        }
        auto popovers = parse_bool_field(keyboard_props_object, "popovers");
        if (!popovers) {
            return std::unexpected(popovers.error());
        }
        Dimension icon_size{.mode = SizeMode::Fixed, .value = 0};
        if (const auto *icon_size_value = find_child_value(keyboard_props_object, "iconSize"); icon_size_value != nullptr) {
            auto parsed_icon_size = parse_dimension(icon_size_value, "keyboardProps.iconSize", environment);
            if (!parsed_icon_size) {
                return std::unexpected(parsed_icon_size.error());
            }
            icon_size = *parsed_icon_size;
        }
        auto target_text_input = parse_string_field(keyboard_props_object, "targetTextInput");
        if (!target_text_input) {
            return std::unexpected(target_text_input.error());
        }
        auto allowed_modes = parse_keyboard_allowed_modes_field(keyboard_props_object);
        if (!allowed_modes) {
            return std::unexpected(allowed_modes.error());
        }
        auto layouts = parse_keyboard_layouts_field(keyboard_props_object);
        if (!layouts) {
            return std::unexpected(layouts.error());
        }
        auto key_styles = parse_keyboard_key_styles_field(keyboard_props_object, environment);
        if (!key_styles) {
            return std::unexpected(key_styles.error());
        }
        auto key_style_refs = parse_keyboard_key_style_refs_field(keyboard_props_object);
        if (!key_style_refs) {
            return std::unexpected(key_style_refs.error());
        }
        node.keyboard_props = KeyboardProps{
            .mode = *mode,
            .popovers = *popovers,
            .icon_size = icon_size,
            .target_text_input = std::move(*target_text_input),
            .allowed_modes = std::move(*allowed_modes),
            .layouts = std::move(*layouts),
            .key_styles = std::move(*key_styles),
            .key_style_refs = std::move(*key_style_refs),
            .resolved_key_styles = {},
        };
    }

    const auto *canvas_props_value = find_child_value(object, "canvas_props");
    if (canvas_props_value != nullptr) {
        if (node.type != NodeType::Canvas) {
            return std::unexpected("Field 'canvasProps' is only allowed on canvas nodes");
        }
        if (!canvas_props_value->is_object()) {
            return std::unexpected("Field 'canvasProps' must be an object");
        }
        const auto &canvas_props_object = canvas_props_value->as_object();
        auto commands = parse_canvas_commands_field(canvas_props_object);
        if (!commands) {
            return std::unexpected(commands.error());
        }
        node.canvas_props.commands = std::move(*commands);
    }

    const auto *layout_value = find_child_value(object, "layout");
    if (layout_value != nullptr) {
        if (!layout_value->is_object()) {
            return std::unexpected("Field 'layout' must be an object");
        }
        const auto &layout_object = layout_value->as_object();
        if (const auto *type_value = find_child_value(layout_object, "type");
                type_value != nullptr && type_value->is_string() && type_value->as_string() == "absolute") {
            return std::unexpected(
                       "layout.type='absolute' is no longer supported; use placement.mode='absolute'"
                   );
        }
        auto rejected_layout_fields = reject_fields(
        layout_object, {
            "x", "y", "width", "height", "grid_column", "grid_row",
            "grid_column_span", "grid_row_span", "align_self", "flex_grow"
        },
        "placement"
                                      );
        if (!rejected_layout_fields) {
            return std::unexpected(rejected_layout_fields.error());
        }

        if (find_child_value(layout_object, "type") != nullptr) {
            auto layout_type = parse_enum_field<LayoutType>(layout_object, "type", node.layout.type);
            if (!layout_type) {
                return std::unexpected(layout_type.error());
            }
            node.layout.type = *layout_type;
        }

        if (find_child_value(layout_object, "flex_flow") != nullptr) {
            auto flex_flow = parse_enum_field<FlexFlow>(layout_object, "flex_flow", node.layout.flex_flow);
            if (!flex_flow) {
                return std::unexpected(flex_flow.error());
            }
            node.layout.flex_flow = *flex_flow;
        }

        if (find_child_value(layout_object, "main_align") != nullptr) {
            auto main_align = parse_enum_field<Align>(layout_object, "main_align", node.layout.main_align);
            if (!main_align) {
                return std::unexpected(main_align.error());
            }
            node.layout.main_align = *main_align;
        }

        if (find_child_value(layout_object, "cross_align") != nullptr) {
            auto cross_align = parse_enum_field<Align>(layout_object, "cross_align", node.layout.cross_align);
            if (!cross_align) {
                return std::unexpected(cross_align.error());
            }
            node.layout.cross_align = *cross_align;
        }

        if (find_child_value(layout_object, "gap") != nullptr) {
            auto gap = parse_scaled_value(find_child_value(layout_object, "gap"), "gap", "dp", environment.density);
            if (!gap) {
                return std::unexpected(gap.error());
            }
            node.layout.gap = *gap;
        }

        if (find_child_value(layout_object, "grid_template_columns") != nullptr) {
            auto grid_columns = parse_dimension_array(layout_object, "grid_template_columns", environment);
            if (!grid_columns) {
                return std::unexpected(grid_columns.error());
            }
            node.layout.grid_template_columns = std::move(*grid_columns);
        }

        if (find_child_value(layout_object, "grid_template_rows") != nullptr) {
            auto grid_rows = parse_dimension_array(layout_object, "grid_template_rows", environment);
            if (!grid_rows) {
                return std::unexpected(grid_rows.error());
            }
            node.layout.grid_template_rows = std::move(*grid_rows);
        }
    }

    const auto *placement_value = find_child_value(object, "placement");
    if (placement_value != nullptr) {
        if (!placement_value->is_object()) {
            return std::unexpected("Field 'placement' must be an object");
        }
        const auto &placement_object = placement_value->as_object();

        if (find_child_value(placement_object, "mode") != nullptr) {
            auto placement_mode = parse_enum_field<PlacementMode>(placement_object, "mode", node.placement.mode);
            if (!placement_mode) {
                return std::unexpected(placement_mode.error());
            }
            node.placement.mode = *placement_mode;
        }

        if (find_child_value(placement_object, "x") != nullptr) {
            auto x = parse_placement_offset(find_child_value(placement_object, "x"), "x", environment);
            if (!x) {
                return std::unexpected(x.error());
            }
            node.placement.x = *x;
        }

        if (find_child_value(placement_object, "y") != nullptr) {
            auto y = parse_placement_offset(find_child_value(placement_object, "y"), "y", environment);
            if (!y) {
                return std::unexpected(y.error());
            }
            node.placement.y = *y;
        }

        const auto *width_value = find_child_value(placement_object, "width");
        if (width_value != nullptr) {
            auto width = parse_dimension(width_value, "width", environment);
            if (!width) {
                return std::unexpected(width.error());
            }
            node.placement.width = *width;
        }

        const auto *height_value = find_child_value(placement_object, "height");
        if (height_value != nullptr) {
            auto height = parse_dimension(height_value, "height", environment);
            if (!height) {
                return std::unexpected(height.error());
            }
            node.placement.height = *height;
        }

        const auto *aspect_ratio_value = find_child_value(placement_object, "aspect_ratio");
        if (aspect_ratio_value != nullptr) {
            auto aspect_ratio = parse_aspect_ratio(aspect_ratio_value, "aspectRatio");
            if (!aspect_ratio) {
                return std::unexpected(aspect_ratio.error());
            }
            node.placement.aspect_ratio = *aspect_ratio;
        }

        if (find_child_value(placement_object, "align") != nullptr) {
            auto align = parse_enum_field<PlacementAlign>(placement_object, "align", node.placement.align);
            if (!align) {
                return std::unexpected(align.error());
            }
            node.placement.align = *align;
        }

        if (find_child_value(placement_object, "relative_to") != nullptr) {
            auto relative_to = parse_view_reference_field(placement_object, "relative_to");
            if (!relative_to) {
                return std::unexpected(relative_to.error());
            }
            node.placement.relative_to = *relative_to;
        }

        if (find_child_value(placement_object, "grid_column") != nullptr) {
            auto grid_column = parse_int_field(placement_object, "grid_column", node.placement.grid_column);
            if (!grid_column) {
                return std::unexpected(grid_column.error());
            }
            node.placement.grid_column = *grid_column;
        }

        if (find_child_value(placement_object, "grid_row") != nullptr) {
            auto grid_row = parse_int_field(placement_object, "grid_row", node.placement.grid_row);
            if (!grid_row) {
                return std::unexpected(grid_row.error());
            }
            node.placement.grid_row = *grid_row;
        }

        if (find_child_value(placement_object, "grid_column_span") != nullptr) {
            auto grid_column_span = parse_int_field(placement_object, "grid_column_span", node.placement.grid_column_span);
            if (!grid_column_span) {
                return std::unexpected(grid_column_span.error());
            }
            node.placement.grid_column_span = *grid_column_span;
        }

        if (find_child_value(placement_object, "grid_row_span") != nullptr) {
            auto grid_row_span = parse_int_field(placement_object, "grid_row_span", node.placement.grid_row_span);
            if (!grid_row_span) {
                return std::unexpected(grid_row_span.error());
            }
            node.placement.grid_row_span = *grid_row_span;
        }

        if (find_child_value(placement_object, "align_self") != nullptr) {
            auto align_self = parse_enum_field<Align>(placement_object, "align_self", node.placement.align_self);
            if (!align_self) {
                return std::unexpected(align_self.error());
            }
            node.placement.align_self = *align_self;
        }

        if (find_child_value(placement_object, "flex_grow") != nullptr) {
            auto flex_grow = parse_int_field(placement_object, "flex_grow", node.placement.flex_grow);
            if (!flex_grow) {
                return std::unexpected(flex_grow.error());
            }
            if (*flex_grow < 0) {
                return std::unexpected("Field 'flexGrow' must be >= 0");
            }
            node.placement.flex_grow = *flex_grow;
        }
    }

    const auto *style_value = find_child_value(object, "style");
    if (style_value != nullptr) {
        if (!style_value->is_object()) {
            return std::unexpected("Field 'style' must be an object");
        }
        auto style = parse_style_object(style_value->as_object(), environment);
        if (!style) {
            return std::unexpected(style.error());
        }
        node.style = std::move(*style);
    }

    const auto *state_styles_value = find_child_value(object, "state_styles");
    if (state_styles_value != nullptr) {
        if (!state_styles_value->is_object()) {
            return std::unexpected("Field 'stateStyles' must be an object");
        }
        auto state_styles = parse_state_styles_object(state_styles_value->as_object(), environment);
        if (!state_styles) {
            return std::unexpected(state_styles.error());
        }
        node.state_styles = std::move(*state_styles);
    }

    const auto *part_styles_value = find_child_value(object, "part_styles");
    if (part_styles_value != nullptr) {
        if (!part_styles_value->is_object()) {
            return std::unexpected("Field 'partStyles' must be an object");
        }
        auto part_styles = parse_part_styles_object(part_styles_value->as_object(), environment);
        if (!part_styles) {
            return std::unexpected(part_styles.error());
        }
        node.part_styles = std::move(*part_styles);
    }

    auto style_refs = parse_string_array(object, "style_refs");
    if (!style_refs) {
        return std::unexpected(style_refs.error());
    }
    node.style_refs = std::move(*style_refs);

    const auto *events_value = find_child_value(object, "events");
    if (events_value != nullptr) {
        if (!events_value->is_array()) {
            return std::unexpected("Field 'events' must be an array");
        }
        for (const auto &entry : events_value->as_array()) {
            if (!entry.is_object()) {
                return std::unexpected("Event entries must be objects");
            }
            auto binding = parse_event_binding(entry.as_object());
            if (!binding) {
                return std::unexpected(binding.error());
            }
            node.events.push_back(std::move(*binding));
        }
    }

    const auto *animations_value = find_child_value(object, "animations");
    if (animations_value != nullptr) {
        if (!animations_value->is_array()) {
            return std::unexpected("Field 'animations' must be an array");
        }
        for (const auto &entry : animations_value->as_array()) {
            if (!entry.is_object()) {
                return std::unexpected("Animation entries must be objects");
            }
            auto animation = parse_animation(entry.as_object());
            if (!animation) {
                return std::unexpected(animation.error());
            }
            node.animations.push_back(*animation);
        }
    }

    const auto *bindings_value = find_child_value(object, "bindings");
    if (bindings_value != nullptr) {
        if (!bindings_value->is_object()) {
            return std::unexpected("Field 'bindings' must be an object");
        }
        for (const auto &[key, value] : bindings_value->as_object()) {
            if (!value.is_string()) {
                return std::unexpected("Binding expressions must be strings");
            }
            node.bindings.emplace(std::string(key), value.as_string().c_str());
        }
    }

    auto children = parse_children(object, environment, templates, interactions, template_stack);
    if (!children) {
        return std::unexpected(children.error());
    }
    node.children = std::move(*children);

    return node;
}

static std::expected<uint32_t, std::string> parse_image_font_codepoint(std::string_view text)
{
    std::string hex_text(text);
    if (hex_text.starts_with("U+") || hex_text.starts_with("u+")) {
        hex_text.erase(0, 2);
    } else if (hex_text.starts_with("0x") || hex_text.starts_with("0X")) {
        hex_text.erase(0, 2);
    } else {
        return std::unexpected("imageFont glyph codepoint must use U+XXXX or 0xXXXX format");
    }
    if (hex_text.empty()) {
        return std::unexpected("imageFont glyph codepoint must not be empty");
    }

    char *parse_end = nullptr;
    errno = 0;
    const auto parsed = std::strtoul(hex_text.c_str(), &parse_end, 16);
    if (errno != 0 || parse_end == hex_text.c_str() || *parse_end != '\0' || parsed > 0x10FFFFUL ||
            (parsed >= 0xD800UL && parsed <= 0xDFFFUL)) {
        return std::unexpected("Invalid imageFont glyph codepoint: " + std::string(text));
    }
    return static_cast<uint32_t>(parsed);
}

static std::expected<std::vector<ImageFontGlyph>, std::string> parse_image_font_glyphs(
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

static std::vector<uint32_t> sorted_image_font_codepoints(const std::vector<ImageFontGlyph> &glyphs)
{
    std::vector<uint32_t> codepoints;
    codepoints.reserve(glyphs.size());
    for (const auto &glyph : glyphs) {
        codepoints.push_back(glyph.codepoint);
    }
    std::sort(codepoints.begin(), codepoints.end());
    return codepoints;
}

static std::expected<FontAsset, std::string> parse_font_asset(
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

static std::expected<ImageAsset, std::string> parse_image_asset(
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

static std::expected<std::vector<FontAsset>, std::string> parse_font_asset_set(
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

static std::expected<std::vector<ImageAsset>, std::string> parse_image_asset_set(
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

static std::expected<void, std::string> merge_theme_constant_asset(
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

static std::expected<void, std::string> append_and_merge_theme_constant_assets(
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

static std::expected<ThemeAsset, std::string> parse_theme_asset(
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

    boost::json::value constants((boost::json::object()));
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

static std::expected<std::vector<std::string>, std::string> parse_screen_flow_from_array(
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

static std::expected<ScreenFlow, std::string> parse_screen_flow(const boost::json::object &object)
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

} // namespace

static std::expected<ParsedDocument, std::string> parse_document_impl(
    std::string_view json,
    std::string_view base_dir,
    const Environment &environment,
    std::vector<std::string> dependency_files)
{
    BROOKESIA_LOG_TRACE_GUARD();

    BROOKESIA_LOGD(
        "Params: json(size=%1%), base_dir(%2%), environment(%3%)",
        json.size(), base_dir, environment
    );

    const auto total_start = ParserProfileClock::now();
    auto stage_start = total_start;
    boost::system::error_code error_code;
    boost::json::value json_value = boost::json::parse(json, error_code);
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
    boost::json::value constants((boost::json::object()));
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

        auto constant_asset = asset_entry.value;
        auto replace_result = substitute_references(constant_asset, constants, environment);
        if (!replace_result) {
            return std::unexpected(
                       "Failed to resolve asset '" + asset_entry.source_label + "': " + replace_result.error()
                   );
        }

        const auto &constant_object = constant_asset.as_object();
        const auto *data_value = find_child_value(constant_object, "data");
        if (data_value == nullptr || !data_value->is_object()) {
            return std::unexpected(
                       "Failed to parse constant asset '" + asset_entry.source_label +
                       "': Constant asset must contain an object field 'data'"
                   );
        }
        merge_json(constants, *data_value);
    }

    document.constants = constants;
    stage_end = ParserProfileClock::now();
    GUI_INTERFACE_PROFILE_LOGI(
        "GUI parser profile: base_dir(%1%), stage(resolve_constants), assets(%2%), elapsed_ms(%3%), total_ms(%4%)",
        base_dir,
        asset_entries.size(),
        parser_profile_elapsed_ms(stage_start, stage_end),
        parser_profile_elapsed_ms(total_start, stage_end)
    );

    stage_start = ParserProfileClock::now();
    std::vector<ResolvedAssetEntry> resolved_assets;
    for (const auto &asset_entry : asset_entries) {
        const auto &asset_object = asset_entry.value.as_object();
        auto asset_type = parse_string_field(asset_object, "type");
        if (!asset_type) {
            return std::unexpected(
                       "Failed to parse asset '" + asset_entry.source_label + "': " + asset_type.error()
                   );
        }
        if (*asset_type == "constant") {
            continue;
        }

        auto resolved_asset = asset_entry.value;
        auto replace_result = substitute_references(resolved_asset, constants, environment);
        if (!replace_result) {
            return std::unexpected(
                       "Failed to resolve asset '" + asset_entry.source_label + "': " + replace_result.error()
                   );
        }

        resolved_assets.push_back(ResolvedAssetEntry{
            .value = std::move(resolved_asset),
            .base_dir = asset_entry.base_dir,
            .source_label = asset_entry.source_label,
            .type = *asset_type,
        });
    }
    stage_end = ParserProfileClock::now();
    GUI_INTERFACE_PROFILE_LOGI(
        "GUI parser profile: base_dir(%1%), stage(resolve_assets), resolved_assets(%2%), elapsed_ms(%3%), "
        "total_ms(%4%)",
        base_dir,
        resolved_assets.size(),
        parser_profile_elapsed_ms(stage_start, stage_end),
        parser_profile_elapsed_ms(total_start, stage_end)
    );

    stage_start = ParserProfileClock::now();
    for (const auto &asset_entry : resolved_assets) {
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
            std::move(images->begin(), images->end(), std::back_inserter(document.images));
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
    for (const auto &asset_entry : resolved_assets) {
        const auto &resolved_asset_object = asset_entry.value.as_object();
        if (asset_entry.type != "styleSet") {
            continue;
        }
        const auto *styles_value = find_child_value(resolved_asset_object, "styles");
        if (styles_value == nullptr || !styles_value->is_object()) {
            return std::unexpected(
                       "Failed to parse styleSet asset '" + asset_entry.source_label +
                       "': styleSet must contain object field 'styles'"
                   );
        }
        auto styles = parse_named_style_map(styles_value->as_object(), constants, environment, false);
        if (!styles) {
            return std::unexpected(
                       "Failed to parse styleSet asset '" + asset_entry.source_label + "': " + styles.error()
                   );
        }
        for (auto &[key, style] : *styles) {
            document.styles.insert_or_assign(std::move(key), std::move(style));
        }
    }
    stage_end = ParserProfileClock::now();
    GUI_INTERFACE_PROFILE_LOGI(
        "GUI parser profile: base_dir(%1%), stage(parse_style_assets), styles(%2%), elapsed_ms(%3%), total_ms(%4%)",
        base_dir,
        document.styles.size(),
        parser_profile_elapsed_ms(stage_start, stage_end),
        parser_profile_elapsed_ms(total_start, stage_end)
    );

    stage_start = ParserProfileClock::now();
    InteractionTemplateRawMap raw_interactions;
    for (const auto &asset_entry : resolved_assets) {
        const auto &resolved_asset_object = asset_entry.value.as_object();
        if (asset_entry.type != "interactionTemplate") {
            continue;
        }
        auto id = parse_string_field(resolved_asset_object, "id");
        if (!id) {
            return std::unexpected(
                       "Failed to parse interactionTemplate asset '" + asset_entry.source_label + "': " + id.error()
                   );
        }
        auto interaction_object = resolved_asset_object;
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
    for (const auto &asset_entry : resolved_assets) {
        const auto &resolved_asset_object = asset_entry.value.as_object();
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
        auto [unused_it, inserted] = raw_templates.emplace(*id, node_object);
        if (!inserted) {
            return std::unexpected("Duplicate viewTemplate id: " + *id);
        }
    }

    for (const auto &asset_entry : resolved_assets) {
        const auto &resolved_asset_object = asset_entry.value.as_object();
        if (asset_entry.type == "imageSet" || asset_entry.type == "interactionTemplate" ||
                asset_entry.type == "styleSet") {
            continue;
        }
        if (asset_entry.type != "viewTemplate") {
            continue;
        }
        std::vector<std::string> template_stack;
        auto template_object = raw_templates.at(std::string(resolved_asset_object.at("id").as_string().c_str()));
        auto slot_result = apply_template_slots(template_object, nullptr);
        if (!slot_result) {
            return std::unexpected(
                       "Failed to parse viewTemplate asset '" + asset_entry.source_label + "': " + slot_result.error()
                   );
        }
        auto node = parse_view_node(template_object, environment, raw_templates, raw_interactions, template_stack);
        if (!node) {
            return std::unexpected(
                       "Failed to parse viewTemplate asset '" + asset_entry.source_label + "': " + node.error()
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
    for (const auto &asset_entry : resolved_assets) {
        const auto &resolved_asset_object = asset_entry.value.as_object();
        if (asset_entry.type == "imageSet" || asset_entry.type == "interactionTemplate" ||
                asset_entry.type == "styleSet") {
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
            continue;
        }
        if (asset_entry.type == "viewTemplate") {
            continue;
        }
        if (asset_entry.type != "viewScreen") {
            return std::unexpected(
                       "Unsupported asset type in '" + asset_entry.source_label + "': " + asset_entry.type
                   );
        }

        std::vector<std::string> template_stack;
        auto node = parse_view_node(
                        resolved_asset_object,
                        environment,
                        raw_templates,
                        raw_interactions,
                        template_stack,
                        NodeType::Screen
                    );
        if (!node) {
            return std::unexpected(
                       "Failed to parse viewScreen asset '" + asset_entry.source_label + "': " + node.error()
                   );
        }
        document.screens.push_back(std::move(*node));
    }
    stage_end = ParserProfileClock::now();
    GUI_INTERFACE_PROFILE_LOGI(
        "GUI parser profile: base_dir(%1%), stage(parse_flows_screens), flows(%2%), screens(%3%), "
        "elapsed_ms(%4%), total_ms(%5%)",
        base_dir,
        document.screen_flows.size(),
        document.screens.size(),
        parser_profile_elapsed_ms(stage_start, stage_end),
        parser_profile_elapsed_ms(total_start, stage_end)
    );

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
    auto parsed_document = parse_document_impl(json, base_dir, environment, {});
    if (!parsed_document) {
        return std::unexpected(parsed_document.error());
    }
    return parsed_document->document;
}

std::expected<std::vector<FontAsset>, std::string> parse_font_asset_set_json(
    std::string_view json,
    std::string_view base_dir)
{
    auto value = boost::json::parse(json);
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
    auto value = boost::json::parse(json);
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
    auto value = boost::json::parse(json);
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
    const Environment &environment)
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
    return parse_document_impl(*text, file_path.parent_path().string(), environment, std::move(dependency_files));
}

std::expected<Document, std::string> parse_document_file(std::string_view path, const Environment &environment)
{
    auto parsed_document = parse_document_file_with_metadata(path, environment);
    if (!parsed_document) {
        return std::unexpected(parsed_document.error());
    }
    return parsed_document->document;
}

} // namespace esp_brookesia::gui
