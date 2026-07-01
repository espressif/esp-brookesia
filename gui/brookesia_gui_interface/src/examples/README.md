# JSON-UI 示例套件

本目录是 `brookesia_gui_interface` 文档（`../../docs`）的配套示例集合。每个示例都是一个实现
`IExample` 接口的插件，内联自带 JSON-UI 文档，通过 `BROOKESIA_PLUGIN_REGISTER` 自动注册到
`ExampleRegistry`。共享的 `ExampleRunner` 把注册表渲染成一个按类别排列的主页菜单（菜单与返回浮层本身
也用 JSON-UI 协议编写），点击条目进入对应示例界面，界面左上角的浮动返回键回到主页。

## 目录结构

每个示例独立一个 `.cpp`，按类别放在同名子目录下（文件名取示例 id 的后缀）：

```text
src/examples/
  common/
    example_base.hpp   # JsonExample 基类、Checker 断言辅助、资产路径工具
    example_macros.h   # 逐示例编译门控宏
  host/example_runner.cpp   # 共享菜单/路由/返回浮层控制器
  document/   # root / variants / environment / constants
  assets/     # constant / styleset / theme / template
  view/       # 18 个 view 控件示例（screen/container/label/.../frame_view）
  styling/    # flex / grid / placement / style / state_styles / style_refs
  interaction/ # bindings / events / animations / screen_flow
  runtime/    # template / theme_language / animation_api / mount_stack
  assets_bin/ # 共享二进制资产（图片/字体），仅少数示例需要
```

`CMakeLists.txt` 用 `file(GLOB_RECURSE ... src/examples/*.cpp)` 递归收集，新增子目录文件会自动纳入；
但该 glob 非 `CONFIGURE_DEPENDS`，新增/移动文件后需重新 `cmake -S . -B build` 配置一次。

公开接口头位于 `../../include/brookesia/gui_interface/examples/{example.hpp,runner.hpp}`。

## 新增一个示例

1. 在对应类别子目录下新增一个 `.cpp`（如 `view/my_widget.cpp`），内含一个类。
   - 若示例只是一段静态 JSON，继承 `detail::JsonExample`，在构造函数里传入
     `ExampleInfo`、虚拟 root 路径、内联 JSON 和要挂载的 screen 绝对路径；需要在挂载后驱动
     Runtime API（bindings/animations/template…）时重写 `on_mounted` / `on_stopping`；
     需要自动验证时重写 `on_verify`（见“自动验证”）。
   - 若示例需要自定义加载顺序（如先 `load_theme_json` + `set_theme` 再加载文档，或使用
     `start_screen_flow` / `push_transient_screen`），直接实现 `IExample`，可按需重写 `verify`。
2. 用 `BROOKESIA_PLUGIN_REGISTER(IExample, YourClass, "category.id")` 注册，`id` 必须全局唯一，
   它同时作为注册表键、菜单 action 后缀和 `ExampleInfo::id`。
3. 在 `common/example_macros.h` 增加 `BROOKESIA_GUI_INTERFACE_EXAMPLE_<ID>` 门控宏，并用
   `#if BROOKESIA_GUI_INTERFACE_EXAMPLE_<ID> ... #endif` 包裹类体与注册宏。
4. 在 `cmake/pc_platform.cmake` 的 `BROOKESIA_GUI_INTERFACE_EXAMPLE_IDS` 列表与 `Kconfig`
   的 Examples 菜单中登记同名 `<ID>`。

## 编译门控

- 每个示例由宏 `BROOKESIA_GUI_INTERFACE_EXAMPLE_<ID>` 控制；关闭即不编译、不链接、不在菜单出现。
- PC：`cmake/pc_platform.cmake` 为每个示例生成 `option(BROOKESIA_GUI_INTERFACE_PC_EXAMPLE_<ID>)`，
  并定义 `CONFIG_BROOKESIA_GUI_INTERFACE_EXAMPLE_<ID>=0/1`。总开关
  `BROOKESIA_GUI_INTERFACE_PC_ENABLE_EXAMPLES`。
- ESP：`Kconfig` 的 “Enable JSON-UI example suite” 菜单逐项开关（`menuconfig`）。

## 在 PC 上运行

