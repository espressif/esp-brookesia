/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <atomic>
#include <memory>
#include <string>
#include "unity.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "brookesia/lib_utils/thread_config.hpp"
#include "brookesia/service_manager.hpp"
#include "common_def.hpp"

using namespace esp_brookesia::service;

static auto &service_manager = ServiceManager::get_instance();

// ============================================================================
// Test service class
// ============================================================================

class TestService : public ServiceBase {
public:
    static constexpr const char *SERVICE_NAME = "test_service";

    TestService()
        : ServiceBase({
        .name = SERVICE_NAME,
    })
    {
        total_instances_++;
    }

    ~TestService()
    {
        total_instances_--;
    }

    bool on_init() override
    {
        init_count_++;
        BROOKESIA_LOGI("TestService %1% on_init called (count: %2%)", get_attributes().name, init_count_.load());
        return true;
    }

    void on_deinit() override
    {
        deinit_count_++;
        BROOKESIA_LOGI("TestService %1% on_deinit called (count: %2%)", get_attributes().name, deinit_count_.load());
    }

    bool on_start() override
    {
        start_count_++;
        BROOKESIA_LOGI("TestService %1% on_start called (count: %2%)", get_attributes().name, start_count_.load());
        return true;
    }

    void on_stop() override
    {
        stop_count_++;
        BROOKESIA_LOGI("TestService %1% on_stop called (count: %2%)", get_attributes().name, stop_count_.load());
    }

    // Provide function schemas
    std::vector<FunctionSchema> get_function_schemas() override
    {
        return {
            FunctionSchema{
                .name = "add",
                .description = "Add two numbers",
                .parameters = {
                    FunctionParameterSchema{.name = "a", .description = "First number", .type = FunctionValueType::Number},
                    FunctionParameterSchema{.name = "b", .description = "Second number", .type = FunctionValueType::Number},
                },
            },
            FunctionSchema{
                .name = "echo",
                .description = "Echo a string",
                .parameters = {
                    FunctionParameterSchema{.name = "message", .description = "Message to echo", .type = FunctionValueType::String},
                },
            },
        };
    }

    // Provide event schemas
    std::vector<EventSchema> get_event_schemas() override
    {
        return {
            EventSchema{
                .name = "value_changed",
                .description = "Value changed event",
                .items = {
                    EventItemSchema{.name = "value", .description = "New value", .type = EventItemType::Number},
                },
            },
            EventSchema{
                .name = "message_received",
                .description = "Message received event",
                .items = {
                    EventItemSchema{.name = "message", .description = "Received message", .type = EventItemType::String},
                },
            },
        };
    }

    // Provide function handlers
    FunctionHandlerMap get_function_handlers() override
    {
        return {
            {
                "add",
                [this](const FunctionParameterMap & args) -> FunctionResult {
                    return handle_add(args);
                }
            },
            {
                "echo",
                [this](const FunctionParameterMap & args) -> FunctionResult {
                    return handle_echo(args);
                }
            },
        };
    }

    // Public publish_event for testing
    bool test_publish_event(const std::string &event_name, std::vector<EventItem> &&event_values)
    {
        return publish_event(event_name, std::move(event_values));
    }

    bool test_publish_event(const std::string &event_name, boost::json::object &&data_json)
    {
        return publish_event(event_name, std::move(data_json));
    }

    bool test_publish_event(const std::string &event_name, EventItemMap &&event_items)
    {
        return publish_event(event_name, std::move(event_items));
    }

    int get_init_count() const
    {
        return init_count_.load();
    }
    int get_deinit_count() const
    {
        return deinit_count_.load();
    }
    int get_start_count() const
    {
        return start_count_.load();
    }
    int get_stop_count() const
    {
        return stop_count_.load();
    }

    static std::atomic<int> total_instances_;

private:
    FunctionResult handle_add(const FunctionParameterMap &args)
    {
        try {
            double a = std::get<double>(args.at("a"));
            double b = std::get<double>(args.at("b"));
            double result = a + b;

            return FunctionResult{
                .success = true,
                .data = FunctionValue(result),
            };
        } catch (const std::exception &e) {
            return FunctionResult{
                .success = false,
                .error_message = e.what(),
            };
        }
    }

    FunctionResult handle_echo(const FunctionParameterMap &args)
    {
        try {
            std::string message = std::get<std::string>(args.at("message"));

            return FunctionResult{
                .success = true,
                .data = FunctionValue(message),
            };
        } catch (const std::exception &e) {
            return FunctionResult{
                .success = false,
                .error_message = e.what(),
            };
        }
    }

    std::atomic<int> init_count_{0};
    std::atomic<int> deinit_count_{0};
    std::atomic<int> start_count_{0};
    std::atomic<int> stop_count_{0};
};

std::atomic<int> TestService::total_instances_{0};

BROOKESIA_PLUGIN_REGISTER(ServiceBase, TestService, TestService::SERVICE_NAME);

// ============================================================================
// ServiceBinding API testing
// ============================================================================

TEST_CASE("Test APIs: bind service", "[brookesia][service][api][bind]")
{
    BROOKESIA_LOGI("=== Test bind service ===");

    TEST_ASSERT_TRUE(service_manager.init());
    TEST_ASSERT_TRUE(service_manager.start());

    // Bind service (service is initialized in init, bind only starts)
    auto binding = service_manager.bind("test_service");
    TEST_ASSERT_TRUE(binding.is_valid());

    // Get service object
    auto service = binding.get_service();
    TEST_ASSERT_NOT_NULL(service);
    TEST_ASSERT_EQUAL_STRING("test_service", service->get_attributes().name.c_str());
    TEST_ASSERT_TRUE(service->is_initialized());
    TEST_ASSERT_TRUE(service->is_running());

    // Manually release binding (only stops, does not deinitialize)
    binding.release();
    TEST_ASSERT_FALSE(binding.is_valid());
    TEST_ASSERT_FALSE(service->is_running());
    TEST_ASSERT_TRUE(service->is_initialized());  // Still in initialized state

    service_manager.stop();
    service_manager.deinit();
}

