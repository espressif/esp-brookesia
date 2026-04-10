# RainMaker UI Visual Regression Testing

自动化的视觉回归测试系统，用于验证 UI 在不同分辨率下的显示是否正确。

## 功能

- ✅ 支持多种分辨率测试 (320x240, 480x272, 800x480, 1024x600, 1280x720, 1920x1080)
- ✅ 自动截图所有 UI 屏幕
- ✅ 支持中英文双语测试
- ✅ 像素级对比基准图像
- ✅ 生成差异图像
- ✅ HTML 格式测试报告

## 输出目录与 diff 目录（清理行为）

为避免旧截图或旧差异图残留，每次运行会**先清空再写入**：

- **`rainmaker_ui_visual_test`**：在生成基准（`--create-baseline`，目标为基准目录）或普通截图（`-o <dir>`）时，开始截图前会递归清空该目标目录（POSIX/macOS/Linux；Windows 当前仅确保目录存在，不递归删除）。
- **`visual_test_compare.py`**：写入 `--diff-dir` 前会删除并重建该目录，避免旧的 `*_diff.png` 混入本次报告。

## 文件说明

| 文件 | 说明 |
|------|------|
| `ui_pc_visual_test.c` | C 语言视觉测试程序，负责截图 |
| `visual_test_compare.py` | Python 对比脚本，生成差异报告 |
| `lv_conf.h` | LVGL 配置（已启用 `LV_USE_SNAPSHOT`） |
| `CMakeLists.txt` | 构建配置（已添加测试目标） |

## 快速开始

### 1. 构建测试程序

```bash
cd pc
cmake -B build -S .
cmake --build build
```

### 2. 创建基准图像（首次运行）

不带 `-w`/`-H` 且未设置 `RM_PC_LCD_HOR` / `RM_PC_LCD_VER` 时，`--create-baseline` **默认**按与 `run_visual_tests.sh` 相同的 6 种分辨率依次生成基准图：`320×240`、`480×272`、`800×480`、`1024×600`、`1280×720`、`1920×1080`。输出文件名含分辨率前缀（如 `320x240_Login_en.png`），均写入基准目录（默认 `visual_test_baseline`）。

```bash
# 默认：一次生成上述 6 种分辨率的基准图像
./build/rainmaker_ui_visual_test --create-baseline

# 仅生成单一分辨率（任选其一）
./build/rainmaker_ui_visual_test --create-baseline -w 800 -H 480
RM_PC_LCD_HOR=800 RM_PC_LCD_VER=480 ./build/rainmaker_ui_visual_test --create-baseline
```

### 3. 运行测试并生成截图

不带 `-w`/`-H` 且未设置 `RM_PC_LCD_HOR` / `RM_PC_LCD_VER` 时，**普通截图**（`-o`）与 `--create-baseline` 相同，**默认**按 6 种标准分辨率依次截图，文件名带分辨率前缀（如 `320x240_Login_en.png`），均写入输出目录。

```bash
# 默认：一次生成上述 6 种分辨率的截图 → visual_test_output/
./build/rainmaker_ui_visual_test -o visual_test_output

# 仅单一分辨率（任选其一）
./build/rainmaker_ui_visual_test -w 800 -H 480 -o visual_test_output
RM_PC_LCD_HOR=800 RM_PC_LCD_VER=480 ./build/rainmaker_ui_visual_test -o visual_test_output
```

### 4. 对比截图与基准

```bash
# 安装依赖
pip3 install Pillow

# 运行对比
python3 visual_test_compare.py visual_test_baseline visual_test_output

# 自定义阈值
python3 visual_test_compare.py visual_test_baseline visual_test_output --threshold 0.95

# 生成差异图像
python3 visual_test_compare.py visual_test_baseline visual_test_output --diff-dir diffs
```

### 5. 查看 HTML 报告

```bash
# 报告默认生成在 visual_test_report.html
open visual_test_report.html  # macOS
xdg-open visual_test_report.html  # Linux
```

## 命令行选项

### rainmaker_ui_visual_test

```
用法: ./build/rainmaker_ui_visual_test [选项]

选项:
  -w, --width <px>       水平分辨率 (默认: 1024)
  -H, --height <px>      垂直分辨率 (默认: 600)
  -o, --output <dir>      输出目录 (默认: visual_test_output)
  -b, --baseline <dir>    基准目录（用于比较）
  --create-baseline        创建基准截图
  --compare               与基准比较
  --en-only              仅截图英文 UI
  -h, --help             显示帮助

环境变量:
  RM_PC_LCD_HOR / RM_PC_LCD_VER  设置分辨率

示例:
  ./build/rainmaker_ui_visual_test -o out              # 默认 6 种分辨率
  ./build/rainmaker_ui_visual_test -w 800 -H 480 -o out
  RM_PC_LCD_HOR=320 RM_PC_LCD_VER=240 ./build/rainmaker_ui_visual_test -o out
  ./build/rainmaker_ui_visual_test --create-baseline
```

