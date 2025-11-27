/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "unity.h"
#include "brookesia/lib_utils/state_machine.hpp"
#include "brookesia/lib_utils/state_base.hpp"
#include "brookesia/lib_utils/task_scheduler.hpp"
#include "brookesia/lib_utils/log.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <memory>
#include <atomic>
#include <string>
#include <thread>
#include <vector>

using namespace esp_brookesia::lib_utils;

#define TEST_SCHEDULER_CONFIG_FOUR_THREADS esp_brookesia::lib_utils::TaskScheduler::StartConfig{ \
    .worker_configs = {                                                                    \
        {.name = "TS_Worker1", .core_id = 0, .stack_size = 8192},                            \
        {.name = "TS_Worker2", .core_id = 1, .stack_size = 8192},                            \
        {.name = "TS_Worker3", .core_id = 0, .stack_size = 8192},                            \
        {.name = "TS_Worker4", .core_id = 1, .stack_size = 8192},                            \
    },                                                                                        \
    .worker_poll_interval_ms = 1                                                                  \
}

// ==================== Test State Classes ====================

class IdleState : public esp_brookesia::lib_utils::StateBase {
public:
    IdleState(StateMachine *sm) : sm_(sm) {}

    bool on_enter(const std::string &from_state, const std::string &action) override
    {
        // Verify state machine is unlocked
        sm_->is_running();

        BROOKESIA_LOGI("IdleState::on_enter from %1% action %2%", from_state.empty() ? "initial" : from_state, action);
        enter_count_++;
        return true;
    }

    bool on_exit(const std::string &to_state, const std::string &action) override
    {
        // Verify state machine is unlocked
        sm_->is_running();

        BROOKESIA_LOGI("IdleState::on_exit to %1% action %2%", to_state, action);
        exit_count_++;
        return true;
    }

    void on_update() override
    {
        // Verify state machine is unlocked
        sm_->is_running();

        update_count_++;
        BROOKESIA_LOGI("IdleState::on_update (count: %1%)", update_count_.load());
    }

    std::atomic<int> enter_count_{0};
    std::atomic<int> exit_count_{0};
    std::atomic<int> update_count_{0};
    StateMachine *sm_;
};

class RunningState : public esp_brookesia::lib_utils::StateBase {
public:
    RunningState() = default;

    bool on_enter(const std::string &from_state, const std::string &action) override
    {
        BROOKESIA_LOGI("RunningState::on_enter from %1% action %2%", from_state.empty() ? "initial" : from_state, action);
        enter_count_++;
        return true;
    }

    bool on_exit(const std::string &to_state, const std::string &action) override
    {
        BROOKESIA_LOGI("RunningState::on_exit to %1% action %2%", to_state, action);
        exit_count_++;
        return true;
    }

    void on_update() override
    {
        update_count_++;
        BROOKESIA_LOGI("RunningState::on_update (count: %1%)", update_count_.load());
    }

    std::atomic<int> enter_count_{0};
    std::atomic<int> exit_count_{0};
    std::atomic<int> update_count_{0};
};

class ErrorState : public esp_brookesia::lib_utils::StateBase {
public:
    ErrorState() = default;

    bool on_enter(const std::string &from_state, const std::string &action) override
    {
        BROOKESIA_LOGI("ErrorState::on_enter from %1% action %2%", from_state.empty() ? "initial" : from_state, action);
        enter_count_++;
        return true;
    }

    bool on_exit(const std::string &to_state, const std::string &action) override
    {
        BROOKESIA_LOGI("ErrorState::on_exit to %1% action %2%", to_state, action);
        exit_count_++;
        return true;
    }

    std::atomic<int> enter_count_{0};
    std::atomic<int> exit_count_{0};
};

class GuardedState : public esp_brookesia::lib_utils::StateBase {
public:
    GuardedState(bool allow_enter = true, bool allow_exit = true)
        : allow_enter_(allow_enter), allow_exit_(allow_exit) {}

    bool on_enter(const std::string &from_state, const std::string &action) override
    {
        BROOKESIA_LOGI("GuardedState::on_enter from %1% action %2% (allowed: %3%)",
                       from_state.empty() ? "initial" : from_state, action, allow_enter_);
        enter_attempt_count_++;
        if (allow_enter_) {
            enter_count_++;
        }
        return allow_enter_;
    }

