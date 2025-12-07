# ESP-Brookesia Service Helper

## Overview

`brookesia_service_helper` is an ESP-Brookesia service development helper library that provides type-safe definitions, schemas, and utilities for service developers and users to build and use services that comply with the ESP-Brookesia service framework specifications.

This library mainly provides:

- **Type-Safe Definitions**: Provides type definitions such as enums and structs to ensure type safety
- **Function Schema Definitions**: Provides standardized function definitions (FunctionSchema), including function names, parameters, descriptions, etc.
- **Event Schema Definitions**: Provides standardized event definitions (EventSchema), including event names, data formats, etc.
- **Constant Definitions**: Provides constants such as service names and default values
- **Index Enumerations**: Provides enumerations such as function indices and event indices for easy use in service implementations

## Features

### Helper Classes

`brookesia_service_helper` currently provides the following helper classes:

- [NVS](./include/brookesia/service_helper/nvs.hpp)
- [Wifi](./include/brookesia/service_helper/wifi.hpp)

### Type Safety

All definitions use strongly-typed enums and structs to ensure:

- **Compile-Time Type Checking**: Avoids string spelling errors
- **IDE Auto-Completion**: Provides better development experience
- **Code Maintainability**: Unified interface definitions for easier maintenance and extension

### Standardized Interfaces

By providing standardized function and event definitions, it ensures:

- **Interface Consistency**: All services follow the same interface specifications
- **Automatic Documentation Generation**: Function and event descriptions can be used for automatic documentation generation
- **Parameter Validation**: Unified parameter types and validation rules

## Development Environment Requirements

Before using this library, please ensure the following SDK development environment is installed:

- [ESP-IDF](https://github.com/espressif/esp-idf): `>=5.5,<6`

> [!NOTE]
> For SDK installation instructions, please refer to [ESP-IDF Programming Guide - Installation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#get-started-how-to-get-esp-idf)

`brookesia_service_helper` has the following dependencies:

| **Dependency** | **Version Requirement** |
|---------------|-------------------------|
| [brookesia_service_manager](https://components.espressif.com/components/espressif/brookesia_service_manager) | 0.7.* |
| [brookesia_lib_utils](https://components.espressif.com/components/espressif/brookesia_lib_utils) | 0.7.* |

## Adding to Project

`brookesia_service_helper` has been uploaded to the [Espressif Component Registry](https://components.espressif.com/). You can add it to your project in the following ways:

1. **Using Command Line**

   Run the following command in your project directory:

   ```bash
   idf.py add-dependency "espressif/brookesia_service_helper"
   ```

2. **Modify Configuration File**

   Create or modify the *idf_component.yml* file in your project directory:

   ```yaml
   dependencies:
     espressif/brookesia_service_helper: "*"
   ```

For detailed instructions, please refer to [Espressif Documentation - IDF Component Manager](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html).

