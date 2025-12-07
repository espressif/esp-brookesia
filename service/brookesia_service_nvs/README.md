# ESP-Brookesia NVS Service

## Overview

`brookesia_service_nvs` is an NVS (Non-Volatile Storage) service for the ESP-Brookesia ecosystem, providing:

- **Namespace Management**: Namespace-based key-value pair storage supporting multiple independent storage spaces
- **Data Type Support**: Supports three basic data types: `Boolean`, `Integer`, and `String`
- **Persistent Storage**: Data is stored in NVS partition and persists after power loss
- **Thread Safety**: Optionally implements asynchronous task scheduling based on `TaskScheduler` to ensure thread safety
- **Flexible Querying**: Supports listing all key-value pairs in a namespace or fetching values of specified keys on demand

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

### Core Functions

- **List Key-Value Pairs**: List information of all key-value pairs in a namespace (including key names, types, etc.)
- **Set Key-Value Pairs**: Supports batch setting of multiple key-value pairs
- **Get Key-Value Pairs**: Supports getting values of specified keys or getting all key-value pairs in a namespace
- **Erase Key-Value Pairs**: Supports erasing specified keys or clearing an entire namespace

## Development Environment Requirements

Before using this library, please ensure the following SDK development environment is installed:

- [ESP-IDF](https://github.com/espressif/esp-idf): `>=5.5,<6`

> [!NOTE]
> For SDK installation instructions, please refer to [ESP-IDF Programming Guide - Installation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#get-started-how-to-get-esp-idf)

`brookesia_service_nvs` has the following dependencies:

| **Dependency** | **Version Requirement** |
|---------------|-------------------------|
| [cmake_utilities](https://components.espressif.com/components/espressif/cmake_utilities) | 0.* |
| [brookesia_service_manager](https://components.espressif.com/components/espressif/brookesia_service_manager) | 0.7.* |
| [brookesia_service_helper](https://components.espressif.com/components/espressif/brookesia_service_helper) | 0.7.* |

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
