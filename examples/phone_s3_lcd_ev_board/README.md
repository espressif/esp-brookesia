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
    I (25) boot: ESP-IDF v5.5-dev-3644-g28ac0243bb 2nd stage bootloader
    I (25) boot: compile time May 30 2025 16:54:03
    I (25) boot: Multicore bootloader
    I (25) boot: chip revision: v0.2
    I (25) boot: efuse block revision: v1.3
    I (25) qio_mode: Enabling QIO for flash chip GD
    I (26) boot.esp32s3: Boot SPI Speed : 80MHz
    I (26) boot.esp32s3: SPI Mode       : QIO
    I (26) boot.esp32s3: SPI Flash Size : 16MB
    I (26) boot: Enabling RNG early entropy source...
    I (26) boot: Partition Table:
    I (26) boot: ## Label            Usage          Type ST Offset   Length
    I (27) boot:  0 nvs              WiFi data        01 02 00009000 00006000
    I (27) boot:  1 phy_init         RF data          01 01 0000f000 00001000
    I (28) boot:  2 factory          factory app      00 00 00010000 00400000
    I (28) boot: End of partition table
    I (28) esp_image: segment 0: paddr=00010020 vaddr=3c080020 size=2542e0h (2441952) map
    I (391) esp_image: segment 1: paddr=00264308 vaddr=3fca4d00 size=03530h ( 13616) load
    I (394) esp_image: segment 2: paddr=00267840 vaddr=40374000 size=087d8h ( 34776) load
    I (401) esp_image: segment 3: paddr=00270020 vaddr=42000020 size=776b4h (489140) map
    I (473) esp_image: segment 4: paddr=002e76dc vaddr=4037c7d8 size=184b8h ( 99512) load
    I (493) esp_image: segment 5: paddr=002ffb9c vaddr=600fe000 size=00020h (    32) load
    I (505) boot: Loaded app from partition at offset 0x10000
    I (505) boot: Disabling RNG early entropy source...
    I (506) octal_psram: vendor id    : 0x0d (AP)
    I (506) octal_psram: dev id       : 0x03 (generation 4)
    I (506) octal_psram: density      : 0x05 (128 Mbit)
    I (506) octal_psram: good-die     : 0x01 (Pass)
    I (507) octal_psram: Latency      : 0x01 (Fixed)
    I (507) octal_psram: VCC          : 0x00 (1.8V)
    I (507) octal_psram: SRF          : 0x01 (Fast Refresh)
    I (507) octal_psram: BurstType    : 0x01 (Hybrid Wrap)
    I (507) octal_psram: BurstLen     : 0x01 (32 Byte)
    I (508) octal_psram: Readlatency  : 0x02 (10 cycles@Fixed)
    I (508) octal_psram: DriveStrength: 0x00 (1/1)
    I (509) MSPI Timing: PSRAM timing tuning index: 6
    I (509) esp_psram: Found 16MB PSRAM device
    I (509) esp_psram: Speed: 80MHz
    I (688) mmu_psram: Read only data copied and mapped to SPIRAM
    I (728) mmu_psram: Instructions copied and mapped to SPIRAM
    I (729) cpu_start: Multicore app
    I (1396) esp_psram: SPI SRAM memory test OK
    I (1405) cpu_start: Pro cpu start user code
    I (1405) cpu_start: cpu freq: 240000000 Hz
    I (1405) app_init: Application information:
    I (1405) app_init: Project name:     phone_s3_lcd_ev_board
    I (1406) app_init: App version:      v0.4.2-17-g9a34de7
    I (1406) app_init: Compile time:     May 30 2025 16:53:48
    I (1406) app_init: ELF file SHA256:  b2cc97278...
    I (1406) app_init: ESP-IDF:          v5.5-dev-3644-g28ac0243bb
    I (1407) efuse_init: Min chip rev:     v0.0
    I (1407) efuse_init: Max chip rev:     v0.99
    I (1407) efuse_init: Chip rev:         v0.2
    I (1407) heap_init: Initializing. RAM available for dynamic allocation:
    I (1408) heap_init: At 3FCA9110 len 00040600 (257 KiB): RAM
    I (1408) heap_init: At 3FCE9710 len 00005724 (21 KiB): RAM
    I (1408) heap_init: At 3FCF0000 len 00008000 (32 KiB): DRAM
    I (1409) heap_init: At 600FE020 len 00001FC8 (7 KiB): RTCRAM
    I (1409) esp_psram: Adding pool of 13440K of PSRAM memory to heap allocator
    I (1409) esp_psram: Adding pool of 47K of PSRAM memory gap generated due to end address alignment of drom to the heap allocator
    I (1410) spi_flash: detected chip: gd
    I (1410) spi_flash: flash io: qio
    I (1411) sleep_gpio: Configure to isolate all GPIO pins in sleep state
    I (1411) sleep_gpio: Enable automatic switching of GPIO sleep configuration
    I (1412) main_task: Started on CPU0
    I (1460) esp_psram: Reserving pool of 32K of internal memory for DMA/internal allocations
    I (1460) main_task: Calling app_main()
    I (1461) LVGL: Starting LVGL task
    I (1461) bsp_probe: Detect module with 16MB PSRAM
    W (1461) i2c.master: Please check pull-up resistances whether be connected properly. Otherwise unexpected behavior would happen. For more detailed information, please read docs
    I (1462) bsp_probe: Detect sub_board3 with 800x480 LCD (ST7262), Touch (GT1151)
    I (1462) bsp_sub_board: Initialize RGB panel
    I (1569) gt1151: IC version: GT1158_000101(Patch)_0102(Mask)_00(SensorID)
    I (1570) app_main: Display ESP-Brookesia phone demo
    I (1570) app_main: Using stylesheet (800x480 Dark)
    [I][Brookesia][esp_brookesia_core.cpp:0213](beginCore): Library version: 0.5.0
    I (1802) app_main:    Biggest /     Free /    Total
              SRAM : [  176128 /   251603 /   359659]
            PSRAM : [12582912 / 12874240 / 13810944]
    ```

- The following animations show the example running on different development boards.

<p align="center">
<img src="https://dl.espressif.com/AE/esp-dev-kits/esp_ui_phone_s3_lcd_ev_board_480_480.gif" alt ="phone_s3_lcd_ev_board_480_480">
</p>

<p align="center">
ESP32-S3-LCD-EV-Board (with a 480 x 480 resolution LCD screen) (<a href="https://dl.espressif.com/AE/esp-dev-kits/esp_ui_phone_s3_lcd_ev_board_480_480.mp4">Click to view the video</a>)
</p>
<br>

<p align="center">
<img src="https://dl.espressif.com/AE/esp-dev-kits/esp_ui_phone_s3_lcd_ev_board_800_480.gif" alt ="phone_s3_lcd_ev_board_800_480">
</p>

<p align="center">
ESP32-S3-LCD-EV-Board-2 (with a 800 x 480 resolution LCD screen) (<a href="https://dl.espressif.com/AE/esp-dev-kits/esp_ui_phone_s3_lcd_ev_board_800_480.mp4">Click to view the video</a>)
</p>

## Technical Support and Feedback

Please use the following feedback channels:

- For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=22) forum.
- For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-brookesia/issues).

We will get back to you as soon as possible.
