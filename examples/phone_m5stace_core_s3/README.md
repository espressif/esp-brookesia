# M5Stack CoreS3 Running ESP-Brookesia Phone Example

[中文版本](./README_CN.md)

This example demonstrates how to run the ESP-Brookesia Phone on the [M5Stack CoreS3](https://docs.m5stack.com/en/core/CoreS3) with `320 x 240` resolution UI stylesheet.

## Getting Started

### Hardware Requirements

* An M5Stack CoreS3.

### ESP-IDF Required

- This example supports IDF release/v5.1 and later branches. By default, it runs on IDF release/v5.1.
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
    I (24) boot: ESP-IDF v5.5-dev-3644-g28ac0243bb 2nd stage bootloader
    I (25) boot: compile time May 30 2025 15:32:07
    I (25) boot: Multicore bootloader
    I (25) boot: chip revision: v0.1
    I (25) boot: efuse block revision: v1.2
    I (26) qio_mode: Enabling default flash chip QIO
    I (26) boot.esp32s3: Boot SPI Speed : 80MHz
    I (26) boot.esp32s3: SPI Mode       : QIO
    I (26) boot.esp32s3: SPI Flash Size : 16MB
    I (26) boot: Enabling RNG early entropy source...
    I (27) boot: Partition Table:
    I (27) boot: ## Label            Usage          Type ST Offset   Length
    I (27) boot:  0 nvs              WiFi data        01 02 00009000 00006000
    I (28) boot:  1 phy_init         RF data          01 01 0000f000 00001000
    I (28) boot:  2 factory          factory app      00 00 00010000 00400000
    I (29) boot: End of partition table
    I (29) esp_image: segment 0: paddr=00010020 vaddr=3c080020 size=1c50d4h (1855700) map
    I (305) esp_image: segment 1: paddr=001d50fc vaddr=3fca5c00 size=02cech ( 11500) load
    I (307) esp_image: segment 2: paddr=001d7df0 vaddr=40374000 size=08228h ( 33320) load
    I (314) esp_image: segment 3: paddr=001e0020 vaddr=42000020 size=74a9ch (477852) map
    I (385) esp_image: segment 4: paddr=00254ac4 vaddr=4037c228 size=19978h (104824) load
    I (406) esp_image: segment 5: paddr=0026e444 vaddr=600fe000 size=00020h (    32) load
    I (418) boot: Loaded app from partition at offset 0x10000
    I (418) boot: Disabling RNG early entropy source...
    I (419) esp_psram: Found 8MB PSRAM device
    I (419) esp_psram: Speed: 80MHz
    I (635) mmu_psram: Read only data copied and mapped to SPIRAM
    I (695) mmu_psram: Instructions copied and mapped to SPIRAM
    I (696) cpu_start: Multicore app
    I (1348) esp_psram: SPI SRAM memory test OK
    I (1357) cpu_start: Pro cpu start user code
    I (1357) cpu_start: cpu freq: 240000000 Hz
    I (1357) app_init: Application information:
    I (1357) app_init: Project name:     phone_m5stack_core_s3
    I (1357) app_init: App version:      v0.4.2-17-g59514c4-dirty
    I (1357) app_init: Compile time:     May 30 2025 15:31:54
    I (1358) app_init: ELF file SHA256:  d80f1b9ea...
    --- Warning: Checksum mismatch between flashed and built applications. Checksum of built application is ef79260e6f5202cec75e5416bbccb5e965cc5013ec6f35442ce4726db0c74a34
    I (1358) app_init: ESP-IDF:          v5.5-dev-3644-g28ac0243bb
    I (1358) efuse_init: Min chip rev:     v0.0
    I (1359) efuse_init: Max chip rev:     v0.99
    I (1359) efuse_init: Chip rev:         v0.1
    I (1359) heap_init: Initializing. RAM available for dynamic allocation:
    I (1359) heap_init: At 3FCA9768 len 0003FFA8 (255 KiB): RAM
    I (1360) heap_init: At 3FCE9710 len 00005724 (21 KiB): RAM
    I (1360) heap_init: At 3FCF0000 len 00008000 (32 KiB): DRAM
    I (1360) heap_init: At 600FE020 len 00001FC8 (7 KiB): RTCRAM
    I (1361) esp_psram: Adding pool of 5824K of PSRAM memory to heap allocator
    I (1361) esp_psram: Adding pool of 43K of PSRAM memory gap generated due to end address alignment of drom to the heap allocator
    I (1362) spi_flash: detected chip: generic
    I (1362) spi_flash: flash io: qio
    I (1363) sleep_gpio: Configure to isolate all GPIO pins in sleep state
    I (1363) sleep_gpio: Enable automatic switching of GPIO sleep configuration
    I (1364) main_task: Started on CPU0
    I (1365) esp_psram: Reserving pool of 32K of internal memory for DMA/internal allocations
    I (1365) main_task: Calling app_main()
    I (1366) LVGL: Starting LVGL task
    W (1366) i2c.master: Please check pull-up resistances whether be connected properly. Otherwise unexpected behavior would happen. For more detailed information, please read docs
    I (1522) M5Stack: Setting LCD backlight: 100%
    I (1523) app_main: Display ESP-Brookesia phone demo
    I (1523) app_main: Using stylesheet (320x240 Drak)
    [I][Brookesia][esp_brookesia_core.cpp:0213](beginCore): Library version: 0.5.0
    I (1658) app_main:    Biggest /     Free /    Total
              SRAM : [  110592 /   186035 /   358035]
            PSRAM : [ 5898240 /  6003976 /  6008588]
    ```

- The following animations show the example running on the development board.

<p align="center">
<img src="https://dl.espressif.com/AE/esp-dev-kits/esp_ui_phone_m5stace_core_s3.gif" alt ="phone_m5stace_core_s3" width="400">
</p>

<p align="center">
(<a href="https://dl.espressif.com/AE/esp-dev-kits/esp_ui_phone_m5stace_core_s3.mp4">Click to view the video</a>)
</p>

## Technical Support and Feedback

Please use the following feedback channels:

- For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=22) forum.
- For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-brookesia/issues).

We will get back to you as soon as possible.
