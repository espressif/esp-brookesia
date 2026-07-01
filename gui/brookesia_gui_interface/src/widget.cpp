/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/gui_interface/runtime.hpp"
#include "brookesia/gui_interface/macro_configs.h"
#if !BROOKESIA_GUI_INTERFACE_WIDGET_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"

#include <sstream>

namespace esp_brookesia::gui {

Path::Path(std::vector<std::string> segments)
    : segments_(std::move(segments))
{}

Path Path::from_string(std::string_view path)
{
    BROOKESIA_LOG_TRACE_GUARD();

    BROOKESIA_LOGD("Params: path(%1%)", path);

    std::vector<std::string> segments;
    size_t begin = 0;
    while (begin <= path.size()) {
        const size_t end = path.find('/', begin);
        const std::string_view segment = path.substr(
                                             begin, end == std::string_view::npos ? path.size() - begin : end - begin
                                         );
        if (!segment.empty()) {
            segments.emplace_back(segment);
        }
        if (end == std::string_view::npos) {
            break;
        }
        begin = end + 1;
    }
    return Path(std::move(segments));
}

bool Path::empty() const
{
    return segments_.empty();
}

const std::vector<std::string> &Path::segments() const
{
    return segments_;
}

View::View(Runtime *runtime, DocumentId document_id, std::string absolute_path, NodeType type)
    : runtime_(runtime)
    , document_id_(document_id)
    , absolute_path_(std::move(absolute_path))
    , type_(type)
{}

bool View::valid() const
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    return runtime_ != nullptr && runtime_->is_view_valid(*this);
}

std::string View::id() const
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    return runtime_ == nullptr ? std::string() : runtime_->get_view_id(*this);
}

std::string View::absolute_path() const
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    return runtime_ == nullptr ? std::string() : runtime_->get_view_absolute_path(*this);
}

std::optional<ViewStateValue> View::state(ViewStateKind kind) const
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: kind(%2%)", *this, kind);

    return runtime_ == nullptr ? std::nullopt : runtime_->get_view_state(*this, kind);
}

bool View::set_binding_value(std::string_view key, std::string value) const
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: key(%2%), value(%3%)", *this, key, value);

    if (runtime_ == nullptr) {
        return false;
    }
    runtime_->set_binding_value(document_id_, absolute_path_, key, std::move(value));
    return true;
}

std::optional<std::string> View::get_binding_value(std::string_view key) const
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: key(%2%)", *this, key);

    return runtime_ == nullptr ? std::nullopt : runtime_->get_binding_value(document_id_, absolute_path_, key);
}

bool View::set_hidden(bool hidden) const
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: hidden(%2%)", *this, hidden);

    return runtime_ != nullptr && runtime_->set_view_hidden(*this, hidden);
}

esp_brookesia::lib_utils::connection View::on_event(EventType type, EventHandler handler) const
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: type(%2%), handler(valid=%3%)", *this, type, static_cast<bool>(handler));

    return runtime_ == nullptr ? esp_brookesia::lib_utils::connection() :
           runtime_->connect_view_event(*this, type, std::move(handler));
}

View View::find_view(const Path &path) const
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: path(%2%)", *this, path);

    std::ostringstream oss;
    oss << path;
    const auto absolute_path = "/" + oss.str();
    return runtime_ == nullptr ? View() : runtime_->find_view(document_id_, absolute_path);
}

View View::find_view(std::string_view path) const
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: path(%2%)", *this, path);

    return runtime_ == nullptr ? View() : runtime_->find_view(document_id_, path);
}

Screen View::as_screen() const
{
    return type_ == NodeType::Screen ? Screen(runtime_, document_id_, absolute_path_, type_) : Screen();
}

Container View::as_container() const
{
    return type_ == NodeType::Container ? Container(runtime_, document_id_, absolute_path_, type_) : Container();
}

Label View::as_label() const
{
    return type_ == NodeType::Label ? Label(runtime_, document_id_, absolute_path_, type_) : Label();
}

Button View::as_button() const
{
    return type_ == NodeType::Button ? Button(runtime_, document_id_, absolute_path_, type_) : Button();
}

Image View::as_image() const
{
    return type_ == NodeType::Image ? Image(runtime_, document_id_, absolute_path_, type_) : Image();
}

FrameView View::as_frame_view() const
{
    return type_ == NodeType::FrameView ? FrameView(runtime_, document_id_, absolute_path_, type_) : FrameView();
}

TextInput View::as_text_input() const
{
    return type_ == NodeType::TextInput ? TextInput(runtime_, document_id_, absolute_path_, type_) : TextInput();
}

Slider View::as_slider() const
{
    return type_ == NodeType::Slider ? Slider(runtime_, document_id_, absolute_path_, type_) : Slider();
}

