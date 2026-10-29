/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "brookesia/gui_interface.hpp"
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/lib_utils/test_adapter.hpp"
#include "../../src/private/sparse_document.hpp"

using namespace esp_brookesia::gui;

template<typename T>
struct IsOptional : std::false_type {};

template<typename T>
struct IsOptional<std::optional<T>> : std::true_type {};

template<typename T>
struct StyleMemberType;

template<typename T>
struct StyleMemberType<T Style::*> {
    using Type = T;
};

static_assert(std::is_aggregate_v<Style>);
static_assert(std::is_same_v<decltype(Style::bg_color), std::optional<std::string>>);
static_assert(std::is_same_v<decltype(Style::radius), std::optional<int32_t>>);
static_assert(std::is_same_v<decltype(Style::clip_corner), std::optional<bool>>);
static_assert(std::is_same_v<decltype(&Style::radius), std::optional<int32_t> Style::*>);
static_assert(std::is_same_v<decltype((std::declval<Style &>().radius)), std::optional<int32_t> &>);
static_assert(boost::mp11::mp_size<boost::describe::describe_members<Style, boost::describe::mod_public>>::value == 41);

BROOKESIA_TEST_CASE(test_sparse_style_public_api, "Style preserves public optional fields", "[gui][interface][style]")
{
    Style style{};
    style.bg_color = "";
    style.radius = 0;
    style.clip_corner = false;
    boost::mp11::mp_for_each<boost::describe::describe_members<Style, boost::describe::mod_public>>([](auto member) {
        using Member = typename StyleMemberType<std::remove_cv_t<decltype(member.pointer)>>::Type;
        static_assert(IsOptional<Member>::value);
    });
    TEST_ASSERT_TRUE(sizeof(SparseStyle) < sizeof(Style));
    auto member = &Style::radius;
    auto &radius = style.*member;
    TEST_ASSERT_TRUE(radius.has_value());
    TEST_ASSERT_EQUAL(0, radius.value());
    auto copy = style;
    auto moved = std::move(copy);
    TEST_ASSERT_TRUE(moved == style);
    moved.radius.reset();
    TEST_ASSERT_FALSE(moved.radius.has_value());
    TEST_ASSERT_TRUE(style.radius.has_value());
}

BROOKESIA_TEST_CASE(test_sparse_style_round_trip, "Sparse styles preserve explicit empty values", "[gui][interface][style]")
{
    Style style{};
    style.bg_color = "";
    style.radius = 0;
    style.clip_corner = false;
    auto sparse = to_sparse(style);
    TEST_ASSERT_EQUAL_size_t(3, sparse.size());
    auto restored = to_public(sparse);
    TEST_ASSERT_TRUE(restored == style);
    TEST_ASSERT_TRUE(restored.bg_color.has_value());
    TEST_ASSERT_TRUE(restored.bg_color->empty());
    TEST_ASSERT_TRUE(restored.radius.has_value());
    TEST_ASSERT_EQUAL(0, restored.radius.value());
    TEST_ASSERT_TRUE(restored.clip_corner.has_value());
    TEST_ASSERT_FALSE(restored.clip_corner.value());
    TEST_ASSERT_FALSE(restored.padding.has_value());
    auto copied = sparse;
    auto moved = std::move(copied);
    TEST_ASSERT_TRUE(to_public(moved) == style);
    moved.set_radius(std::nullopt);
    TEST_ASSERT_FALSE(to_public(moved).radius.has_value());
    TEST_ASSERT_TRUE(to_public(sparse).radius.has_value());
}

BROOKESIA_TEST_CASE(test_sparse_style_all_fields, "Sparse conversion preserves every public style field", "[gui][interface][style]")
{
    Style original;
    int32_t index = 0;
    boost::mp11::mp_for_each<boost::describe::describe_members<Style, boost::describe::mod_public>>([&](auto member) {
        auto &field = original.*member.pointer;
        using Value = typename std::remove_reference_t<decltype(field)>::value_type;
        if constexpr (std::is_same_v<Value, std::string>) {
            field = member.name;
        } else if constexpr (std::is_same_v<Value, bool>) {
            field = (index % 2 == 0);
        } else if constexpr (std::is_same_v<Value, TextAlign>) {
            field = TextAlign::Center;
        } else {
            field = index;
        }
        ++index;
    });
    auto sparse = to_sparse(original);
    TEST_ASSERT_EQUAL_size_t(41, sparse.size());
    TEST_ASSERT_TRUE(to_public(sparse) == original);
}

