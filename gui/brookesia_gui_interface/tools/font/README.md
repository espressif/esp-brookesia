# Brookesia 中文字体字符集裁剪工具

`charset_filter.py` 用于把较大的中文字符集文件，例如 `charset_zh_en_gbk.txt`，裁剪成更适合 **Brookesia / ESP32 / LVGL** 使用的中英文字符集，并可选地调用 `fonttools subset` 直接生成裁剪后的 `.ttf` / `.otf` / `.woff` / `.woff2` 字体文件。

该工具适合用于文泉驿微米黑、思源黑体、Noto Sans CJK SC 等中文字体的嵌入式字库裁剪流程。

> 注意：本工具不会下载、打包或分发任何字体文件。字体文件需要你在本机自行安装，并在使用时遵守对应字体的授权协议。

---

## 1. 适用场景

Brookesia 这类运行在 ESP32 / LVGL 上的系统 UI 通常不适合直接内置完整中文字体。完整 CJK 字体往往有数 MB 到数十 MB，不利于固件体积、Flash 占用和资源管理。

该脚本可以帮助你生成不同级别的字符集：

| 字符集级别 | 说明 | 适合场景 |
|---|---|---|
| `none` | 不保留 GB 汉字，只保留英文、数字、符号和强制保留字符 | 纯英文 UI，或只保留项目文案 |
| `l1` | 保留 GB2312 一级常用汉字 | 嵌入式系统 UI 推荐 |
| `l2` | 保留 GB2312 一级 + 二级汉字 | 需要显示更多动态中文内容 |
| `gbk` | 保留 GBK 中可解码出的 CJK 汉字 | 覆盖更广，但体积更大 |

对于 Brookesia，推荐优先使用：

```text
固定系统 UI：GB2312 一级字 + UI 文案强制保留
动态中文文本：GB2312 一级 + 二级字
极限小体积：只保留 UI 文案字符
```

---

## 2. 文件说明

```text
charset_filter.py
```

主脚本，用于过滤字符集并可选生成子集字体。

> 输出目录约定：`--output` / `--removed` / `--stats` / `--subset-output` 传入相对路径时，会自动写入到脚本所在目录下的 `build/` 子目录；传入绝对路径则保持原样。可以使用 `--build-dir <path>` 覆盖默认的 `build/` 位置。

常见输入输出文件如下：

```text
charset_zh_en_gbk.txt
```

输入字符集文件。通常包含英文、数字、常用符号和 GBK 汉字。

```text
charset_zh_en_gb2312_l1.txt
```

输出字符集文件。只保留 GB2312 一级常用字、中英文基础符号，以及手动强制保留的 UI 文案字符。

```text
removed_chars.txt
```

被裁掉的字符，方便人工检查。

```text
stats.json
```

统计信息，包括输入字符数量、保留字符数量、删除字符数量、GB 级别等。

```text
brookesia_strings.txt
```

项目 UI 文案文件。文件中的字符会被强制保留，适合放置 Brookesia Settings App、Wi‑Fi、Display、Sound、About 等界面文案。

---

## 3. 环境准备

### 3.1 基础环境

脚本的字符集过滤功能只依赖 Python 标准库。

建议环境：

```text
Python 3.10+
```

检查版本：

```bash
python3 --version
```

### 3.2 可选：安装 fonttools

如果只想生成字符集 `.txt`，不需要安装 fonttools。

如果希望一步生成裁剪后的字体文件，需要安装 fonttools：

```bash
pip install fonttools
```

