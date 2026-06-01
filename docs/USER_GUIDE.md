# Bear2Wave 用户指南

> **应用内 Help** 使用精美 HTML：`docs/help/*.html`（菜单 Help → Help Contents）。  
> 本文档为 Markdown 副本，便于 GitHub / 离线阅读；修改帮助请优先编辑 HTML。

本文档面向日常使用 Bear2Wave 的验证/调试工程师，涵盖从打开波形到 AI 分析、会话保存的完整流程。

---

## 概述

Bear2Wave 是一款面向 RTL 仿真的波形查看器，操作习惯参考 [GTKWave](https://github.com/gtkwave/gtkwave)，并增加：

- **大文件懒加载**：FST/VZT/LXT2/GHW/VCD(lazy) 先加载层次，再按视口与选中信号加载跳变
- **Compare 双窗对比**：两路轨迹联动播放头与时间视口
- **AI 分析**：基于视口内波形摘要，调用 DeepSeek（或 Ollama 本地）辅助解读
- **本地统计**：边沿、占空比、毛刺、握手规则等，无需 API Key

**推荐工作流**：仿真输出 **FST** → 只添加需要的信号 → 平移/缩放定位问题 → Marker 测量 → 本地报告 / AI 解释 → 保存 `.bwv` 会话。

---

## 快速入门

### 1. 启动与打开文件

- 启动 `Bear2Wave.exe` 后，可通过启动窗口打开最近项目，或直接进入主窗口。
- **File → Open Trace…**（`Ctrl+O`）或工具栏 **Open**，选择波形文件。
- 支持格式：`.fst` `.vcd` `.vzt` `.lxt` `.lxt2` `.ghw`（后几种需编译选项，见「文件格式」）。

### 2. 添加信号到波形区

1. 左侧 **模块树** 展开设计层次（默认根节点「VCD Modules」）。
2. 点击模块，右侧 **信号列表** 显示该模块下信号（虚拟列表，支持大量信号）。
3. **双击** 信号加入右侧波形显示区；可重复添加同一路信号。
4. 底部 **搜索框** 可过滤信号名（`Ctrl+Shift+F` 快速聚焦）。

> **重要**：Bear2Wave **不会**自动加载全芯片所有信号的波形数据。这与 GTKWave 一样——只加载你加入波形区的信号，是大文件可用的关键。

### 3. 浏览时间

- **拖动** 波形区：移动播放头（红色竖线），底部滑块同步。
- **滚轮**（默认）：缩放时间轴；可在 Markers 菜单开启 **Alternate Wheel Mode** 改为滚轮移动时间。
- **底部 From/To** 文本框：精确设置可见时间范围。
- **Play/Pause**：自动播放播放头（适合演示短区间）。

### 4. 保存工作现场

- **File → Write Session**（`Ctrl+S`）保存 `.bwv` 会话（推荐）。
- 下次 **Read Session** 恢复：显示列表、视口、播放头、标记、别名、注释行、窗口布局等。

---

## 主界面布局

```
┌─────────────────────────────────────────────────────────────────┐
│ Menu: File | Edit | Time | Markers | Measure | Compare | View | AI | Help │
├──────────────┬──────────────────────────────────┬───────────────┤
│ 模块树       │  波形显示区（多路信号、小地图）     │  AI 分析面板   │
│ + 信号列表   │  播放头 / Marker / 测量 A-B        │  （可隐藏）    │
├──────────────┴──────────────────────────────────┴───────────────┤
│ 搜索 | 缩放按钮 | 时间滑块 | Play | From/To | 测量模式            │
└─────────────────────────────────────────────────────────────────┘
```

| 区域 | 作用 |
|------|------|
| 模块树 | 按设计层次浏览；展开状态可写入 `.bwv` |
| 信号列表 | 当前模块下的信号；双击添加 |
| 波形区 | 已添加信号的阶梯波形；左键拖播放头，Ctrl+左键 A/B 测量 |
| AI 面板 | 本地统计 + AI 请求；`Ctrl+Shift+A` 显示/隐藏 |
| 底栏 | 时间导航、播放、可见范围 |

---

## 波形交互

### 缩放与平移

| 操作 | 方式 |
|------|------|
| 放大/缩小 | 滚轮；菜单 Time → Zoom；底栏 **+/-**；快捷键 `+` `-` `Ctrl+0` |
| 翻页 | `PgUp` / `PgDn`；Time → Page |
| 平移视窗 | `Ctrl+←/→`；Time → Shift |
| 播放头微调 | `←/→`（Shift 加大步长） |
| 跳到开头/末尾 | `Home` / `End`（播放头）；`Ctrl+Home/End`（视窗） |

### 边沿跳转

- 在已显示信号上，`,` / `.` 跳转到上/下一个跳变沿（相对播放头）。
- 菜单：**Markers → Find Previous/Next Edge**。

### 小地图

- 波形区底部缩略图显示全局时间范围与当前视口；可拖动快速定位。

### 光标值

- **View** 或工具栏可切换是否在波形旁显示当前时刻的信号值。

---

## 标记与测量

### 命名标记（Named Markers）

| 操作 | 说明 |
|------|------|
| **Shift+点击** 波形区 | 放置命名标记 |
| **Alt+H** | 在当前播放头放置标记 |
| 拖动标记 | Shift+拖动可移动（未锁定时） |
| **B** | 将主测量标记复制到 B 标记 |

### A/B 测量（Primary Marker）

- **Ctrl+左键拖动**：设置 A、B 测量区间，显示 ΔT。
- **Esc**：清除测量与取消进行中的 lazy 加载。

### 测量菜单

- **Measure → Show Measurement**：查看频率/占空比等（与 Measure 模式相关）。

---

## 编辑信号列表

波形显示区中的每一 **行** 可独立编辑（先点击该行选中）：

| 菜单 / 快捷键 | 功能 |
|---------------|------|
| **Insert Blank**（Ctrl+B） | 插入空行分隔 |
| **Insert Comment** | 插入注释行（仅文字，无波形） |
| **Delete** / Delete 键 | 删除选中行 |
| **Cut / Copy / Paste** | 剪贴板操作 |
| **Combine Down/Up**（F3/F5） | 合并相邻信号为总线组 |
| **Data Format** | 二进制/十六进制/ASCII 等显示进制 |
| **Alias Highlighted Trace** | 为信号设置别名 |
| **Highlight Regexp / All** | 高亮匹配信号 |

右键点击波形区信号行可打开 **上下文菜单**（含 **AI Analyze This Signal**）。

---

## 会话文件 (.bwv)

Bear2Wave 原生会话格式为 **`.bwv`**（v3），兼容 **`.b2w`** 与 GTKWave **`.gtkw`**（部分布局信息）。

### 保存内容（.bwv v3）

- 波形文件路径（相对会话文件）
- 时间视口：`time_offset`、`display_range`、`playhead`
- **显示列表顺序**：信号、空行、注释行、每路 radix
- 命名标记、A/B 测量区间
- 信号别名
- 模块树展开路径
- 窗口大小、分割条位置、AI 面板是否显示

### 操作

| 菜单 | 快捷键 |
|------|--------|
| Read Session | — |
| Write Session | Ctrl+S |
| Write Session As | Shift+Ctrl+S |

**建议**：每个仿真场景保存一个 `<用例名>.bwv`，与 FST 放在同一目录，路径可相对引用。

### 与 GTKWave 交换

- 导出 **`.gtkw`** 可被 GTKWave 读取信号列表；Bear2Wave 专有项（注释行、AI 状态等）不会写入 gtkw。
- 从 GTKWave 读取的 `.gtkw` 可恢复 dump 路径与 `+signal` 列表。

---

## Compare 对比模式

| 菜单 | 功能 |
|------|------|
| **Open Second Trace for Compare**（Ctrl+Shift+O） | 打开第二路波形到新窗口 |
| **Link Playheads Across Windows** | 多窗播放头同步 |
| **Link Time View Across Windows** | 多窗缩放/平移同步 |
| **Tile Windows Horizontally** | 水平平铺窗口 |

AI 分析可勾选 **附加 Compare 差异摘要**（需先打开第二路轨迹）。

---

## AI 与本地分析

### 本地分析（无需网络）

1. 打开 **AI** 面板（`Ctrl+Shift+A`）。
2. 在 Notebook 中选择 **本地统计 / 区间 / 总线 / 规则** 等 Tab。
3. 点击 **刷新统计** 或 **完整报告**。
4. 可将报告 **导出**，或 **送 AI 解释**。

本地功能包括：边沿计数、占空比、周期估算、X/Z 检测、毛刺、valid/ready 握手摘要、Compare 差异等。

### AI 分析（DeepSeek / Ollama）

1. **AI → Set API Key**，或在 AI 面板保存 DeepSeek 密钥（存于 `%APPDATA%\Bear2Wave\config.ini`）。
2. 波形区添加信号 → **从波形刷新**。
3. 选择模板或编辑问题 → **AI 分析**（后台线程，可取消）。

| 功能 | 说明 |
|------|------|
| 右键 **AI Analyze This Signal** | 针对单路信号 |
| **多轮对话** | 保留最近若干轮上下文 |
| **跳转时间** | 从 AI 回复中解析时间戳并跳转 |
| **Ollama 本地** | 设置中启用，默认 `127.0.0.1:11434` |

详见仓库 `docs/AI_USAGE.md`。

---

## 文件格式

| 扩展名 | 说明 | 大文件建议 |
|--------|------|------------|
| `.fst` | Verilator/GTKWave 常用二进制 | **首选** |
| `.vcd` | 文本 VCD | 小文件；大文件自动 lazy（可配置） |
| `.vzt` | GTKWave 压缩 | 需 `BEAR2WAVE_WITH_VZT` 编译 |
| `.lxt` / `.lxt2` | LXT 系列 | 需 `BEAR2WAVE_WITH_LXT2` |
| `.ghw` | GHDL 波形 | 需 `BEAR2WAVE_WITH_GHW` |
| `.csv` | 表格数据 | 轻量测试 |

大 VCD（默认 >50MB）打开时会提示考虑 `vcd2fst` 转换。

---

## 性能建议

1. 仿真时使用 `$dumpfile("waves.fst")` 或 `vcd2fst`。
2. **只添加调试需要的信号**（10～50 路通常足够）。
3. 平移/缩放时 lazy 格式仅加载视口 ± 边距 内的跳变。
4. 超大设计可设置环境变量限制内存（见 Help → Environment Variables）。

常用变量：`BEAR2WAVE_LOAD_MARGIN`、`BEAR2WAVE_MAX_LOADED_CHANGES`、`BEAR2WAVE_VCD_LAZY`。

---

## View 菜单与诊断

| 项 | 作用 |
|----|------|
| **Debug log window**（Ctrl+Shift+L） | 显示 wx 日志 |
| **Verbose FST load** | FST 加载详情 |
| **Dump waveform / trace summary** | 导出当前轨迹摘要 |

加载 FST 时状态栏显示 **块进度 / 路数 / ETA**。

---

## 故障排除

| 现象 | 建议 |
|------|------|
| 打开 VCD 很慢或内存暴涨 | 转 FST；或设置 `BEAR2WAVE_VCD_LAZY=1` |
| 波形空白 | 确认信号已加入波形区；等待 lazy 加载完成 |
| 平移后加载久 | 正常（后台读块）；按 **Esc** 可取消 |
| 会话加载信号找不到 | 检查 `.bwv` 中 trace 路径；信号名是否与 dump 一致 |
| AI 无响应 | 检查 API Key / 网络；或使用 **本地报告** |
| Help 文档空白 | 确保 `docs/` 目录与 exe 相对路径正确，或从源码目录运行 |

---

## 获取更多信息

- 仓库 **README.md**：安装与编译
- **docs/SHORTCUTS.md**：快捷键完整列表
- **docs/ENVIRONMENT.md**：环境变量完整列表
- **docs/LARGE_TRACE_PERFORMANCE.md**：大文件性能详解

---

*Bear2Wave — 仿 GTKWave 工作流，面向现代大仿真调试。*