TEST_CASE("Test APIs: bind non-existent service", "[brookesia][service][api][bind_not_exist]")
{
    BROOKESIA_LOGI("=== Test bind non-existent service ===");

    TEST_ASSERT_TRUE(service_manager.init());
    TEST_ASSERT_TRUE(service_manager.start());

    auto binding = service_manager.bind("NonExistentService");
    TEST_ASSERT_FALSE(binding.is_valid());

    service_manager.stop();
    service_manager.deinit();
}

TEST_CASE("Test APIs: binding auto release", "[brookesia][service][api][bind_auto_release]")
{
    BROOKESIA_LOGI("=== Test binding auto release ===");

    TEST_ASSERT_TRUE(service_manager.init());
    TEST_ASSERT_TRUE(service_manager.start());

    std::shared_ptr<ServiceBase> service = nullptr;
    {
        auto binding = service_manager.bind("test_service");
        TEST_ASSERT_TRUE(binding.is_valid());
        service = binding.get_service();
        TEST_ASSERT_TRUE(service->is_running());
        // binding is automatically released when the scope ends
    }

    // Service should be stopped, but still initialized
    TEST_ASSERT_FALSE(service->is_running());
    TEST_ASSERT_TRUE(service->is_initialized());

    service_manager.stop();
    service_manager.deinit();
}

TEST_CASE("Test APIs: multiple bindings share same instance", "[brookesia][service][api][multiple_bindings]")
{
    BROOKESIA_LOGI("=== Test multiple bindings ===");

    TEST_ASSERT_TRUE(service_manager.init());
    TEST_ASSERT_TRUE(service_manager.start());

    // Create multiple bindings
    auto binding1 = service_manager.bind("test_service");
    auto binding2 = service_manager.bind("test_service");
    auto binding3 = service_manager.bind("test_service");

    TEST_ASSERT_TRUE(binding1.is_valid());
    TEST_ASSERT_TRUE(binding2.is_valid());
    TEST_ASSERT_TRUE(binding3.is_valid());

    // All bindings share the same service instance
    auto service1 = binding1.get_service();
    auto service2 = binding2.get_service();
    auto service3 = binding3.get_service();

    TEST_ASSERT_EQUAL(service1.get(), service2.get());
    TEST_ASSERT_EQUAL(service1.get(), service3.get());
    TEST_ASSERT_EQUAL(service2.get(), service3.get());

    service_manager.stop();
    service_manager.deinit();
}

TEST_CASE("Test APIs: binding move semantics", "[brookesia][service][api][bind_move]")
{
    BROOKESIA_LOGI("=== Test binding move semantics ===");

    TEST_ASSERT_TRUE(service_manager.init());
    TEST_ASSERT_TRUE(service_manager.start());

    auto binding1 = service_manager.bind("test_service");
    TEST_ASSERT_TRUE(binding1.is_valid());
    auto service = binding1.get_service();

    // Move constructor
    auto binding2 = std::move(binding1);
    TEST_ASSERT_FALSE(binding1.is_valid());
    TEST_ASSERT_TRUE(binding2.is_valid());
    TEST_ASSERT_TRUE(service->is_running());

    // Move assignment
    ServiceBinding binding3;
    binding3 = std::move(binding2);
    TEST_ASSERT_FALSE(binding2.is_valid());
    TEST_ASSERT_TRUE(binding3.is_valid());
    TEST_ASSERT_TRUE(service->is_running());

    service_manager.stop();
    service_manager.deinit();
}

// ============================================================================
// Service lifecycle testing
// ============================================================================

TEST_CASE("Test APIs: service lifecycle callbacks", "[brookesia][service][api][lifecycle]")
{
    BROOKESIA_LOGI("=== Test service lifecycle callbacks ===");

    ServiceRegistry::release_all_instances();
    ServiceRegistry::register_plugin<TestService>("test_service", []() {
        return std::make_unique<TestService>();
    });

    TEST_ASSERT_TRUE(service_manager.init());
    TEST_ASSERT_TRUE(service_manager.start());

    // Service is initialized in init, bind starts
    auto binding = service_manager.bind("test_service");
    TEST_ASSERT_TRUE(binding.is_valid());

    auto service = std::dynamic_pointer_cast<TestService>(binding.get_service());
    TEST_ASSERT_NOT_NULL(service);
    TEST_ASSERT_EQUAL(1, service->get_init_count());
    TEST_ASSERT_EQUAL(1, service->get_start_count());

    // Release only stops, does not deinitialize
    binding.release();
    TEST_ASSERT_EQUAL(1, service->get_stop_count());
    TEST_ASSERT_EQUAL(0, service->get_deinit_count());

    // Deinitialize only when deinit is called
    service_manager.stop();
    service_manager.deinit();
    TEST_ASSERT_EQUAL(1, service->get_deinit_count());
}

TEST_CASE("Test APIs: service state after bind", "[brookesia][service][api][state_after_bind]")
{
    BROOKESIA_LOGI("=== Test service state after bind ===");

    ServiceRegistry::release_all_instances();
    ServiceRegistry::register_plugin<TestService>("test_service", []() {
        return std::make_unique<TestService>();
    });

    TEST_ASSERT_TRUE(service_manager.init());
    TEST_ASSERT_TRUE(service_manager.start());

    // Service should be initialized and running after bind
    auto binding = service_manager.bind("test_service");
    TEST_ASSERT_TRUE(binding.is_valid());

    auto service = binding.get_service();
    TEST_ASSERT_TRUE(service->is_initialized());
    TEST_ASSERT_TRUE(service->is_running());

    service_manager.stop();
    service_manager.deinit();
}

