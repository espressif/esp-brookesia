# brookesia_app_rainmaker

[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)

[English](README.md) | 中文

## 概述

**brookesia_app_rainmaker** 是基于 [ESP-Brookesia](https://github.com/espressif/esp-brookesia) 手机框架的 **ESP RainMaker** 风格智能家居应用：在 LVGL 上提供登录、首页、设备、场景、日程、自动化与用户设置等界面。应用以 Brookesia `App` 插件形式注册；业务逻辑与可选的 **PC 端模拟构建**（[`pc/`](pc/)）共享，便于无硬件时开发与测试 UI。

- **许可协议：** [Apache-2.0](https://www.apache.org/licenses/LICENSE-2.0)（各源文件含 SPDX 头）。
- **典型目标芯片：** 使用 phone 系统的 ESP32-S3 / ESP32-P4 产品（例如 `products/phone/phone_s3_box_3`、`products/phone/phone_p4_function_ev_board`）。

## RainMaker 使用前提与限制

### 使用前准备

1. 使用 **RainMaker Home** 或旧版 **ESP RainMaker** 移动应用，将标准 **灯 / 开关 / 风扇** 等 RainMaker 设备完成配网，并绑定到你的 RainMaker 账号。
2. 在本应用中使用**同一账号**登录后，即可在设备列表中看到上述设备；控制能力与 RainMaker Home 类似。

### 标准设备固件

标准灯、开关、风扇等设备的参考固件，可通过网页 **[Tryout Center](https://evaluation.rainmaker.espressif.com/#tryout-center)** 获取并烧录。

### 配套应用下载

| 应用 | 说明 |
|------|------|
| **RainMaker Home** | [下载与入门](https://docs.rainmaker.espressif.com/docs/dev/phone-app/home-app/home-app-get-started/#download-esp-rainmaker-home) |
| **ESP RainMaker（旧版，Android）** | [Google Play](https://play.google.com/store/apps/details?id=com.espressif.rainmaker) |
| **ESP RainMaker（旧版，iOS）** | 需使用**非中国大陆区** Apple ID，在 [App Store](https://apps.apple.com/app/esp-rainmaker/id1497491540) 安装。 |

### 当前限制

1. **设备状态不会自动同步到界面。** 设备端暂不具备与手机端对等的通知机制，列表与详情中的状态变化需**手动刷新**后才能看到；后续计划通过集成 RainMaker Controller 等相关能力，实现状态自动更新。
2. **暂不支持在本应用内为设备配网。** 配网仍需通过 RainMaker Home 或旧版 ESP RainMaker 完成；后续版本可能增加在本应用内配网的能力。

## 用户使用说明

[用户使用说明](USER_GUIDE_CN.md)

## 功能概要

- RainMaker 相关流程：设备列表、标准灯 / 开关 / 风扇详情、场景与日程等。
- 国际化接口（`ui_i18n`）及 `ui/fonts/` 下的中文字体资源。
- `backend/` 下与固件、PC 桩共用的后端辅助逻辑。

## 依赖项

见 [`idf_component.yml`](idf_component.yml)：

| 组件 | 作用 |
|------|------|
| `brookesia_core` | Brookesia phone 框架（manifest 内 `override_path`）。 |
| `espressif/rmaker_user_api` | RainMaker User API 头文件与集成。 |

其余依赖由**具体产品工程**的固件拉取（LVGL、Wi-Fi、RainMaker 协议栈等）。

## 接入固件工程

1. 在产品工程的 `main/idf_component.yml` 中增加本组件，例如：

   ```yaml
   dependencies:
     brookesia_app_rainmaker:
       version: "*"
       override_path: "../../../../apps/brookesia_app_rainmaker"
   ```

   请按你工程中 `main` 目录相对路径调整 `override_path`。

2. 确保 `brookesia_core` 及 RainMaker 相关组件与该产品已使用版本一致。

3. 在产品目录下（如 `products/phone/phone_s3_box_3`）执行：

   ```bash
   idf.py set-target esp32s3
   idf.py build flash monitor
   ```

应用在 [`esp_brookesia_app_rainmaker.cpp`](esp_brookesia_app_rainmaker.cpp) 中以名称 **`ESP RainMaker`** 通过 `ESP_UTILS_REGISTER_PLUGIN_WITH_CONSTRUCTOR` 注册。

## 目录结构

| 路径 | 说明 |
|------|------|
| `esp_brookesia_app_rainmaker.cpp` | 应用入口：`RainmakerDemo`、LVGL UI 初始化/反初始化。 |
| `ui/` | LVGL 界面、图片、字体、国际化与手势。 |
| `backend/` | RainMaker 后端衔接（设备上编译时 `CMakeLists.txt` 会排除 `pc/`）。 |
| `pc/` | SDL2 + LVGL 主机模拟器与冒烟/布局/视觉测试 — 见 [`pc/README_CN.md`](pc/README_CN.md)。 |

## CMake 说明

[`CMakeLists.txt`](CMakeLists.txt) 通过 `GLOB` 收集 `*.c` / `*.cpp`，并 **排除** `pc/**`，因此模拟器源码不会参与设备端编译。新增 UI 源文件后若未生效，建议执行 `idf.py fullclean` 后再编译。

## 相关资源

- [ESP-IDF 版权头规范](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/contribute/copyright-guide.html)
- [ESP RainMaker](https://github.com/espressif/esp-rainmaker)

## 许可协议

SPDX-License-Identifier: Apache-2.0

版权所有 2026 乐鑫信息科技（上海）股份有限公司。具体以各源文件 SPDX 头为准。
