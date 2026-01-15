/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <chrono>
#include <sstream>
#include <string>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <stack>
#include <vector>
#include <limits>
#include "brookesia/lib_utils/describe_helpers.hpp"

namespace esp_brookesia::lib_utils {

/**
 * @brief Time profiler for performance measurement
 *
 * This class provides hierarchical time profiling capabilities with support
 * for nested scopes, cross-thread events, and detailed statistics reporting.
 */
class TimeProfiler {
public:
    /**
     * @brief Profiler tree node representing a timing scope
     */
    struct Node {
        std::string name;  ///< Name of this profiling scope
        double total = 0.0;  ///< Total time spent in this scope
        size_t count = 0;  ///< Number of times this scope was entered
        double min = std::numeric_limits<double>::infinity();  ///< Minimum time recorded
        double max = 0.0;  ///< Maximum time recorded
        std::map<std::string, std::unique_ptr<Node>> children;  ///< Child scopes
        Node *parent = nullptr;  ///< Parent scope

        Node(const std::string &n, Node *p = nullptr) : name(n), parent(p) {}
    };

    /**
     * @brief Statistics for a single profiling node
     */
    struct NodeStatistics {
        std::string name;              ///< Name of the profiling scope
        size_t count = 0;              ///< Number of times this scope was entered
        double total = 0.0;            ///< Total time spent in this scope
        double self_time = 0.0;       ///< Time spent in this scope excluding children
        double avg = 0.0;              ///< Average time per call
        double min = 0.0;              ///< Minimum time recorded
        double max = 0.0;              ///< Maximum time recorded
        double pct_parent = 0.0;       ///< Percentage of parent's total time
        double pct_total = 0.0;        ///< Percentage of overall total time
        std::vector<NodeStatistics> children;  ///< Child node statistics

        NodeStatistics() = default;
        NodeStatistics(const std::string &n) : name(n) {}
    };

    /**
     * @brief Complete statistics report for the profiler
     */
    struct Statistics {
        std::string unit_name;         ///< Time unit name (e.g., "ms", "us", "s")
        double overall_total = 0.0;   ///< Overall total time
        std::vector<NodeStatistics> root_children;  ///< Root level node statistics

        Statistics() = default;
    };

    /**
     * @brief Formatting options for profiler output
     */
    struct FormatOptions {
        /**
         * @brief Sort order for profiler output
         */
        enum class SortBy {
            TotalDesc,  ///< Sort by total time descending
            NameAsc,    ///< Sort by name ascending
            None        ///< No sorting
        };

        /**
         * @brief Time unit for display
         */
        enum class TimeUnit {
            Microseconds,  ///< Display in microseconds
            Milliseconds,  ///< Display in milliseconds
            Seconds        ///< Display in seconds
        };

        int name_width = 32;  ///< Width of name column
        int calls_width = 6;  ///< Width of calls column
        int num_width = 10;  ///< Width of numeric columns
        int percent_width = 7;  ///< Width of percentage column
        int precision = 2;  ///< Decimal precision for numbers
        bool use_unicode = false;  ///< Use ASCII characters to ensure alignment
        bool show_percentages = true;  ///< Show percentage columns
        bool use_color = false;  ///< Use ANSI color codes in output
        SortBy sort_by = SortBy::TotalDesc;  ///< Sort order for output
        TimeUnit time_unit = TimeUnit::Milliseconds;  ///< Time unit for display
    };

    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = std::chrono::time_point<Clock>;

    /**
     * @brief Get the singleton instance of TimeProfiler
     *
     * @return Reference to the singleton TimeProfiler instance
     */
    static TimeProfiler &get_instance()
    {
        static TimeProfiler inst;
        return inst;
    }

    // ========== Configuration ==========

    /**
     * @brief Set formatting options for output
     *
     * @param[in] options Format options to apply
     */
    void set_format_options(const FormatOptions &options);

    // ========== RAII Scope Profiling ==========

    /**
     * @brief Enter a profiling scope
     *
     * Should be paired with leave_scope(). Prefer using BROOKESIA_TIME_PROFILER_SCOPE
     * macro for automatic scope management.
     *
     * @param[in] name Name of the scope to profile
     */
    void enter_scope(const std::string &name);

    /**
     * @brief Leave the current profiling scope
     *
     * Records the elapsed time since the corresponding enter_scope() call.
     */
    void leave_scope();

    // ========== Cross-thread Event Profiling ==========

    /**
     * @brief Start timing a named event
     *
     * Used for timing events that may span across function boundaries or threads.
     * Must be paired with end_event() with the same name.
     *
     * @param[in] name Name of the event to start timing
     */
    void start_event(const std::string &name);

    /**
     * @brief End timing a named event
     *
     * Records the elapsed time since the corresponding start_event() call.
     *
     * @param[in] name Name of the event to stop timing
     */
    void end_event(const std::string &name);

    // ========== Output and Management ==========

    /**
     * @brief Get profiling statistics
     *
     * Returns a structured representation of all profiling data.
     *
     * @return Statistics structure containing all profiling information
     */
    Statistics get_statistics() const;