    bool on_exit(const std::string &to_state, const std::string &action) override
    {
        BROOKESIA_LOGI("GuardedState::on_exit to %1% action %2% (allowed: %3%)", to_state, action, allow_exit_);
        exit_attempt_count_++;
        if (allow_exit_) {
            exit_count_++;
        }
        return allow_exit_;
    }

    void set_allow_enter(bool allow)
    {
        allow_enter_ = allow;
    }
    void set_allow_exit(bool allow)
    {
        allow_exit_ = allow;
    }

    std::atomic<int> enter_count_{0};
    std::atomic<int> exit_count_{0};
    std::atomic<int> enter_attempt_count_{0};
    std::atomic<int> exit_attempt_count_{0};

private:
    bool allow_enter_;
    bool allow_exit_;
};

// ==================== Test Cases: Basic State Machine Functionality ====================

TEST_CASE("Test state machine basic transition", "[utils][state_machine][basic]")
{
    BROOKESIA_LOGI("=== State Machine Basic Transition Test ===");

    auto scheduler = std::make_shared<TaskScheduler>();
    scheduler->start();

    StateMachine sm;

    auto idle = std::make_shared<IdleState>(&sm);
    auto running = std::make_shared<RunningState>();

    sm.add_state("idle", idle);
    sm.add_state("running", running);
    sm.add_transition("idle", "start", "running");
    sm.add_transition("running", "stop", "idle");

    // Start the state machine
    sm.start(scheduler, "idle");
    vTaskDelay(pdMS_TO_TICKS(100));

    TEST_ASSERT_EQUAL(1, idle->enter_count_.load());
    TEST_ASSERT_EQUAL(0, idle->exit_count_.load());

    // Trigger state transition
    sm.trigger_action("start");
    vTaskDelay(pdMS_TO_TICKS(100));

    TEST_ASSERT_EQUAL(1, idle->exit_count_.load());
    TEST_ASSERT_EQUAL(1, running->enter_count_.load());

    // Trigger state transition again
    sm.trigger_action("stop");
    vTaskDelay(pdMS_TO_TICKS(100));

    TEST_ASSERT_EQUAL(1, running->exit_count_.load());
    TEST_ASSERT_EQUAL(2, idle->enter_count_.load());
}

TEST_CASE("Test state machine with update interval", "[utils][state_machine][update]")
{
    BROOKESIA_LOGI("=== State Machine Update Interval Test ===");

    auto scheduler = std::make_shared<TaskScheduler>();
    scheduler->start();

    StateMachine sm;

    auto idle = std::make_shared<IdleState>(&sm);
    idle->set_update_interval(50); // 50ms update interval

    sm.add_state("idle", idle);
    sm.start(scheduler, "idle");

    // Wait for enough time for update to be called multiple times
    vTaskDelay(pdMS_TO_TICKS(300));

    BROOKESIA_LOGI("Update count: %1%", idle->update_count_.load());
    // 300ms / 50ms ≈ 6 times, but considering scheduling delay, at least 3 times should be called
    TEST_ASSERT_GREATER_THAN(3, idle->update_count_.load());
}

TEST_CASE("Test state machine with timeout", "[utils][state_machine][timeout]")
{
    BROOKESIA_LOGI("=== State Machine Timeout Test ===");

    auto scheduler = std::make_shared<TaskScheduler>();
    scheduler->start();

    StateMachine sm;

    auto idle = std::make_shared<IdleState>(&sm);
    auto running = std::make_shared<RunningState>();

    idle->set_timeout(200, "timeout"); // 200ms after trigger timeout action

    sm.add_state("idle", idle);
    sm.add_state("running", running);
    sm.add_transition("idle", "timeout", "running");

    sm.start(scheduler, "idle");
    vTaskDelay(pdMS_TO_TICKS(100));

    // Before timeout, running state should not be entered
    TEST_ASSERT_EQUAL(0, running->enter_count_.load());

    // Wait for timeout
    vTaskDelay(pdMS_TO_TICKS(200));

    // After timeout, should automatically transition to running state
    TEST_ASSERT_EQUAL(1, idle->exit_count_.load());
    TEST_ASSERT_EQUAL(1, running->enter_count_.load());
}