TEST_CASE("Test APIs: shared service lifecycle with ref counting", "[brookesia][service][api][shared_lifecycle]")
{
    BROOKESIA_LOGI("=== Test shared service lifecycle ===");

    ServiceRegistry::release_all_instances();
    ServiceRegistry::register_plugin<TestService>("test_service", []() {
        return std::make_unique<TestService>();
    });

    TEST_ASSERT_TRUE(service_manager.init());
    TEST_ASSERT_TRUE(service_manager.start());

    auto binding1 = service_manager.bind("test_service");
    auto binding2 = service_manager.bind("test_service");

    auto service1 = std::dynamic_pointer_cast<TestService>(binding1.get_service());
    auto service2 = std::dynamic_pointer_cast<TestService>(binding2.get_service());

    // Two bindings share the same service instance
    TEST_ASSERT_EQUAL(service1.get(), service2.get());

    // Service is initialized and started only once
    TEST_ASSERT_EQUAL(1, service1->get_init_count());
    TEST_ASSERT_EQUAL(1, service1->get_start_count());

    // Release first binding, service is still running (another binding exists)
    binding1.release();
    TEST_ASSERT_TRUE(service2->is_running());
    TEST_ASSERT_EQUAL(0, service2->get_stop_count());

    // Release second binding, service stops (but does not deinitialize)
    binding2.release();
    TEST_ASSERT_EQUAL(1, service1->get_stop_count());
    TEST_ASSERT_EQUAL(0, service1->get_deinit_count());
    TEST_ASSERT_TRUE(service1->is_initialized());

    // Deinitialize only when deinit is called
    service_manager.stop();
    service_manager.deinit();
    TEST_ASSERT_EQUAL(1, service1->get_deinit_count());
}

// ============================================================================
// ServiceBase API testing
// ============================================================================

TEST_CASE("Test APIs: call service function - add", "[brookesia][service][api][call_function_add]")
{
    BROOKESIA_LOGI("=== Test call service function - add ===");

    TEST_ASSERT_TRUE(service_manager.init());
    TEST_ASSERT_TRUE(service_manager.start());

    auto binding = service_manager.bind("test_service");
    TEST_ASSERT_TRUE(binding.is_valid());

    auto service = binding.get_service();
    TEST_ASSERT_NOT_NULL(service);

    // Call add function
    FunctionParameterMap args;
    args["a"] = FunctionValue(10.0);
    args["b"] = FunctionValue(20.0);

    auto result = service->call_function_sync("add", std::move(args), 100);
    TEST_ASSERT_TRUE_MESSAGE(result.success, result.error_message.c_str());
    TEST_ASSERT_TRUE(result.has_data());
    TEST_ASSERT_EQUAL_DOUBLE(30.0, std::get<double>(*result.data));

    service_manager.stop();
    service_manager.deinit();
}

TEST_CASE("Test APIs: call service function - echo", "[brookesia][service][api][call_function_echo]")
{
    BROOKESIA_LOGI("=== Test call service function - echo ===");

    TEST_ASSERT_TRUE(service_manager.init());
    TEST_ASSERT_TRUE(service_manager.start());

    auto binding = service_manager.bind("test_service");
    TEST_ASSERT_TRUE(binding.is_valid());

    auto service = binding.get_service();
    TEST_ASSERT_NOT_NULL(service);

    // Call echo function
    FunctionParameterMap args;
    args["message"] = FunctionValue("Hello, World!");

    auto result = service->call_function_sync("echo", std::move(args), 100);
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_TRUE(result.has_data());
    TEST_ASSERT_EQUAL_STRING("Hello, World!", std::get<std::string>(*result.data).c_str());

    service_manager.stop();
    service_manager.deinit();
}

TEST_CASE("Test APIs: call non-existent function", "[brookesia][service][api][call_function_not_exist]")
{
    BROOKESIA_LOGI("=== Test call non-existent function ===");

    TEST_ASSERT_TRUE(service_manager.init());
    TEST_ASSERT_TRUE(service_manager.start());

    auto binding = service_manager.bind("test_service");
    TEST_ASSERT_TRUE(binding.is_valid());

    auto service = binding.get_service();
    TEST_ASSERT_NOT_NULL(service);

    // Call non-existent function
    FunctionParameterMap args;
    auto result = service->call_function_sync("non_existent", std::move(args), 100);
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_FALSE(result.error_message.empty());

    service_manager.stop();
    service_manager.deinit();
}

TEST_CASE("Test APIs: call function with json object - add", "[brookesia][service][api][call_function_json_add]")
{
    BROOKESIA_LOGI("=== Test call function with json object - add ===");

    TEST_ASSERT_TRUE(service_manager.init());
    TEST_ASSERT_TRUE(service_manager.start());

    auto binding = service_manager.bind("test_service");
    TEST_ASSERT_TRUE(binding.is_valid());

    auto service = binding.get_service();
    TEST_ASSERT_NOT_NULL(service);

    // Call add function using boost::json::object
    boost::json::object args_json;
    args_json["a"] = 15.0;
    args_json["b"] = 25.0;

    auto result = service->call_function_sync("add", std::move(args_json), 100);
    TEST_ASSERT_TRUE_MESSAGE(result.success, result.error_message.c_str());
    TEST_ASSERT_TRUE(result.has_data());
    TEST_ASSERT_EQUAL_DOUBLE(40.0, std::get<double>(*result.data));

    service_manager.stop();
    service_manager.deinit();
}

TEST_CASE("Test APIs: call function with json object - echo", "[brookesia][service][api][call_function_json_echo]")
{
    BROOKESIA_LOGI("=== Test call function with json object - echo ===");

    TEST_ASSERT_TRUE(service_manager.init());
    TEST_ASSERT_TRUE(service_manager.start());

    auto binding = service_manager.bind("test_service");
    TEST_ASSERT_TRUE(binding.is_valid());

    auto service = binding.get_service();
    TEST_ASSERT_NOT_NULL(service);

    // Call echo function using boost::json::object
    boost::json::object args_json;
    args_json["message"] = "JSON Test Message";

    auto result = service->call_function_sync("echo", std::move(args_json), 100);
    TEST_ASSERT_TRUE_MESSAGE(result.success, result.error_message.c_str());
    TEST_ASSERT_TRUE(result.has_data());
    TEST_ASSERT_EQUAL_STRING("JSON Test Message", std::get<std::string>(*result.data).c_str());

    service_manager.stop();
    service_manager.deinit();
}