Ubuntu / Debian 上可以使用虚拟环境：

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install fonttools
```

---

## 4. 准备输入字符集

假设你已经有：

```text
charset_zh_en_gbk.txt
```

如果没有，可以用前置脚本或其他方式生成一个包含中英文字符的基础字符集。该工具的核心目标是从这个较大的字符集里进一步裁剪出更适合 Brookesia 使用的版本。

示例输入文件内容可以是连续字符，也可以是多行文本：

```text
ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789
设置无线网络显示声音关于亮度深色模式固件版本
，。！？：；（）【】《》+-*/=%
```

默认情况下，脚本会忽略回车、换行和 Tab。

---

## 5. 快速开始

生成 **GB2312 一级常用字 + 中英文符号** 字符集：

```bash
python3 charset_filter.py \
  --input charset_zh_en_gbk.txt \
  --output charset_zh_en_gb2312_l1.txt \
  --removed removed_chars.txt \
  --stats stats.json \
  --gb-level l1
```

简写形式：

```bash
python3 charset_filter.py \
  -i charset_zh_en_gbk.txt \
  -o charset_zh_en_gb2312_l1.txt \
  --removed removed_chars.txt \
  --stats stats.json \
  --gb-level l1
```

执行完成后会在脚本所在目录的 `build/` 子目录下生成：

```text
build/charset_zh_en_gb2312_l1.txt
build/removed_chars.txt
build/stats.json
```

> 如果需要把输出写到其它位置，可以在命令里加上 `--build-dir <path>`，或直接给 `--output` 等参数传入绝对路径。

终端会输出类似信息：

```text
charset filtering done
  input chars          : 21900
  GB level             : l1
  kept chars           : 3900
  removed chars        : 18000
  added outside source : 0
  build dir            : /path/to/gui/brookesia_gui_interface/tools/font/build
  output               : /path/to/gui/brookesia_gui_interface/tools/font/build/charset_zh_en_gb2312_l1.txt
  removed output       : /path/to/gui/brookesia_gui_interface/tools/font/build/removed_chars.txt
  stats output         : /path/to/gui/brookesia_gui_interface/tools/font/build/stats.json
```

实际数字和路径取决于你的输入字符集内容以及脚本所在目录。

---

## 6. 强制保留 Brookesia UI 文案

建议创建一个 `brookesia_strings.txt`，把系统界面中一定会出现的文字写进去。

示例：

```text
Brookesia
Settings
Wi-Fi
Display
Sound
About
System
Device
Network
Brightness
Dark Mode
Light Mode
Volume
Mute
Connected
Disconnected
Available Networks
Firmware Version
Storage
Memory
Restart
Power
Back
Done
Cancel
Apply

设置
无线网络
显示
声音
关于
系统
设备
网络
亮度
深色模式
浅色模式
音量
静音
已连接
未连接
可用网络
固件版本
存储空间
内存
重启
电源
返回
完成
取消
应用
```

运行：

```bash
python3 charset_filter.py \
  -i charset_zh_en_gbk.txt \
  -o charset_brookesia_gb2312_l1.txt \
  --removed removed_brookesia_gb2312_l1.txt \
  --stats stats_brookesia_gb2312_l1.json \
  --gb-level l1 \
  --ui-strings brookesia_strings.txt
```

执行完成后产物会落到 `build/` 下：

```text
build/charset_brookesia_gb2312_l1.txt
build/removed_brookesia_gb2312_l1.txt
build/stats_brookesia_gb2312_l1.json
```

`--ui-strings` 指定的文件中出现的字符会被强制保留。即使某些字符不在 GB2312 一级字范围内，也会加入输出字符集。

可以指定多个 UI 文案文件：

```bash
python3 charset_filter.py \
  -i charset_zh_en_gbk.txt \
  -o charset_brookesia.txt \
  --gb-level l1 \
  --ui-strings strings/settings.txt \
  --ui-strings strings/wifi.txt \
  --ui-strings strings/about.txt
