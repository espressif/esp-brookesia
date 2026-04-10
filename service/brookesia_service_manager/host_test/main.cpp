/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <variant>
#include "brookesia/service_manager.hpp"

using namespace esp_brookesia::service;

namespace {

class EchoService : public ServiceBase {
public:
    EchoService()
        : ServiceBase(Attributes{.name = "echo"})
    {
    }

    std::vector<FunctionSchema> get_function_schemas() override
    {
        return {{
                .name = "echo",
                .description = "Return the input string",
                .parameters = {{
                        .name = "message",
                        .description = "Message to echo",
                        .type = FunctionValueType::String,
                    }
                },
                .require_scheduler = true,
            }};
    }

protected:
    FunctionHandlerMap get_function_handlers() override
    {
        return {{
                "echo",
                [](FunctionParameterMap &&args) -> FunctionResult {
                    auto it = args.find("message");
                    if (it == args.end())
                    {
                        return FunctionResult{.success = false, .error_message = "missing message"};
                    }
                    return FunctionResult{.success = true, .data = it->second};
                },
            }};
    }
};

bool verify_echo_call(ServiceBase &service)
{
    auto result = service.call_function_sync(
    "echo", FunctionParameterMap{{"message", FunctionValue(std::string("host-ok"))}}, 500
                  );

    if (!result.success || !result.has_data()) {
        std::cerr << "Echo function failed" << std::endl;
        return false;
    }

    if (!std::holds_alternative<std::string>(*result.data)) {
        std::cerr << "Echo result type mismatch" << std::endl;
        return false;
    }

    if (std::get<std::string>(*result.data) != "host-ok") {
        std::cerr << "Echo result content mismatch" << std::endl;
        return false;
    }

    return true;
}

} // namespace

int main()
{
    auto &manager = ServiceManager::get_instance();

    if (!manager.start()) {
        std::cerr << "Failed to start service manager" << std::endl;
        return EXIT_FAILURE;
    }

    auto service = std::make_shared<EchoService>();
    if (!manager.add_service(service)) {
        std::cerr << "Failed to add test service" << std::endl;
        return EXIT_FAILURE;
    }

    auto binding = manager.bind("echo");
    if (!binding.is_valid()) {
        std::cerr << "Failed to bind test service" << std::endl;
        return EXIT_FAILURE;
    }

    auto bound_service = binding.get_service();
    if (!bound_service || !verify_echo_call(*bound_service)) {
        return EXIT_FAILURE;
    }

    if (!manager.start_rpc_server()) {
        std::cerr << "Failed to start RPC server" << std::endl;
        return EXIT_FAILURE;
    }

    manager.stop_rpc_server();
    manager.stop();
    manager.deinit();

    std::cout << "brookesia_service_manager host smoke test passed" << std::endl;
    return EXIT_SUCCESS;
}
