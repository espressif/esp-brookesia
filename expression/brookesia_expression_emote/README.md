# ESP-Brookesia Expression Emote

* [中文版本](./README_CN.md)

## Overview

`brookesia_expression_emote` is an ESP-Brookesia expression emote management component, implemented based on the ESP-Brookesia service framework, providing:

- **Emoji Set Management**: Supports setting and managing emojis for expressing different emotional states.
- **Animation Set Management**: Supports setting, inserting, and stopping animations, providing rich animation effects.
- **Event Message Set Management**: Supports setting event messages to associate expressions with events.
- **Resource Management**: Supports loading and managing expression asset sources, enabling flexible configuration of expression resources.
- **Animation Control**: Supports animation playback control, including waiting for animation frame completion, stopping animations, and other operations.

## Table of Contents

- [ESP-Brookesia Expression Emote](#esp-brookesia-expression-emote)
  - [Overview](#overview)
  - [Table of Contents](#table-of-contents)
  - [How to Use](#how-to-use)
    - [Development Environment Requirements](#development-environment-requirements)
    - [Adding to Project](#adding-to-project)

## How to Use

### Development Environment Requirements

Before using this library, please ensure the following SDK development environment is installed:

- [ESP-IDF](https://github.com/espressif/esp-idf): `>=5.5,<6`

> [!NOTE]
> For SDK installation instructions, please refer to [ESP-IDF Programming Guide - Installation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#get-started-how-to-get-esp-idf)

### Adding to Project

`brookesia_expression_emote` has been uploaded to the [Espressif Component Registry](https://components.espressif.com/). You can add it to your project in the following ways:

1. **Using Command Line**

   Run the following command in your project directory:

   ```bash
   idf.py add-dependency "espressif/brookesia_expression_emote"
   ```

2. **Modify Configuration File**

   Create or modify the *idf_component.yml* file in your project directory:

   ```yaml
   dependencies:
     espressif/brookesia_expression_emote: "*"
   ```

For detailed instructions, please refer to [Espressif Documentation - IDF Component Manager](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html).