TEST_CASE("Test APIs: call function with vector - add", "[brookesia][service][api][call_function_vector_add]")
{
    BROOKESIA_LOGI("=== Test call function with vector - add ===");

    TEST_ASSERT_TRUE(service_manager.init());
    TEST_ASSERT_TRUE(service_manager.start());

    auto binding = service_manager.bind("test_service");
    TEST_ASSERT_TRUE(binding.is_valid());

    auto service = binding.get_service();
    TEST_ASSERT_NOT_NULL(service);

    // Call add function using std::vector<FunctionValue> (in order of parameters)
    std::vector<FunctionValue> args_vector;
    args_vector.push_back(FunctionValue(5.0));   // a
    args_vector.push_back(FunctionValue(7.0));   // b

    auto result = service->call_function_sync("add", std::move(args_vector), 100);
    TEST_ASSERT_TRUE_MESSAGE(result.success, result.error_message.c_str());
    TEST_ASSERT_TRUE(result.has_data());
    TEST_ASSERT_EQUAL_DOUBLE(12.0, std::get<double>(*result.data));

    service_manager.stop();
    service_manager.deinit();
}

TEST_CASE("Test APIs: call function with vector - echo", "[brookesia][service][api][call_function_vector_echo]")
{
    BROOKESIA_LOGI("=== Test call function with vector - echo ===");

    TEST_ASSERT_TRUE(service_manager.init());
    TEST_ASSERT_TRUE(service_manager.start());

    auto binding = service_manager.bind("test_service");
    TEST_ASSERT_TRUE(binding.is_valid());

    auto service = binding.get_service();
    TEST_ASSERT_NOT_NULL(service);

    // Call echo function using std::vector<FunctionValue>
    std::vector<FunctionValue> args_vector;
    args_vector.push_back(FunctionValue("Vector Test Message"));  // message

    auto result = service->call_function_sync("echo", std::move(args_vector), 100);
    TEST_ASSERT_TRUE_MESSAGE(result.success, result.error_message.c_str());
    TEST_ASSERT_TRUE(result.has_data());
    TEST_ASSERT_EQUAL_STRING("Vector Test Message", std::get<std::string>(*result.data).c_str());

    service_manager.stop();
    service_manager.deinit();
}

TEST_CASE("Test APIs: call function with vector - wrong argument count", "[brookesia][service][api][call_function_vector_wrong_count]")
{
    BROOKESIA_LOGI("=== Test call function with vector - wrong argument count ===");

    TEST_ASSERT_TRUE(service_manager.init());
    TEST_ASSERT_TRUE(service_manager.start());

    auto binding = service_manager.bind("test_service");
    TEST_ASSERT_TRUE(binding.is_valid());

    auto service = binding.get_service();
    TEST_ASSERT_NOT_NULL(service);

    // Call add function with incorrect number of parameters (needs 2, only provides 1)
    std::vector<FunctionValue> args_vector;
    args_vector.push_back(FunctionValue(10.0));  // Only a, missing b

    auto result = service->call_function_sync("add", std::move(args_vector), 100);
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_FALSE(result.error_message.empty());
    BROOKESIA_LOGI("Expected error: %1%", result.error_message);

    service_manager.stop();
    service_manager.deinit();
}

TEST_CASE("Test APIs: subscribe and publish event", "[brookesia][service][api][event]")
{
    BROOKESIA_LOGI("=== Test subscribe and publish event ===");

    TEST_ASSERT_TRUE(service_manager.init());
    TEST_ASSERT_TRUE(service_manager.start());

    auto binding = service_manager.bind("test_service");
    TEST_ASSERT_TRUE(binding.is_valid());

    auto service = std::dynamic_pointer_cast<TestService>(binding.get_service());
    TEST_ASSERT_NOT_NULL(service);

    // Subscribe to event
    std::atomic<bool> event_received{false};
    std::atomic<double> received_value{0.0};

    EventRegistry::SignalConnection connection = service->subscribe_event("value_changed",
    [&event_received, &received_value](const std::string & event_name, const EventItemMap & data) {
        BROOKESIA_LOGI("Event received: %1%", event_name);
        event_received = true;
        if (data.find("value") != data.end()) {
            received_value = std::get<double>(data.at("value"));
        }
    });
    TEST_ASSERT_TRUE(connection.connected());

    // Publish event
    std::vector<EventItem> values;
    values.push_back(EventItem(42.0));
    bool published = service->test_publish_event("value_changed", std::move(values));
    TEST_ASSERT_TRUE(published);

    // Wait for event processing
    vTaskDelay(pdMS_TO_TICKS(100));

    // Verify event received
    TEST_ASSERT_TRUE(event_received.load());
    TEST_ASSERT_EQUAL_DOUBLE(42.0, received_value.load());

    service_manager.stop();
    service_manager.deinit();
}

TEST_CASE("Test APIs: publish event with string data", "[brookesia][service][api][event_string]")
{
    BROOKESIA_LOGI("=== Test publish event with string data ===");

    TEST_ASSERT_TRUE(service_manager.init());
    TEST_ASSERT_TRUE(service_manager.start());

    auto binding = service_manager.bind("test_service");
    TEST_ASSERT_TRUE(binding.is_valid());

    auto service = std::dynamic_pointer_cast<TestService>(binding.get_service());
    TEST_ASSERT_NOT_NULL(service);

    // Subscribe to event
    std::atomic<bool> event_received{false};
    std::string received_message;

    EventRegistry::SignalConnection connection;
    connection = service->subscribe_event("message_received",
    [&event_received, &received_message](const std::string & event_name, const EventItemMap & data) {
        BROOKESIA_LOGI("Event received: %1%", event_name);
        event_received = true;
        if (data.find("message") != data.end()) {
            received_message = std::get<std::string>(data.at("message"));
        }
    });
    TEST_ASSERT_TRUE(connection.connected());

    // Publish event
    std::vector<EventItem> values;
    values.push_back(EventItem("Test Message"));
    bool published = service->test_publish_event("message_received", std::move(values));
    TEST_ASSERT_TRUE(published);

    // Wait for event processing
    vTaskDelay(pdMS_TO_TICKS(100));

    // Verify event received
    TEST_ASSERT_TRUE(event_received.load());
    TEST_ASSERT_EQUAL_STRING("Test Message", received_message.c_str());

    service_manager.stop();
    service_manager.deinit();
}

