# ESP-Brookesia MCP Utilities Library

* [中文版本](./README_CN.md)

## Overview

`brookesia_mcp_utils` bridges Brookesia services and the [ESP MCP (Model Context Protocol) engine](https://docs.espressif.com/projects/esp-iot-solution/en/latest/ai/mcp.html). It lets you register existing service functions or custom callbacks as MCP tools and generate tool handles for use with the MCP engine.

## Table of Contents

- [ESP-Brookesia MCP Utilities Library](#esp-brookesia-mcp-utilities-library)
  - [Overview](#overview)
  - [Table of Contents](#table-of-contents)
  - [Features](#features)
  - [How to Use](#how-to-use)
    - [Development Environment](#development-environment)
    - [Add to Your Project](#add-to-your-project)

## Features

- **Service Tools**: Expose functions of registered Brookesia services as MCP tools; calls are forwarded to the corresponding service. Tool names follow the format `Service.<service_name>.<function_name>`.
- **Custom Tools**: Implement MCP tools with custom C++ callbacks, including parameter schema and optional context. Tool names follow the format `Custom.<function_name>`.
- **ToolRegistry**: Manages registration and removal of both tool types and produces `esp_mcp_tool_t*` arrays for the MCP engine.

## How to Use

### Development Environment

Before using this library, ensure the following SDK is installed:

- [ESP-IDF](https://github.com/espressif/esp-idf): `>=5.5,<6`

> [!NOTE]
> See [ESP-IDF Get Started - Installation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#get-started-how-to-get-esp-idf) for SDK installation.

### Add to Your Project

`brookesia_mcp_utils` is available on the [Espressif Component Registry](https://components.espressif.com/). You can add it to your project in either of these ways:

1. **Command line**

   From your project directory, run:

   ```bash
   idf.py add-dependency "espressif/brookesia_mcp_utils"
   ```

2. **Configuration file**

   Create or edit *idf_component.yml* in your project directory:

   ```yaml
   dependencies:
     espressif/brookesia_mcp_utils: "*"
   ```

For more details, see [Espressif Documentation - IDF Component Manager](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html).