    /**
     * @brief Generate and output profiling report
     *
     * Prints a hierarchical report of all recorded timings to the log.
     * This method internally calls get_statistics() and formats the output.
     */
    void report();

    /**
     * @brief Clear all profiling data
     *
     * Resets all timing statistics and clears the profiling tree.
     */
    void clear();

private:
    TimeProfiler(): root_("root") {}

    static std::stack<Node *> &thread_local_stack();

    static std::map<Node *, TimePoint> &start_times();

    Node *find_or_create_node(const std::string &name);

    double to_unit(std::chrono::duration<double> d) const;

    std::string unit_name() const;

    void print_node(std::ostringstream &oss,
                    Node *node,
                    const std::string &prefix,
                    bool is_last,
                    double parent_total,
                    double overall_total) const;

    void print_header(std::ostringstream &oss) const;

    mutable std::mutex mutex_;
    Node root_;

    std::map<std::string, std::deque<TimePoint>> event_stacks_;
    FormatOptions format_{};

    static std::string format_percent(double pct);

    double sum_children_total(const Node *node) const;

    std::vector<Node *> sorted_children(Node *node) const;

    NodeStatistics build_node_statistics(const Node *node, double parent_total, double overall_total) const;

    void print_node_from_statistics(std::ostringstream &oss,
                                    const NodeStatistics &stats,
                                    const std::string &prefix,
                                    bool is_last) const;
};

/**
 * @brief RAII wrapper for time profiling scopes
 *
 * Automatically calls enter_scope() on construction and leave_scope() on destruction.
 * Use the BROOKESIA_TIME_PROFILER_SCOPE macro for convenient usage.
 */
class TimeProfilerScope {
public:
    /**
     * @brief Constructor that enters a profiling scope
     *
     * @param[in] name Name of the scope to profile
     */
    explicit TimeProfilerScope(const std::string &name);

    /**
     * @brief Destructor that leaves the profiling scope
     *
     * Automatically records the elapsed time for this scope.
     */
    ~TimeProfilerScope();
};

BROOKESIA_DESCRIBE_ENUM(TimeProfiler::FormatOptions::SortBy, TotalDesc, NameAsc, None)
BROOKESIA_DESCRIBE_ENUM(TimeProfiler::FormatOptions::TimeUnit, Microseconds, Milliseconds, Seconds)
BROOKESIA_DESCRIBE_STRUCT(
    TimeProfiler::FormatOptions, (), (name_width, calls_width, num_width, percent_width, precision, use_unicode,
                                      show_percentages, use_color, sort_by, time_unit)
)
BROOKESIA_DESCRIBE_STRUCT(
    TimeProfiler::NodeStatistics, (), (name, count, total, self_time, avg, min, max, pct_parent, pct_total, children)
)
BROOKESIA_DESCRIBE_STRUCT(
    TimeProfiler::Statistics, (), (unit_name, overall_total, root_children)
)

} // namespace esp_brookesia::lib_utils

#define _BROOKESIA_TIME_PROFILER_CONCAT(a, b) a##b
#define BROOKESIA_TIME_PROFILER_CONCAT(a, b) _BROOKESIA_TIME_PROFILER_CONCAT(a, b)

/**
 * @brief Create a profiling scope for the current block
 *
 * This macro creates a TimeProfilerScope object that automatically
 * measures the execution time of the current scope.
 *
 * @param name Name of the profiling scope
 *
 * @example
 * void my_function() {
 *     BROOKESIA_TIME_PROFILER_SCOPE("my_function");
 *     // Code to profile
 * }
 */
#define BROOKESIA_TIME_PROFILER_SCOPE(name) \
    esp_brookesia::lib_utils::TimeProfilerScope BROOKESIA_TIME_PROFILER_CONCAT(_time_profiler_scope_, __LINE__)(name)

/**
 * @brief Start timing a named event
 *
 * Used for profiling events that span across function boundaries or threads.
 * Must be paired with BROOKESIA_TIME_PROFILER_END_EVENT.
 *
 * @param name Name of the event to start timing
 */
#define BROOKESIA_TIME_PROFILER_START_EVENT(name) \
    esp_brookesia::lib_utils::TimeProfiler::get_instance().start_event(name)

/**
 * @brief End timing a named event
 *
 * Records the elapsed time since BROOKESIA_TIME_PROFILER_START_EVENT was called
 * with the same name.
 *
 * @param name Name of the event to stop timing
 */
#define BROOKESIA_TIME_PROFILER_END_EVENT(name) \
    esp_brookesia::lib_utils::TimeProfiler::get_instance().end_event(name)

/**
 * @brief Generate and output profiling report
 *
 * Prints a hierarchical report of all recorded timings.
 */
#define BROOKESIA_TIME_PROFILER_REPORT() \
    esp_brookesia::lib_utils::TimeProfiler::get_instance().report()

/**
 * @brief Clear all profiling data
 *
 * Resets all timing statistics and clears the profiling tree.
 */
#define BROOKESIA_TIME_PROFILER_CLEAR() \
    esp_brookesia::lib_utils::TimeProfiler::get_instance().clear()
