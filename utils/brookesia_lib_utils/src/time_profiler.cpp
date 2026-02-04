/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <thread>
#include <boost/format.hpp>
#include "brookesia/lib_utils/macro_configs.h"
#if !BROOKESIA_UTILS_TIME_PROFILER_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/lib_utils/log.hpp"
#include "brookesia/lib_utils/time_profiler.hpp"

namespace esp_brookesia::lib_utils {

void TimeProfiler::set_format_options(const FormatOptions &options)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: options(%1%)", BROOKESIA_DESCRIBE_TO_STR(options));

    std::lock_guard<std::mutex> lock(mutex_);
    format_ = options;
}

void TimeProfiler::enter_scope(const std::string &name)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: name(%1%)", name);

    auto &local_stack = thread_local_stack();
    Node *parent = local_stack.empty() ? &root_ : local_stack.top();

    Node *child = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto &child_ptr = parent->children[name];
        if (!child_ptr) {
            child_ptr = std::make_unique<Node>(name, parent);
        }
        child = child_ptr.get();
    }

    local_stack.push(child);
    start_times()[child] = Clock::now();
}

void TimeProfiler::leave_scope()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto &local_stack = thread_local_stack();
    if (local_stack.empty()) {
        return;
    }
    Node *node = local_stack.top();
    local_stack.pop();

    auto end = Clock::now();
    auto start_it = start_times().find(node);
    if (start_it != start_times().end()) {
        double duration = to_unit(end - start_it->second);
        start_times().erase(start_it);
        std::lock_guard<std::mutex> lock(mutex_);
        node->total += duration;
        if (duration < node->min) {
            node->min = duration;
        }
        if (duration > node->max) {
            node->max = duration;
        }
        node->count++;
    }
}

void TimeProfiler::start_event(const std::string &name)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: name(%1%)", name);

    std::lock_guard<std::mutex> lock(mutex_);
    event_stacks_[name].push_back(Clock::now());
}

void TimeProfiler::end_event(const std::string &name)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: name(%1%)", name);

    TimePoint end = Clock::now();
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = event_stacks_.find(name);
    if (it == event_stacks_.end() || it->second.empty()) {
        return;
    }
    TimePoint start = it->second.front();
    it->second.pop_front();
    if (it->second.empty()) {
        event_stacks_.erase(it);
    }
    double duration = to_unit(end - start);

    Node *node = find_or_create_node(name);
    node->total += duration;
    if (duration < node->min) {
        node->min = duration;
    }
    if (duration > node->max) {
        node->max = duration;
    }
    node->count++;
}

TimeProfiler::Statistics TimeProfiler::get_statistics() const
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    std::lock_guard<std::mutex> lock(mutex_);

    Statistics stats;
    stats.unit_name = unit_name();
    stats.overall_total = sum_children_total(&root_);

    auto children = sorted_children(const_cast<Node *>(&root_));
    stats.root_children.reserve(children.size());
    for (const Node *child : children) {
        stats.root_children.push_back(build_node_statistics(child, stats.overall_total, stats.overall_total));
    }

    return stats;
}

void TimeProfiler::report()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    const Statistics stats = get_statistics();

    std::ostringstream oss;
    oss << "\n=== Performance Tree Report ===\n";
    oss << "(Unit: " << stats.unit_name << ")\n";

    print_header(oss);

    for (size_t i = 0; i < stats.root_children.size(); ++i) {
        const bool is_last = (i + 1 == stats.root_children.size());
        print_node_from_statistics(oss, stats.root_children[i], "", is_last);
    }
    oss << "===============================\n";

    // Output everything in one call
    BROOKESIA_LOGI("%1%", oss.str());
}

void TimeProfiler::clear()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    std::lock_guard<std::mutex> lock(mutex_);
    root_.children.clear();
    event_stacks_.clear();
}

std::stack<TimeProfiler::Node *> &TimeProfiler::thread_local_stack()
{
    static thread_local std::stack<Node *> stack;
    return stack;
}