BROOKESIA_TEST_CASE(test_sparse_document_round_trip, "Sparse document preserves nested styles", "[gui][interface][style]")
{
    Document document;
    document.theme_sensitive = true;
    document.constants = boost::json::parse(R"({"k":0})");
    document.themes.emplace_back();
    document.themes.back().id = "theme";
    document.themes.back().styles["label"].state_styles["pressed"].radius = 0;
    document.styles["button"].part_styles["indicator"].state_styles["checked"].clip_corner = false;
    document.screens.emplace_back();
    auto &screen = document.screens.back();
    screen.id = "screen";
    screen.style.bg_color = "";
    screen.children.emplace_back();
    screen.children.back().id = "child";
    screen.children.back().state_styles["pressed"].opacity = 0;
    auto sparse = to_sparse(document);
    auto restored = to_public(sparse);
    TEST_ASSERT_TRUE(restored.theme_sensitive);
    TEST_ASSERT_TRUE(restored.constants == document.constants);
    TEST_ASSERT_EQUAL_STRING("theme", restored.themes.at(0).id.c_str());
    TEST_ASSERT_TRUE(restored.themes.at(0).styles.at("label").state_styles.at("pressed").radius.has_value());
    TEST_ASSERT_FALSE(restored.styles.at("button").part_styles.at("indicator").state_styles.at("checked")
                      .clip_corner.value());
    TEST_ASSERT_EQUAL_STRING("child", restored.screens.at(0).children.at(0).id.c_str());
    TEST_ASSERT_EQUAL(0, restored.screens.at(0).children.at(0).state_styles.at("pressed").opacity.value());
    auto snapshot = to_public_node_snapshot(sparse.screens.at(0));
    TEST_ASSERT_TRUE(snapshot.children.empty());
    TEST_ASSERT_TRUE(snapshot.style.bg_color.has_value());
}

BROOKESIA_TEST_CASE(test_sparse_style_interning, "Equal sparse resolved styles share storage", "[gui][interface][style]")
{
    std::vector<std::weak_ptr<const SparseResolvedStyle>> pool;
    ResolvedStyle first;
    first.style.radius = 0;
    first.state_styles["pressed"].clip_corner = false;
    ResolvedStyle same = first;
    ResolvedStyle different = first;
    different.style.radius = 1;
    bool reused = false;
    auto a = intern_sparse_resolved_style(pool, to_sparse(first), &reused);
    TEST_ASSERT_FALSE(reused);
    auto b = intern_sparse_resolved_style(pool, to_sparse(same), &reused);
    TEST_ASSERT_TRUE(reused);
    TEST_ASSERT_TRUE(a == b);
    auto c = intern_sparse_resolved_style(pool, to_sparse(different), &reused);
    TEST_ASSERT_FALSE(reused);
    TEST_ASSERT_TRUE(a != c);
    std::weak_ptr<const SparseResolvedStyle> weak = a;
    a.reset();
    b.reset();
    TEST_ASSERT_TRUE(weak.expired());
    auto again = intern_sparse_resolved_style(pool, to_sparse(first), &reused);
    TEST_ASSERT_FALSE(reused);
    TEST_ASSERT_TRUE(to_public(*again).style == first.style);
}

BROOKESIA_TEST_CASE(test_style_reflection, "Style reflection preserves public JSON fields", "[gui][interface][style]")
{
    Style original{};
    original.bg_color = "";
    original.radius = 0;
    original.clip_corner = false;
    auto json = BROOKESIA_DESCRIBE_TO_JSON(original);
    TEST_ASSERT_TRUE(json.is_object());
    const auto &object = json.as_object();
    TEST_ASSERT_EQUAL_size_t(41, object.size());
    TEST_ASSERT_TRUE(object.at("bg_color").as_string().empty());
    TEST_ASSERT_EQUAL(0, object.at("radius").as_int64());
    TEST_ASSERT_FALSE(object.at("clip_corner").as_bool());
    TEST_ASSERT_TRUE(object.at("padding").is_null());
    Style restored;
    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_FROM_JSON(json, restored));
    TEST_ASSERT_TRUE(restored == original);
}
