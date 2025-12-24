# ESP-Brookesia SNTP Service

* [中文版本](./README_CN.md)

## Overview

`brookesia_service_sntp` is an SNTP (Simple Network Time Protocol) service for the ESP-Brookesia ecosystem, providing:

- **NTP Server Management**: Supports configuring multiple NTP servers and automatically selects available servers from the server list for time synchronization
- **Timezone Settings**: Supports setting system timezone and automatically applies timezone offset
- **Automatic Time Synchronization**: Automatically detects network connection status and starts time synchronization when network is available
- **Status Query**: Supports querying time synchronization status, currently configured server list, and timezone information
- **Persistent Storage**: Optionally works with `brookesia_service_nvs` service to persistently save NTP server list and timezone information

## Table of Contents

- [ESP-Brookesia SNTP Service](#esp-brookesia-sntp-service)
  - [Overview](#overview)
  - [Table of Contents](#table-of-contents)
  - [Features](#features)
    - [NTP Server Management](#ntp-server-management)
    - [Timezone Settings](#timezone-settings)
    - [Core Functions](#core-functions)
    - [Automatic Management](#automatic-management)
  - [Development Environment Requirements](#development-environment-requirements)
  - [Adding to Project](#adding-to-project)

## Features

### NTP Server Management

The SNTP service supports configuring multiple NTP servers:

- **Default Server**: Uses `"pool.ntp.org"` as the default NTP server
- **Multiple Server Support**: Can configure multiple NTP servers, and the service will automatically select available servers
- **Server List**: Supports getting the list of all currently configured NTP servers

### Timezone Settings

The SNTP service supports setting system timezone:

- **Default Timezone**: Default timezone is `"CST-8"` (China Standard Time, UTC+8)
- **Timezone Format**: Supports standard timezone string formats (such as `"UTC"`, `"CST-8"`, `"EST-5"`, etc.)
- **Automatic Application**: Automatically applies to system time after setting timezone

### Core Functions

- **Set NTP Servers**: Supports setting one or multiple NTP servers
- **Set Timezone**: Supports setting system timezone
- **Start Service**: Starts the SNTP service and begins time synchronization
- **Stop Service**: Stops the SNTP service and stops time synchronization
- **Get Server List**: Gets the list of currently configured NTP servers
- **Get Timezone**: Gets the currently configured timezone
- **Check Sync Status**: Checks whether system time has been synchronized with NTP servers
- **Reset Data**: Resets all configuration data to default values

### Automatic Management

- **Automatic Configuration Loading**: Automatically loads saved configuration from NVS when service starts
- **Automatic Configuration Saving**: Automatically saves configuration to NVS after configuration changes
- **Network Detection**: Automatically detects network connection status and starts time synchronization when network is available
- **Status Monitoring**: Automatically monitors time synchronization status and updates sync flag

## Development Environment Requirements

Before using this library, please ensure the following SDK development environment is installed:

- [ESP-IDF](https://github.com/espressif/esp-idf): `>=5.5,<6`

> [!NOTE]
> For SDK installation instructions, please refer to [ESP-IDF Programming Guide - Installation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#get-started-how-to-get-esp-idf)

## Adding to Project

`brookesia_service_sntp` has been uploaded to the [Espressif Component Registry](https://components.espressif.com/). You can add it to your project in the following ways:

1. **Using Command Line**

   Run the following command in your project directory:

   ```bash
   idf.py add-dependency "espressif/brookesia_service_sntp"
   ```

2. **Modify Configuration File**

   Create or modify the *idf_component.yml* file in your project directory:

   ```yaml
   dependencies:
     espressif/brookesia_service_sntp: "*"
   ```

For detailed instructions, please refer to [Espressif Documentation - IDF Component Manager](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html).