```

---

## 7. 直接生成裁剪后的字体文件

如果已经安装 `fonttools`，可以在过滤字符集后直接生成子集字体。

以文泉驿微米黑为例，Ubuntu / Debian 上字体路径通常类似：

```text
/usr/share/fonts/truetype/wqy/wqy-microhei.ttc
```

可以先确认字体位置：

```bash
fc-match -v "WenQuanYi Micro Hei" | grep file
```

然后运行：

```bash
python3 charset_filter.py \
  -i charset_zh_en_gbk.txt \
  -o charset_brookesia_gb2312_l1.txt \
  --removed removed_brookesia_gb2312_l1.txt \
  --stats stats_brookesia_gb2312_l1.json \
  --gb-level l1 \
  --ui-strings brookesia_strings.txt \
  --font-path /usr/share/fonts/truetype/wqy/wqy-microhei.ttc \
  --font-number 0 \
  --subset-output wqy-microhei-brookesia-gb2312-l1.ttf
```

生成结果：

```text
build/charset_brookesia_gb2312_l1.txt
build/removed_brookesia_gb2312_l1.txt
build/stats_brookesia_gb2312_l1.json
build/wqy-microhei-brookesia-gb2312-l1.ttf
```

> 同样地，给 `--subset-output` 传入相对路径时会落到 `build/`，传入绝对路径则按指定位置输出。

如果希望进一步减小字体体积，可以加入：

```bash
--no-hinting
```

完整示例：

```bash
python3 charset_filter.py \
  -i charset_zh_en_gbk.txt \
  -o charset_brookesia_gb2312_l1.txt \
  --gb-level l1 \
  --ui-strings brookesia_strings.txt \
  --font-path /usr/share/fonts/truetype/wqy/wqy-microhei.ttc \
  --font-number 0 \
  --subset-output wqy-microhei-brookesia-gb2312-l1-nohint.ttf \
  --no-hinting
```

---

## 8. TTC 字体集合的 font-number

文泉驿微米黑通常是 `.ttc` 字体集合，里面可能包含多个字体面。

常见情况：

```text
--font-number 0    WenQuanYi Micro Hei
--font-number 1    WenQuanYi Micro Hei Mono
```

你可以用下面脚本查看 `.ttc` 中包含哪些字体面：

```bash
python3 - <<'PY'
from fontTools.ttLib import TTCollection

path = "/usr/share/fonts/truetype/wqy/wqy-microhei.ttc"
ttc = TTCollection(path)

for i, font in enumerate(ttc.fonts):
    print(i, font["name"].getDebugName(1), "/", font["name"].getDebugName(4))
PY
```

如果缺少 `fontTools` 模块，先安装：

```bash
pip install fonttools
```

---

## 9. GB 级别说明

### 9.1 `--gb-level none`

不保留 GB 中文汉字，只保留基础符号和强制保留字符。

```bash
python3 charset_filter.py \
  -i charset_zh_en_gbk.txt \
  -o charset_ui_only.txt \
  --gb-level none \
  --ui-strings brookesia_strings.txt
```

适合极限压缩体积，只显示固定 UI 文案。

### 9.2 `--gb-level l1`

保留 GB2312 一级常用汉字。

```bash
python3 charset_filter.py \
  -i charset_zh_en_gbk.txt \
  -o charset_gb2312_l1.txt \
  --gb-level l1
```

适合系统设置、按钮、弹窗、基础中文显示。

### 9.3 `--gb-level l2`

保留 GB2312 一级 + 二级汉字。

```bash
python3 charset_filter.py \
  -i charset_zh_en_gbk.txt \
  -o charset_gb2312_l1_l2.txt \
  --gb-level l2
```

适合需要显示更多动态中文文本的场景，例如：

```text
通知内容
设备名称
文件名
错误日志
较多用户可见文本
```

### 9.4 `--gb-level gbk`

保留 GBK 双字节空间中可解码出的 CJK 汉字。

```bash
python3 charset_filter.py \
  -i charset_zh_en_gbk.txt \
  -o charset_gbk_cjk.txt \
  --gb-level gbk
