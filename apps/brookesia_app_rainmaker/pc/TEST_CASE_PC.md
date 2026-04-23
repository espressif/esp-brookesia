# PC 版 RainMaker UI 测试说明

## 环境

- 依赖：CMake、SDL2、libcurl（可选）、ESP-IDF 的 `cJSON.c`、`IDF_PATH`、`managed_components` 中的 LVGL 与 `rmaker_user_api` 头文件（见 `CMakeLists.txt` 注释）。
- 配置与构建：

```bash
export IDF_PATH=/path/to/esp-idf
cd components/apps/rainmaker/pc
cmake -B build -S .
cmake --build build
```

## 自动化用例

### 1. `rainmaker_ui_pc_smoke`（无头 / 无窗口）

- **目的**：在无 SDL 窗口、仅注册最小 LVGL 显示驱动的情况下，创建全部 `RM_HOST_BUILD` 界面、校验中英文 `ui_str` 表无漏项、校验 `ui_display` 缩放与色相字符串格式化。
- **运行**：`./build/rainmaker_ui_pc_smoke`（默认 1024×600）
- **分辨率**：与 `rainmaker_ui_pc` 一致，可用 `-w` / `-H` 或环境变量 `RM_PC_LCD_HOR`、`RM_PC_LCD_VER`（合法范围 240–4096）。示例：`RM_PC_LCD_HOR=320 RM_PC_LCD_VER=240 ./build/rainmaker_ui_pc_smoke` 或 `./build/rainmaker_ui_pc_smoke -w 320 -H 240`。
- **CTest**：`ctest --test-dir build`，包含：
  - `pc_ui_smoke`：默认逻辑分辨率（1024×600）
  - `pc_ui_smoke_320`：320×240
  - `pc_ui_smoke_1024`：1024×600（显式环境变量，与默认等价）
- **说明**：当前无头流程**不循环调用** `lv_timer_handler()`（与带 SDL 的 `./rainmaker_ui_pc` 不同），以避免在部分平台上触发的定时器相关崩溃；完整帧推进与交互仍以 SDL 可执行文件为准。无头测试仅调用 `lv_theme_basic_init` 以初始化主题相关静态资源，**不**调用 `lv_disp_set_theme`（避免在部分主机上对默认屏做全量主题套用时的间歇性 fault；SDL 可执行文件仍会正常 `lv_disp_set_theme`）。多分辨率下 smoke 可快速回归 i18n 与界面图构建，**不能替代** SDL 下的视觉与点击路径测试。

### 2. `rainmaker_ui_layout_test`（无头 / 几何断言）

- **目的**：在 `RM_HOST_BUILD` 下注入最小首页设备数据（见 `rainmaker_app_backend_layout_test.c`），打开「选择设备」屏，对首行 picker（图标 / 文本槽 / 名称标签）做**几何不变量**断言：名称与文本槽不得与图标水平重叠、名称不得明显偏到行左侧。用于在 CI 中捕获窄屏下「图标与文字叠在一起」类布局回归。
- **与 smoke 的分工**：`rainmaker_ui_pc_smoke` 覆盖 **i18n / 全屏创建 / 显示缩放** 等静态检查；`rainmaker_ui_layout_test` 覆盖 **特定屏在固定分辨率下的控件相对位置**。二者均为无头、不泵 `lv_timer_handler()`；**不**断言像素/抗锯齿，也**不**替代 SDL 手动点击与全界面目视检查。
- **运行**：`./build/rainmaker_ui_layout_test`（默认 1024×600）；`./build/rainmaker_ui_layout_test -w 320 -H 240` 或 `RM_PC_LCD_HOR=320 RM_PC_LCD_VER=240 ./build/rainmaker_ui_layout_test`。
- **CTest**：`ctest --test-dir build -R pc_ui_layout`，包含 `pc_ui_layout_default`、`pc_ui_layout_320`、`pc_ui_layout_1024`。

### 3. `http_smoke`（网络，可选）

- 在已链接 libcurl 时构建，用于探测 `api.rainmaker.espressif.com` 连通性（与 UI 无直接耦合）。

## 手动用例（SDL 真机/桌面）

### 4. `rainmaker_ui_pc`

