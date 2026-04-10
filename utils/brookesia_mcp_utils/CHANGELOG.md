# ChangeLog

## v0.7.0 - 2026-04-10

### Initial Release

- feat(repo): add `brookesia_mcp_utils` component bridging Brookesia services and the ESP MCP engine
- feat(registry): add `ToolRegistry` class for registering and materializing MCP tools
- feat(registry): `add_service_tools(service_name, function_names)` — register existing service functions as MCP tools; tool names are prefixed `Service.<service_name>.<function_name>`
- feat(registry): `add_custom_tool(CustomTool)` — register a custom callback-backed tool; tool names are prefixed `Custom.<schema_name>`
- feat(registry): `remove_tool(name)` / `remove_all_tools()` — unregister tools by name or bulk
- feat(registry): `generate_tools()` — produce `esp_mcp_tool_t *` descriptors for injection into the MCP engine
- feat(types): add `ServiceTool` struct (service + function name) and `CustomTool` struct (schema + callback + context)
- feat(types): add `CustomToolCallback` type alias (`FunctionResult(*)(FunctionParameterMap&&)`)
- feat(types): add `CUSTOM_TOOL_PARAMETER_CONTEXT_NAME` constant for context parameter forwarding
- feat(deps): depend on `espressif/mcp-c-sdk 1.*` (public) and `espressif/brookesia_service_manager 0.7.*` (public)