```

覆盖更广，但字体体积会明显增大。

---

## 10. 参数说明

### 输入输出参数

| 参数 | 说明 |
|---|---|
| `-i, --input` | 输入字符集文件，必填 |
| `-o, --output` | 输出过滤后的字符集文件，必填，相对路径会落到 `build/` 下 |
| `--removed` | 输出被移除字符的文件，相对路径会落到 `build/` 下 |
| `--stats` | 输出统计信息 JSON，相对路径会落到 `build/` 下 |
| `--build-dir` | 输出根目录，默认 `<script_dir>/build`，仅影响相对路径 |
| `--append-newline` | 输出文件末尾追加换行 |

### 字符保留策略参数

| 参数 | 说明 |
|---|---|
| `--gb-level` | 指定 GB 中文级别：`none`、`l1`、`l2`、`gbk` |
| `--no-basic-symbols` | 不自动保留 ASCII、Latin-1、CJK 标点、全角符号 |
| `--extra-symbols` | 指定额外保留符号字符串 |
| `--extra-symbols-file` | 指定额外保留字符文件，可重复使用 |
| `--ui-strings` | 指定强制保留的 UI 文案文件，可重复使用 |
| `--exclude-file` | 指定强制移除字符文件，可重复使用 |
| `--strict-source` | 只保留输入字符集中原本存在的字符 |
| `--keep-whitespace` | 保留输入和文案文件中的空白字符 |

### 字体裁剪参数

| 参数 | 说明 |
|---|---|
| `--font-path` | 输入字体文件路径，例如 `.ttc`、`.ttf`、`.otf` |
| `--font-number` | `.ttc` 字体集合中的字体索引，默认 `0` |
| `--subset-output` | 输出子集字体文件路径，相对路径会落到 `build/` 下 |
| `--fonttools-bin` | fonttools 可执行文件名或路径，默认 `fonttools` |
| `--no-hinting` | 传递给 fonttools，用于移除 hinting、减小体积 |
| `--flavor` | 输出 flavor，例如 `woff` 或 `woff2` |
| `--fonttools-extra-arg` | 额外传递给 `fonttools subset` 的参数，可重复使用 |

---

## 11. `--strict-source` 的作用

默认情况下，`--ui-strings` 和 `--extra-symbols-file` 中的字符可以加入到输出字符集，即使它们原本不在 `--input` 文件里。

如果希望输出字符严格限制在输入文件已有字符内，可以加入：

```bash
--strict-source
```

示例：

```bash
python3 charset_filter.py \
  -i charset_zh_en_gbk.txt \
  -o charset_strict.txt \
  --gb-level l1 \
  --ui-strings brookesia_strings.txt \
  --strict-source
```

这种情况下，`brookesia_strings.txt` 中不在输入字符集内的字符不会被额外加入。

---

## 12. 强制删除部分字符

如果你希望排除某些字符，可以创建：

```text
exclude_chars.txt
```

例如：

```text
￥€£★☆☃
```

运行：

```bash
python3 charset_filter.py \
  -i charset_zh_en_gbk.txt \
  -o charset_filtered.txt \
  --gb-level l1 \
  --exclude-file exclude_chars.txt
```

`--exclude-file` 在最后生效，因此即使某个字符被 GB 级别、基础符号或 UI 文案选中，也会被强制移除。

---

## 13. Brookesia 推荐命令

> 以下命令中的 `--output` / `--removed` / `--stats` / `--subset-output` 均使用相对文件名，最终都会生成到脚本所在目录下的 `build/` 子目录中。

### 13.1 固定系统 UI 字库

适合 Settings App、Wi‑Fi、Display、Sound、About、状态栏、弹窗等固定文案。

```bash
python3 charset_filter.py \
  -i charset_zh_en_gbk.txt \
  -o charset_brookesia_ui_l1.txt \
  --removed removed_brookesia_ui_l1.txt \
  --stats stats_brookesia_ui_l1.json \
  --gb-level l1 \
  --ui-strings brookesia_strings.txt