// ==================== Test Cases: State Guards ====================

TEST_CASE("Test state machine with entry guard", "[utils][state_machine][guard]")
{
    BROOKESIA_LOGI("=== State Machine Entry Guard Test ===");

    auto scheduler = std::make_shared<TaskScheduler>();
    scheduler->start();

    StateMachine sm;

    auto idle = std::make_shared<IdleState>(&sm);
    auto guarded = std::make_shared<GuardedState>(false, true); // Not allowed to enter

    sm.add_state("idle", idle);
    sm.add_state("guarded", guarded);
    sm.add_transition("idle", "enter_guarded", "guarded");

    sm.start(scheduler, "idle");
    vTaskDelay(pdMS_TO_TICKS(100));

    // Try to enter the guarded state
    sm.trigger_action("enter_guarded");
    vTaskDelay(pdMS_TO_TICKS(100));

    // Entry rejected, should roll back to idle state
    TEST_ASSERT_EQUAL(1, guarded->enter_attempt_count_.load());
    TEST_ASSERT_EQUAL(0, guarded->enter_count_.load());
    TEST_ASSERT_EQUAL(2, idle->enter_count_.load()); // Initial entry + roll back to enter again
}

TEST_CASE("Test state machine with exit guard", "[utils][state_machine][guard]")
{
    BROOKESIA_LOGI("=== State Machine Exit Guard Test ===");

    auto scheduler = std::make_shared<TaskScheduler>();
    scheduler->start();

    StateMachine sm;

    auto guarded = std::make_shared<GuardedState>(true, false); // Not allowed to exit
    auto idle = std::make_shared<IdleState>(&sm);

    sm.add_state("guarded", guarded);
    sm.add_state("idle", idle);
    sm.add_transition("guarded", "exit", "idle");

    sm.start(scheduler, "guarded");
    vTaskDelay(pdMS_TO_TICKS(100));

    // Try to exit the guarded state
    sm.trigger_action("exit");
    vTaskDelay(pdMS_TO_TICKS(100));

    // Exit rejected, should stay in guarded state
    TEST_ASSERT_EQUAL(1, guarded->exit_attempt_count_.load());
    TEST_ASSERT_EQUAL(0, guarded->exit_count_.load());
    TEST_ASSERT_EQUAL(0, idle->enter_count_.load());
}

// ==================== Test Cases: Complex Scenarios ====================

TEST_CASE("Test state machine multiple states", "[utils][state_machine][complex]")
{
    BROOKESIA_LOGI("=== State Machine Multiple States Test ===");

    auto scheduler = std::make_shared<TaskScheduler>();
    scheduler->start();

    StateMachine sm;

    auto idle = std::make_shared<IdleState>(&sm);
    auto running = std::make_shared<RunningState>();
    auto error = std::make_shared<ErrorState>();

    sm.add_state("idle", idle);
    sm.add_state("running", running);
    sm.add_state("error", error);

    sm.add_transition("idle", "start", "running");
    sm.add_transition("running", "stop", "idle");
    sm.add_transition("running", "error", "error");
    sm.add_transition("error", "reset", "idle");

    sm.start(scheduler, "idle");
    vTaskDelay(pdMS_TO_TICKS(50));

    // idle -> running
    sm.trigger_action("start");
    vTaskDelay(pdMS_TO_TICKS(50));
    TEST_ASSERT_EQUAL(1, running->enter_count_.load());

    // running -> error
    sm.trigger_action("error");
    vTaskDelay(pdMS_TO_TICKS(50));
    TEST_ASSERT_EQUAL(1, error->enter_count_.load());

    // error -> idle
    sm.trigger_action("reset");
    vTaskDelay(pdMS_TO_TICKS(50));
    TEST_ASSERT_EQUAL(2, idle->enter_count_.load());
}

