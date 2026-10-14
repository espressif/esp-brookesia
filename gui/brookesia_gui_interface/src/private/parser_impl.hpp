#pragma once

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

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
#include <vector>

#include "boost/json.hpp"
#include "boost/unordered/unordered_flat_map.hpp"
#include "boost/unordered/unordered_flat_set.hpp"
#include "brookesia/service_helper/system/storage.hpp"

#if BROOKESIA_GUI_INTERFACE_ENABLE_PROFILE_LOG
#   define GUI_INTERFACE_PROFILE_LOGI(...) BROOKESIA_LOGI(__VA_ARGS__)
#else
#   define GUI_INTERFACE_PROFILE_LOGI(...) do { if (false) { BROOKESIA_LOGI(__VA_ARGS__); } } while (0)
#endif

namespace esp_brookesia::gui {

namespace parser_detail {

using StorageHelper = service::helper::Storage;
using ParserProfileClock = std::chrono::steady_clock;

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

std::expected<std::string, std::string> read_text_file(const std::filesystem::path &path)
;
std::expected<std::vector<std::string>, std::string> read_text_files(const std::vector<std::filesystem::path> &paths)
;

std::expected<boost::json::value, std::string> parse_json_text(
    const std::filesystem::path &path,
    const std::string &text)
;

std::filesystem::path resolve_path(
    const std::filesystem::path &base_dir, const std::string &path_string)
;

std::string normalize_object_key(std::string_view key)
;

std::optional<std::string> make_camel_case_key(std::string_view key)
;

bool is_valid_color_literal(std::string_view color)
;

boost::json::object::const_iterator find_key(const boost::json::object &object, std::string_view key)
;

boost::json::object::iterator find_key(boost::json::object &object, std::string_view key)
;

const boost::json::value *find_child_value(const boost::json::object &object, std::string_view key)
;

std::expected<void, std::string> append_root_asset_entries(
    const boost::json::object &object,
    std::string_view key,
    const std::filesystem::path &base_dir,
    boost::unordered_flat_set<std::string> &loaded_paths,
    std::vector<std::string> *dependency_files,
    std::vector<RootAssetEntry> &ordered_entries,
    std::string_view inline_entry_label)
;

std::expected<std::vector<std::string>, std::string> parse_string_array(
    const boost::json::object &object, std::string_view key)
;

void merge_json(boost::json::value &destination, const boost::json::value &source)
;

void merge_json(boost::json::value &destination, boost::json::value &&source)
;

bool is_reference_string(std::string_view text);

std::string join_json_path(std::string_view prefix, std::string_view key)
;

std::expected<void, std::string> flatten_color_constants(
    const boost::json::value &value,
    std::string_view prefix,
    std::map<std::string, std::string> &colors)
;

const boost::json::value *resolve_constant(
    const boost::json::value &constants, std::string_view path)
;

bool is_reference_string(std::string_view text)
;

bool is_expression_string(std::string_view text)
;

std::string trim_expression_text(std::string_view text)
;

std::string format_expression_number(double value)
;

boost::json::value environment_reference_value(const Environment &environment, std::string_view path)
;

std::expected<ExpressionValue, std::string> parse_expression_numeric_text(std::string_view text)
;

std::expected<ExpressionScalar, std::string> expression_value_from_json(
    const boost::json::value &value,
    std::string_view reference)
;

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

    bool is_comparison(ExpressionTokenType type)
    {
        return type == ExpressionTokenType::Equal || type == ExpressionTokenType::NotEqual ||
               type == ExpressionTokenType::Greater || type == ExpressionTokenType::Less ||
               type == ExpressionTokenType::GreaterEqual || type == ExpressionTokenType::LessEqual;
    }

    std::optional<ExpressionValue> scalar_to_number(const ExpressionScalar &scalar)
    {
        if (!std::holds_alternative<ExpressionValue>(scalar)) {
            return std::nullopt;
        }
        return std::get<ExpressionValue>(scalar);
    }