PC runner 入口在 `../../gui/brookesia_gui_lvgl/host/main.cpp`，由 `simulator/CMakeLists.txt`
装配为可执行目标 `brookesia_gui_lvgl_examples`（以 whole-archive 链接示例库，确保插件注册符号不被裁剪）。

```bash
cd simulator
cmake -S . -B build
cmake --build build --target brookesia_gui_lvgl_examples
cmake --build build --target run_examples   # 或直接运行 bin/brookesia_gui_lvgl_examples
```

不带参数为交互式菜单。命令行还支持非交互的冒烟/自检模式：

```bash
bin/brookesia_gui_lvgl_examples --list                    # 列出全部已注册示例
bin/brookesia_gui_lvgl_examples --smoke                   # 依次挂载每个示例并自检
bin/brookesia_gui_lvgl_examples --smoke --duration-ms=300 # 调整每项停留时长
bin/brookesia_gui_lvgl_examples --example=view.slider     # 只跑指定示例（隐含 --smoke）
bin/brookesia_gui_lvgl_examples --smoke --no-verify       # 仅挂载，不做自检
bin/brookesia_gui_lvgl_examples --smoke --strict          # 把 SKIP 视为失败
```

## 自动验证

示例可在挂载后回读 UI 状态，断言“是否符合预期”。`--smoke` 默认在每个示例 `run()` 后、`stop()` 前
调用其 `verify()`，输出 `PASS/FAIL/SKIP`；存在 `FAIL`（或 `--strict` 下存在 `SKIP`）时进程以非 0 退出。

- `JsonExample` 子类重写受保护的 `on_verify(Checker&, Runtime&, const Environment&, DocumentId)`：
  返回 `false`（默认）表示“无检查”→ `SKIP`；返回 `true` 则由 `Checker` 是否累计失败决定 `PASS/FAIL`。
- `Checker` 提供断言辅助（路径以 screen 根相对或绝对 `/` 开头）：`require_view`、`check_label_text`、
  `check_slider_value/check_progress_value/check_arc_value`、`check_switch_checked/check_checkbox_checked`、
  `check_dropdown_index`、`check_image_src`、`check_text_input_text`、`check_binding`、
  `check_frame_nonempty`、`check_below`、`check_theme/check_language`、以及通用 `check(cond, msg)`。
- 自定义 `IExample` 可直接重写 `verify(Runtime&, const Environment&, const InputProbe&)` 返回 `VerifyOutcome`。

### 真实点击（行为性验证）

事件驱动的示例（如 `interaction.events`、`interaction.screen_flow`）通过 `Checker::tap(path)` 触发
**真实点击**：runner 在 `InputProbe` 中提供注入能力，底层调用
`service::Display::inject_touch()` 合成触点，覆盖 `get_touch_snapshot`，驱动真实
`lv_indev → LV_EVENT_CLICKED → effects/handlers`。

- `Checker::has_input()` 判断当前平台是否提供注入器；`Checker::tap(path)` 取目标 `frame` 中心点击，
  无几何/注入失败记失败，无注入器返回 `false` 由调用方降级为结构性断言（仍 `PASS`，不 `SKIP`）。
- PC runner（`host/main.cpp`）的 `make_input_probe()` 实现“按下→放锁让 LVGL 计时线程处理→抬起→清除”
  的时序；`verify()` 在持有 LVGL 锁时调用，`tap` 内部临时放锁/再加锁以保持锁平衡。

全部 40 个示例均已实现自检（`--smoke` 期望 40 PASS / 0 SKIP / 0 FAIL）。

## 共享二进制资产

- 绝大多数示例 JSON 内联，无需文件系统。
- 仅图片/字体等真实二进制放在 `assets_bin/`。
- PC：通过编译期宏 `EXAMPLES_ASSETS_DIR` 指向 `assets_bin`，示例用 `detail::make_asset_path()`
  或在 JSON 中以相对 `base_dir` 的方式引用（如 `imageSet.src = "images/brookesia_logo.png"`）。
- ESP（后续）：把 `assets_bin/` 打入 littlefs 分区并挂载到 `/littlefs/assets`，
  `detail::examples_asset_dir()` 已用 `#if ESP_PLATFORM` 统一根路径。

## ESP runner（暂缓）

`examples/gui/test_menu` 的 ESP runner 尚未实现。示例插件与 `ExampleRunner` 在设计上即平台无关：
资产路径已按平台包装、未绑定 PC-only API，后续可直接复用同一套示例库。
