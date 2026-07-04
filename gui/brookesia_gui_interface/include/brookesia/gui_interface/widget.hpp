/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include "brookesia/lib_utils/signal.hpp"
#include "brookesia/gui_interface/enums.hpp"
#include "brookesia/gui_interface/event.hpp"
#include "brookesia/gui_interface/handles.hpp"
#include "brookesia/gui_interface/macro_configs.h"
#include "brookesia/lib_utils/describe_helpers.hpp"

namespace esp_brookesia::gui {

class Runtime;
class Screen;
class Container;
class Label;
class Button;
class Image;
class FrameView;
class TextInput;
class Slider;
class Switch;
class Checkbox;
class Dropdown;
class ProgressBar;
class Spinner;
class Arc;
class Line;
class Table;
class Keyboard;
class Canvas;

class Path {
public:
    Path() = default;
    explicit Path(std::vector<std::string> segments);

    static Path from_string(std::string_view path);

    bool empty() const;
    const std::vector<std::string> &segments() const;

    friend std::ostream &operator<<(std::ostream &os, const Path &path)
    {
        bool first = true;
        for (const auto &segment : path.segments_) {
            if (!first) {
                os << '/';
            }
            os << segment;
            first = false;
        }
        return os;
    }

private:
    std::vector<std::string> segments_;
};

class View {
public:
    using EventHandler = std::function<void(const Event &)>;

    View() = default;

    bool valid() const;
    std::string id() const;
    std::string absolute_path() const;
    std::optional<ViewStateValue> state(ViewStateKind kind) const;
    bool set_binding_value(std::string_view key, std::string value) const;
    std::optional<std::string> get_binding_value(std::string_view key) const;
    bool set_hidden(bool hidden) const;
    esp_brookesia::lib_utils::connection on_event(EventType type, EventHandler handler) const;

    View find_view(const Path &path) const;
    View find_view(std::string_view path) const;

    Screen as_screen() const;
    Container as_container() const;
    Label as_label() const;
    Button as_button() const;
    Image as_image() const;
    FrameView as_frame_view() const;
    TextInput as_text_input() const;
    Slider as_slider() const;
    Switch as_switch() const;
    Checkbox as_checkbox() const;
    Dropdown as_dropdown() const;
    ProgressBar as_progress_bar() const;
    Spinner as_spinner() const;
    Arc as_arc() const;
    Line as_line() const;
    Table as_table() const;
    Keyboard as_keyboard() const;
    Canvas as_canvas() const;

    friend std::ostream &operator<<(std::ostream &os, const View &view)
    {
        os << "{runtime=@0x" << std::hex << reinterpret_cast<std::uintptr_t>(view.runtime_) << std::dec
           << ", document_id=" << view.document_id_
           << ", absolute_path='" << view.absolute_path_ << "'"
           << ", type=" << BROOKESIA_DESCRIBE_ENUM_TO_STR(view.type_)
           << "}";
        return os;
    }

protected:
    friend class Runtime;
    friend class Screen;
    friend class Container;
    friend class Label;
    friend class Button;
    friend class Image;
    friend class FrameView;
    friend class TextInput;
    friend class Slider;
    friend class Switch;
    friend class Checkbox;
    friend class Dropdown;
    friend class ProgressBar;
    friend class Spinner;
    friend class Arc;
    friend class Line;
    friend class Table;
    friend class Keyboard;
    friend class Canvas;

    View(Runtime *runtime, DocumentId document_id, std::string absolute_path, NodeType type);

    Runtime *runtime_ = nullptr;
    DocumentId document_id_;
    std::string absolute_path_;
    NodeType type_ = NodeType::Max;
};

class Screen : public View {
public:
    using View::View;
};

class Container : public View {
public:
    using View::View;
};

class Label : public View {
public:
    using View::View;

    bool set_text(std::string_view text) const;
    std::string text() const;
};

class Button : public View {
public:
    using ClickHandler = std::function<void()>;

    using View::View;

    esp_brookesia::lib_utils::connection on_click(ClickHandler handler) const;
};

class Image : public View {
public:
    using View::View;

    bool set_src(std::string_view src) const;
    std::string src() const;
};

class FrameView : public View {
public:
    using View::View;
};

class TextInput : public View {
public:
    using View::View;

    bool set_text(std::string_view text) const;
    std::string text() const;
};

class Slider : public View {
public:
    using View::View;

    bool set_value(int32_t value) const;
    int32_t value() const;
};

class Switch : public View {
public:
    using View::View;

    bool set_checked(bool checked) const;
    bool checked() const;
};

class Checkbox : public View {
public:
    using View::View;

    bool set_checked(bool checked) const;
    bool checked() const;
};

class Dropdown : public View {
public:
    using View::View;

    bool set_selected_index(int32_t index) const;
    int32_t selected_index() const;
};

class ProgressBar : public View {
public:
    using View::View;

    bool set_value(int32_t value) const;
    int32_t value() const;
};

class Arc : public View {
public:
    using View::View;

    bool set_value(int32_t value) const;
    int32_t value() const;
};

class Table : public View {
public:
    using View::View;

    bool set_cell_text(int32_t row, int32_t column, std::string_view text) const;
};

class Spinner : public View {
public:
    using View::View;
};

class Line : public View {
public:
    using View::View;
};

class Keyboard : public View {
public:
    using View::View;
};

class Canvas : public View {
public:
    using View::View;
};

} // namespace esp_brookesia::gui
