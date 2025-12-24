# 使用示例

* [English Version](./Usage.md)

## 目录

- [使用示例](#使用示例)
  - [目录](#目录)
  - [本地服务](#本地服务)
    - [基础用法](#基础用法)
    - [同步函数调用](#同步函数调用)
    - [异步函数调用](#异步函数调用)
    - [并发函数调用](#并发函数调用)
    - [多种参数格式](#多种参数格式)
    - [事件发布/订阅](#事件发布订阅)
  - [远程服务](#远程服务)
    - [RPC 服务器](#rpc-服务器)
    - [RPC 客户端](#rpc-客户端)
  - [创建自定义服务](#创建自定义服务)

## 本地服务

### 基础用法

```cpp
#include "brookesia/lib_utils.hpp"
#include "brookesia/service_manager.hpp"

using namespace esp_brookesia::service;

void app_main()
{
    auto &service_manager = ServiceManager::get_instance();

    // 1. 初始化并启动服务管理器
    if (!service_manager.start()) {
        BROOKESIA_LOGE("Failed to start service manager");
        return;
    }

    // 2. 绑定服务（首次绑定时自动启动目标服务，多个绑定共享同一服务实例）
    const std::string service_name = "test_service";
    auto binding = service_manager.bind(service_name);
    if (!binding.is_valid()) {
        BROOKESIA_LOGE("Failed to bind service");
        return;
    }

    auto service = binding.get_service();

    // ... 使用服务 ...

    // 3. 解除绑定
    // binding 在超出作用域时自动解除绑定
    // 或者使用 release() 手动解除绑定
    binding.release();

    // 4. 停止服务管理器
    service_manager.stop();
}
```

### 同步函数调用

```cpp
// 通过绑定获取服务
auto binding = service_manager.bind("test_service");
auto service = binding.get_service();

// 调用函数并设置超时（阻塞）
FunctionParameterMap args;
args["a"] = FunctionValue(10.0);
args["b"] = FunctionValue(20.0);

auto result = service->call_function_sync("add", std::move(args), 100);  // 100ms 超时
if (result.success) {
    double value = std::get<double>(*result.data);
    BROOKESIA_LOGI("Result: %1%", value);
} else {
    BROOKESIA_LOGE("Error: %1%", result.error_message.c_str());
}
```

### 异步函数调用

```cpp
// 通过绑定获取服务
auto binding = service_manager.bind("test_service");
auto service = binding.get_service();

// 启动异步调用（立即返回）
FunctionParameterMap args;
args["a"] = FunctionValue(10.0);
args["b"] = FunctionValue(20.0);

auto future = service->call_function_async("add", std::move(args));

// 在函数执行时做其他工作...
do_other_work();

// 需要结果时等待
auto result = future.get();  // 阻塞直到结果准备好
if (result.success) {
    double value = std::get<double>(*result.data);
    BROOKESIA_LOGI("Result: %1%", value);
}
```

### 并发函数调用

```cpp
// 通过绑定获取服务
auto binding = service_manager.bind("test_service");
auto service = binding.get_service();

// 并发启动多个异步调用
std::vector<std::future<FunctionResult>> futures;

for (int i = 0; i < 5; i++) {
    FunctionParameterMap args;
    args["a"] = FunctionValue(static_cast<double>(i));
    args["b"] = FunctionValue(1.0);
    futures.push_back(service->call_function_async("add", std::move(args)));
}

// 收集结果（并行执行，非顺序执行）
for (auto &future : futures) {
    auto result = future.get();
    if (result.success) {
        double value = std::get<double>(*result.data);
        BROOKESIA_LOGI("Result: %1%", value);
    }
}
```

### 多种参数格式

```cpp
// 通过绑定获取服务
auto binding = service_manager.bind("test_service");
auto service = binding.get_service();

// 1. 使用 std::vector
std::vector<FunctionValue> args_vector;
args_vector.push_back(FunctionValue(15.0));  // 第一个参数
args_vector.push_back(FunctionValue(25.0));  // 第二个参数
auto result = service->call_function_sync("add", std::move(args_vector), 100);

// 2. 使用 JSON 对象
boost::json::object args_json;
args_json["a"] = 15.0;
args_json["b"] = 25.0;
auto result = service->call_function_sync("add", std::move(args_json), 100);

// 异步调用也支持相同的格式
auto future = service->call_function_async("add", std::move(args_json));
```

### 事件发布/订阅

* 发布服务事件

```cpp
// publish_event 是 ServiceBase 的 protected 成员函数，因此需要在子类成员函数中调用
{
    // 发送事件：以 key-value 形式发送事件数据
    publish_event("value_changed", {{"value", 100.0}});

    // 发送事件：以 ordered array 形式发送事件数据
    publish_event("value_changed", {100.0});

    // 发送事件：以 JSON 对象形式发送事件数据
    publish_event("value_changed", boost::json::object{{"value", 100.0}});
}
```

* 订阅服务事件

```cpp
// 通过绑定获取服务
auto binding = service_manager.bind("test_service");
auto service = binding.get_service();

// 订阅服务事件
auto connection = service->subscribe_event(
    "value_changed",
    [](const std::string &event_name, const EventItemMap &data) {
        if (data.find("value") != data.end()) {
            double value = std::get<double>(data.at("value"));
            BROOKESIA_LOGI("Event received: %1%, value = %2%", event_name.c_str(), value);
        }
    }
);

// 检查订阅是否成功
if (connection.connected()) {
    BROOKESIA_LOGI("Successfully subscribed to event");
}

// ... 应用逻辑 ...

// 连接在超出作用域时自动断开
// 或者使用 std::move 转移所有权以延长生命周期
auto new_connection = std::move(connection);
```

## 远程服务

远程服务通过 RPC 机制实现跨设备或跨语言的服务调用，使用 TCP Socket 和 JSON 协议进行通信，需要确保服务器和客户端在同一局域网内。

### RPC 服务器

```cpp
auto &service_manager = ServiceManager::get_instance();

// 1. 初始化并启动服务管理器
if (!service_manager.start()) {
    BROOKESIA_LOGE("Failed to start service manager");
    return;
}

// 2. 启动 RPC 服务器（使用默认配置：端口 65500，最大连接数 2）
if (!service_manager.start_rpc_server()) {
    BROOKESIA_LOGE("Failed to start RPC server");
    return;
}

// 3. 将 RPC 服务器连接到服务（空列表表示连接所有服务）
if (!service_manager.connect_rpc_server_to_services({"test_service"})) {
    BROOKESIA_LOGE("Failed to connect RPC server to services");
    return;
}

// ... 使用 RPC 服务器 ...

// 4. 清理
service_manager.disconnect_rpc_server_from_services();
service_manager.stop_rpc_server();
```

### RPC 客户端

* 单次调用（无需创建客户端）

```cpp
auto &service_manager = ServiceManager::get_instance();

// 直接调用远程函数（内部会自动创建和销毁客户端）
boost::json::object params;
params["a"] = 10.0;
params["b"] = 20.0;

auto result = service_manager.call_rpc_function_sync(
    "192.168.1.100",      // host
    "test_service",        // service_name
    "add",                 // function_name
    std::move(params),     // params
    2000,                  // timeout_ms (可选，默认使用配置值)
    65500                  // port (可选，默认使用配置值)
);

if (result.success) {
    double value = std::get<double>(*result.data);
    BROOKESIA_LOGI("Remote call result: %1%", value);
} else {
    BROOKESIA_LOGE("Remote call failed: %1%", result.error_message.c_str());
}
```

* 创建并管理 RPC 客户端

```cpp
auto &service_manager = ServiceManager::get_instance();

// 1. 创建 RPC 客户端（使用默认配置）
auto client = service_manager.new_rpc_client();
if (!client) {
    BROOKESIA_LOGE("Failed to create RPC client");
    return;
}

// 2. 连接到 RPC 服务器（假设 RPC 服务器地址为 192.168.1.100，端口为 65500）
if (!client->connect("192.168.1.100", 65500, 2000)) {  // host, port, timeout_ms
    BROOKESIA_LOGE("Failed to connect to RPC server");
    return;
}

// 3. 检查连接状态
if (client->is_connected()) {
    BROOKESIA_LOGI("RPC client connected successfully");
}

// ... 使用客户端调用远程函数或订阅事件 ...

// 4. 断开连接
client->disconnect();
```

* 同步远程函数调用

```cpp
// 通过 RPC 客户端调用远程函数
boost::json::object params;
params["a"] = 10.0;
params["b"] = 20.0;

auto result = client->call_function_sync("test_service", "add", std::move(params), 2000);
// 参数：service_name, function_name, params, timeout_ms

if (result.success) {
    double value = std::get<double>(*result.data);
    BROOKESIA_LOGI("Remote call result: %1%", value);
} else {
    BROOKESIA_LOGE("Remote call failed: %1%", result.error_message.c_str());
}
```

* 异步远程函数调用

```cpp
// 启动异步调用（立即返回）
boost::json::object params;
params["a"] = 15.0;
params["b"] = 25.0;

auto future = client->call_function_async("test_service", "add", std::move(params));
// 参数：service_name, function_name, params

// 在函数执行时做其他工作...
do_other_work();

// 需要结果时等待
auto result = future.get();  // 阻塞直到结果准备好
if (result.success) {
    double value = std::get<double>(*result.data);
    BROOKESIA_LOGI("Remote call result: %1%", value);
}
```

* 并发远程函数调用

```cpp
// 并发启动多个异步调用
std::vector<std::future<FunctionResult>> futures;

for (int i = 0; i < 5; i++) {
    boost::json::object params;
    params["a"] = static_cast<double>(i);
    params["b"] = 1.0;
    futures.push_back(client->call_function_async("test_service", "add", std::move(params)));
}

// 收集结果（并行执行，非顺序执行）
for (auto &future : futures) {
    auto result = future.get();
    if (result.success) {
        double value = std::get<double>(*result.data);
        BROOKESIA_LOGI("Remote call result: %1%", value);
    }
}
```

* 远程事件订阅

```cpp
// 订阅远程服务事件
std::string subscription_id = client->subscribe_event(
    "test_service",  // service_name
    "value_changed", // event_name
    [](const EventDispatcher::EventItemMap &data) {
        if (data.find("value") != data.end()) {
            double value = std::get<double>(data.at("value"));
            BROOKESIA_LOGI("Remote event received: value = %1%", value);
        }
    },
    2000  // timeout_ms
);

if (subscription_id.empty()) {
    BROOKESIA_LOGE("Failed to subscribe to remote event");
} else {
    BROOKESIA_LOGI("Successfully subscribed to remote event, subscription_id: %1%", subscription_id.c_str());
}

// ... 应用逻辑 ...

// 取消订阅
if (!subscription_id.empty()) {
    if (client->unsubscribe_events("test_service", {subscription_id}, 2000)) {
        BROOKESIA_LOGI("Successfully unsubscribed from remote event");
    }
}
```

## 创建自定义服务

要创建自定义服务，请继承 `ServiceBase` 实现所需的虚函数，并在 `CMakeLists.txt` 的 `idf_component_register()` 中添加 `WHOLE_ARCHIVE` 选项以确保服务被正确链接：

* **service_test.hpp**

```cpp
#include "brookesia/service_manager/service/base.hpp"

namespace esp_brookesia::service {

class ServiceTest : public ServiceBase {
public:
    enum FunctionIndex {
        FunctionIndexAdd,
        FunctionIndexDivide,
        FunctionIndexMax,
    };

    enum EventIndex {
        EventIndexValueChange,
        EventIndexMax,
    };

    static constexpr const char *SERVICE_NAME = "service_test";
    inline static const FunctionSchema FUNCTION_SCHEMAS[FunctionIndexMax] = {
        [FunctionIndexAdd] = {
            "add",
            "Add two numbers together",
            {
                {"a", "First number", FunctionValueType::Number},
                {"b", "Second number", FunctionValueType::Number}
            }
        },
        [FunctionIndexDivide] = {
            "divide",
            "Divide two numbers",
            {
                {"a", "First number", FunctionValueType::Number},
                {"b", "Second number", FunctionValueType::Number}
            }
        },
    };
    inline static const EventSchema EVENT_SCHEMAS[EventIndexMax] = {
        [EventIndexValueChange] = {
            "value_change",
            "Value change event",
            {
                {"value", "", EventItemType::Number}
            }
        },
    };

    ServiceTest()
        : ServiceBase({
        .name = SERVICE_NAME,
        // .dependencies = {}, // 可选，依赖的服务列表,会按照顺序依次启动依赖的服务
        // .task_scheduler_config = {} // 可选，任务调度器配置，
                                       // 配置后会自动将服务请求任务调度到该调度器中执行，
                                       // 否则会使用 ServiceManager 的调度器执行服务请求任务
    })
    {}
    ~ServiceTest() = default;

private:
    bool on_init() override;
    void on_deinit() override;
    bool on_start() override;
    void on_stop() override;

    // 函数实现
    std::expected<double, std::string> function_add(double a, double b);
    std::expected<double, std::string> function_divide(double a, double b);

    // 提供函数定义
    std::vector<FunctionSchema> get_function_schemas() override
    {
        return std::vector<FunctionSchema>(FUNCTION_SCHEMAS, FUNCTION_SCHEMAS + FunctionIndexMax);
    }
    // 提供事件定义
    std::vector<EventSchema> get_event_schemas() override
    {
        return std::vector<EventSchema>(EVENT_SCHEMAS, EVENT_SCHEMAS + EventIndexMax);
    }
    // 提供函数处理句柄
    ServiceBase::FunctionHandlerMap get_function_handlers() override
    {
        return {
            BROOKESIA_SERVICE_FUNC_HANDLER_2(
                FUNCTION_SCHEMAS[FunctionIndexAdd].name,
                FUNCTION_SCHEMAS[FunctionIndexAdd].parameters[0].name, double,
                FUNCTION_SCHEMAS[FunctionIndexAdd].parameters[1].name, double,
                function_add(PARAM1, PARAM2)
            ),
            BROOKESIA_SERVICE_FUNC_HANDLER_2(
                FUNCTION_SCHEMAS[FunctionIndexDivide].name,
                FUNCTION_SCHEMAS[FunctionIndexDivide].parameters[0].name, double,
                FUNCTION_SCHEMAS[FunctionIndexDivide].parameters[1].name, double,
                function_divide(PARAM1, PARAM2)
            ),
        };
    }
};

} // namespace esp_brookesia::service
```

* **service_test.cpp**

```cpp
#include "service_test.hpp"

namespace esp_brookesia::service {

bool ServiceTest::on_init()
{
    // ...
    return true;
}

void ServiceTest::on_deinit()
{
    // ...
}

bool ServiceTest::on_start()
{
    // ...
    return true;
}

void ServiceTest::on_stop()
{
    // ...
}

std::expected<double, std::string> ServiceTest::function_add(double a, double b)
{
    return a + b;
}

std::expected<double, std::string> ServiceTest::function_divide(double a, double b)
{
    if (b == 0) {
        return std::unexpected("Division by zero");
    }
    return a / b;
}

// 利用插件机制注册服务
BROOKESIA_PLUGIN_REGISTER(ServiceBase, MyService, "my_service");

// 如果服务子类采用单例模式，可以使用 `BROOKESIA_PLUGIN_REGISTER_SINGLETON` 宏注册服务
// BROOKESIA_PLUGIN_REGISTER_SINGLETON(ServiceBase, MyService, "my_service", MyService::get_instance());

} // namespace esp_brookesia::service

```

* **CMakeLists.txt**
```cmake
idf_component_register(
    SRCS "service_test.cpp"
    INCLUDE_DIRS "."
    WHOLE_ARCHIVE
)
```

>[!NOTE]
> * 自定义服务示例请参考 [service_test.hpp](test_apps/main/service_test.hpp)、[service_test.cpp](test_apps/main/service_test.cpp) 或其他服务组件。
> * 需要在服务组件 *CMakeLists.txt* 的 `idf_component_register` 中添加 `WHOLE_ARCHIVE` 选项以确保服务被正确链接，否则会导致服务无法被正确链接。
