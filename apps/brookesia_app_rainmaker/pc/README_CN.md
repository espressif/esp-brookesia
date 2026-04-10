# RainMaker UI — PC 端模拟器（SDL2 + LVGL）

[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)

[English](README.md) | 中文

## 概述

本目录在 PC 上构建 **RainMaker 手机 UI 的桌面模拟器**：使用 **SDL2** 显示、**LVGL 8**，与固件共用 [`../ui`](../ui) 与 [`../backend`](../backend) 源码，通过主机桩文件（`rainmaker_*_pc.c`）替代 ESP-IDF 驱动。

适用于在不烧录 ESP32 的情况下调试布局、国际化与导航。若安装 **libcurl**，可更接近真实 RainMaker 传输路径（`RM_PC_HAVE_CURL`）。

- **许可协议：** Apache-2.0（见 `CMakeLists.txt` 与各源文件 SPDX）。
- **环境要求：** CMake **3.16+**、C99、**SDL2**、ESP-IDF **`cJSON.c`**、LVGL 源码树、**`app_rmaker_user_api.h`**（来自 Component Manager / `managed_components`）。

## 环境准备

### 1. ESP-IDF（仅用于编译 `cJSON.c`）

构建脚本通过环境变量 **`IDF_PATH`** 定位 ESP-IDF 中的 `components/json/cJSON/cJSON.c`：

```bash
export IDF_PATH=/path/to/esp-idf
```

若未设置 `IDF_PATH`，可手动指定：

```bash
cmake -B build -S . -DCJSON_SRC=/path/to/esp-idf/components/json/cJSON/cJSON.c
```

### 2. LVGL（`lvgl__lvgl`）

建议先在手机产品工程中拉取一次 managed 组件，保证与固件 LVGL 版本一致：

```bash
cd /path/to/esp-brookesia/products/phone/phone_s3_box_3
idf.py reconfigure
```

CMake 会按顺序自动查找，例如：

- `products/phone/phone_s3_box_3/managed_components/lvgl__lvgl`
- 或仓库根目录 `managed_components/lvgl__lvgl`

也可手动指定：

```bash
export LVGL_ROOT=/绝对路径/lvgl__lvgl
# 或
cmake -B build -S . -DLVGL_ROOT=/绝对路径/lvgl__lvgl
```

### 3. RainMaker User API 头文件

需存在 **`app_rmaker_user_api.h`**，通常在：

`.../managed_components/espressif__rmaker_user_api/include`

在依赖 `espressif/rmaker_user_api` 的产品目录执行 `idf.py reconfigure`，或：

```bash
cmake -B build -S . -DRMAKER_USER_API_INC=/path/to/include
```

### 4. 系统依赖

| 系统 | 示例包 |
|------|--------|
| **Ubuntu / Debian** | `sudo apt install cmake build-essential libsdl2-dev libcurl4-openssl-dev` |
| **macOS（Homebrew）** | `brew install cmake sdl2 curl` |
| **Windows** | 安装 CMake、SDL2 开发库；可选安装 curl；路径尽量避免空格。 |

**SDL2** 为必需。**libcurl** 可选；未找到时仍可配置工程，但不会定义 `RM_PC_HAVE_CURL`（见 `CMakeLists.txt`）。

### 5. Python（可选，用于视觉对比）

使用 [`visual_test_compare.py`](visual_test_compare.py) 时：

```bash
pip3 install Pillow
```

## 配置与编译

在 **`pc/`** 目录下执行：

```bash
export IDF_PATH=/path/to/esp-idf
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

Release 构建：

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

## 运行模拟器

主程序：**`rainmaker_ui_pc`**

```bash
./build/rainmaker_ui_pc
```

分辨率（命令行或环境变量）：

```bash
./build/rainmaker_ui_pc --width 1280 --height 720
./build/rainmaker_ui_pc -w 1280 -H 720
export RM_PC_LCD_HOR=1024 RM_PC_LCD_VER=600
./build/rainmaker_ui_pc
```

## 其他构建目标

| 目标 | 说明 |
|------|------|
| `rainmaker_ui_pc_smoke` | 冒烟 / 最小运行（CTest 使用）。 |
| `rainmaker_ui_layout_test` | 布局几何检查（如 Select Devices 行）。 |
| `rainmaker_ui_visual_test` | 截图用于视觉回归（需 `lv_conf.h` 中 LVGL 快照等配置）。 |
| `http_smoke` | 仅在检测到 **CURL** 时编译的简单 HTTP 测试。 |

## 运行测试（CTest）

```bash
cd build
ctest --output-on-failure
# 或在 pc/ 目录下：
ctest --test-dir build
```

部分用例通过环境变量 `RM_PC_LCD_HOR` / `RM_PC_LCD_VER` 覆盖多分辨率。

## 视觉回归（可选）

详细步骤见 **[VISUAL_TEST_README.md](VISUAL_TEST_README.md)**（基准图、`visual_test_compare.py`、HTML 报告）。每次生成基准或 `-o` 输出前会清空目标目录；`visual_test_compare.py` 在写入 diff 前会清空 `--diff-dir`。简要流程：

```bash
./build/rainmaker_ui_visual_test --create-baseline   # 首次生成基准（默认 6 种分辨率；单分辨率见 VISUAL_TEST_README）
./build/rainmaker_ui_visual_test -o visual_test_output   # 截图同样默认 6 种分辨率（单档见 VISUAL_TEST_README）
python3 visual_test_compare.py visual_test_baseline visual_test_output
```

也可使用辅助脚本 [`run_visual_tests.sh`](run_visual_tests.sh)。

## 常见问题

| 现象 | 处理建议 |
|------|----------|
| `LVGL not found` | 设置 `LVGL_ROOT`，或在 `phone_s3_box_3` 下执行 `idf.py` 拉取 `managed_components`。 |
| 找不到 `cJSON.c` | 设置 `IDF_PATH` 或 `-DCJSON_SRC=...`。 |
| 找不到 `app_rmaker_user_api.h` | 在含 `rmaker_user_api` 的产品中 `idf.py reconfigure`，或 `-DRMAKER_USER_API_INC=...`。 |
| SDL 窗口异常 | 检查 SDL2 安装；Linux 确认已安装 `libsdl2-dev`。 |

## 相关文档

- [应用说明（英文）](../README.md) / [中文](../README_CN.md)
- [ESP-IDF 入门指南](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html)

## 许可协议

SPDX-License-Identifier: Apache-2.0

版权所有 2026 乐鑫信息科技（上海）股份有限公司。