```

### 13.2 动态中文文本字库

适合需要显示 SSID、设备名、通知、错误信息等动态中文内容。

```bash
python3 charset_filter.py \
  -i charset_zh_en_gbk.txt \
  -o charset_brookesia_dynamic_l2.txt \
  --removed removed_brookesia_dynamic_l2.txt \
  --stats stats_brookesia_dynamic_l2.json \
  --gb-level l2 \
  --ui-strings brookesia_strings.txt
```

### 13.3 极限小体积 UI-only 字库

只保留 UI 文案字符、英文、数字和基础符号。

```bash
python3 charset_filter.py \
  -i charset_zh_en_gbk.txt \
  -o charset_brookesia_ui_only.txt \
  --removed removed_brookesia_ui_only.txt \
  --stats stats_brookesia_ui_only.json \
  --gb-level none \
  --ui-strings brookesia_strings.txt
```

### 13.4 直接生成文泉驿微米黑子集字体

```bash
python3 charset_filter.py \
  -i charset_zh_en_gbk.txt \
  -o charset_brookesia_ui_l1.txt \
  --gb-level l1 \
  --ui-strings brookesia_strings.txt \
  --font-path /usr/share/fonts/truetype/wqy/wqy-microhei.ttc \
  --font-number 0 \
  --subset-output wqy-microhei-brookesia-ui-l1.ttf
```

---

## 14. 转成 LVGL 字体

生成 `.ttf` 后，可以继续使用 `lv_font_conv` 转成 LVGL 使用的 C 文件。脚本默认会把子集字体放在 `build/` 下，因此 `--font` 通常要带上 `build/` 前缀。

示例：

```bash
lv_font_conv \
  --font build/wqy-microhei-brookesia-ui-l1.ttf \
  --size 24 \
  --bpp 2 \
  --format lvgl \
  --output build/brookesia_font_zh_24.c
```

如果需要多个字号，建议分别生成：

```text
brookesia_font_zh_18.c
brookesia_font_zh_22.c
brookesia_font_zh_28.c
brookesia_font_zh_36.c
```

常见建议：

| 用途 | 字号 | bpp 建议 |
|---|---:|---:|
| 状态栏、小提示 | 16–18 px | 2 |
| 列表正文 | 20–24 px | 2 |
| 页面标题 | 28–36 px | 3 或 4 |
| 大数字 / 锁屏时间 | 48 px 以上 | 3 或 4 |

---

## 15. 检查字体是否包含指定字符

可以用下面脚本检查生成后的字体是否包含某些字符：

```bash
python3 - <<'PY'
from fontTools.ttLib import TTFont

font_path = "build/wqy-microhei-brookesia-ui-l1.ttf"
font = TTFont(font_path)

cmap = {}
for table in font["cmap"].tables:
    cmap.update(table.cmap)

for ch in "设置Wi-FiDisplaySoundAbout你好中英文あ한":
    print(ch, "OK" if ord(ch) in cmap else "MISSING")

