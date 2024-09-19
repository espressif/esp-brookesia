# ESP32-S3-BOX Running ESP-Brookesia Phone Example

[中文版本](./README_CN.md)

This example demonstrates how to run the ESP-Brookesia Phone on the [ESP32-S3-BOX](https://github.com/espressif/esp-box/tree/master) with `320 x 240` resolution UI stylesheet.

## Getting Started

### Hardware Requirements

* An ESP32-S3-BOX.

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
    I (26) boot: ESP-IDF v5.1.4-605-g5c57dfe949 2nd stage bootloader
    I (27) boot: compile time Aug 22 2024 13:59:10
    I (27) boot: Multicore bootloader
    I (27) boot: chip revision: v0.1
    I (27) qio_mode: Enabling default flash chip QIO
    I (28) boot.esp32s3: Boot SPI Speed : 80MHz
    I (28) boot.esp32s3: SPI Mode       : QIO
    I (28) boot.esp32s3: SPI Flash Size : 16MB
    I (29) boot: Enabling RNG early entropy source...
    I (29) boot: Partition Table:
    I (29) boot: ## Label            Usage          Type ST Offset   Length
    I (29) boot:  0 nvs              WiFi data        01 02 00009000 00006000
    I (30) boot:  1 phy_init         RF data          01 01 0000f000 00001000
    I (30) boot:  2 factory          factory app      00 00 00010000 00400000
    I (31) boot: End of partition table
    I (31) esp_image: segment 0: paddr=00010020 vaddr=3c080020 size=19d088h (1691784) map
    I (288) esp_image: segment 1: paddr=001ad0b0 vaddr=3fc9a900 size=02e34h ( 11828) load
    I (291) esp_image: segment 2: paddr=001afeec vaddr=40374000 size=0012ch (   300) load
    I (291) esp_image: segment 3: paddr=001b0020 vaddr=42000020 size=77f4ch (491340) map
    I (366) esp_image: segment 4: paddr=00227f74 vaddr=4037412c size=166f0h ( 91888) load
    I (393) boot: Loaded app from partition at offset 0x10000
    I (394) boot: Disabling RNG early entropy source...
    I (394) cpu_start: Multicore app
    I (394) octal_psram: vendor id    : 0x0d (AP)
    I (395) octal_psram: dev id       : 0x02 (generation 3)
    I (395) octal_psram: density      : 0x03 (64 Mbit)
    I (395) octal_psram: good-die     : 0x01 (Pass)
    I (396) octal_psram: Latency      : 0x01 (Fixed)
    I (396) octal_psram: VCC          : 0x01 (3V)
    I (396) octal_psram: SRF          : 0x01 (Fast Refresh)
    I (396) octal_psram: BurstType    : 0x01 (Hybrid Wrap)
    I (397) octal_psram: BurstLen     : 0x01 (32 Byte)
    I (397) octal_psram: Readlatency  : 0x02 (10 cycles@Fixed)
    I (397) octal_psram: DriveStrength: 0x00 (1/1)
    I (398) MSPI Timing: PSRAM timing tuning index: 5
    I (399) esp_psram: Found 8MB PSRAM device
    I (399) esp_psram: Speed: 80MHz
    I (439) mmu_psram: Instructions copied and mapped to SPIRAM
    I (563) mmu_psram: Read only data copied and mapped to SPIRAM
    I (563) cpu_start: Pro cpu up.
    I (563) cpu_start: Starting app cpu, entry point is 0x4037580c
    0x4037580c: call_start_cpu1 at /home/lzw/esp/work/esp-idf-github/components/esp_system/port/cpu_start.c:159

    I (0) cpu_start: App cpu up.
    I (867) esp_psram: SPI SRAM memory test OK
    I (876) cpu_start: Pro cpu start user code
    I (876) cpu_start: cpu freq: 240000000 Hz
    I (877) cpu_start: Application information:
    I (877) cpu_start: Project name:     esp_brookesia_phone_s3_box
    I (877) cpu_start: App version:      v0.1.1-7-gaed94e2-dirty
    I (877) cpu_start: Compile time:     Aug 22 2024 13:58:19
    I (878) cpu_start: ELF file SHA256:  22d4d2eb050b2593...
    I (878) cpu_start: ESP-IDF:          v5.1.4-605-g5c57dfe949
    I (878) cpu_start: Min chip rev:     v0.0
    I (879) cpu_start: Max chip rev:     v0.99
    I (879) cpu_start: Chip rev:         v0.1
    I (879) heap_init: Initializing. RAM available for dynamic allocation:
    I (880) heap_init: At 3FC9E8A8 len 0004AE68 (299 KiB): DRAM
    I (880) heap_init: At 3FCE9710 len 00005724 (21 KiB): STACK/DRAM
    I (880) heap_init: At 3FCF0000 len 00008000 (32 KiB): DRAM
    I (881) heap_init: At 600FE010 len 00001FD8 (7 KiB): RTCRAM
    I (881) esp_psram: Adding pool of 6016K of PSRAM memory to heap allocator
    I (882) spi_flash: detected chip: gd
    I (882) spi_flash: flash io: qio
    I (882) sleep: Configure to isolate all GPIO pins in sleep state
    I (883) sleep: Enable automatic switching of GPIO sleep configuration
    I (883) app_start: Starting scheduler on CPU0
    I (883) app_start: Starting scheduler on CPU1
    I (883) main_task: Started on CPU0
    I (884) esp_psram: Reserving pool of 32K of internal memory for DMA/internal allocations
    I (884) main_task: Calling app_main()
    I (885) LVGL: Starting LVGL task
    I (885) gpio: GPIO[4]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
    I (886) gpio: GPIO[48]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
    I (1007) gpio: GPIO[3]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:2
    I (1008) ESP-BOX: Setting LCD backlight: 100%
    I (1008) app_main: Display ESP BROOKESIA phone demo
    I (1008) app_main: Using external stylesheet
    I (1009) esp-brookesia: [esp_brookesia_core.cpp:150](beginCore)Library version: 0.1.0
    I (1078) app_main:    Biggest /     Free /    Total
              SRAM : [  151552 /   245703 /   402787]
            PSRAM : [ 6029312 /  6157156 /  6160384]
    ```

## Technical Support and Feedback

Please use the following feedback channels:

- For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=22) forum.
- For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-brookesia/issues).

We will get back to you as soon as possible.