TEST_CASE("Test APIs: multiple event subscribers", "[brookesia][service][api][event_multiple_subscribers]")
{
    BROOKESIA_LOGI("=== Test multiple event subscribers ===");

    TEST_ASSERT_TRUE(service_manager.init());
    TEST_ASSERT_TRUE(service_manager.start());

    auto binding = service_manager.bind("test_service");
    TEST_ASSERT_TRUE(binding.is_valid());

    auto service = std::dynamic_pointer_cast<TestService>(binding.get_service());
    TEST_ASSERT_NOT_NULL(service);

    // Subscribe to event (multiple subscribers)
    std::atomic<int> subscriber1_count{0};
    std::atomic<int> subscriber2_count{0};

    EventRegistry::SignalConnection connection1, connection2;

    connection1 = service->subscribe_event(
                      "value_changed",
    [&subscriber1_count](const std::string &, const EventItemMap &) {
        subscriber1_count++;
    });
    TEST_ASSERT_TRUE(connection1.connected());

    connection2 = service->subscribe_event(
                      "value_changed",
    [&subscriber2_count](const std::string &, const EventItemMap &) {
        subscriber2_count++;
    });
    TEST_ASSERT_TRUE(connection2.connected());

    // Publish event
    std::vector<EventItem> values;
    values.push_back(EventItem(100.0));
    service->test_publish_event("value_changed", std::move(values));

    // Wait for event processing
    vTaskDelay(pdMS_TO_TICKS(100));

    // Verify all subscribers received event
    TEST_ASSERT_EQUAL(1, subscriber1_count.load());
    TEST_ASSERT_EQUAL(1, subscriber2_count.load());

    service_manager.stop();
    service_manager.deinit();
}

TEST_CASE("Test APIs: publish event with json object", "[brookesia][service][api][event_json_object]")
{
    BROOKESIA_LOGI("=== Test publish event with json object ===");

    TEST_ASSERT_TRUE(service_manager.init());
    TEST_ASSERT_TRUE(service_manager.start());

    auto binding = service_manager.bind("test_service");
    TEST_ASSERT_TRUE(binding.is_valid());

    auto service = std::dynamic_pointer_cast<TestService>(binding.get_service());
    TEST_ASSERT_NOT_NULL(service);

    // Subscribe to event
    std::atomic<bool> event_received{false};
    std::atomic<double> received_value{0.0};

    EventRegistry::SignalConnection connection;
    connection = service->subscribe_event("value_changed",
    [&event_received, &received_value](const std::string & event_name, const EventItemMap & data) {
        BROOKESIA_LOGI("Event received: %1%", event_name);
        event_received = true;
        if (data.find("value") != data.end()) {
            received_value = std::get<double>(data.at("value"));
        }
    });
    TEST_ASSERT_TRUE(connection.connected());

    // Publish event using boost::json::object
    boost::json::object data_json;
    data_json["value"] = 88.5;
    bool published = service->test_publish_event("value_changed", std::move(data_json));
    TEST_ASSERT_TRUE(published);

    // Wait for event processing
    vTaskDelay(pdMS_TO_TICKS(100));

    // Verify event received
    TEST_ASSERT_TRUE(event_received.load());
    TEST_ASSERT_EQUAL_DOUBLE(88.5, received_value.load());

    service_manager.stop();
    service_manager.deinit();
}

TEST_CASE("Test APIs: publish event with data map", "[brookesia][service][api][event_event_items]")
{
    BROOKESIA_LOGI("=== Test publish event with data map ===");

    TEST_ASSERT_TRUE(service_manager.init());
    TEST_ASSERT_TRUE(service_manager.start());

    auto binding = service_manager.bind("test_service");
    TEST_ASSERT_TRUE(binding.is_valid());

    auto service = std::dynamic_pointer_cast<TestService>(binding.get_service());
    TEST_ASSERT_NOT_NULL(service);

    // Subscribe to event
    std::atomic<bool> event_received{false};
    std::string received_message;

    EventRegistry::SignalConnection connection;
    connection = service->subscribe_event("message_received",
    [&event_received, &received_message](const std::string & event_name, const EventItemMap & data) {
        BROOKESIA_LOGI("Event received: %1%", event_name);
        event_received = true;
        if (data.find("message") != data.end()) {
            received_message = std::get<std::string>(data.at("message"));
        }
    });
    TEST_ASSERT_TRUE(connection.connected());

    // Publish event using EventItemMap
    EventItemMap event_items;
    event_items["message"] = EventItem("EventItemMap Test");
    bool published = service->test_publish_event("message_received", std::move(event_items));
    TEST_ASSERT_TRUE(published);

    // Wait for event processing
    vTaskDelay(pdMS_TO_TICKS(100));

    // Verify event received
    TEST_ASSERT_TRUE(event_received.load());
    TEST_ASSERT_EQUAL_STRING("EventItemMap Test", received_message.c_str());

    service_manager.stop();
    service_manager.deinit();
}