- **运行**：`./build/rainmaker_ui_pc`
- **分辨率**：`./build/rainmaker_ui_pc --width 1280 --height 720`（短选项 `-w` / `-H`）；未传参时可使用环境变量 `RM_PC_LCD_HOR`、`RM_PC_LCD_VER`（合法范围 240–4096，默认 1024×600）。`--help` 打印用法。
- **建议步骤**：
  1. 启动后窗口标题为 RainMaker PC 登录子集；确认无立即崩溃。
  2. 登录或未登录路径下切换 Home / 日程 / 场景 / 用户等（与当前 PC 裁剪范围一致）。
  3. 在「用户」中切换语言 EN/中文，确认主要界面文案刷新。
  4. 用键盘左右键验证手势导航回退（见 `main_pc.c`）。

## 5. 多分辨率全界面检查（320×240 与 1024×600）

PC 端 **不提供** Automations 页面（与固件差异）；视觉回归脚本亦不再截取/对比该屏。PC 上需覆盖的界面如下；请在 **两种分辨率** 下各执行一遍 SDL 手动步骤，并在表中记录现象（通过 / 问题描述 + 截图路径）。

| 类别 | 界面 |
|------|------|
| 入口 | Login |
| 主导航（四 Tab：Home → Schedules → Scenes → User） | Home、Schedules、Scenes、User |
| 流程页 | CreateSchedule、CreateScene、SelectDevices、SelectActions（SelectDevices 行内布局可由 `rainmaker_ui_layout_test` 做自动化几何检查） |
| 设备详情 | Light Detail、Switch Details、Fan Details、Device Setting |

**启动命令（构建目录 `build` 下）：**

```bash
./build/rainmaker_ui_pc -w 320 -H 240
./build/rainmaker_ui_pc -w 1024 -H 600
```

**每种分辨率建议步骤：**

1. 启动无崩溃；四 Tab 切换（与 §4 一致）；用户页语言 EN/中文；键盘左右键手势导航（见 `main_pc.c` / `ui_gesture_nav.c`）。
2. **视觉**：无文字重叠、无控件裁切、无不可点区域（320×240 为极端比例，重点排查）。
3. **全页面可达**：Login → Home；各 Tab 可滚动处滚到底；Schedules → CreateSchedule；Scenes → CreateScene → 必要时 SelectDevices / SelectActions；从 Home/设备列表进入 Light / Switch / Fan 详情（依赖 PC stub 是否有设备，无设备则注明「未覆盖」）；从详情进 Device Setting（若流程存在）。
4. **Msgbox**：触发至少一次错误/成功类提示，确认小屏上弹窗完整。

**手动记录表（按需填写）：**

| 分辨率 | 界面/步骤 | 结果 |
|--------|-----------|------|
| 320×240 | … | 通过 / … |
| 1024×600 | … | 通过 / … |

## 回归记录（本仓库近期修复）

- **i18n**：补全 `UI_STR_TIME_FORMAT_PM` / `UI_STR_TIME_FORMAT_AM` 在中英文字符串表中的条目，避免 `ui_str` 对未初始化下标返回空串（由 `rainmaker_ui_pc_smoke` 捕获）。
- **多分辨率 smoke**：`rainmaker_ui_pc_smoke` 支持运行时分辨率（堆上 line buffer），CTest 增加 `pc_ui_smoke_320` / `pc_ui_smoke_1024`。
- **布局几何**：`rainmaker_ui_layout_test` + `pc_ui_layout_*` 对 Select Devices 首行做无头几何断言（见上文 §2）。

## 可选扩展：SDL 离屏截图 / Golden 像素比对

本仓库**未**默认启用 PNG golden 流水线。若需覆盖「渲染观感」类回归，可在 `rainmaker_ui_pc` + SDL 路径上增加：固定 `--width`/`--height`、语言与假数据、关闭动画后导出 framebuffer 为 PNG，再用 `perceptualdiff` / `compare -metric AE` 等与仓库内 **golden** 对比；更新基线时设置环境变量（例如 `UPDATE_GOLDEN=1`）本地生成再提交。Linux CI 可考虑 `xvfb-run`。维护成本与机器间字体渲染差异需自行评估；**优先级低于** `rainmaker_ui_layout_test` 的几何断言。
