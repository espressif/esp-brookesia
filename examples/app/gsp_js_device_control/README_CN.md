# GSP JavaScript 设备控制

[English](./README.md)

一个面向 ESP-Brookesia GSP 界面的小应用，可以在开发板运行时安装。它显示屏幕亮度，并提供滑块来调节。业务逻辑是 JavaScript，页面是 GSP 场景，两者都可以修改，重新生成安装包后通过 USB 更新应用，无需重新编译或烧录固件。

## 功能

- 两个页面：状态页，以及带滑块的亮度页
- 通过 Display 服务读取和调节背光，并显示服务返回的数值
- 通过 USB 安装、更新和卸载
- 显示在启动页的设置按钮旁

## 环境要求

- 运行 [System GSP App](../../system/gsp_app/README_CN.md) 并使用 GSP 界面（默认）的 ESP32-P4 Function EV Board
- Python 3
- GSP 场景编译器：`python -m pip install -U esp-gsp-tools`
- USB 命令行工具：`python -m pip install brookesia-usb`

## 构建

```bash
python3 tools/build_package.py --version 0.1.0
```

安装包生成在 `build/brookesia.gsp.device_control.debug.0.1.0.bpk`。

如果构建时找不到 `gspc`，请把 `GSPC_EXECUTABLE` 设为它的路径，或使用 `--gspc <path>` 指定。

## 安装

连接开发板的 USB Serial/JTAG 接口，等启动页出现，并关闭所有串口监视器，然后执行：

```bash
brookesia-usb install build/brookesia.gsp.device_control.debug.0.1.0.bpk
```

**Device Control** 会出现在启动页的设置按钮旁。启动页最多显示四个已安装的应用，重启后依然保留。

## 更新

用更高的版本号重新构建，再用同样的命令安装，已安装的版本会被替换。

```bash
python3 tools/build_package.py --version 0.2.0
brookesia-usb install build/brookesia.gsp.device_control.debug.0.2.0.bpk
```

损坏的安装包，或为其他界面构建的安装包会被拒绝，已安装的版本继续可用。

## 卸载

```bash
brookesia-usb call SystemCore ListApps '{}'
brookesia-usb call SystemCore UninstallApp '{"AppId": <id>}'
```

`<id>` 取第一条命令输出中 `brookesia.gsp.device_control` 的 `app_id`。重新安装后 id 可能变化，卸载前请重新列出。

## 使用方法

1. 在启动页点击 **Device Control**。
2. 状态页显示包版本和当前亮度。
3. 点击 **Brightness**，拖动滑块。屏幕亮度随之改变，显示的数值是从 Display 服务读回的。
4. 点击 **Back** 回到状态页，点击 **Exit** 回到启动页。

## 自定义

| 路径 | 用途 |
| --- | --- |
| `package/app/app.js` | 应用逻辑 |
| `scenes/status.json`、`scenes/brightness.json` | 两个页面 |
| `package/manifest.json` | 名称、版本和所需服务 |
| `tools/build_package.py` | 编译页面并生成 `.bpk` |

修改后用新的 `--version` 重新构建并再次安装。

### JavaScript 接口

`app.js` 定义 `globalThis.brookesia_app`，包含以下回调：

| 回调 | 调用时机 |
| --- | --- |
| `on_start()` | 应用打开 |
| `on_stop()` | 应用关闭 |
| `on_action(action, path, payloadJson)` | 页面控件被操作，或页面切换完成 |
| `on_event(serviceName, eventName, itemsJson)` | 收到已订阅的服务事件 |

`on_action` 会收到两种动作：

- `gsp.call`：带 `callback` 的控件被操作。载荷包含 `scene`、`action_id`、`arg` 和 `key`，其中 `key` 是场景 JSON 里回调名称的 FNV-1a 哈希，计算方法见 `app.js`。
- `gsp.scene`：用 `gsp_show_page` 请求的页面已经显示。新页面上的控件到这时才存在，请在这里更新它们。

修改屏幕内容用以下函数：

| 函数 | 作用 |
| --- | --- |
| `brookesia.gsp_set_text(name, text)` | 设置文本标签的内容 |
| `brookesia.gsp_set_value(name, value)` | 设置滑块的数值 |
| `brookesia.gsp_show_page(scene)` | 切换到指定序号的页面，序号按构建顺序 |

文本标签只有在场景 JSON 里设置了 `bind` 才能修改。调用服务用 `brookesia.call_service_function`，应用需要的服务写在 `manifest.json` 中。

## 说明

- 安装包只能运行在使用 GSP 界面的固件上。安装到 LVGL 固件会被拒绝。
- `python3 tools/build_package.py --preview` 在 ESP-GSP 模拟器（esp-gsp-tools 里的 `gsp_sim_host`）中显示页面，只展示布局，不运行 `app.js`，也不会访问 Display 服务。
- Figma 导出生成的是 LVGL JSON UI 包，不能生成 GSP 页面。

## 常见问题

| 现象 | 处理方法 |
| --- | --- |
| `brookesia-usb` 超时，或提示端口被占用 | 关闭占用开发板端口的串口监视器等程序 |
| 复位后第一条命令失败，或列表为空 | 等启动页出现后再执行一次 |
| 安装失败，提示 GUI backend | 固件使用的是 LVGL 界面，请构建并烧录 GSP 界面 |
