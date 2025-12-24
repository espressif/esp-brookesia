# ESP-Brookesia NVS Service

* [中文版本](./README_CN.md)

## Overview

`brookesia_service_nvs` is an NVS (Non-Volatile Storage) service for the ESP-Brookesia ecosystem, providing:

- **Namespace Management**: Namespace-based key-value pair storage supporting multiple independent storage spaces
- **Data Type Support**: Supports three basic data types: `Boolean`, `Integer`, and `String`
- **Persistent Storage**: Data is stored in NVS partition and persists after power loss
- **Thread Safety**: Optionally implements asynchronous task scheduling based on `TaskScheduler` to ensure thread safety
- **Flexible Querying**: Supports listing all key-value pairs in a namespace or fetching values of specified keys on demand

## Table of Contents

- [ESP-Brookesia NVS Service](#esp-brookesia-nvs-service)
  - [Overview](#overview)
  - [Table of Contents](#table-of-contents)
  - [Features](#features)
    - [Namespace Management](#namespace-management)
    - [Supported Data Types](#supported-data-types)
    - [Core Functions](#core-functions)
  - [Development Environment Requirements](#development-environment-requirements)
  - [Adding to Project](#adding-to-project)

## Features

### Namespace Management

The NVS service uses namespaces to organize data, where each namespace is an independent storage space:

- **Default Namespace**: If no namespace is specified, the default namespace `"storage"` will be used
- **Multiple Namespaces**: Multiple namespaces can be created to organize different types of data
- **Namespace Isolation**: Data in different namespaces are independent and do not affect each other

### Supported Data Types

The NVS service supports the following three data types:

- **Boolean (Bool)**: `true` or `false`
- **Integer (Int)**: 32-bit signed integer
- **String**: UTF-8 encoded string

> [!NOTE]
> In addition to the three basic data types mentioned above, the template helper functions `save_key_value()` and `get_key_value()` provided by [brookesia_service_helper](https://components.espressif.com/components/espressif/brookesia_service_helper) also support storing and retrieving data of any type:
> - **Basic Types**: `bool`, `int32_t`, and all integer types with ≤32 bits (`int8_t`, `uint8_t`, `int16_t`, `uint16_t`, `char`, `short`, etc.) are stored directly for optimal performance
> - **Extended Integer Types**: Integer types with >32 bits (`int64_t`, `uint64_t`, `long long`, etc.) are stored via JSON serialization
> - **Floating Point Types**: Floating point types such as `float`, `double` are stored via JSON serialization
> - **String Types**: String types such as `std::string`, `const char*` are stored via JSON serialization
> - **Complex Types**: Complex types such as `std::vector`, `std::map`, custom structs, etc. are stored via JSON serialization

### Core Functions

- **List Key Information**: List information of all keys in a namespace (including key names, types, etc.)
- **Set Key-Value Pairs**: Supports batch setting of multiple key-value pairs
- **Get Key-Value Pairs**: Supports getting values of specified keys or getting all key-value pairs in a namespace
- **Erase Key-Value Pairs**: Supports erasing specified keys or clearing an entire namespace

## Development Environment Requirements

Before using this library, please ensure the following SDK development environment is installed:

- [ESP-IDF](https://github.com/espressif/esp-idf): `>=5.5,<6`

> [!NOTE]
> For SDK installation instructions, please refer to [ESP-IDF Programming Guide - Installation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#get-started-how-to-get-esp-idf)

## Adding to Project

`brookesia_service_nvs` has been uploaded to the [Espressif Component Registry](https://components.espressif.com/). You can add it to your project in the following ways:

1. **Using Command Line**

   Run the following command in your project directory:

   ```bash
   idf.py add-dependency "espressif/brookesia_service_nvs"
   ```

2. **Modify Configuration File**

   Create or modify the *idf_component.yml* file in your project directory:

   ```yaml
   dependencies:
     espressif/brookesia_service_nvs: "*"
   ```

For detailed instructions, please refer to [Espressif Documentation - IDF Component Manager](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html).