TEST_CASE("Test state machine invalid transitions", "[utils][state_machine][invalid]")
{
    BROOKESIA_LOGI("=== State Machine Invalid Transitions Test ===");

    auto scheduler = std::make_shared<TaskScheduler>();
    scheduler->start();

    StateMachine sm;

    auto idle = std::make_shared<IdleState>(&sm);
    auto running = std::make_shared<RunningState>();

    sm.add_state("idle", idle);
    sm.add_state("running", running);
    sm.add_transition("idle", "start", "running");

    sm.start(scheduler, "idle");
    vTaskDelay(pdMS_TO_TICKS(50));

    // Trigger non-existent action
    sm.trigger_action("invalid_action");
    vTaskDelay(pdMS_TO_TICKS(50));

    // State should not change
    TEST_ASSERT_EQUAL(1, idle->enter_count_.load());
    TEST_ASSERT_EQUAL(0, idle->exit_count_.load());
    TEST_ASSERT_EQUAL(0, running->enter_count_.load());
}

TEST_CASE("Test state machine self transition", "[utils][state_machine][self]")
{
    BROOKESIA_LOGI("=== State Machine Self Transition Test ===");

    auto scheduler = std::make_shared<TaskScheduler>();
    scheduler->start();

    StateMachine sm;

    auto idle = std::make_shared<IdleState>(&sm);

    sm.add_state("idle", idle);
    sm.add_transition("idle", "refresh", "idle");

    sm.start(scheduler, "idle");
    vTaskDelay(pdMS_TO_TICKS(50));

    TEST_ASSERT_EQUAL(1, idle->enter_count_.load());

    // Trigger self transition (should be ignored, because target state is the same as current state)
    sm.trigger_action("refresh");
    vTaskDelay(pdMS_TO_TICKS(50));

    // Self transition ignored, count remains the same
    TEST_ASSERT_EQUAL(1, idle->enter_count_.load());
    TEST_ASSERT_EQUAL(0, idle->exit_count_.load());
}

// ==================== Test Cases: Concurrent and Task Cancellation ====================

TEST_CASE("Test state machine task cancellation on transition", "[utils][state_machine][cancel]")
{
    BROOKESIA_LOGI("=== State Machine Task Cancellation Test ===");

    auto scheduler = std::make_shared<TaskScheduler>();
    scheduler->start();

    StateMachine sm;

    auto idle = std::make_shared<IdleState>(&sm);
    auto running = std::make_shared<RunningState>();

    idle->set_update_interval(50);
    running->set_update_interval(50);

    sm.add_state("idle", idle);
    sm.add_state("running", running);
    sm.add_transition("idle", "start", "running");

    sm.start(scheduler, "idle");
    vTaskDelay(pdMS_TO_TICKS(200));

    int idle_updates = idle->update_count_.load();
    BROOKESIA_LOGI("Idle updates before transition: %1%", idle_updates);
    TEST_ASSERT_GREATER_THAN(0, idle_updates);

    // Transition to running state
    sm.trigger_action("start");
    vTaskDelay(pdMS_TO_TICKS(200));

    // idle's update should stop, running's update should start
    int idle_updates_after = idle->update_count_.load();
    int running_updates = running->update_count_.load();
    BROOKESIA_LOGI("Idle updates after: %1%, Running updates: %2%", idle_updates_after, running_updates);

    // Allow small error (asynchronous scheduling delay)
    TEST_ASSERT_TRUE(idle_updates_after <= idle_updates + 2);
    TEST_ASSERT_GREATER_THAN(0, running_updates);
}

TEST_CASE("Test state machine timeout cancellation on transition", "[utils][state_machine][timeout_cancel]")
{
    BROOKESIA_LOGI("=== State Machine Timeout Cancellation Test ===");

    auto scheduler = std::make_shared<TaskScheduler>();
    scheduler->start();

    StateMachine sm;

    auto idle = std::make_shared<IdleState>(&sm);
    auto running = std::make_shared<RunningState>();
    auto error = std::make_shared<ErrorState>();

    idle->set_timeout(500, "timeout"); // 500ms timeout

    sm.add_state("idle", idle);
    sm.add_state("running", running);
    sm.add_state("error", error);
    sm.add_transition("idle", "timeout", "error");
    sm.add_transition("idle", "start", "running");

    sm.start(scheduler, "idle");
    vTaskDelay(pdMS_TO_TICKS(100));

    // Transition to running state before timeout
    sm.trigger_action("start");
    vTaskDelay(pdMS_TO_TICKS(100));

    TEST_ASSERT_EQUAL(1, running->enter_count_.load());

    // Wait for the original timeout time
    vTaskDelay(pdMS_TO_TICKS(500));

    // Timeout task should be cancelled, error state should not be entered
    TEST_ASSERT_EQUAL(0, error->enter_count_.load());
}

