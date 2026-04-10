# ChangeLog

## v0.7.1 - 2026-04-10

### Breaking Changes:

- break(manager): rename `FunctionId::ActivateAgent` to `FunctionId::SetTargetAgent`; updated schema description from "activate an agent" to "set target agent, activated by triggering `Activate` action"
- break(manager): rename `FunctionActivateAgentParam` to `FunctionSetTargetAgentParam`
- break(manager): reorder `FunctionId` enum values to group related operations together

### Enhancements:

- feat(manager): add `FunctionId::GetAgentNames` to retrieve available agent name list
- feat(manager): add `FunctionId::GetTargetAgent` to query currently targeted agent name
- feat(manager): include `TimeSync` in allowed values for `TriggerGeneralAction` and `GeneralActionTriggered` event
- feat(manager): enrich `SetAgentInfo` schema with JSON object example for the `Info` parameter
- feat(xiaozhi): add `AddMCP_ToolsWithServiceFunction` function to register service functions as MCP tools
- feat(xiaozhi): add `AddMCP_ToolsWithCustomFunction` function to register custom MCP tool definitions
- feat(xiaozhi): add `RemoveMCP_Tools` function to unregister MCP tools by name
- feat(xiaozhi): add `ExplainImage` function for image captioning via the XiaoZhi backend
- feat(xiaozhi): add `#include "brookesia/service_manager/function/definition.hpp"` for `FunctionValueType::RawBuffer` support
- feat(docs): add Doxygen documentation to all public types, methods, and structs across all helper headers
- feat(build): add `uint8_t` underlying type to all `FunctionId`, `EventId`, and parameter enum classes
- feat(test): add `host_test/` scaffolding with CMakeLists.txt and schema-dump `main.cpp` covering all four agent helpers
- feat(schema): update function and event description strings to a concise LLM-friendly format (return type, allowed values, examples)

## v0.7.0 - 2026-02-02

### Initial Release

- feat(repo): add agent helper library for ESP-Brookesia ecosystem
- feat(manager): add Manager helper with type-safe function and event definitions for agent management
- feat(coze): add Coze helper with authentication, robot info, and event schemas
- feat(openai): add OpenAI helper with model configuration and session management schemas
- feat(xiaozhi): add Xiaozhi helper with OTA configuration and event schemas
- feat(repo): provide unified calling interfaces based on C++20 Concepts and CRTP pattern
- feat(repo): integrate with `brookesia_service_helper` for shared base functionality
