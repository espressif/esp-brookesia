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
    I (19) boot: ESP-IDF v5.1.4-605-g5c57dfe949 2nd stage bootloader
    I (19) boot: compile time Aug 22 2024 17:33:55
    I (19) boot: Multicore bootloader
    I (20) boot: chip revision: v0.2
    I (20) qio_mode: Enabling QIO for flash chip GD
    I (20) boot.esp32s3: Boot SPI Speed : 80MHz
    I (20) boot.esp32s3: SPI Mode       : QIO
    I (21) boot.esp32s3: SPI Flash Size : 16MB
    I (21) boot: Enabling RNG early entropy source...
    I (21) boot: Partition Table:
    I (21) boot: ## Label            Usage          Type ST Offset   Length
    I (22) boot:  0 nvs              WiFi data        01 02 00009000 00006000
    I (22) boot:  1 phy_init         RF data          01 01 0000f000 00001000
    I (23) boot:  2 factory          factory app      00 00 00010000 00400000
    I (23) boot: End of partition table
    I (23) esp_image: segment 0: paddr=00010020 vaddr=3c080020 size=19df50h (1695568) map
    I (281) esp_image: segment 1: paddr=001adf78 vaddr=3fc9a900 size=020a0h (  8352) load
    I (283) esp_image: segment 2: paddr=001b0020 vaddr=42000020 size=7a540h (501056) map
    I (359) esp_image: segment 3: paddr=0022a568 vaddr=3fc9c9a0 size=00e44h (  3652) load
    I (360) esp_image: segment 4: paddr=0022b3b4 vaddr=40374000 size=168d4h ( 92372) load
    I (388) boot: Loaded app from partition at offset 0x10000
    I (388) boot: Disabling RNG early entropy source...
    I (388) cpu_start: Multicore app
    I (389) octal_psram: vendor id    : 0x0d (AP)
    I (389) octal_psram: dev id       : 0x03 (generation 4)
    I (389) octal_psram: density      : 0x05 (128 Mbit)
    I (390) octal_psram: good-die     : 0x01 (Pass)
    I (390) octal_psram: Latency      : 0x01 (Fixed)
    I (390) octal_psram: VCC          : 0x00 (1.8V)
    I (391) octal_psram: SRF          : 0x01 (Fast Refresh)
    I (391) octal_psram: BurstType    : 0x01 (Hybrid Wrap)
    I (391) octal_psram: BurstLen     : 0x01 (32 Byte)
    I (392) octal_psram: Readlatency  : 0x02 (10 cycles@Fixed)
    I (392) octal_psram: DriveStrength: 0x00 (1/1)
    I (393) MSPI Timing: PSRAM timing tuning index: 6
    I (393) esp_psram: Found 16MB PSRAM device
    I (393) esp_psram: Speed: 80MHz
    I (434) mmu_psram: Instructions copied and mapped to SPIRAM
    I (556) mmu_psram: Read only data copied and mapped to SPIRAM
    I (557) cpu_start: Pro cpu up.
    I (557) cpu_start: Starting app cpu, entry point is 0x4037582c
    0x4037582c: call_start_cpu1 at /home/lzw/esp/work/esp-idf-github/components/esp_system/port/cpu_start.c:159

    I (0) cpu_start: App cpu up.
    I (1262) esp_psram: SPI SRAM memory test OK
    I (1271) cpu_start: Pro cpu start user code
    I (1271) cpu_start: cpu freq: 240000000 Hz
    I (1271) cpu_start: Application information:
    I (1271) cpu_start: Project name:     esp_brookesia_phone_s3_box
    I (1271) cpu_start: App version:      v0.1.1-7-g944dbe6-dirty
    I (1272) cpu_start: Compile time:     Aug 22 2024 17:33:48
    I (1272) cpu_start: ELF file SHA256:  1b5d20740169b201...
    I (1272) cpu_start: ESP-IDF:          v5.1.4-605-g5c57dfe949
    I (1273) cpu_start: Min chip rev:     v0.0
    I (1273) cpu_start: Max chip rev:     v0.99
    I (1273) cpu_start: Chip rev:         v0.2
    I (1274) heap_init: Initializing. RAM available for dynamic allocation:
    I (1274) heap_init: At 3FC9E9D0 len 0004AD40 (299 KiB): DRAM
    I (1274) heap_init: At 3FCE9710 len 00005724 (21 KiB): STACK/DRAM
    I (1275) heap_init: At 3FCF0000 len 00008000 (32 KiB): DRAM
    I (1275) heap_init: At 600FE010 len 00001FD8 (7 KiB): RTCRAM
    I (1276) esp_psram: Adding pool of 14208K of PSRAM memory to heap allocator
    I (1276) spi_flash: detected chip: gd
    I (1276) spi_flash: flash io: qio
    I (1277) sleep: Configure to isolate all GPIO pins in sleep state
    I (1277) sleep: Enable automatic switching of GPIO sleep configuration
    I (1278) app_start: Starting scheduler on CPU0
    I (1278) app_start: Starting scheduler on CPU1
    I (1278) main_task: Started on CPU0
    I (1279) esp_psram: Reserving pool of 32K of internal memory for DMA/internal allocations
    I (1279) main_task: Calling app_main()
    I (1280) LVGL: Starting LVGL task
    I (1280) gpio: GPIO[4]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
    I (1281) gpio: GPIO[48]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
    I (1281) ili9341: LCD panel create success, version: 1.2.0
    W (1401) ili9341: The 36h command has been used and will be overwritten by external initialization sequence
    W (1401) ili9341: The 3Ah command has been used and will be overwritten by external initialization sequence
    W (1404) GT911: Unable to initialize the I2C address
    I (1404) gpio: GPIO[3]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:2
    I (1405) GT911: TouchPad_ID:0x39,0x31,0x31
    I (1405) GT911: TouchPad_Config_Version:65
    I (1405) ESP-BOX-3: Setting LCD backlight: 100%
    I (1405) app_main: Display ESP BROOKESIA phone demo
    I (1405) app_main: Using external stylesheet
    I (1406) esp-brookesia: [esp_brookesia_core.cpp:150](beginCore)Library version: 0.1.0
    I (1472) app_main:    Biggest /     Free /    Total
              SRAM : [  151552 /   245403 /   402491]
          PSRAM : [14417920 / 14545632 / 14548992]
    ```

- The following animations show the example running on the development board.

<p align="center">
<img src="https://dl.espressif.com/AE/esp-dev-kits/esp_ui_phone_s3_box_3.gif" alt ="esp_brookesia_phone_s3_box_3" width="500">
</p>

<p align="center">
(<a href="https://dl.espressif.com/AE/esp-dev-kits/esp_ui_phone_s3_box_3.mp4">Click to view the video</a>)
</p>

## Technical Support and Feedback

Please use the following feedback channels:

- For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=22) forum.
- For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-brookesia/issues).

We will get back to you as soon as possible.
