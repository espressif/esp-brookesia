# ESP-Brookesia HAL Boards

* [中文版本](./README_CN.md)

## Overview

`brookesia_hal_boards` is the board configuration collection for ESP-Brookesia. It uses YAML files to describe the peripheral topology and device parameters for each supported board, allowing `brookesia_hal_adaptor` to initialize hardware at runtime without any board-specific hardcoding.

## Table of Contents

- [ESP-Brookesia HAL Boards](#esp-brookesia-hal-boards)
  - [Overview](#overview)
  - [Table of Contents](#table-of-contents)
  - [Supported Boards](#supported-boards)
  - [Directory Structure](#directory-structure)
    - [board\_info.yaml](#board_infoyaml)
    - [board\_devices.yaml](#board_devicesyaml)
    - [board\_peripherals.yaml](#board_peripheralsyaml)
    - [sdkconfig.defaults.board](#sdkconfigdefaultsboard)
    - [setup\_device.c](#setup_devicec)
  - [Usage](#usage)
    - [Select a Board](#select-a-board)
    - [Add a Custom Board](#add-a-custom-board)
  - [Requirements](#requirements)
  - [Add to Your Project](#add-to-your-project)

## Supported Boards

| Board Name | Chip | Description |
|------------|------|-------------|
| `esp_vocat_board_v1_0` | ESP32-S3 | ESP VoCat Board V1.0 — AI Pet Companion Development Board |
| `esp_vocat_board_v1_2` | ESP32-S3 | ESP VoCat Board V1.2 — AI Pet Companion Development Board |
| `esp_box_3` | ESP32-S3 | ESP-BOX-3 Development Board |
| `esp32_s3_korvo2_v3` | ESP32-S3 | ESP32-S3-Korvo-2 V3 Development Board |
| `esp32_p4_function_ev` | ESP32-P4 | ESP32-P4-Function-EV-Board |
| `esp_sensair_shuttle` | ESP32-C5 | ESP Sensair Shuttle Module |

## Directory Structure

Each board has its own subdirectory under `boards/<board-name>/` containing the following files:

```
boards/
└── <board>/
    ├── board_info.yaml          # Board metadata (name, chip, version, manufacturer, etc.)
    ├── board_devices.yaml       # Logical device configurations (audio codec, LCD, touch, storage, etc.)
    ├── board_peripherals.yaml   # Low-level peripheral configurations (I2C/I2S/SPI buses, GPIO, LEDC, etc.)
    ├── sdkconfig.defaults.board # Board-specific Kconfig defaults (Flash, PSRAM, etc.)
    └── setup_device.c           # Board-specific device factory callbacks (for custom driver initialization)
```

### board_info.yaml

Stores basic metadata about the board for `esp_board_manager` to identify and match:

```yaml
board: esp_vocat_board_v1_2
chip: esp32s3
version: 1.2.0
description: "ESP VoCat Board V1.2 - AI Pet Companion Development Board"
manufacturer: "ESPRESSIF"
```

### board_devices.yaml

Describes the logical functional modules on the board. Common device types include:

| Device Type | Description |
|-------------|-------------|
| `audio_codec` | Audio codec chip (DAC playback / ADC recording); supports ES8311, ES7210, internal ADC, etc. |
| `display_lcd` | LCD display; supports SPI (ST77916, ILI9341), DSI (EK79007), and other interfaces |
| `lcd_touch` | Touch panel; supports CST816S, GT911, and other I2C touch controllers |
| `ledc_ctrl` | PWM backlight control via LEDC |
| `fs_fat` / `fs_spiffs` | File system storage; supports SD card (SDMMC/SPI) and SPIFFS |
| `camera` | Camera (CSI interface) |
| `power_ctrl` | GPIO-based power control (audio power, LCD/SD card power, etc.) |
| `gpio_ctrl` | General-purpose GPIO control (LEDs, buttons, etc.) |

### board_peripherals.yaml

Describes the pin assignments and parameters for low-level hardware resources, including:

- **I2C**: SDA/SCL pins, port number
- **I2S**: MCLK/BCLK/WS/DOUT/DIN pins, sample rate, bit depth
- **SPI**: MOSI/MISO/CLK/CS pins, SPI host, transfer size
- **LEDC**: Backlight GPIO, PWM frequency and resolution
- **GPIO**: Power control, amplifier enable, LED, and other standalone pin configurations

### sdkconfig.defaults.board

Contains board-specific Kconfig defaults tightly coupled to the hardware, such as Flash size, PSRAM mode and speed, CPU frequency, data cache line size, and `brookesia_hal_adaptor` audio recording parameters:

```kconfig
CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_BROOKESIA_HAL_ADAPTOR_AUDIO_CODEC_RECORDER_SAMPLE_RATE=16000
...
```

### setup_device.c

Provides factory callbacks for devices that require a custom driver initialization flow, such as injecting vendor-specific initialization command sequences into an LCD driver:

```c
esp_err_t lcd_panel_factory_entry_t(
    esp_lcd_panel_io_handle_t io,
    const esp_lcd_panel_dev_config_t *panel_dev_config,
    esp_lcd_panel_handle_t *ret_panel)
{
    // Inject ST77916 vendor_config (custom register initialization sequence)
    ...
}
```

## Usage

### Select a Board

Run the following command in the project root directory, specifying the target board name:

```bash
idf.py gen-bmgr-config -b <board>
```

Available `<board>` values are listed in [Supported Boards](#supported-boards). After running the command, `esp_board_manager` reads the corresponding YAML configuration and generates the board-level code. Then build and flash as usual:

```bash
idf.py -p PORT build flash monitor
```

> [!NOTE]
> If `brookesia_hal_boards` is included as a local path dependency, specify the `boards/` directory path with the `-c` option:
>
> ```bash
> idf.py gen-bmgr-config -b <board> -c path/to/brookesia_hal_boards/boards
> ```
>
> In example projects that use `idf_ext.py`, this argument is injected automatically at build time and does not need to be added manually.

### Add a Custom Board

Create a new board subdirectory under `boards/` (or any custom directory) and add the following files following the structure of an existing board:

1. **`board_info.yaml`**: Fill in the board name, chip, version, and description.
2. **`board_peripherals.yaml`**: Configure peripherals according to the actual pin and bus assignments.
3. **`board_devices.yaml`**: Describe the on-board devices with their types and configurations.
4. **`sdkconfig.defaults.board`**: Add board-specific Kconfig defaults.
5. **`setup_device.c`** *(optional)*: Implement factory functions if the driver requires extra initialization steps.

Once done, run `idf.py gen-bmgr-config -b <new_board>` to use the new board.

> [!NOTE]
> For the complete `esp_board_manager` configuration format reference, see the [esp_board_manager component documentation](https://github.com/espressif/esp-gmf/blob/main/packages/esp_board_manager/README.md).

## Requirements

- [ESP-IDF](https://github.com/espressif/esp-idf): `>=5.5,<6`

> [!NOTE]
> For SDK installation instructions, refer to the [ESP-IDF Programming Guide — Get Started](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#get-started-how-to-get-esp-idf).

## Add to Your Project

`brookesia_hal_boards` is published to the [Espressif Component Registry](https://components.espressif.com/) and can be added in two ways:

1. **Command line**

   ```bash
   idf.py add-dependency "espressif/brookesia_hal_boards"
   ```

2. **idf_component.yml**

   ```yaml
   dependencies:
     espressif/brookesia_hal_boards: "*"
   ```

For more details, refer to the [Espressif Documentation — IDF Component Manager](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html).
