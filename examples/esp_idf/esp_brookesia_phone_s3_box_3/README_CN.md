# ESP32-S3-BOX-3 运行 ESP-Brookesia Phone 示例

[English Version](./README.md)

这个示例演示了如何在 [ESP32-S3-BOX-3](https://github.com/espressif/esp-box/tree/master) 开发板上运行 ESP-Brookesia Phone，并使用 `320 x 240` 分辨率的 UI 样式表。

## 入门指南

### 硬件要求

* 一个 ESP32-S3-BOX-3 开发板。

### ESP-IDF 要求

- 此示例支持 IDF release/v5.1 及更高的分支。默认情况下，它在 IDF release/v5.1 上运行。
- 请按照 [ESP-IDF 编程指南](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html) 设置开发环境。**我们强烈建议**您 [构建第一个项目](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html#build-your-first-project)，以熟悉 ESP-IDF 并确保环境设置正确。

### 获取 esp-brookesia 仓库

要从 esp-brookesia 示例开始，请在终端运行以下命令，将仓库克隆到本地电脑：

```
git clone --recursive https://github.com/espressif/esp-brookesia.git
```

### 配置

  运行 ``idf.py menuconfig`` 并修改 esp-brookesia 配置。

## 如何使用示例

### 构建和烧录示例

构建项目并将其烧录到开发板，然后运行监视工具查看串行输出（将 `PORT` 替换为您的开发板串口名称）：

```c
idf.py -p PORT flash monitor
```

要退出串行监视器，请输入 ``Ctrl-]``。

完整的配置和使用 ESP-IDF 构建项目的步骤，请参见 [ESP-IDF 入门指南](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/get-started/index.html)。

### 示例输出

- 完整日志如下：

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

- 以下动画展示了示例在开发板上运行的效果。

<p align="center">
<img src="https://dl.espressif.com/AE/esp-dev-kits/esp_ui_phone_s3_box_3.gif" alt ="esp_brookesia_phone_s3_box_3" width="500">
</p>

<p align="center">
（<a href="https://dl.espressif.com/AE/esp-dev-kits/esp_ui_phone_s3_box_3.mp4">点击查看视频</a>）
</p>

## 技术支持和反馈

请使用以下反馈渠道：

- 有技术问题，请访问 [esp32.com](https://esp32.com/viewforum.php?f=35) 论坛。
- 如需提交功能请求或错误报告，请创建 [GitHub issue](https://github.com/espressif/esp-brookesia/issues)。

我们会尽快回复您。