// ==================== Test Cases: Edge Cases ====================

TEST_CASE("Test state machine start with invalid state", "[utils][state_machine][edge]")
{
    BROOKESIA_LOGI("=== State Machine Invalid Start Test ===");

    auto scheduler = std::make_shared<TaskScheduler>();
    scheduler->start();

    StateMachine sm;

    auto idle = std::make_shared<IdleState>(&sm);
    sm.add_state("idle", idle);

    // Try to start from a non-existent state
    sm.start(scheduler, "non_existent");
    vTaskDelay(pdMS_TO_TICKS(50));

    // Should not enter any state
    TEST_ASSERT_EQUAL(0, idle->enter_count_.load());
}

TEST_CASE("Test state machine rapid transitions", "[utils][state_machine][rapid]")
{
    BROOKESIA_LOGI("=== State Machine Rapid Transitions Test ===");

    auto scheduler = std::make_shared<TaskScheduler>();
    scheduler->start();

    StateMachine sm;

    auto idle = std::make_shared<IdleState>(&sm);
    auto running = std::make_shared<RunningState>();

    sm.add_state("idle", idle);
    sm.add_state("running", running);
    sm.add_transition("idle", "start", "running");
    sm.add_transition("running", "stop", "idle");

    sm.start(scheduler, "idle");
    vTaskDelay(pdMS_TO_TICKS(50));

    // Trigger multiple transitions quickly
    for (int i = 0; i < 10; i++) {
        sm.trigger_action("start");
        vTaskDelay(pdMS_TO_TICKS(20));
        sm.trigger_action("stop");
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    vTaskDelay(pdMS_TO_TICKS(100));

    BROOKESIA_LOGI("Idle enter: %1%, exit: %2%", idle->enter_count_.load(), idle->exit_count_.load());
    BROOKESIA_LOGI("Running enter: %1%, exit: %2%", running->enter_count_.load(), running->exit_count_.load());

    // Verify consistency of state transitions
    TEST_ASSERT_GREATER_THAN(5, idle->enter_count_.load());
    TEST_ASSERT_GREATER_THAN(5, running->enter_count_.load());
}

TEST_CASE("Test state machine concurrent trigger actions", "[utils][state_machine][concurrent]")
{
    BROOKESIA_LOGI("=== State Machine Concurrent Trigger Actions Test ===");

    auto scheduler = std::make_shared<TaskScheduler>();
    scheduler->start(TEST_SCHEDULER_CONFIG_FOUR_THREADS);

    StateMachine sm;

    // Create states that track all lifecycle calls
    class TrackingState : public esp_brookesia::lib_utils::StateBase {
    public:
        TrackingState(const std::string &name)
            : name_(name) {}

        bool on_enter(const std::string &from_state, const std::string &action) override
        {
            boost::lock_guard<boost::mutex> lock(mutex_);
            enter_count_++;
            enter_from_.push_back(from_state.empty() ? "initial" : from_state);
            BROOKESIA_LOGI("%1%::on_enter from %2% action %3% (count: %4%)",
                           name_, from_state.empty() ? "initial" : from_state, action, enter_count_.load());
            return true;
        }

        bool on_exit(const std::string &to_state, const std::string &action) override
        {
            boost::lock_guard<boost::mutex> lock(mutex_);
            exit_count_++;
            exit_to_.push_back(to_state);
            BROOKESIA_LOGI("%1%::on_exit to %2% action %3% (count: %4%)", name_, to_state, action, exit_count_.load());
            // Add a small delay to increase the chance of race condition
            vTaskDelay(pdMS_TO_TICKS(10));
            return true;
        }

        std::string name_;
        std::atomic<int> enter_count_{0};
        std::atomic<int> exit_count_{0};
        std::vector<std::string> enter_from_;
        std::vector<std::string> exit_to_;
        boost::mutex mutex_;
    };

    auto state_a = std::make_shared<TrackingState>("StateA");
    auto state_b = std::make_shared<TrackingState>("StateB");
    auto state_c = std::make_shared<TrackingState>("StateC");

    sm.add_state("A", state_a);
    sm.add_state("B", state_b);
    sm.add_state("C", state_c);
    sm.add_transition("A", "to_b", "B");
    sm.add_transition("A", "to_c", "C");

    // Start in state A
    sm.start(scheduler, "A");
    vTaskDelay(pdMS_TO_TICKS(50));

    TEST_ASSERT_EQUAL(1, state_a->enter_count_.load());
    BROOKESIA_LOGI("Current state: %1%", sm.get_current_state().c_str());

    // Concurrent trigger two actions: almost simultaneously trigger to_b and to_c
    // 预期行为：只有第一个动作应该成功触发转换
    // Expected behavior: only the first action should successfully trigger the transition
    // Problem behavior: both actions may be based on state A
    BROOKESIA_LOGI("Triggering concurrent actions...");

    // Use multiple threads to trigger concurrently
    std::atomic<bool> action1_result{false};
    std::atomic<bool> action2_result{false};

    std::thread t1([&sm, &action1_result]() {
        BROOKESIA_LOGI("Thread 1: trigger to_b");
        action1_result = sm.trigger_action("to_b");
        BROOKESIA_LOGI("Thread 1: result = %1%", action1_result.load());
    });

    std::thread t2([&sm, &action2_result]() {
        BROOKESIA_LOGI("Thread 2: trigger to_c");
        action2_result = sm.trigger_action("to_c");
        BROOKESIA_LOGI("Thread 2: result = %1%", action2_result.load());
    });

    t1.join();
    t2.join();

    // Wait for all transitions to complete
    vTaskDelay(pdMS_TO_TICKS(200));

    std::string final_state = sm.get_current_state();
    BROOKESIA_LOGI("Final state: %1%", final_state.c_str());
    BROOKESIA_LOGI("StateA: enter=%1%, exit=%2%", state_a->enter_count_.load(), state_a->exit_count_.load());
    BROOKESIA_LOGI("StateB: enter=%1%, exit=%2%", state_b->enter_count_.load(), state_b->exit_count_.load());
    BROOKESIA_LOGI("StateC: enter=%1%, exit=%2%", state_c->enter_count_.load(), state_c->exit_count_.load());

    // Check state transition records
    {
        boost::lock_guard<boost::mutex> lock(state_a->mutex_);
        BROOKESIA_LOGI("StateA exit_to history:");
        for (size_t i = 0; i < state_a->exit_to_.size(); i++) {
            BROOKESIA_LOGI("  [%1%] -> %2%", i, state_a->exit_to_[i].c_str());
        }
    }

    // Expected: A should only exit once (to B or to C)
    // Problem: If there is a concurrency issue, A may exit twice
    TEST_ASSERT_EQUAL(1, state_a->exit_count_.load());

    // Expected: only one of B or C should be entered
    int total_enters = state_b->enter_count_.load() + state_c->enter_count_.load();
    TEST_ASSERT_EQUAL(1, total_enters);

    // Expected: final state should be B or C
    TEST_ASSERT_TRUE(final_state == "B" || final_state == "C");
}

// ==================== Test Cases: Transition Finish Callback ====================

TEST_CASE("Test state machine transition finish callback", "[utils][state_machine][callback]")
{
    BROOKESIA_LOGI("=== State Machine Transition Finish Callback Test ===");

    auto scheduler = std::make_shared<TaskScheduler>();
    scheduler->start();

    StateMachine sm;

    auto idle = std::make_shared<IdleState>(&sm);
    auto running = std::make_shared<RunningState>();
    auto error = std::make_shared<ErrorState>();

    sm.add_state("idle", idle);
    sm.add_state("running", running);
    sm.add_state("error", error);
    sm.add_transition("idle", "start", "running");
    sm.add_transition("running", "stop", "idle");
    sm.add_transition("running", "error", "error");

    // Track callback invocations
    struct CallbackTracker {
        std::vector<std::tuple<std::string, std::string, std::string>> invocations;
        boost::mutex mutex_;

        void record(const std::string &from, const std::string &action, const std::string &to)
        {
            boost::lock_guard<boost::mutex> lock(mutex_);
            invocations.push_back(std::make_tuple(from, action, to));
            BROOKESIA_LOGI("Callback: from='%1%', action='%2%', to='%3%'", from, action, to);
        }

        size_t count()
        {
            boost::lock_guard<boost::mutex> lock(mutex_);
            return invocations.size();
        }

        void clear()
        {
            boost::lock_guard<boost::mutex> lock(mutex_);
            invocations.clear();
        }
    };

    CallbackTracker tracker;

    // Set callback
    sm.register_transition_finish_callback(
    [&sm, &tracker](const std::string & from, const std::string & action, const std::string & to) {
        tracker.record(from, action, to);
        // Verify state machine is unlocked
        sm.is_running();
    });

    // Start state machine
    sm.start(scheduler, "idle");
    vTaskDelay(pdMS_TO_TICKS(50));

    // Initial state entry should not trigger callback (no transition)
    TEST_ASSERT_EQUAL(0, tracker.count());

    // Trigger transition: idle -> running
    sm.trigger_action("start");
    vTaskDelay(pdMS_TO_TICKS(50));

    // Callback should be called once
    TEST_ASSERT_EQUAL(1, tracker.count());
    {
        boost::lock_guard<boost::mutex> lock(tracker.mutex_);
        auto &inv = tracker.invocations[0];
        // Note: first parameter is the target state (after transition), not the source state
        TEST_ASSERT_EQUAL_STRING("idle", std::get<0>(inv).c_str());
        TEST_ASSERT_EQUAL_STRING("start", std::get<1>(inv).c_str());
        TEST_ASSERT_EQUAL_STRING("running", std::get<2>(inv).c_str());
    }

    // Trigger transition: running -> idle
    sm.trigger_action("stop");
    vTaskDelay(pdMS_TO_TICKS(50));

    // Callback should be called again
    TEST_ASSERT_EQUAL(2, tracker.count());
    {
        boost::lock_guard<boost::mutex> lock(tracker.mutex_);
        auto &inv = tracker.invocations[1];
        // Note: first parameter is the target state (after transition), not the source state
        TEST_ASSERT_EQUAL_STRING("running", std::get<0>(inv).c_str());
        TEST_ASSERT_EQUAL_STRING("stop", std::get<1>(inv).c_str());
        TEST_ASSERT_EQUAL_STRING("idle", std::get<2>(inv).c_str());
    }

    // Trigger transition: idle -> running again
    sm.trigger_action("start");
    vTaskDelay(pdMS_TO_TICKS(50));

    // Trigger transition: running -> error
    sm.trigger_action("error");
    vTaskDelay(pdMS_TO_TICKS(50));

    // Should have 4 callbacks total
    TEST_ASSERT_EQUAL(4, tracker.count());
}

TEST_CASE("Test state machine transition finish callback with self transition", "[utils][state_machine][callback]")
{
    BROOKESIA_LOGI("=== State Machine Transition Finish Callback Self Transition Test ===");

    auto scheduler = std::make_shared<TaskScheduler>();
    scheduler->start();

    StateMachine sm;

    auto idle = std::make_shared<IdleState>(&sm);

    sm.add_state("idle", idle);
    sm.add_transition("idle", "refresh", "idle");

    int callback_count = 0;
    sm.register_transition_finish_callback([&sm, &callback_count](const std::string &, const std::string &, const std::string &) {
        callback_count++;
        // Verify state machine is unlocked
        sm.is_running();
    });

    sm.start(scheduler, "idle");
    vTaskDelay(pdMS_TO_TICKS(50));

    // Trigger self transition
    sm.trigger_action("refresh");
    vTaskDelay(pdMS_TO_TICKS(50));

    // Self transition shouldn't be ignored, callback should be called
    TEST_ASSERT_EQUAL(1, callback_count);
}

TEST_CASE("Test state machine transition finish callback with guard rejection", "[utils][state_machine][callback]")
{
    BROOKESIA_LOGI("=== State Machine Transition Finish Callback Guard Rejection Test ===");

    auto scheduler = std::make_shared<TaskScheduler>();
    scheduler->start();

    StateMachine sm;

    auto idle = std::make_shared<IdleState>(&sm);
    auto guarded = std::make_shared<GuardedState>(false, true); // Disallow entry

    sm.add_state("idle", idle);
    sm.add_state("guarded", guarded);
    sm.add_transition("idle", "enter_guarded", "guarded");

    int callback_count = 0;
    sm.register_transition_finish_callback([&sm, &callback_count](const std::string &, const std::string &, const std::string &) {
        callback_count++;
        // Verify state machine is unlocked
        sm.is_running();
    });

    sm.start(scheduler, "idle");
    vTaskDelay(pdMS_TO_TICKS(50));

    // Try to enter guarded state (should fail)
    sm.trigger_action("enter_guarded");
    vTaskDelay(pdMS_TO_TICKS(50));

    // Transition failed, callback should not be called
    TEST_ASSERT_EQUAL(0, callback_count);
}

TEST_CASE("Test state machine transition finish callback multiple transitions", "[utils][state_machine][callback]")
{
    BROOKESIA_LOGI("=== State Machine Transition Finish Callback Multiple Transitions Test ===");

    auto scheduler = std::make_shared<TaskScheduler>();
    scheduler->start();

    StateMachine sm;

    auto idle = std::make_shared<IdleState>(&sm);
    auto running = std::make_shared<RunningState>();

    sm.add_state("idle", idle);
    sm.add_state("running", running);
    sm.add_transition("idle", "start", "running");
    sm.add_transition("running", "stop", "idle");

    std::vector<std::string> transition_actions;
    boost::mutex actions_mutex;

    sm.register_transition_finish_callback(
    [&sm, &transition_actions, &actions_mutex](const std::string & from, const std::string & action, const std::string & to) {
        boost::lock_guard<boost::mutex> lock(actions_mutex);
        std::string transition = from + " -> " + action + " -> " + to;
        transition_actions.push_back(transition);
        BROOKESIA_LOGI("Transition: %1%", transition.c_str());

        // Verify state machine is unlocked
        sm.is_running();
    });

    sm.start(scheduler, "idle");
    vTaskDelay(pdMS_TO_TICKS(50));

    // Perform multiple transitions
    for (int i = 0; i < 5; i++) {
        sm.trigger_action("start");
        vTaskDelay(pdMS_TO_TICKS(50));
        sm.trigger_action("stop");
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    vTaskDelay(pdMS_TO_TICKS(100));

    // Should have 10 callbacks (5 start + 5 stop transitions)
    {
        boost::lock_guard<boost::mutex> lock(actions_mutex);
        TEST_ASSERT_EQUAL(10, transition_actions.size());
    }
}

// ==================== Test Cases: Combined Functionality ====================

TEST_CASE("Test state machine with update and timeout", "[utils][state_machine][combined]")
{
    BROOKESIA_LOGI("=== State Machine Update + Timeout Test ===");

    auto scheduler = std::make_shared<TaskScheduler>();
    scheduler->start();

    StateMachine sm;

    auto idle = std::make_shared<IdleState>(&sm);
    auto running = std::make_shared<RunningState>();

    idle->set_update_interval(50);
    idle->set_timeout(300, "timeout");

    sm.add_state("idle", idle);
    sm.add_state("running", running);
    sm.add_transition("idle", "timeout", "running");

    sm.start(scheduler, "idle");

    // Wait for a while, update should be called multiple times
    vTaskDelay(pdMS_TO_TICKS(200));
    int updates_before_timeout = idle->update_count_.load();
    BROOKESIA_LOGI("Updates before timeout: %1%", updates_before_timeout);
    TEST_ASSERT_GREATER_THAN(2, updates_before_timeout);

    // Wait for timeout (timeout is 300ms, already waited 200ms, wait another 150ms to ensure timeout triggers)
    vTaskDelay(pdMS_TO_TICKS(150));

    // After timeout, should transition to running state
    TEST_ASSERT_EQUAL(1, running->enter_count_.load());

    // idle's update should stop (allow small error, because asynchronous scheduling)
    int updates_after_timeout = idle->update_count_.load();
    BROOKESIA_LOGI("Updates after timeout: %1%", updates_after_timeout);
    // Verify update has stopped or only increased very little (considering asynchronous scheduling delay)
    TEST_ASSERT_TRUE(updates_after_timeout <= updates_before_timeout + 2);
}