std::map<TimeProfiler::Node *, TimeProfiler::TimePoint> &TimeProfiler::start_times()
{
    static thread_local std::map<Node *, TimePoint> times;
    return times;
}

TimeProfiler::Node *TimeProfiler::find_or_create_node(const std::string &name)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: name(%1%)", name);

    auto &child = root_.children[name];
    if (!child) {
        child = std::make_unique<Node>(name, &root_);
    }
    return child.get();
}

double TimeProfiler::to_unit(std::chrono::duration<double> d) const
{
    switch (format_.time_unit) {
    case FormatOptions::TimeUnit::Microseconds: return d.count() * 1000000.0;
    case FormatOptions::TimeUnit::Milliseconds: return d.count() * 1000.0;
    case FormatOptions::TimeUnit::Seconds: return d.count();
    }
    return d.count();
}

std::string TimeProfiler::unit_name() const
{
    switch (format_.time_unit) {
    case FormatOptions::TimeUnit::Microseconds: return "us";
    case FormatOptions::TimeUnit::Milliseconds: return "ms";
    case FormatOptions::TimeUnit::Seconds: return "s";
    }
    return "unknown";
}

void TimeProfiler::print_node(std::ostringstream &oss,
                              Node *node,
                              const std::string &prefix,
                              bool is_last,
                              double parent_total,
                              double overall_total) const
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    const double avg = node->count > 0 ? node->total / static_cast<double>(node->count) : 0.0;
    const double minv = node->count > 0 ? node->min : 0.0;
    const double maxv = node->count > 0 ? node->max : 0.0;
    const double children_sum = sum_children_total(node);
    const double self_time = node->total - children_sum;
    const double pct_parent = parent_total > 0.0 ? (node->total * 100.0 / parent_total) : 0.0;
    const double pct_total = overall_total > 0.0 ? (node->total * 100.0 / overall_total) : 0.0;

    // Use more generic ASCII characters to ensure consistent width
    const char *branch_mid = "|- ";
    const char *branch_end = "`- ";
    const char *pad_mid = "|  ";
    const char *pad_end = "   ";
    const std::string connector = std::string(is_last ? branch_end : branch_mid);
    const std::string next_prefix = prefix + (is_last ? pad_end : pad_mid);

    const std::string display_prefix = prefix + connector;

    // Color codes
    const char *color_reset = "\033[0m";
    const char *color_start = "";
    if (format_.use_color) {
        if (pct_total >= 50.0) {
            color_start = "\033[31m";    // red
        } else if (pct_total >= 20.0) {
            color_start = "\033[33m";    // yellow
        } else if (pct_total >= 5.0) {
            color_start = "\033[36m";    // cyan
        }
    }
    const bool colorize = format_.use_color && *color_start != '\0';

    // Calculate actual display width (excluding color codes)
    int occupy = static_cast<int>(display_prefix.size() + node->name.size());
    int pad = format_.name_width - occupy;
    if (pad < 1) {
        pad = 1;
    }

    // Build colored name (color codes do not occupy display width)
    const std::string name_colored = colorize ? (std::string(color_start) + node->name + color_reset) : node->name;

    // Use boost::format to build formatted string
    std::string fmt_str = "%s%s%s | %";
    fmt_str += std::to_string(format_.calls_width) + "d";
    fmt_str += " | %";
    fmt_str += std::to_string(format_.num_width) + "." + std::to_string(format_.precision) + "f";
    fmt_str += " | %";
    fmt_str += std::to_string(format_.num_width) + "." + std::to_string(format_.precision) + "f";
    fmt_str += " | %";
    fmt_str += std::to_string(format_.num_width) + "." + std::to_string(format_.precision) + "f";
    fmt_str += " | %";
    fmt_str += std::to_string(format_.num_width) + "." + std::to_string(format_.precision) + "f";
    fmt_str += " | %";
    fmt_str += std::to_string(format_.num_width) + "." + std::to_string(format_.precision) + "f";

    boost::format fmt(fmt_str);
    fmt % display_prefix % name_colored % std::string(pad, ' ') % node->count % node->total % self_time % avg % minv % maxv;

    oss << fmt.str();

    if (format_.show_percentages) {
        // Build percentage format string
        std::string pct_fmt_str = " | %";
        pct_fmt_str += std::to_string(format_.percent_width) + ".2f%%";
        pct_fmt_str += " | %";
        pct_fmt_str += std::to_string(format_.percent_width) + ".2f%%";

        boost::format pct(pct_fmt_str);
        pct % pct_parent % pct_total;
        oss << pct.str();
    }
    oss << "\n";

    auto children = sorted_children(node);
    for (size_t i = 0; i < children.size(); ++i) {
        Node *child = children[i];
        const bool child_is_last = (i + 1 == children.size());
        print_node(oss, child, next_prefix, child_is_last, /*parent_total=*/node->total, overall_total);
    }
}

