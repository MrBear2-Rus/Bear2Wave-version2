# Bear2Wave

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://isocpp.org/)
[![wxWidgets](https://img.shields.io/badge/wxWidgets-3.2%2B-green.svg)](https://www.wxwidgets.org/)
[![Platform](https://img.shields.io/badge/platform-Windows%20x64-lightgrey.svg)]()
[![Status](https://img.shields.io/badge/status-Alpha%20%E5%8F%91%E5%B8%83-orange.svg)]()

**Bear2Wave** 是一款**仿照 [GTKWave](https://github.com/gtkwave/gtkwave) 使用习惯重新设计**的波形查看器：面向 Verilog/VHDL 仿真产生的 VCD、FST、VZT、LXT2、GHW 等轨迹，提供模块树选信号、波形平移缩放、Marker 测量等熟悉流程，并在同一套界面上增加**大文件懒加载**、**双文件对比**与 **AI 波形分析**。

> **说明**：本项目**不是** GTKWave 官方分支，也**不**捆绑 GTKWave 可执行文件；波形**读取**复用业界通用的 libfst / GTKWave 读取库 / libghw，**界面与业务逻辑**为 Bear2Wave 独立实现（C++17 + wxWidgets + OpenGL）。

**当前版本：Alpha（首次公开发布）** — 功能可用，接口与行为仍可能调整，欢迎 Issue 反馈。

---

## 与 GTKWave 的关系（请务必阅读）

### 我们在做什么

| 维度 | 说明 |
|------|------|
| **定位** | 以 GTKWave 为**参考原型**，在 Windows 上实现「打开轨迹 → 选信号 → 看波形 → 测量」的主路径，并针对大仿真与现代 UI 做改造 |
| **兼容目标** | 尽量支持 GTKWave 用户熟悉的**文件格式**与**操作习惯**（FST 优先、层次浏览、Marker 等） |
| **差异方向** | 现代桌面 UI（wxWidgets）、内置 AI 分析、Compare 双窗、视口懒加载与性能优化，而非 1:1 克隆 GTKWave 全部菜单/Tcl |

### 复用与自研

| 来源 | 用途 |
|------|------|
| [libfst](https://github.com/gtkwave/libfst) | FST 读写 |
| GTKWave `libvzt` / `liblxt`（见 `third_party/gtkwave`） | VZT、LXT/LXT2 读取（可选编译） |
| [GHDL](https://github.com/ghdl/ghdl) `libghw` | GHW 读取（可选编译） |
| **Bear2Wave 自研** | 主窗口、波形绘制、懒加载调度、AI 面板、`trace_load_signals` 统一 API、Compare 等 |

### 与 GTKWave 对照（诚实预期）

| 能力 | GTKWave | Bear2Wave（当前） |
|------|---------|------------------|
| FST / VCD / VZT / LXT2 / GHW | 完整 | 支持（部分格式需编译开关） |
| 大文件 / 懒加载 | 成熟 | 已实现视口按需加载（持续优化） |
| Tcl 脚本 | 完整命令集 | 可选嵌入式 Tcl，**未**完全兼容 `gtkwave.rc` |
| Transaction / FSM 专用视图 | 有 | 尚未对齐 |
| AI 分析 | 无 | **有**（DeepSeek，基于视口波形摘要） |
| 界面框架 | GTK+2 | wxWidgets |

若你来自 GTKWave：可把 Bear2Wave 理解为**同一类工具的新实现**，适合「FST + 少量信号 + 需要 AI 辅助读波形」的场景；**不能**指望直接照搬全部 GTKWave 脚本与高级特性。

---

## 特性概览

- **多格式统一打开**：`trace_load_from_path()` 按扩展名分发  
- **大文件友好**：FST/VZT/LXT2/GHW 先加载层次，再按视口与选中信号加载跳变  
- **波形交互**：缩放、平移、小地图、播放头、Marker、多进制（`WaveformRadix`）  
- **信号浏览**：模块树 + 信号列表 + 搜索  
- **Compare**：第二轨迹窗口，可联动播放头与时间轴  
- **AI 分析**：针对波形区已显示信号，附带时间窗内跳变摘要（见下文）  

---

## 快速开始（发布版构建）

### 环境要求

- Windows 10/11 **x64**
- Visual Studio 2022（「使用 C++ 的桌面开发」）
- wxWidgets 3.2+（MSVC x64）
- vcpkg（推荐：zlib、bzip2，用于 VZT/LXT2）

### 获取与编译

```powershell
git clone https://github.com/<你的用户名>/Bear2Wave.git
cd Bear2Wave
```

1. 用 Visual Studio 打开 **`TEST1/TEST1.vcxproj`**。  
2. 选择 **x64 · Release**（发布建议用 Release）。  
3. 按本机路径修改项目中的 **wxWidgets / vcpkg** 包含目录与库目录。  
4. 生成 → 运行 **`TEST1/x64/Release/TEST1.exe`**（Debug 则为 `Debug` 目录）。

**可选：启用 VZT / LXT2 / GHW（与 GTKWave 相同的底层读库）**

```powershell
powershell -ExecutionPolicy Bypass -File tools\fetch_gtkwave_libs.ps1
vcpkg install zlib:x64-windows bzip2:x64-windows liblzma:x64-windows
copy TEST1\Bear2WaveTraceFormats.props.example TEST1\Bear2WaveTraceFormats.props
# 编辑 VcpkgRoot 后在 VS 导入 Bear2WaveTraceFormats.props（x64）
```

详见 [docs/TRACE_FORMATS_BUILD.md](docs/TRACE_FORMATS_BUILD.md)。

### 推荐首次体验流程（与 GTKWave 类似）

1. 打开 `tests/traces/bear2wave_sample.fst`（或你的仿真 FST）。  
2. 左侧模块树 → 双击信号加入波形区（**不会**自动铺满全芯片信号）。  
3. 拖动时间轴查看；需要测量时使用 Marker 菜单。  
4. （可选）AI 面板填入 [DeepSeek](https://platform.deepseek.com/) API Key → **Analyze**。

---

## 支持的文件格式

| 扩展名 | 说明 | 大文件建议 |
|--------|------|------------|
| `.fst` | Verilator / GTKWave 常用 | **首选** |
| `.vcd` | 文本 VCD | 小文件；过大将提示 `vcd2fst` |
| `.vzt` | GTKWave 压缩格式 | 需 `BEAR2WAVE_WITH_VZT` |
| `.lxt` / `.lxt2` | GTKWave LXT 系列 | 需 `BEAR2WAVE_WITH_LXT2` |
| `.ghw` | GHDL 波形 | 需 `BEAR2WAVE_WITH_GHW` |
| `.csv` | 表格 | 轻量数据 |

---

## AI 波形分析（Bear2Wave 独有）

- 只分析**波形显示区**中的信号（与 GTKWave 中手动加入 waves 一致）。  
- 自动汇总：视口时间、Marker、每路 `@ 时间: 值` 抽样。  
- 需自备 DeepSeek API Key；导出报告不含密钥。  

| 环境变量 | 默认 |
|----------|------|
| `BEAR2WAVE_AI_MAX_EDGES_PER_SIG` | 200 |
| `BEAR2WAVE_AI_MAX_CONTEXT_CHARS` | 30000 |

---

## 大文件性能调优

仿照 GTKWave「只看需要的信号」的思路，并增加视口增量加载、绘制缓存防抖等，详见：

**[docs/LARGE_TRACE_PERFORMANCE.md](docs/LARGE_TRACE_PERFORMANCE.md)**

| 环境变量 | 默认 | 含义 |
|----------|------|------|
| `BEAR2WAVE_LOAD_MARGIN` | 0.2 | 视口外预加载边距 |
| `BEAR2WAVE_CACHE_DEBOUNCE_MS` | 75 | 绘制缓存防抖 |
| `BEAR2WAVE_TRACE_LOAD_DEBOUNCE_MS` | 120 | 后台加载防抖 |
| `BEAR2WAVE_MAX_SIGNAL_LIST` | 8000 | 信号列表行数上限 |
| `BEAR2WAVE_VCD_WARN_MB` | 50 | 大 VCD 警告阈值 |

---

## 仓库结构

```text
Bear2Wave/                     # 仓库根（GitHub 展示本 README）
├── TEST1/                     # 主程序
│   ├── ui/MainFrame.hpp
│   ├── panels/WaveformPanel.hpp
│   ├── fst_loader.cpp / trace_loader.cpp / vcd.*
│   ├── AIAnalysisPanel.*
│   └── TEST1.vcxproj
├── third_party/               # libfst、gtkwave 读库、libghw
├── tools/                     # fetch_gtkwave_libs.ps1、trace_tools
├── tests/traces/              # 样例波形
└── docs/
```

---

## 文档

| 文档 | 内容 |
|------|------|
| [docs/TRACE_FORMATS_BUILD.md](docs/TRACE_FORMATS_BUILD.md) | 与 GTKWave 相同的读库如何编进工程 |
| [docs/LARGE_TRACE_PERFORMANCE.md](docs/LARGE_TRACE_PERFORMANCE.md) | 大文件性能 |
| [docs/AI_ANALYSIS_TODO.md](docs/AI_ANALYSIS_TODO.md) | AI / 本地分析规划 |

---

## 发布与许可证

- **首次公开**：Alpha，源代码开放。  
- **许可证**：[MIT License](LICENSE)（见仓库根目录 `LICENSE`）。  
- **第三方**：libfst、GTKWave 读取库、libghw 等遵循其各自许可证，见 `third_party/`。  
- **商标**：GTKWave 为 Tony Bybell 等人的作品，本项目名称 **Bear2Wave** 与 GTKWave 无隶属关系。

### 发布前自检（维护者）

- [ ] 将 `git clone` 地址改为真实 GitHub URL  
- [ ] 确认 `TEST1/TEST1.vcxproj` 已纳入版本库（若 `.gitignore` 排除了 `*.vcxproj` 需调整）  
- [ ] Release x64 构建通过并自测打开 FST + 加信号 + AI  
- [ ] （可选）在 `docs/` 添加界面截图并取消注释 README 图片链接  

---

## 已知限制

- 仅主要在 **Windows x64** 验证；非 GTKWave 的跨平台发布。  
- Tcl、Transaction、完整 marker 命令集等与 GTKWave **未完全对齐**。  
- AI 请求目前仍会短暂阻塞 UI（异步化在规划中）。  

---

## 致谢

- **[GTKWave](https://github.com/gtkwave/gtkwave)** — 交互设计与文件格式的事实标准，本项目仿照其工作流进行改造  
- **[wxWidgets](https://www.wxwidgets.org/)** — 图形界面  
- **libfst / libvzt / liblxt / libghw** — 波形读取  
- **[DeepSeek](https://www.deepseek.com/)** — AI API  

---

<p align="center">
  <b>Bear2Wave</b> — 仿 GTKWave 之用，添 AI 与现代化波形调试体验<br>
  <sub>非 GTKWave 官方产品 · 独立开源实现</sub>
</p>