print("total codepoints:", len(cmap))
PY
```

如果你的目标是“中英文”，日文假名和韩文通常应该显示为 `MISSING`，而中文、英文、数字和常用符号应该显示为 `OK`。

---

## 16. 常见问题

### Q1：为什么不直接用 GBK 全量？

GBK 覆盖更广，但裁剪后的字体仍然可能偏大。ESP32 项目通常需要控制 Flash 和资源分区体积，因此更建议使用 `l1` 或 `l2`。

### Q2：`l1` 会不会缺字？

会。GB2312 一级字适合常见 UI 文案，但对人名、地名、专业术语、动态通知可能缺字。解决方式是：

```text
固定 UI：l1 + ui-strings
动态文本：l2 或 gbk
```

### Q3：UI 文案字符不在 GB2312 一级字里怎么办？

把文案放进 `brookesia_strings.txt`，并通过 `--ui-strings brookesia_strings.txt` 强制保留。

### Q4：生成字体时提示找不到 fonttools？

安装：

```bash
pip install fonttools
```

如果安装在虚拟环境里，先激活虚拟环境：

```bash
source .venv/bin/activate
```

### Q5：`.ttc` 文件应该用哪个 `--font-number`？

文泉驿微米黑通常使用：

```text
--font-number 0
```

如果要使用等宽版本，可以尝试：

```text
--font-number 1
```

建议用本文第 8 节脚本确认。

### Q6：是否建议输出 `.otf`？

对于 Brookesia / LVGL，更推荐保留 `.ttf`，然后使用 `lv_font_conv` 转成 LVGL C 字体。`.otf` 通常不是必要步骤。

### Q7：`--no-hinting` 是否一定要加？

不一定。`--no-hinting` 可以减小字体体积，但小字号显示效果可能略有差异。建议分别生成带 hinting 和不带 hinting 的字体，在目标屏幕上对比后再决定。

---

## 17. 推荐目录结构

只用本脚本时，默认产物会集中放在脚本目录下的 `build/` 中：

```text
tools/font/
├── charset_filter.py
├── charset_zh_en_gbk.txt
├── brookesia_strings.txt
└── build/
    ├── charset_brookesia_ui_l1.txt
    ├── removed_brookesia_ui_l1.txt
    ├── stats_brookesia_ui_l1.json
    └── wqy-microhei-brookesia-ui-l1.ttf
```

如果想接入更完整的字体流水线，可以按下面的结构组织外部目录，并通过 `--build-dir` 或绝对路径把产物写入对应位置：

```text
fonts/
├── source/
│   └── wqy-microhei.ttc
├── charset/
│   ├── charset_zh_en_gbk.txt
│   ├── brookesia_strings.txt
│   ├── charset_brookesia_ui_l1.txt
│   ├── removed_brookesia_ui_l1.txt
│   └── stats_brookesia_ui_l1.json
├── subset/
│   └── wqy-microhei-brookesia-ui-l1.ttf
└── lvgl/
    ├── brookesia_font_zh_18.c
    ├── brookesia_font_zh_24.c
    └── brookesia_font_zh_32.c
```

---

## 18. 许可说明

`charset_filter.py` 本身不包含任何字体数据。

使用字体时，请单独确认字体授权。例如：

```text
文泉驿微米黑：请保留对应授权说明
思源黑体 / Noto Sans CJK：请遵守 SIL Open Font License
其他商业免费字体：请确认是否允许嵌入固件和再分发
```

对于商业产品，建议在固件、资源包或文档中保留字体名称、来源和许可证信息。

---

## 19. 最小可用命令汇总

> 下列命令的输出文件均使用相对路径，最终都会落到脚本所在目录的 `build/` 下；如需写到别的位置，使用 `--build-dir <path>` 或给参数传绝对路径。

只生成字符集：

```bash
python3 charset_filter.py \
  -i charset_zh_en_gbk.txt \
  -o charset_brookesia_ui_l1.txt \
  --gb-level l1 \
  --ui-strings brookesia_strings.txt
```

生成字符集并输出统计：

```bash
python3 charset_filter.py \
  -i charset_zh_en_gbk.txt \
  -o charset_brookesia_ui_l1.txt \
  --removed removed_brookesia_ui_l1.txt \
  --stats stats_brookesia_ui_l1.json \
  --gb-level l1 \
  --ui-strings brookesia_strings.txt
```

生成文泉驿微米黑子集字体：

```bash
python3 charset_filter.py \
  -i charset_zh_en_gbk.txt \
  -o charset_brookesia_ui_l1.txt \
  --gb-level l1 \
  --ui-strings brookesia_strings.txt \
  --font-path /usr/share/fonts/truetype/wqy/wqy-microhei.ttc \
  --font-number 0 \
  --subset-output wqy-microhei-brookesia-ui-l1.ttf
```