void TimeProfiler::print_header(std::ostringstream &oss) const
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    // Use boost::format to build header
    std::string header_fmt = "%-";
    header_fmt += std::to_string(format_.name_width) + "s";
    header_fmt += " | %";
    header_fmt += std::to_string(format_.calls_width) + "s";
    header_fmt += " | %";
    header_fmt += std::to_string(format_.num_width) + "s";
    header_fmt += " | %";
    header_fmt += std::to_string(format_.num_width) + "s";
    header_fmt += " | %";
    header_fmt += std::to_string(format_.num_width) + "s";
    header_fmt += " | %";
    header_fmt += std::to_string(format_.num_width) + "s";
    header_fmt += " | %";
    header_fmt += std::to_string(format_.num_width) + "s";

    boost::format fmt(header_fmt);
    fmt % "Name" % "calls" % "total" % "self" % "avg" % "min" % "max";

    std::string header_line = fmt.str();

    if (format_.show_percentages) {
        std::string pct_fmt = " | %";
        pct_fmt += std::to_string(format_.percent_width) + "s";
        pct_fmt += " | %";
        pct_fmt += std::to_string(format_.percent_width) + "s";

        boost::format pct(pct_fmt);
        pct % "%parent" % "%total";
        header_line += pct.str();
    }

    oss << header_line << "\n";
    oss << std::string(header_line.size(), '-') << "\n";
}

std::string TimeProfiler::format_percent(double pct)
{
    boost::format fmt("%.2f%%");
    fmt % pct;
    return fmt.str();
}

double TimeProfiler::sum_children_total(const Node *node) const
{
    double sum = 0.0;
    for (const auto &kv : node->children) {
        if (kv.second) {
            sum += kv.second->total;
        }
    }
    return sum;
}

std::vector<TimeProfiler::Node *> TimeProfiler::sorted_children(Node *node) const
{
    std::vector<Node *> result;
    result.reserve(node->children.size());
    for (auto &kv : node->children) {
        if (kv.second) {
            result.push_back(kv.second.get());
        }
    }
    switch (format_.sort_by) {
    case FormatOptions::SortBy::TotalDesc:
        std::sort(result.begin(), result.end(), [](const Node * a, const Node * b) {
            return a->total > b->total;
        });
        break;
    case FormatOptions::SortBy::NameAsc:
        std::sort(result.begin(), result.end(), [](const Node * a, const Node * b) {
            return a->name < b->name;
        });
        break;
    case FormatOptions::SortBy::None:
        break;
    }
    return result;
}

TimeProfiler::NodeStatistics TimeProfiler::build_node_statistics(const Node *node, double parent_total, double overall_total) const
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    NodeStatistics stats(node->name);
    stats.count = node->count;
    stats.total = node->total;

    const double children_sum = sum_children_total(node);
    stats.self_time = node->total - children_sum;
    stats.avg = node->count > 0 ? node->total / static_cast<double>(node->count) : 0.0;
    stats.min = node->count > 0 ? node->min : 0.0;
    stats.max = node->count > 0 ? node->max : 0.0;
    stats.pct_parent = parent_total > 0.0 ? (node->total * 100.0 / parent_total) : 0.0;
    stats.pct_total = overall_total > 0.0 ? (node->total * 100.0 / overall_total) : 0.0;

    // Build children statistics
    auto children = sorted_children(const_cast<Node *>(node));
    stats.children.reserve(children.size());
    for (const Node *child : children) {
        stats.children.push_back(build_node_statistics(child, node->total, overall_total));
    }

    return stats;
}