TEST_CASE("Test APIs: publish event with json object - multiple fields", "[brookesia][service][api][event_json_multiple]")
{
    BROOKESIA_LOGI("=== Test publish event with json object - multiple fields ===");

    TEST_ASSERT_TRUE(service_manager.init());
    TEST_ASSERT_TRUE(service_manager.start());

    auto binding = service_manager.bind("test_service");
    TEST_ASSERT_TRUE(binding.is_valid());

    auto service = std::dynamic_pointer_cast<TestService>(binding.get_service());
    TEST_ASSERT_NOT_NULL(service);

    // Subscribe to event
    std::atomic<bool> event_received{false};
    std::atomic<double> received_value{0.0};

    EventRegistry::SignalConnection connection;
    connection = service->subscribe_event("value_changed",
    [&event_received, &received_value](const std::string & event_name, const EventItemMap & data) {
        BROOKESIA_LOGI("Event received: %1% with %2% fields", event_name, data.size());
        event_received = true;
        if (data.find("value") != data.end()) {
            received_value = std::get<double>(data.at("value"));
        }
    });
    TEST_ASSERT_TRUE(connection.connected());

    // Publish event using boost::json::object (contains multiple fields, but event schema only needs value)
    boost::json::object data_json;
    data_json["value"] = 123.45;
    data_json["extra_field"] = "should be ignored by validation";  // Extra field
    bool published = service->test_publish_event("value_changed", std::move(data_json));
    TEST_ASSERT_TRUE(published);

    // Wait for event processing
    vTaskDelay(pdMS_TO_TICKS(100));

    // Verify event received
    TEST_ASSERT_TRUE(event_received.load());
    TEST_ASSERT_EQUAL_DOUBLE(123.45, received_value.load());

    service_manager.stop();
    service_manager.deinit();
}

// ============================================================================
// State query testing
// ============================================================================

TEST_CASE("Test APIs: is_initialized", "[brookesia][service][api][is_initialized]")
{
    BROOKESIA_LOGI("=== Test is_initialized ===");

    TEST_ASSERT_FALSE(service_manager.is_initialized());

    TEST_ASSERT_TRUE(service_manager.init());
    TEST_ASSERT_TRUE(service_manager.is_initialized());

    service_manager.deinit();
    TEST_ASSERT_FALSE(service_manager.is_initialized());
}

TEST_CASE("Test APIs: is_running", "[brookesia][service][api][is_running]")
{
    BROOKESIA_LOGI("=== Test is_running ===");

    TEST_ASSERT_TRUE(service_manager.init());
    TEST_ASSERT_FALSE(service_manager.is_running());

    TEST_ASSERT_TRUE(service_manager.start());
    TEST_ASSERT_TRUE(service_manager.is_running());

    service_manager.stop();
    TEST_ASSERT_FALSE(service_manager.is_running());

    service_manager.deinit();
}

// ============================================================================
// Boundary condition testing
// ============================================================================

TEST_CASE("Test APIs: bind before init", "[brookesia][service][api][bind_before_init]")
{
    BROOKESIA_LOGI("=== Test bind before init ===");

    // Bind service before init should fail
    auto binding = service_manager.bind("test_service");
    TEST_ASSERT_FALSE(binding.is_valid());
}

TEST_CASE("Test APIs: bind with empty name", "[brookesia][service][api][bind_empty_name]")
{
    BROOKESIA_LOGI("=== Test bind with empty name ===");

    TEST_ASSERT_TRUE(service_manager.init());
    TEST_ASSERT_TRUE(service_manager.start());

    auto binding = service_manager.bind("");
    TEST_ASSERT_FALSE(binding.is_valid());

    service_manager.stop();
    service_manager.deinit();
}

// ============================================================================
// Concurrent testing
// ============================================================================