    std::optional<bool> scalar_to_bool(const ExpressionScalar &scalar)
    {
        if (!std::holds_alternative<bool>(scalar)) {
            return std::nullopt;
        }
        return std::get<bool>(scalar);
    }

    std::expected<bool, std::string> compare_scalars(
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

    std::expected<bool, std::string> compare_numbers(
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
    std::expected<bool, std::string> compare_values(const T &left, const T &right, ExpressionTokenType op)
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

    std::expected<ExpressionValue, std::string> apply_additive(
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

    std::expected<ExpressionValue, std::string> apply_multiplicative(
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

boost::json::value expression_value_to_json(const ExpressionScalar &scalar)
;

std::expected<boost::json::value, std::string> evaluate_expression_string(
    std::string_view text,
    const boost::json::value &constants,
    const Environment &environment)
;

std::expected<Reference, std::string> parse_reference_string(std::string_view text)
;

bool is_style_font_path(const std::vector<std::string> &path)
;

bool is_theme_style_font_path(const std::vector<std::string> &path)
;

bool is_image_props_src_path(const std::vector<std::string> &path)
;

bool is_keyboard_key_image_path(const std::vector<std::string> &path)
;

bool is_placement_relative_to_path(const std::vector<std::string> &path)
;

bool is_font_fallback_path(const std::vector<std::string> &path)
;

bool is_style_color_path(const std::vector<std::string> &path)
;

std::expected<void, std::string> substitute_references(
    boost::json::value &value,
    const boost::json::value &constants,
    const Environment &environment,
    std::vector<std::string> path = {})
;

std::expected<std::string, std::string> parse_resource_reference_field(
    const boost::json::object &object,
    std::string_view key,
    std::string_view namespace_name,
    std::string_view migration_hint)
;

std::expected<std::string, std::string> parse_view_reference_field(
    const boost::json::object &object,
    std::string_view key)
;

std::expected<std::vector<std::string>, std::string> parse_resource_reference_array_field(
    const boost::json::object &object,
    std::string_view key,
    std::string_view namespace_name,
    std::string_view migration_hint)
;

std::expected<std::string, std::string> parse_string_field(
    const boost::json::object &object, std::string_view key, std::string default_value
);
std::expected<int32_t, std::string> parse_scaled_value(
    const boost::json::value *value, std::string_view field_name, std::string_view unit, float scale
);
std::expected<int32_t, std::string> parse_int_field(
    const boost::json::object &object, std::string_view key, int32_t default_value
);

std::expected<Style, std::string> parse_style_object(
    const boost::json::object &style_object,
    const Environment &environment,
    bool allow_font = true,
    bool allow_font_size = true)
;

std::expected<StateStyleMap, std::string> parse_state_styles_object(
    const boost::json::object &state_styles_object,
    const Environment &environment)
;

std::expected<PartStyleSet, std::string> parse_part_style_set_object(
    const boost::json::object &part_style_object,
    const Environment &environment)
;

std::expected<PartStyleMap, std::string> parse_part_styles_object(
    const boost::json::object &part_styles_object,
    const Environment &environment)
;

std::expected<StyleSet, std::string> parse_style_set_object(
    const boost::json::object &style_object,
    const Environment &environment)
;

const boost::unordered_flat_set<std::string> &get_theme_subtype_style_keys()
;

std::expected<std::map<std::string, StyleSet>, std::string> parse_named_style_map(
    const boost::json::object &styles_object,
    const boost::json::value &constants,
    const Environment &environment,
    bool allow_subtype_keys)
;

std::expected<int32_t, std::string> parse_scaled_value(
    const boost::json::value *value, std::string_view field_name, std::string_view unit, float scale)
;

std::expected<PlacementOffset, std::string> parse_placement_offset(
    const boost::json::value *value, std::string_view field_name, const Environment &environment)
;

std::expected<Dimension, std::string> parse_dimension(
    const boost::json::value *value, std::string_view field_name, const Environment &environment)
;

std::expected<float, std::string> parse_positive_float_text(
    std::string_view text, std::string_view field_name)
;

std::expected<float, std::string> parse_aspect_ratio(
    const boost::json::value *value, std::string_view field_name)
;

template <typename T>
std::expected<T, std::string> parse_enum_field(
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

std::expected<std::string, std::string> parse_string_field(
    const boost::json::object &object, std::string_view key, std::string default_value = {})
;

std::expected<bool, std::string> parse_bool_field(
    const boost::json::object &object, std::string_view key, bool default_value = false)
;

std::expected<int32_t, std::string> parse_int_field(
    const boost::json::object &object, std::string_view key, int32_t default_value = 0)
;

bool is_supported_keyboard_mode(std::string_view mode)
;

std::expected<KeyboardKey, std::string> parse_keyboard_key(const boost::json::value &value)
;

std::expected<KeyboardLayout, std::string> parse_keyboard_layout(const boost::json::value &value)
;

std::expected<std::map<std::string, KeyboardLayout>, std::string> parse_keyboard_layouts_field(
    const boost::json::object &object)
;

std::expected<std::vector<std::string>, std::string> parse_keyboard_allowed_modes_field(
    const boost::json::object &object)
;

std::expected<KeyboardKeyStyle, std::string> parse_keyboard_key_style(
    const boost::json::value &value, std::string_view class_name, const Environment &environment)
;

bool is_supported_keyboard_key_style_class(std::string_view class_name)
;

std::expected<std::map<std::string, KeyboardKeyStyle>, std::string> parse_keyboard_key_styles_field(
    const boost::json::object &object, const Environment &environment)
;

std::expected<std::map<std::string, std::string>, std::string> parse_keyboard_key_style_refs_field(
    const boost::json::object &object)
;

std::expected<PivotValue, std::string> parse_pivot_value(
    const boost::json::value *value,
    std::string_view field_name,
    PivotValue default_value = {})
;

std::expected<PivotValue, std::string> parse_pivot_field(
    const boost::json::object &object,
    std::string_view key,
    PivotValue default_value = {})
;

std::expected<std::vector<Dimension>, std::string> parse_dimension_array(
    const boost::json::object &object,
    std::string_view key,
    const Environment &environment)
;

std::expected<void, std::string> reject_fields(
    const boost::json::object &object, std::initializer_list<std::string_view> fields, std::string_view target)
;

std::expected<std::vector<Point>, std::string> parse_points_field(const boost::json::object &object)
;

std::expected<std::vector<TableCell>, std::string> parse_table_cells_field(const boost::json::object &object)
;

std::expected<std::vector<CanvasCommand>, std::string> parse_canvas_commands_field(
    const boost::json::object &object)
;

std::expected<Animation, std::string> parse_animation(const boost::json::object &object);

std::expected<std::string, std::string> parse_event_effect_value(
    const boost::json::object &object,
    std::string_view key)
;

std::expected<EventPropertyUpdate, std::string> parse_event_property_update(
    const boost::json::object &object,
    std::string_view default_target = "self")
;

std::expected<EventEffect, std::string> parse_event_effect(const boost::json::object &object)
;

std::expected<EventBinding, std::string> parse_event_binding(const boost::json::object &object)
;

std::expected<Animation, std::string> parse_animation(const boost::json::object &object)
;

std::expected<Node, std::string> parse_view_node(
    const boost::json::object &object,
    const Environment &environment,
    const TemplateRawMap &templates,
    const InteractionTemplateRawMap &interactions,
    std::vector<std::string> &template_stack,
    std::optional<NodeType> forced_type = std::nullopt);
std::expected<FontAsset, std::string> parse_font_asset(
    const boost::json::object &object, const std::filesystem::path &asset_dir);

std::vector<std::string> split_template_path(std::string_view path)
;

boost::json::object *find_template_override_target(
    boost::json::object &root,
    std::string_view relative_path)
;

std::expected<void, std::string> apply_template_overrides(
    boost::json::object &root,
    const boost::json::object &overrides)
;

std::expected<void, std::string> expand_template_slots_in_children(
    boost::json::array &children,
    const boost::json::object *slots,
    boost::unordered_flat_set<std::string> &declared_slots)
;

std::expected<void, std::string> apply_template_slots(
    boost::json::object &root,
    const boost::json::object *slots)
;

std::expected<Node, std::string> parse_template_ref(
    const boost::json::object &object,
    const Environment &environment,
    const TemplateRawMap &templates,
    const InteractionTemplateRawMap &interactions,
    std::vector<std::string> &template_stack)
;

std::expected<std::vector<Node>, std::string> parse_children(
    const boost::json::object &object,
    const Environment &environment,
    const TemplateRawMap &templates,
    const InteractionTemplateRawMap &interactions,
    std::vector<std::string> &template_stack)
;

std::expected<NodeType, std::string> parse_node_type(std::string_view type)
;

bool default_clickable_for_node_type(NodeType type)
;

bool default_scrollable_for_node_type(NodeType type)
;

CommonProps get_builtin_default_common_props(NodeType type)
;

Layout get_builtin_default_layout(NodeType type)
;

Placement get_builtin_default_placement(NodeType type)
;

void merge_object_defaults(boost::json::object &target, const boost::json::object &defaults)
;

std::expected<void, std::string> prepend_array_defaults(
    boost::json::object &target,
    const boost::json::object &defaults,
    std::string_view field)
;

std::expected<void, std::string> apply_interaction_template_object(
    boost::json::object &target,
    const boost::json::object &interaction,
    std::string_view id)
;

std::expected<boost::json::object, std::string> apply_interaction_templates(
    const boost::json::object &source,
    const InteractionTemplateRawMap &interactions)
;

std::expected<Node, std::string> parse_view_node(
    const boost::json::object &source_object,
    const Environment &environment,
    const TemplateRawMap &templates,
    const InteractionTemplateRawMap &interactions,
    std::vector<std::string> &template_stack,
    std::optional<NodeType> forced_type)
;

std::expected<uint32_t, std::string> parse_image_font_codepoint(std::string_view text)
;

std::expected<std::vector<ImageFontGlyph>, std::string> parse_image_font_glyphs(
    const boost::json::object &object,
    const std::filesystem::path &asset_dir,
    std::string_view owner)
;

std::vector<uint32_t> sorted_image_font_codepoints(const std::vector<ImageFontGlyph> &glyphs)
;

std::expected<FontAsset, std::string> parse_font_asset(
    const boost::json::object &object, const std::filesystem::path &asset_dir)
;

std::expected<ImageAsset, std::string> parse_image_asset(
    const boost::json::object &object, const std::filesystem::path &asset_dir)
;

std::expected<std::vector<FontAsset>, std::string> parse_font_asset_set(
    const boost::json::object &object,
    const std::filesystem::path &asset_dir)
;

std::expected<std::vector<ImageAsset>, std::string> parse_image_asset_set(
    const boost::json::object &object,
    const std::filesystem::path &asset_dir)
;

std::expected<void, std::string> merge_theme_constant_asset(
    const RootAssetEntry &asset_entry,
    boost::json::value &constants)
;

std::expected<void, std::string> append_and_merge_theme_constant_assets(
    const boost::json::object &object,
    std::string_view key,
    const std::filesystem::path &base_dir,
    boost::unordered_flat_set<std::string> &loaded_paths,
    boost::json::value &constants,
    std::string_view inline_entry_label)
;

std::expected<ThemeAsset, std::string> parse_theme_asset(
    const boost::json::object &object,
    const std::filesystem::path &base_dir,
    const Environment &environment)
;

std::expected<std::vector<std::string>, std::string> parse_screen_flow_from_array(
    const boost::json::object &object)
;

std::expected<ScreenFlow, std::string> parse_screen_flow(const boost::json::object &object)
;


} // namespace parser_detail

} // namespace esp_brookesia::gui

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
