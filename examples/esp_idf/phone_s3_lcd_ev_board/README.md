# ESP32-S3-LCD-EV-Board Running ESP-Brookesia Phone Example

[中文版本](./README_CN.md)

This example demonstrates how to run the ESP-Brookesia Phone on the [ESP32-S3-LCD-EV-Board](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-lcd-ev-board/index.html) with `480 x 480` and `800 x 480` resolution UI stylesheets.

## Getting Started

### Hardware Requirements

* An ESP32-S3-LCD-EV-Board with a `480 x 480` or `800 x 480` resolution LCD screen.

### ESP-IDF Required

- This example supports IDF release/v5.3 and later branches. By default, it runs on IDF release/v5.3.
- Please follow the [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html) to set up the development environment. **We highly recommend** you [Build Your First Project](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#build-your-first-project) to get familiar with ESP-IDF and make sure the environment is set up correctly.

### Get the esp-brookesia Repository

To start from the examples in esp-brookesia, clone the repository to the local PC by running the following commands in the terminal:

```
git clone --recursive https://github.com/espressif/esp-brookesia.git
```

### Configuration

Run `idf.py menuconfig` and modify the esp-brookesia configuration.

## How to Use the Example

### Build and Flash the Example

Build the project and flash it to the board, then run monitor tool to view serial output (replace `PORT` with your board's serial port name):

```c
idf.py -p PORT flash monitor
```

To exit the serial monitor, type `Ctrl-]`.

See the [ESP-IDF Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) for full steps to configure and use ESP-IDF to build projects.

### Example Output

- The complete log is as follows:

    ```bash
    I (27) boot: ESP-IDF v5.1.4-605-g5c57dfe949 2nd stage bootloader
    I (27) boot: compile time Aug  9 2024 15:34:23
    I (27) boot: Multicore bootloader
    I (27) boot: chip revision: v0.2
    I (27) qio_mode: Enabling QIO for flash chip GD
    I (27) boot.esp32s3: Boot SPI Speed : 80MHz
    I (28) boot.esp32s3: SPI Mode       : QIO
    I (28) boot.esp32s3: SPI Flash Size : 16MB
    I (28) boot: Enabling RNG early entropy source...
    I (29) boot: Partition Table:
    I (29) boot: ## Label            Usage          Type ST Offset   Length
    I (29) boot:  0 nvs              WiFi data        01 02 00009000 00006000
    I (30) boot:  1 phy_init         RF data          01 01 0000f000 00001000
    I (30) boot:  2 factory          factory app      00 00 00010000 00400000
    I (30) boot: End of partition table
    I (31) esp_image: segment 0: paddr=00010020 vaddr=3c080020 size=2a4850h (2771024) map
    I (451) esp_image: segment 1: paddr=002b4878 vaddr=3fc98e00 size=031a4h ( 12708) load
    I (454) esp_image: segment 2: paddr=002b7a24 vaddr=40374000 size=085f4h ( 34292) load
    I (461) esp_image: segment 3: paddr=002c0020 vaddr=42000020 size=732dch (471772) map
    I (533) esp_image: segment 4: paddr=00333304 vaddr=4037c5f4 size=0c774h ( 51060) load
    I (551) boot: Loaded app from partition at offset 0x10000
    I (551) boot: Disabling RNG early entropy source...
    I (552) cpu_start: Multicore app
    I (552) octal_psram: vendor id    : 0x0d (AP)
    I (552) octal_psram: dev id       : 0x03 (generation 4)
    I (553) octal_psram: density      : 0x05 (128 Mbit)
    I (553) octal_psram: good-die     : 0x01 (Pass)
    I (553) octal_psram: Latency      : 0x01 (Fixed)
    I (553) octal_psram: VCC          : 0x00 (1.8V)
    I (554) octal_psram: SRF          : 0x01 (Fast Refresh)
    I (554) octal_psram: BurstType    : 0x01 (Hybrid Wrap)
    I (554) octal_psram: BurstLen     : 0x01 (32 Byte)
    I (555) octal_psram: Readlatency  : 0x02 (10 cycles@Fixed)
    I (555) octal_psram: DriveStrength: 0x00 (1/1)
    I (556) MSPI Timing: PSRAM timing tuning index: 5
    I (556) esp_psram: Found 16MB PSRAM device
    I (556) esp_psram: Speed: 80MHz
    I (597) mmu_psram: Instructions copied and mapped to SPIRAM
    I (800) mmu_psram: Read only data copied and mapped to SPIRAM
    I (800) cpu_start: Pro cpu up.
    I (800) cpu_start: Starting app cpu, entry point is 0x40375764
    0x40375764: call_start_cpu1 at /home/lzw/esp/work/esp-idf-github/components/esp_system/port/cpu_start.c:159

    I (0) cpu_start: App cpu up.
    I (1462) esp_psram: SPI SRAM memory test OK
    I (1471) cpu_start: Pro cpu start user code
    I (1471) cpu_start: cpu freq: 240000000 Hz
    I (1471) cpu_start: Application information:
    I (1471) cpu_start: Project name:     esp_brookesia_phone_s3_lcd_ev_board
    I (1472) cpu_start: App version:      7d4f9ae-dirty
    I (1472) cpu_start: Compile time:     Aug  9 2024 15:34:15
    I (1472) cpu_start: ELF file SHA256:  9cb90bd6afc1f054...
    I (1473) cpu_start: ESP-IDF:          v5.1.4-605-g5c57dfe949
    I (1473) cpu_start: Min chip rev:     v0.0
    I (1473) cpu_start: Max chip rev:     v0.99
    I (1474) cpu_start: Chip rev:         v0.2
    I (1474) heap_init: Initializing. RAM available for dynamic allocation:
    I (1474) heap_init: At 3FC9D248 len 0004C4C8 (305 KiB): DRAM
    I (1475) heap_init: At 3FCE9710 len 00005724 (21 KiB): STACK/DRAM
    I (1475) heap_init: At 3FCF0000 len 00008000 (32 KiB): DRAM
    I (1476) heap_init: At 600FE010 len 00001FD8 (7 KiB): RTCRAM
    I (1476) esp_psram: Adding pool of 13120K of PSRAM memory to heap allocator
    I (1477) spi_flash: detected chip: gd
    I (1477) spi_flash: flash io: qio
    I (1477) sleep: Configure to isolate all GPIO pins in sleep state
    I (1477) sleep: Enable automatic switching of GPIO sleep configuration
    I (1478) app_start: Starting scheduler on CPU0
    I (1478) app_start: Starting scheduler on CPU1
    I (1478) main_task: Started on CPU0
    I (1479) esp_psram: Reserving pool of 32K of internal memory for DMA/internal allocations
    I (1479) main_task: Calling app_main()
    I (1479) bsp_probe: Detect module with 16MB PSRAM
    I (1480) bsp_probe: Detect sub_board2 with 480x480 LCD (GC9503), Touch (FT5x06)
    I (1480) gpio: GPIO[3]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0
    I (1481) bsp_sub_board: Install panel IO
    I (1481) lcd_panel.io.3wire_spi: Panel IO create success, version: 1.0.1
    I (1482) bsp_sub_board: Initialize RGB panel
    W (3091) lcd_panel.io.3wire_spi: Delete but keep CS line inactive
    I (3118) gc9503: LCD panel create success, version: 3.0.1
    I (3121) bsp_lvgl_port: Create LVGL task
    I (3121) bsp_lvgl_port: Starting LVGL task
    I (3141) app_main: Display ESP BROOKESIA phone demo
    I (3182) esp-brookesia: [esp_brookesia_core.cpp:150](beginCore)Library version: 0.1.0
    I (3320) app_main:    Biggest /     Free /    Total
            SRAM : [  225280 /   317739 /   408515]
          PSRAM : [12320768 / 12509956 / 13434880]
    ```

- The following animations show the example running on different development boards.

<p align="center">
<img src="https://dl.espressif.com/AE/esp-dev-kits/esp_ui_phone_s3_lcd_ev_board_480_480.gif" alt ="esp_brookesia_phone_s3_lcd_ev_board_480_480">
</p>

<p align="center">
ESP32-S3-LCD-EV-Board (with a 480 x 480 resolution LCD screen) (<a href="https://dl.espressif.com/AE/esp-dev-kits/esp_ui_phone_s3_lcd_ev_board_480_480.mp4">Click to view the video</a>)
</p>
<br>

<p align="center">
<img src="https://dl.espressif.com/AE/esp-dev-kits/esp_ui_phone_s3_lcd_ev_board_800_480.gif" alt ="esp_brookesia_phone_s3_lcd_ev_board_800_480">
</p>

<p align="center">
ESP32-S3-LCD-EV-Board-2 (with a 800 x 480 resolution LCD screen) (<a href="https://dl.espressif.com/AE/esp-dev-kits/esp_ui_phone_s3_lcd_ev_board_800_480.mp4">Click to view the video</a>)
</p>

## Technical Support and Feedback

Please use the following feedback channels:

- For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=22) forum.
- For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-brookesia/issues).

We will get back to you as soon as possible.
