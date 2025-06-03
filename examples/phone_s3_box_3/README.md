# ESP32-S3-BOX-3 Running ESP-Brookesia Phone Example

[中文版本](./README_CN.md)

This example demonstrates how to run the ESP-Brookesia Phone on the [ESP32-S3-BOX-3](https://github.com/espressif/esp-box/tree/master) with `320 x 240` resolution UI stylesheet.

## Getting Started

### Hardware Requirements

* An ESP32-S3-BOX-3.

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
    I (18) boot: ESP-IDF v5.5-dev-3644-g28ac0243bb 2nd stage bootloader
    I (19) boot: compile time May 30 2025 16:47:16
    I (19) boot: Multicore bootloader
    I (19) boot: chip revision: v0.2
    I (19) boot: efuse block revision: v1.3
    I (20) qio_mode: Enabling QIO for flash chip GD
    I (20) boot.esp32s3: Boot SPI Speed : 80MHz
    I (20) boot.esp32s3: SPI Mode       : QIO
    I (20) boot.esp32s3: SPI Flash Size : 16MB
    I (21) boot: Enabling RNG early entropy source...
    I (21) boot: Partition Table:
    I (21) boot: ## Label            Usage          Type ST Offset   Length
    I (21) boot:  0 nvs              WiFi data        01 02 00009000 00006000
    I (22) boot:  1 phy_init         RF data          01 01 0000f000 00001000
    I (22) boot:  2 factory          factory app      00 00 00010000 00400000
    I (23) boot: End of partition table
    I (23) esp_image: segment 0: paddr=00010020 vaddr=3c080020 size=1c5f04h (1859332) map
    I (299) esp_image: segment 1: paddr=001d5f2c vaddr=3fca6800 size=031d0h ( 12752) load
    I (302) esp_image: segment 2: paddr=001d9104 vaddr=40374000 size=06f14h ( 28436) load
    I (308) esp_image: segment 3: paddr=001e0020 vaddr=42000020 size=76e50h (486992) map
    I (381) esp_image: segment 4: paddr=00256e78 vaddr=4037af14 size=1b894h (112788) load
    I (402) esp_image: segment 5: paddr=00272714 vaddr=600fe000 size=00020h (    32) load
    I (415) boot: Loaded app from partition at offset 0x10000
    I (416) boot: Disabling RNG early entropy source...
    I (416) octal_psram: vendor id    : 0x0d (AP)
    I (417) octal_psram: dev id       : 0x03 (generation 4)
    I (417) octal_psram: density      : 0x05 (128 Mbit)
    I (417) octal_psram: good-die     : 0x01 (Pass)
    I (417) octal_psram: Latency      : 0x01 (Fixed)
    I (418) octal_psram: VCC          : 0x00 (1.8V)
    I (418) octal_psram: SRF          : 0x01 (Fast Refresh)
    I (418) octal_psram: BurstType    : 0x01 (Hybrid Wrap)
    I (418) octal_psram: BurstLen     : 0x01 (32 Byte)
    I (419) octal_psram: Readlatency  : 0x02 (10 cycles@Fixed)
    I (419) octal_psram: DriveStrength: 0x00 (1/1)
    I (420) MSPI Timing: PSRAM timing tuning index: 6
    I (420) esp_psram: Found 16MB PSRAM device
    I (420) esp_psram: Speed: 80MHz
    I (557) mmu_psram: Read only data copied and mapped to SPIRAM
    I (597) mmu_psram: Instructions copied and mapped to SPIRAM
    I (597) cpu_start: Multicore app
    I (1293) esp_psram: SPI SRAM memory test OK
    I (1301) cpu_start: Pro cpu start user code
    I (1301) cpu_start: cpu freq: 240000000 Hz
    I (1301) app_init: Application information:
    I (1302) app_init: Project name:     phone_s3_box_3
    I (1302) app_init: App version:      v0.4.2-17-gdca3bd4-dirty
    I (1302) app_init: Compile time:     May 30 2025 16:47:01
    I (1302) app_init: ELF file SHA256:  3513ade3e...
    I (1302) app_init: ESP-IDF:          v5.5-dev-3644-g28ac0243bb
    I (1303) efuse_init: Min chip rev:     v0.0
    I (1303) efuse_init: Max chip rev:     v0.99
    I (1303) efuse_init: Chip rev:         v0.2
    I (1303) heap_init: Initializing. RAM available for dynamic allocation:
    I (1304) heap_init: At 3FCAA908 len 0003EE08 (251 KiB): RAM
    I (1304) heap_init: At 3FCE9710 len 00005724 (21 KiB): RAM
    I (1304) heap_init: At 3FCF0000 len 00008000 (32 KiB): DRAM
    I (1305) heap_init: At 600FE020 len 00001FC8 (7 KiB): RTCRAM
    I (1305) esp_psram: Adding pool of 14016K of PSRAM memory to heap allocator
    I (1305) esp_psram: Adding pool of 40K of PSRAM memory gap generated due to end address alignment of drom to the heap allocator
    I (1306) spi_flash: detected chip: gd
    I (1306) spi_flash: flash io: qio
    I (1307) sleep_gpio: Configure to isolate all GPIO pins in sleep state
    I (1307) sleep_gpio: Enable automatic switching of GPIO sleep configuration
    I (1308) main_task: Started on CPU0
    I (1309) esp_psram: Reserving pool of 4K of internal memory for DMA/internal allocations
    I (1309) main_task: Calling app_main()
    I (1310) LVGL: Starting LVGL task
    W (1310) ledc: GPIO 47 is not usable, maybe conflict with others
    W (1310) i2c.master: Please check pull-up resistances whether be connected properly. Otherwise unexpected behavior would happen. For more detailed information, please read docs
    I (1311) ili9341: LCD panel create success, version: 1.2.0
    W (1432) ili9341: The 36h command has been used and will be overwritten by external initialization sequence
    W (1432) ili9341: The 3Ah command has been used and will be overwritten by external initialization sequence
    I (1464) GT911: I2C address initialization procedure skipped - using default GT9xx setup
    I (1465) GT911: TouchPad_ID:0x39,0x31,0x31
    I (1465) GT911: TouchPad_Config_Version:65
    I (1465) ESP-BOX-3: Setting LCD backlight: 100%
    I (1465) app_main: Display ESP-Brookesia phone demo
    I (1466) app_main: Using stylesheet (320x240 Drak)
    [I][Brookesia][esp_brookesia_core.cpp:0213](beginCore): Library version: 0.5.0
    I (1601) app_main:    Biggest /     Free /    Total
            SRAM : [  131072 /   181731 /   324851]
            PSRAM : [14155776 / 14388488 / 14393564]
    ```

- The following animations show the example running on the development board.

<p align="center">
<img src="https://dl.espressif.com/AE/esp-dev-kits/esp_ui_phone_s3_box_3.gif" alt ="phone_s3_box_3" width="500">
</p>

<p align="center">
(<a href="https://dl.espressif.com/AE/esp-dev-kits/esp_ui_phone_s3_box_3.mp4">Click to view the video</a>)
</p>

## Technical Support and Feedback

Please use the following feedback channels:

- For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=22) forum.
- For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-brookesia/issues).

We will get back to you as soon as possible.
