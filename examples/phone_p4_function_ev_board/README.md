# ESP32-P4-Function-EV-Board Running ESP-Brookesia Phone Example

[中文版本](./README_CN.md)

This example demonstrates how to run the ESP-Brookesia Phone on the [ESP32-P4-Function-EV-Board](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32p4/esp32-p4-function-ev-board/index.html) with a `1024 x 600` resolution UI stylesheet.

## Getting Started

### Hardware Requirements

* An ESP32-P4-Function-EV-Board with a `1024 x 600` resolution LCD screen.

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
    I (27) boot: ESP-IDF v5.5-dev-3644-g28ac0243bb 2nd stage bootloader
    I (28) boot: compile time May 30 2025 16:34:29
    I (28) boot: Multicore bootloader
    I (30) boot: chip revision: v1.3
    I (32) boot: efuse block revision: v0.3
    I (36) qio_mode: Enabling default flash chip QIO
    I (40) boot.esp32p4: SPI Speed      : 80MHz
    I (44) boot.esp32p4: SPI Mode       : QIO
    I (48) boot.esp32p4: SPI Flash Size : 16MB
    I (51) boot: Enabling RNG early entropy source...
    I (56) boot: Partition Table:
    I (59) boot: ## Label            Usage          Type ST Offset   Length
    I (65) boot:  0 nvs              WiFi data        01 02 00009000 00006000
    I (71) boot:  1 phy_init         RF data          01 01 0000f000 00001000
    I (78) boot:  2 factory          factory app      00 00 00010000 00f00000
    I (85) boot: End of partition table
    I (88) esp_image: segment 0: paddr=00010020 vaddr=48090020 size=23cf34h (2346804) map
    I (385) esp_image: segment 1: paddr=0024cf5c vaddr=30100000 size=00068h (   104) load
    I (387) esp_image: segment 2: paddr=0024cfcc vaddr=4ff00000 size=0304ch ( 12364) load
    I (392) esp_image: segment 3: paddr=00250020 vaddr=48000020 size=89960h (563552) map
    I (467) esp_image: segment 4: paddr=002d9988 vaddr=4ff0304c size=20388h (131976) load
    I (488) esp_image: segment 5: paddr=002f9d18 vaddr=4ff23400 size=03208h ( 12808) load
    I (492) esp_image: segment 6: paddr=002fcf28 vaddr=50108080 size=00020h (    32) load
    I (500) boot: Loaded app from partition at offset 0x10000
    I (500) boot: Disabling RNG early entropy source...
    I (514) hex_psram: vendor id    : 0x0d (AP)
    I (514) hex_psram: Latency      : 0x01 (Fixed)
    I (514) hex_psram: DriveStr.    : 0x00 (25 Ohm)
    I (515) hex_psram: dev id       : 0x03 (generation 4)
    I (519) hex_psram: density      : 0x07 (256 Mbit)
    I (524) hex_psram: good-die     : 0x06 (Pass)
    I (528) hex_psram: SRF          : 0x02 (Slow Refresh)
    I (533) hex_psram: BurstType    : 0x00 ( Wrap)
    I (537) hex_psram: BurstLen     : 0x03 (2048 Byte)
    I (541) hex_psram: BitMode      : 0x01 (X16 Mode)
    I (546) hex_psram: Readlatency  : 0x04 (14 cycles@Fixed)
    I (551) hex_psram: DriveStrength: 0x00 (1/1)
    I (555) MSPI DQS: tuning success, best phase id is 0
    I (727) MSPI DQS: tuning success, best delayline id is 16
    I esp_psram: Found 32MB PSRAM device
    I esp_psram: Speed: 200MHz
    I (840) mmu_psram: .rodata xip on psram
    I (867) mmu_psram: .text xip on psram
    I (868) hex_psram: psram CS IO is dedicated
    I (869) cpu_start: Multicore app
    I (1304) esp_psram: SPI SRAM memory test OK
    I (1313) cpu_start: Pro cpu start user code
    I (1313) cpu_start: cpu freq: 360000000 Hz
    I (1313) app_init: Application information:
    I (1314) app_init: Project name:     phone_p4_function_ev_board
    I (1319) app_init: App version:      v0.4.2-17-g1469bf3-dirty
    I (1325) app_init: Compile time:     May 30 2025 16:34:17
    I (1330) app_init: ELF file SHA256:  e346bed66...
    I (1334) app_init: ESP-IDF:          v5.5-dev-3644-g28ac0243bb
    I (1340) efuse_init: Min chip rev:     v0.1
    I (1344) efuse_init: Max chip rev:     v1.99
    I (1348) efuse_init: Chip rev:         v1.3
    I (1352) heap_init: Initializing. RAM available for dynamic allocation:
    I (1358) heap_init: At 4FF283B0 len 00012C10 (75 KiB): RAM
    I (1363) heap_init: At 4FF3AFC0 len 00004BF0 (18 KiB): RAM
    I (1368) heap_init: At 4FF40000 len 00040000 (256 KiB): RAM
    I (1374) heap_init: At 501080A0 len 00007F60 (31 KiB): RTCRAM
    I (1379) heap_init: At 30100068 len 00001F98 (7 KiB): TCM
    I (1384) esp_psram: Adding pool of 29888K of PSRAM memory to heap allocator
    I (1391) esp_psram: Adding pool of 25K of PSRAM memory gap generated due to end address alignment of irom to the heap allocator
    I (1402) esp_psram: Adding pool of 12K of PSRAM memory gap generated due to end address alignment of drom to the heap allocator
    W (1414) spi_flash: GigaDevice detected but related driver is not linked, please check option `SPI_FLASH_SUPPORT_GD_CHIP`
    I (1424) spi_flash: detected chip: generic
    I (1428) spi_flash: flash io: qio
    I (1431) main_task: Started on CPU0
    I (1434) esp_psram: Reserving pool of 32K of internal memory for DMA/internal allocations
    I (1442) main_task: Calling app_main()
    I (1445) LVGL: Starting LVGL task
    W (1448) ledc: GPIO 26 is not usable, maybe conflict with others
    I (1454) ESP32_P4_EV: MIPI DSI PHY Powered on
    I (1459) ESP32_P4_EV: Install MIPI DSI LCD control panel
    I (1463) ESP32_P4_EV: Install EK79007 LCD control panel
    I (1468) ek79007: version: 1.0.2
    I (1628) ESP32_P4_EV: Display initialized
    I (1628) ESP32_P4_EV: Display resolution 1024x600
    W (1635) i2c.master: Please check pull-up resistances whether be connected properly. Otherwise unexpected behavior would happen. For more detailed information, please read docs
    I (1640) GT911: I2C address initialization procedure skipped - using default GT9xx setup
    I (1648) GT911: TouchPad_ID:0x39,0x31,0x31
    I (1652) GT911: TouchPad_Config_Version:89
    I (1656) ESP32_P4_EV: Setting LCD backlight: 100%
    I (1660) app_main: Display ESP-Brookesia phone demo
    I (1665) app_main: Using stylesheet (1024x600 Drak)
    [I][Brookesia][esp_brookesia_core.cpp:0213](beginCore): Library version: 0.5.0
    I (1717) MEM:    Biggest /     Free /    Total
            SRAM : [   38912 /   111607 /   431863]
          PSRAM : [29360128 / 29409200 / 30643968]
    ```

- The following animations show the example running on the development board.

<p align="center">
<img src="https://dl.espressif.com/AE/esp-dev-kits/esp_ui_phone_p4_function_ev_board_1024_600_2.gif" alt ="phone_p4_function_ev_board">
</p>

<p align="center">
(<a href="https://dl.espressif.com/AE/esp-dev-kits/esp_ui_phone_demo_1024_600_compress.mp4">Click to view the video</a>)
</p>

## Technical Support and Feedback

Please use the following feedback channels:

- For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=22) forum.
- For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-brookesia/issues).

We will get back to you as soon as possible.