Switch View::as_switch() const
{
    return type_ == NodeType::Switch ? Switch(runtime_, document_id_, absolute_path_, type_) : Switch();
}

Checkbox View::as_checkbox() const
{
    return type_ == NodeType::Checkbox ? Checkbox(runtime_, document_id_, absolute_path_, type_) : Checkbox();
}

Dropdown View::as_dropdown() const
{
    return type_ == NodeType::Dropdown ? Dropdown(runtime_, document_id_, absolute_path_, type_) : Dropdown();
}

ProgressBar View::as_progress_bar() const
{
    return type_ == NodeType::ProgressBar ? ProgressBar(runtime_, document_id_, absolute_path_, type_) : ProgressBar();
}

Spinner View::as_spinner() const
{
    return type_ == NodeType::Spinner ? Spinner(runtime_, document_id_, absolute_path_, type_) : Spinner();
}

Arc View::as_arc() const
{
    return type_ == NodeType::Arc ? Arc(runtime_, document_id_, absolute_path_, type_) : Arc();
}

Line View::as_line() const
{
    return type_ == NodeType::Line ? Line(runtime_, document_id_, absolute_path_, type_) : Line();
}

Table View::as_table() const
{
    return type_ == NodeType::Table ? Table(runtime_, document_id_, absolute_path_, type_) : Table();
}

Keyboard View::as_keyboard() const
{
    return type_ == NodeType::Keyboard ? Keyboard(runtime_, document_id_, absolute_path_, type_) : Keyboard();
}

Canvas View::as_canvas() const
{
    return type_ == NodeType::Canvas ? Canvas(runtime_, document_id_, absolute_path_, type_) : Canvas();
}

bool Label::set_text(std::string_view text) const
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: text(%2%)", *this, text);

    return runtime_ != nullptr && runtime_->set_view_text(*this, text);
}

std::string Label::text() const
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    return runtime_ == nullptr ? std::string() : runtime_->get_view_text(*this);
}

esp_brookesia::lib_utils::connection Button::on_click(ClickHandler handler) const
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: handler(valid=%2%)", *this, static_cast<bool>(handler));

    return runtime_ == nullptr ? esp_brookesia::lib_utils::connection() : runtime_->connect_button_click(*this, std::move(handler));
}

bool Image::set_src(std::string_view src) const
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: src(%2%)", *this, src);

    return runtime_ != nullptr && runtime_->set_view_src(*this, src);
}

std::string Image::src() const
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    return runtime_ == nullptr ? std::string() : runtime_->get_view_src(*this);
}

bool TextInput::set_text(std::string_view text) const
{
    return runtime_ != nullptr && runtime_->set_view_text(*this, text);
}

std::string TextInput::text() const
{
    return runtime_ == nullptr ? std::string() : runtime_->get_view_text(*this);
}

bool Slider::set_value(int32_t value) const
{
    return runtime_ != nullptr && runtime_->set_view_value(*this, value);
}

int32_t Slider::value() const
{
    return runtime_ == nullptr ? 0 : runtime_->get_view_value(*this);
}

bool Switch::set_checked(bool checked) const
{
    return runtime_ != nullptr && runtime_->set_view_checked(*this, checked);
}

bool Switch::checked() const
{
    return runtime_ != nullptr && runtime_->get_view_checked(*this);
}

bool Checkbox::set_checked(bool checked) const
{
    return runtime_ != nullptr && runtime_->set_view_checked(*this, checked);
}

bool Checkbox::checked() const
{
    return runtime_ != nullptr && runtime_->get_view_checked(*this);
}

bool Dropdown::set_selected_index(int32_t index) const
{
    return runtime_ != nullptr && runtime_->set_view_selected_index(*this, index);
}

int32_t Dropdown::selected_index() const
{
    return runtime_ == nullptr ? 0 : runtime_->get_view_selected_index(*this);
}

bool ProgressBar::set_value(int32_t value) const
{
    return runtime_ != nullptr && runtime_->set_view_value(*this, value);
}

int32_t ProgressBar::value() const
{
    return runtime_ == nullptr ? 0 : runtime_->get_view_value(*this);
}

bool Arc::set_value(int32_t value) const
{
    return runtime_ != nullptr && runtime_->set_view_value(*this, value);
}

int32_t Arc::value() const
{
    return runtime_ == nullptr ? 0 : runtime_->get_view_value(*this);
}

bool Table::set_cell_text(int32_t row, int32_t column, std::string_view text) const
{
    return runtime_ != nullptr && runtime_->set_table_cell_text(*this, row, column, text);
}

} // namespace esp_brookesia::gui