### visual_test_compare.py

```
用法: python3 visual_test_compare.py baseline_dir output_dir [选项]

选项:
  baseline_dir            包含基准截图的目录
  output_dir              包含当前测试截图的目录
  --threshold, -t         通过阈值 (默认: 0.99)
  --diff-dir, -d          差异图像目录 (默认: visual_test_diff)
  --report, -r            HTML 报告文件 (默认: visual_test_report.html)
  --resolutions           要测试的分辨率，逗号分隔
  --convert              使用 ImageMagick 将 PPM 转换为 PNG

示例:
  python3 visual_test_compare.py visual_test_baseline visual_test_output
  python3 visual_test_compare.py baseline output --threshold 0.95
  python3 visual_test_compare.py baseline output --resolutions "800x480,1024x600"
```

## 使用 CTest 运行测试

```bash
# 运行所有测试
ctest --test-dir build

# 只运行视觉测试
ctest --test-dir build -R visual

# 详细输出
ctest --test-dir build -R visual -V
```

## 集成到 CI/CD

### GitHub Actions 示例

```yaml
name: Visual Regression Tests

on: [push, pull_request]

jobs:
  visual-tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3

      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y libsdl2-dev libcurl4-openssl-dev
          pip3 install Pillow

      - name: Build
        run: |
          cd pc
          cmake -B build -S .
          cmake --build build

      - name: Run visual tests
        run: |
          cd pc
          ./build/rainmaker_ui_visual_test -o test_output
          python3 visual_test_compare.py visual_test_baseline test_output

      - name: Upload results
        uses: actions/upload-artifact@v3
        with:
          name: visual-test-results
          path: |
            pc/test_output/
            pc/visual_test_report.html
```

## 截图格式

当前实现保存为 PPM 格式（简单位图），可以转换为 PNG：

```bash
# 使用 ImageMagick
convert screenshot.ppm screenshot.png

# 或使用 Python
from PIL import Image
Image.open('screenshot.ppm').save('screenshot.png')
```

## 支持的屏幕

| 屏幕名称 | 说明 |
|-----------|------|
| Login | 登录界面 |
| Home | 主页 |
| Schedules | 日程列表 |
| CreateSchedule | 创建日程 |
| Scenes | 场景列表 |
| CreateScene | 创建场景 |
| SelectDevices | 选择设备 |
| SelectActions | 选择动作 |
| User | 用户设置 |
| LightDetail | 灯光详情 |
| SwitchDetail | 开关详情 |
| FanDetail | 风扇详情 |
| DeviceSetting | 设备设置 |

## 故障排除

### 截图为空或黑色

确保：
1. `lv_conf.h` 中 `LV_USE_SNAPSHOT = 1`
2. 重新构建项目
3. 屏幕已正确加载（`lv_disp_load_scr()`）

### 对比全部失败

检查：
1. 基准图像路径是否正确
2. 分辨率是否匹配
3. 图像格式是否一致

### PC 上对比失败，但烧录到设备显示正常

这是预期可能出现的情况，不代表固件 UI 坏了：

- **基准过旧**：改了文案、字体或布局后，旧 baseline 与当前程序截图必然不一致。请在本机重新执行 `--create-baseline`，再跑 `-o` 与 `visual_test_compare.py`。
- **PC 与设备的差异**：视觉测试在主机上用 SDL2 + 主机字体/FreeType 渲染；真机用嵌入式字库与不同 DPI。极少数字形（如 `°`）在两边子集不同时，像素级对比也可能略有差异；以真机为准时，以设备显示为准，PC 侧以「更新 baseline 后仍稳定」为回归目标。

### 内存不足

在 `lv_conf.h` 中增加 `LV_MEM_SIZE`：

```c
#define LV_MEM_SIZE (8U * 1024U * 1024U)   /* 8 MB */
```

## 扩展

### 添加新屏幕

1. 在 `SCREENS` 列表中添加屏幕名称
2. 在 `run_visual_tests()` 函数中添加截图代码

### 添加新分辨率

在 `visual_test_compare.py` 的 `RESOLUTIONS` 列表中添加：

```python
RESOLUTIONS = [
    (800, 600),  # 新分辨率
    # ...
]
```

## 许可证

Apache-2.0
