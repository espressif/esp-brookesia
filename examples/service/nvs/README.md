# NVS Service Example

[中文版本](./README_CN.md)

This example demonstrates how to use the ESP-Brookesia NVS (Non-Volatile Storage) service for data storage and retrieval operations. The example showcases basic operations, type-safe operations, namespace management, and error handling.

## 📑 Table of Contents

- [NVS Service Example](#nvs-service-example)
  - [📑 Table of Contents](#-table-of-contents)
  - [✨ Features](#-features)
  - [🚩 Quick Start](#-quick-start)
    - [Hardware Requirements](#hardware-requirements)
    - [Development Environment](#development-environment)
  - [🔨 How to Use](#-how-to-use)
  - [🚀 Quick Experience](#-quick-experience)
  - [📖 Example Description](#-example-description)
    - [Basic Operations Demo](#basic-operations-demo)
    - [Type-Safe Operations Demo](#type-safe-operations-demo)
    - [Namespace Management Demo](#namespace-management-demo)
    - [Error Handling Demo](#error-handling-demo)
  - [🔍 Troubleshooting](#-troubleshooting)
    - [NVS Initialization Failed](#nvs-initialization-failed)
    - [Data Read Failed](#data-read-failed)
    - [Value Verification Failed](#value-verification-failed)
  - [💬 Support and Feedback](#-support-and-feedback)

## ✨ Features

- 💾 **Basic Operations**: Support for setting (`Set`), getting (`Get`), listing (`List`), and erasing (`Erase`) key-value pairs
- 🔒 **Type Safety**: Provides type-safe APIs with automatic serialization/deserialization for various data types
- 📦 **Multiple Data Types**: Supports basic types (`bool`, `int32_t`, `uint8_t`, `int16_t`) and complex types (`string`, `int64_t`, `uint64_t`, `float`, `double`, `vector`, complex structs, etc.)
- 🗂️ **Namespace Management**: Supports multiple namespaces for data isolation and management
- ✅ **Value Verification**: Automatically verifies that read values match written values to ensure data integrity
- ⚠️ **Error Handling**: Demonstrates how to handle error cases such as non-existent keys and type mismatches

## 🚩 Quick Start

### Hardware Requirements

Development board with `ESP32-S3`, `ESP32-P4`, or `ESP32-C5` chip and `Flash >= 4MB`

### Development Environment

Please refer to the following documentation:

- [ESP-Brookesia Programming Guide - Versioning](https://docs.espressif.com/projects/esp-brookesia/en/latest/getting_started.html#getting-started-versioning)
- [ESP-Brookesia Programming Guide - Development Environment Setup](https://docs.espressif.com/projects/esp-brookesia/en/latest/getting_started.html#getting-started-dev-environment)

## 🔨 How to Use

Please refer to [ESP-Brookesia Programming Guide - How to Use Example Projects](https://docs.espressif.com/projects/esp-brookesia/en/latest/getting_started.html#getting-started-example-projects).

## 🚀 Quick Experience

After the firmware is flashed successfully, the device will automatically run the example program. You can view the following demonstrations through the serial monitor:

1. **Basic Operations Demo**: Demonstrates how to use Set, Get, List, and Erase functions for basic key-value pair operations
2. **Type-Safe Operations Demo**: Demonstrates how to use type-safe APIs to store and retrieve various data types
3. **Namespace Management Demo**: Demonstrates how to use multiple namespaces for data isolation
4. **Error Handling Demo**: Demonstrates how to handle various error situations

All demonstrations automatically verify that read values match written values to ensure data integrity.

## 📖 Example Description

### Basic Operations Demo

Demonstrates the general API of the NVS service with four basic operations:

- **Set**: Batch set multiple key-value pairs (strings, integers, booleans)
- **List**: List all key-value pairs and their types in a namespace
- **Get**: Get specified key-value pairs and verify that values match the set values
- **Erase**: Erase specified key-value pairs and verify the erase results

### Type-Safe Operations Demo

Demonstrates how to use type-safe `save_key_value` and `get_key_value` APIs to handle various data types:

**Direct Storage Types** (no serialization required):

- `bool`: Boolean values
- `int32_t`: 32-bit signed integers
- `uint8_t`, `int16_t`: Small integer types (≤32 bits)

**Serialized Storage Types** (serialization required):

- `std::string`: Strings
- `int64_t`, `uint64_t`: Large integer types (>32 bits)
- `float`, `double`: Floating-point numbers
- `std::vector<int>`: Integer vectors
- `ComplexStruct`: Complex structs (containing strings, integers, booleans, floats, vectors, nested structs, etc.)

All types are automatically serialized/deserialized, and values are verified for correctness after reading.

> [!NOTE]
> Compared to using the generic RPC-style API for data storage and retrieval, it is recommended to use the type-safe API for data storage and retrieval, as they are easier to use and less error-prone.

### Namespace Management Demo

Demonstrates how to use multiple namespaces for data isolation:

- Store data with the same key names in different namespaces (`storage`, `config`, `stats`)
- Read data from different namespaces to verify data isolation
- List all entries in each namespace

### Error Handling Demo

Demonstrates how to handle common error situations:

- **Non-existent Keys**: Attempt to get non-existent keys and display error messages
- **Type Mismatch**: Attempt to read data with incorrect types and display type mismatch errors

## 🔍 Troubleshooting

### NVS Initialization Failed

Ensure that the NVS partition is correctly configured in the partition table. Check the `partitions.csv` file to ensure it contains the NVS partition definition.

### Data Read Failed

- Check if the key name is correct
- Check if the namespace is correct
- Check if the data type matches
- Check error messages in the serial log

### Value Verification Failed

If value verification fails, possible causes:

- Data type mismatch
- Serialization/deserialization issues
- NVS storage corruption

Check the detailed error messages in the serial log and examine the written and read values.

## 💬 Support and Feedback

Please provide feedback through the following channels:

- For feature requests or bug reports, please create a new [GitHub Issue](https://github.com/espressif/esp-brookesia/issues)

We will reply as soon as possible.