TEST_CASE("Test APIs: concurrent bind services", "[brookesia][service][api][concurrent_bind]")
{
    BROOKESIA_LOGI("=== Test concurrent bind services ===");

    TEST_ASSERT_TRUE(service_manager.init());
    TEST_ASSERT_TRUE(service_manager.start());

    // Concurrent bind the same service (share the same instance)
    const int thread_count = 4;
    std::atomic<int> bind_success_count{0};
    boost::thread_group threads;

    for (int i = 0; i < thread_count; i++) {
        BROOKESIA_THREAD_CONFIG_GUARD({});
        threads.create_thread([&bind_success_count]() {
            auto binding = service_manager.bind("test_service");
            if (binding.is_valid()) {
                bind_success_count++;
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        });
    }

    threads.join_all();

    BROOKESIA_LOGI("Bind success count: %1%", bind_success_count.load());
    TEST_ASSERT_EQUAL(thread_count, bind_success_count.load());

    service_manager.stop();
    service_manager.deinit();
}

// ============================================================================
// Async function call testing
// ============================================================================

TEST_CASE("Test APIs: async call function - add", "[brookesia][service][api][call_function_async_add]")
{
    BROOKESIA_LOGI("=== Test async call function - add ===");

    TEST_ASSERT_TRUE(service_manager.init());
    TEST_ASSERT_TRUE(service_manager.start());

    auto binding = service_manager.bind("test_service");
    TEST_ASSERT_TRUE(binding.is_valid());

    auto service = binding.get_service();
    TEST_ASSERT_NOT_NULL(service);

    // Call add function asynchronously
    FunctionParameterMap args;
    args["a"] = FunctionValue(10.0);
    args["b"] = FunctionValue(20.0);

    auto future = service->call_function_async("add", std::move(args));

    // Wait for result
    auto result = future.get();
    TEST_ASSERT_TRUE_MESSAGE(result.success, result.error_message.c_str());
    TEST_ASSERT_TRUE(result.has_data());
    TEST_ASSERT_EQUAL_DOUBLE(30.0, std::get<double>(*result.data));

    service_manager.stop();
    service_manager.deinit();
}

TEST_CASE("Test APIs: async call function - echo", "[brookesia][service][api][call_function_async_echo]")
{
    BROOKESIA_LOGI("=== Test async call function - echo ===");

    TEST_ASSERT_TRUE(service_manager.init());
    TEST_ASSERT_TRUE(service_manager.start());

    auto binding = service_manager.bind("test_service");
    TEST_ASSERT_TRUE(binding.is_valid());

    auto service = binding.get_service();
    TEST_ASSERT_NOT_NULL(service);

    // Call echo function asynchronously
    FunctionParameterMap args;
    args["message"] = FunctionValue("Async Hello!");

    auto future = service->call_function_async("echo", std::move(args));

    // Wait for result
    auto result = future.get();
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_TRUE(result.has_data());
    TEST_ASSERT_EQUAL_STRING("Async Hello!", std::get<std::string>(*result.data).c_str());

    service_manager.stop();
    service_manager.deinit();
}

TEST_CASE("Test APIs: async call function with vector", "[brookesia][service][api][call_function_async_vector]")
{
    BROOKESIA_LOGI("=== Test async call function with vector ===");

    TEST_ASSERT_TRUE(service_manager.init());
    TEST_ASSERT_TRUE(service_manager.start());

    auto binding = service_manager.bind("test_service");
    TEST_ASSERT_TRUE(binding.is_valid());

    auto service = binding.get_service();
    TEST_ASSERT_NOT_NULL(service);

    // Call add function using std::vector<FunctionValue>
    std::vector<FunctionValue> args_vector;
    args_vector.push_back(FunctionValue(15.0));  // a
    args_vector.push_back(FunctionValue(25.0));  // b

    auto future = service->call_function_async("add", std::move(args_vector));

    // Wait for result
    auto result = future.get();
    TEST_ASSERT_TRUE_MESSAGE(result.success, result.error_message.c_str());
    TEST_ASSERT_TRUE(result.has_data());
    TEST_ASSERT_EQUAL_DOUBLE(40.0, std::get<double>(*result.data));

    service_manager.stop();
    service_manager.deinit();
}

TEST_CASE("Test APIs: async call function with json object", "[brookesia][service][api][call_function_async_json]")
{
    BROOKESIA_LOGI("=== Test async call function with json object ===");

    TEST_ASSERT_TRUE(service_manager.init());
    TEST_ASSERT_TRUE(service_manager.start());

    auto binding = service_manager.bind("test_service");
    TEST_ASSERT_TRUE(binding.is_valid());

    auto service = binding.get_service();
    TEST_ASSERT_NOT_NULL(service);

    // Call add function using boost::json::object
    boost::json::object args_json;
    args_json["a"] = 100.0;
    args_json["b"] = 200.0;

    auto future = service->call_function_async("add", std::move(args_json));

    // Wait for result
    auto result = future.get();
    TEST_ASSERT_TRUE_MESSAGE(result.success, result.error_message.c_str());
    TEST_ASSERT_TRUE(result.has_data());
    TEST_ASSERT_EQUAL_DOUBLE(300.0, std::get<double>(*result.data));

    service_manager.stop();
    service_manager.deinit();
}

TEST_CASE("Test APIs: async call multiple functions concurrently", "[brookesia][service][api][call_function_async_concurrent]")
{
    BROOKESIA_LOGI("=== Test async call multiple functions concurrently ===");

    TEST_ASSERT_TRUE(service_manager.init());
    TEST_ASSERT_TRUE(service_manager.start());

    auto binding = service_manager.bind("test_service");
    TEST_ASSERT_TRUE(binding.is_valid());

    auto service = binding.get_service();
    TEST_ASSERT_NOT_NULL(service);

    // Launch multiple async calls
    std::vector<std::future<FunctionResult>> futures;

    for (int i = 0; i < 5; i++) {
        FunctionParameterMap args;
        args["a"] = FunctionValue(static_cast<double>(i));
        args["b"] = FunctionValue(static_cast<double>(i * 10));
        futures.push_back(service->call_function_async("add", std::move(args)));
    }

    // Collect results
    for (int i = 0; i < 5; i++) {
        auto result = futures[i].get();
        TEST_ASSERT_TRUE_MESSAGE(result.success, result.error_message.c_str());
        TEST_ASSERT_TRUE(result.has_data());
        double expected = static_cast<double>(i + i * 10);
        TEST_ASSERT_EQUAL_DOUBLE(expected, std::get<double>(*result.data));
    }

    service_manager.stop();
    service_manager.deinit();
}

TEST_CASE("Test APIs: async call with custom timeout check", "[brookesia][service][api][call_function_async_timeout]")
{
    BROOKESIA_LOGI("=== Test async call with custom timeout check ===");

    TEST_ASSERT_TRUE(service_manager.init());
    TEST_ASSERT_TRUE(service_manager.start());

    auto binding = service_manager.bind("test_service");
    TEST_ASSERT_TRUE(binding.is_valid());

    auto service = binding.get_service();
    TEST_ASSERT_NOT_NULL(service);

    // Call function asynchronously
    FunctionParameterMap args;
    args["message"] = FunctionValue("Custom timeout test");

    auto future = service->call_function_async("echo", std::move(args));

    // Custom timeout check (1 second)
    auto status = future.wait_for(std::chrono::seconds(1));
    TEST_ASSERT_EQUAL(std::future_status::ready, status);

    if (status == std::future_status::ready) {
        auto result = future.get();
        TEST_ASSERT_TRUE(result.success);
        TEST_ASSERT_TRUE(result.has_data());
        TEST_ASSERT_EQUAL_STRING("Custom timeout test", std::get<std::string>(*result.data).c_str());
    }

    service_manager.stop();
    service_manager.deinit();
}

TEST_CASE("Test APIs: async vs sync call comparison", "[brookesia][service][api][async_vs_sync]")
{
    BROOKESIA_LOGI("=== Test async vs sync call comparison ===");

    TEST_ASSERT_TRUE(service_manager.init());
    TEST_ASSERT_TRUE(service_manager.start());

    auto binding = service_manager.bind("test_service");
    TEST_ASSERT_TRUE(binding.is_valid());

    auto service = binding.get_service();
    TEST_ASSERT_NOT_NULL(service);

    // Async call
    FunctionParameterMap async_args;
    async_args["a"] = FunctionValue(50.0);
    async_args["b"] = FunctionValue(50.0);
    auto future = service->call_function_async("add", std::move(async_args));
    auto async_result = future.get();

    // Sync call (same parameters)
    FunctionParameterMap sync_args;
    sync_args["a"] = FunctionValue(50.0);
    sync_args["b"] = FunctionValue(50.0);
    auto sync_result = service->call_function_sync("add", std::move(sync_args), 100);

    // Results should be the same
    TEST_ASSERT_TRUE(async_result.success);
    TEST_ASSERT_TRUE(sync_result.success);
    TEST_ASSERT_EQUAL_DOUBLE(
        std::get<double>(*async_result.data),
        std::get<double>(*sync_result.data)
    );

    service_manager.stop();
    service_manager.deinit();
}

TEST_CASE("Test APIs: async call non-existent function", "[brookesia][service][api][call_function_async_not_exist]")
{
    BROOKESIA_LOGI("=== Test async call non-existent function ===");

    TEST_ASSERT_TRUE(service_manager.init());
    TEST_ASSERT_TRUE(service_manager.start());

    auto binding = service_manager.bind("test_service");
    TEST_ASSERT_TRUE(binding.is_valid());

    auto service = binding.get_service();
    TEST_ASSERT_NOT_NULL(service);

    // Call non-existent function asynchronously
    FunctionParameterMap args;
    auto future = service->call_function_async("non_existent", std::move(args));

    // Should get error result
    auto result = future.get();
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_FALSE(result.error_message.empty());
    BROOKESIA_LOGI("Expected error: %1%", result.error_message);

    service_manager.stop();
    service_manager.deinit();
}

TEST_CASE("Test APIs: async call with mixed sync and async", "[brookesia][service][api][mixed_sync_async]")
{
    BROOKESIA_LOGI("=== Test mixed sync and async calls ===");

    TEST_ASSERT_TRUE(service_manager.init());
    TEST_ASSERT_TRUE(service_manager.start());

    auto binding = service_manager.bind("test_service");
    TEST_ASSERT_TRUE(binding.is_valid());

    auto service = binding.get_service();
    TEST_ASSERT_NOT_NULL(service);

    // Launch async calls
    std::vector<std::future<FunctionResult>> futures;
    for (int i = 0; i < 3; i++) {
        FunctionParameterMap args;
        args["a"] = FunctionValue(static_cast<double>(i));
        args["b"] = FunctionValue(1.0);
        futures.push_back(service->call_function_async("add", std::move(args)));
    }

    // Do sync calls while async calls are running
    for (int i = 0; i < 3; i++) {
        FunctionParameterMap args;
        args["message"] = FunctionValue("Sync message " + std::to_string(i));
        auto sync_result = service->call_function_sync("echo", std::move(args), 100);
        TEST_ASSERT_TRUE(sync_result.success);
    }

    // Collect async results
    for (int i = 0; i < 3; i++) {
        auto result = futures[i].get();
        TEST_ASSERT_TRUE_MESSAGE(result.success, result.error_message.c_str());
        TEST_ASSERT_EQUAL_DOUBLE(static_cast<double>(i + 1), std::get<double>(*result.data));
    }

    service_manager.stop();
    service_manager.deinit();
}

TEST_CASE("Test APIs: async call before service running", "[brookesia][service][api][call_function_async_not_running]")
{
    BROOKESIA_LOGI("=== Test async call before service running ===");

    TEST_ASSERT_TRUE(service_manager.init());
    TEST_ASSERT_TRUE(service_manager.start());

    auto binding = service_manager.bind("test_service");
    TEST_ASSERT_TRUE(binding.is_valid());

    auto service = binding.get_service();
    TEST_ASSERT_NOT_NULL(service);

    // Stop the service
    binding.release();
    TEST_ASSERT_FALSE(service->is_running());

    // Try to call function when service is not running
    FunctionParameterMap args;
    args["a"] = FunctionValue(10.0);
    args["b"] = FunctionValue(20.0);

    auto future = service->call_function_async("add", std::move(args));

    // Should get error result immediately
    auto result = future.get();
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_FALSE(result.error_message.empty());
    BROOKESIA_LOGI("Expected error: %1%", result.error_message);

    service_manager.stop();
    service_manager.deinit();
}

// ============================================================================
// Stress testing
// ============================================================================

TEST_CASE("Test APIs: stress - rapid bind release", "[brookesia][service][api][stress_bind_release]")
{
    BROOKESIA_LOGI("=== Test stress - rapid bind release ===");

    TEST_ASSERT_TRUE(service_manager.init());
    TEST_ASSERT_TRUE(service_manager.start());

    const int iterations = 10;
    for (int i = 0; i < iterations; i++) {
        auto binding = service_manager.bind("test_service");
        TEST_ASSERT_TRUE(binding.is_valid());
        auto service = binding.get_service();
        TEST_ASSERT_TRUE(service->is_running());
        binding.release();
        TEST_ASSERT_FALSE(service->is_running());
    }

    service_manager.stop();
    service_manager.deinit();
}

TEST_CASE("Test APIs: stress - rapid async calls", "[brookesia][service][api][stress_async_calls]")
{
    BROOKESIA_LOGI("=== Test stress - rapid async calls ===");

    TEST_ASSERT_TRUE(service_manager.init());
    TEST_ASSERT_TRUE(service_manager.start());

    auto binding = service_manager.bind("test_service");
    TEST_ASSERT_TRUE(binding.is_valid());

    auto service = binding.get_service();
    TEST_ASSERT_NOT_NULL(service);

    const int iterations = 50;
    std::vector<std::future<FunctionResult>> futures;
    futures.reserve(iterations);

    // Launch many async calls
    for (int i = 0; i < iterations; i++) {
        FunctionParameterMap args;
        args["a"] = FunctionValue(static_cast<double>(i));
        args["b"] = FunctionValue(1.0);
        futures.push_back(service->call_function_async("add", std::move(args)));
    }

    // Collect all results
    int success_count = 0;
    for (int i = 0; i < iterations; i++) {
        auto result = futures[i].get();
        if (result.success) {
            success_count++;
            TEST_ASSERT_EQUAL_DOUBLE(static_cast<double>(i + 1), std::get<double>(*result.data));
        }
    }

    BROOKESIA_LOGI("Success count: %d/%d", success_count, iterations);
    TEST_ASSERT_EQUAL(iterations, success_count);

    service_manager.stop();
    service_manager.deinit();
}