void TimeProfiler::print_node_from_statistics(std::ostringstream &oss,
        const NodeStatistics &stats,
        const std::string &prefix,
        bool is_last) const
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    // Use more generic ASCII characters to ensure consistent width
    const char *branch_mid = "|- ";
    const char *branch_end = "`- ";
    const char *pad_mid = "|  ";
    const char *pad_end = "   ";
    const std::string connector = std::string(is_last ? branch_end : branch_mid);
    // Calculate next_prefix BEFORE using prefix for display_prefix
    // This ensures children get the correct prefix with padding
    const std::string next_prefix = prefix + (is_last ? pad_end : pad_mid);

    const std::string display_prefix = prefix + connector;

    // Color codes
    const char *color_reset = "\033[0m";
    const char *color_start = "";
    if (format_.use_color) {
        if (stats.pct_total >= 50.0) {
            color_start = "\033[31m";    // red
        } else if (stats.pct_total >= 20.0) {
            color_start = "\033[33m";    // yellow
        } else if (stats.pct_total >= 5.0) {
            color_start = "\033[36m";    // cyan
        }
    }
    const bool colorize = format_.use_color && *color_start != '\0';

    // Calculate actual display width (excluding color codes)
    int occupy = static_cast<int>(display_prefix.size() + stats.name.size());
    int pad = format_.name_width - occupy;
    if (pad < 1) {
        pad = 1;
    }

    // Build colored name (color codes do not occupy display width)
    const std::string name_colored = colorize ? (std::string(color_start) + stats.name + color_reset) : stats.name;

    // Use boost::format to build formatted string
    std::string fmt_str = "%s%s%s | %";
    fmt_str += std::to_string(format_.calls_width) + "d";
    fmt_str += " | %";
    fmt_str += std::to_string(format_.num_width) + "." + std::to_string(format_.precision) + "f";
    fmt_str += " | %";
    fmt_str += std::to_string(format_.num_width) + "." + std::to_string(format_.precision) + "f";
    fmt_str += " | %";
    fmt_str += std::to_string(format_.num_width) + "." + std::to_string(format_.precision) + "f";
    fmt_str += " | %";
    fmt_str += std::to_string(format_.num_width) + "." + std::to_string(format_.precision) + "f";
    fmt_str += " | %";
    fmt_str += std::to_string(format_.num_width) + "." + std::to_string(format_.precision) + "f";

    boost::format fmt(fmt_str);
    fmt % display_prefix % name_colored % std::string(pad, ' ') % stats.count % stats.total % stats.self_time % stats.avg % stats.min % stats.max;

    oss << fmt.str();

    if (format_.show_percentages) {
        // Build percentage format string
        std::string pct_fmt_str = " | %";
        pct_fmt_str += std::to_string(format_.percent_width) + ".2f%%";
        pct_fmt_str += " | %";
        pct_fmt_str += std::to_string(format_.percent_width) + ".2f%%";

        boost::format pct(pct_fmt_str);
        pct % stats.pct_parent % stats.pct_total;
        oss << pct.str();
    }
    oss << "\n";

    // Print children
    for (size_t i = 0; i < stats.children.size(); ++i) {
        const bool child_is_last = (i + 1 == stats.children.size());
        print_node_from_statistics(oss, stats.children[i], next_prefix, child_is_last);
    }
}

TimeProfilerScope::TimeProfilerScope(const std::string &name)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    TimeProfiler::get_instance().enter_scope(name);
}

TimeProfilerScope::~TimeProfilerScope()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    TimeProfiler::get_instance().leave_scope();
}

} // namespace esp_brookesia::lib_utils
