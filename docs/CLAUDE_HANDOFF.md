# Bear2Wave — 未完成工作交接清单（给 Claude）

> **用途**：交给 Claude / 其他 agent 接续开发。  
> **仓库根目录**：`e:\EDA_Race\TEST1 - 1\TEST1`  
> **更新**：2026-05-27（E5-1 收尾完成：`MainFrameMenus` 接入 + 构建卫生 + `m_vcdData` 审计 + fzt 修复；P4-3 阶段 C：VCD 时间索引 + setvbuf；删除 WaveformPanel 死代码 ~310 行）

---

## 构建与文档入口

| 项 | 路径 |
|----|------|
| 主工程 | `TEST1/TEST1.vcxproj` |
| 可执行文件 | `out/x64/Debug/Bear2Wave.exe` |
| 工程化总清单 | [LARGE_FILE_AND_ENGINEERING_TODO.md](LARGE_FILE_AND_ENGINEERING_TODO.md) |
| 大 VCD 专项任务单 | [VCD_LARGE_FILE_TASK.md](VCD_LARGE_FILE_TASK.md) |
| 已实现能力摘要 | [LARGE_TRACE_PERFORMANCE.md](LARGE_TRACE_PERFORMANCE.md) |
| AI 路线（基本完成） | [AI_ANALYSIS_TODO.md](AI_ANALYSIS_TODO.md) |
| 测试波形 | `tests/traces/` |
| CLI 工具 | `tools/trace_tools.cpp` |

**构建命令（Windows x64 Debug）**：

```powershell
& "D:\Program_Files\VS2022\MSBuild\Current\Bin\MSBuild.exe" "e:\EDA_Race\TEST1 - 1\TEST1\TEST1\TEST1.vcxproj" /p:Configuration=Debug /p:Platform=x64
```

（若 VS 路径不同，用 `vswhere -find MSBuild\**\Bin\MSBuild.exe` 定位。）

---

## 一、本轮架构（E5-1）— 已完成

| 组件 | 路径 | 职责 |
|------|------|------|
| `TraceDocument` | `TEST1/core/TraceDocument.h`, `TraceDocument.cpp` | `vcd_t*` 生命周期、路径、CSV 堆、模块索引 |
| `WaveformController` | `TEST1/core/WaveformController.h`, `WaveformController.cpp` | 加载/清空视图、缩放、翻页、游标 |
| `WaveformSessionController` | `TEST1/core/WaveformSessionController.h`, `WaveformSessionController.cpp` | `.bwv` / `.gtkw` / `.b2w` 收集、恢复、保存 |
| `SignalModuleTree` | `TEST1/ui/SignalModuleTree.h`, `SignalModuleTree.cpp` | 左侧模块树 + 信号列表 |
| `MenuIds.hpp` | `TEST1/ui/MenuIds.hpp` | 菜单 ID 去冲突（Markers vs Edit、File Close 等） |

**当前规模**：`TEST1/ui/MainFrame.hpp` 约 **2700+ 行**（仍偏大，需继续拆）。

**目标架构**：

```text
MyFrame (UI 壳)
  → WaveformSessionController   （会话 I/O）
  → WaveformController            （加载/缩放/游标）
  → TraceDocument                 （vcd_t*、路径、模块索引、CSV）
  → trace_loader / 各 format loader
WaveformPanel（wx 控件）→ WaveformRenderer / WaveformPainter
SignalModuleTree（左侧树）
```

---

## 二、E5-1 收尾（优先 — 架构线）

### 2.1 接入已有但未连线的 `MainFrameMenus` ⚠️

| 项 | 说明 |
|----|------|
| **现状** | `TEST1/ui/MainFrameMenus.hpp`、`MainFrameMenus.cpp`（约 330 行）已存在 |
| **问题** | 未加入 `TEST1.vcxproj`；`MainFrame.hpp` 未调用 `MainFrameMenus::CreateMenuBar` / `BindMenuEvents` |
| **风险** | 菜单 ID 可能与当前 `BearMenuId` / `MenuIds.hpp` 不一致（例如仍用 `1004` 作 Close，正确应为 `BearMenuId::File::Close` = 1015） |

**任务**：

1. 对齐 `MainFrameMenus.cpp` 与 `TEST1/ui/MenuIds.hpp`
2. 从 `MainFrame.hpp` 删除重复的菜单创建与 `Bind(wxEVT_MENU, …)` 大块
3. `MyFrame` 需将菜单 handler 暴露给 `MainFrameMenus`（`public`、`friend`，或薄包装函数）
4. 在 `TEST1.vcxproj` 加入 `ui\MainFrameMenus.cpp`，全量编译

**验收**：菜单功能与改前一致；`MainFrame.hpp` 再减少约 **500+ 行**。

---

### 2.2 继续从 `MainFrame.hpp` 拆分

| 子任务 | 建议新文件 | 内容 |
|--------|------------|------|
| 文件/工程打开 | `ui/MainFrameFileOps.cpp` | `SetProjectDir`、`OpenTraceFileDialog`、`OnOpenNewLab`、`FinishTraceLoadUI` |
| Time / Zoom | `ui/MainFrameTimeOps.cpp` | 时间菜单；统一走 `m_waveController` |
| Markers | `ui/MainFrameMarkers.cpp` | 命名标记、测量、边沿查找 |
| Radix / Format | `ui/MainFrameRadix.cpp` | `ApplyDataFormatToTargets` |
| Compare | 已有 `ui/WaveformCompareHub.*` | 新 `MyFrame` 窗口须同样初始化 `SetTraceDocument`、`m_waveController`、`m_sessionController` |

---

### 2.3 `TraceDocument` 访问统一

`MainFrame` 已部分改为 `m_traceDoc.Vcd()`。

**仍直接读 `m_wavePanel->m_vcdData` 的位置**（建议改为 `TraceDocument` 或 Panel 只读访问器）：

| 文件 |
|------|
| `TEST1/AIAnalysisPanel.cpp` |
| `TEST1/waveform_analysis.cpp` |
| `TEST1/waveform_local_stats.cpp` |
| `TEST1/panels/WaveformPanel.hpp`（内部可保留，对外宜收敛） |
| `TEST1/panels/WaveformPainter.cpp` |

---

### 2.4 构建卫生

| 项 | 现状 | 任务 |
|----|------|------|
| `WaveformSession.cpp` | `TEST1/Main.cpp` 内 `#include "core/WaveformSession.cpp"` | 改为 `TEST1.vcxproj` 的 `ClCompile` |
| `TclScriptEngine.cpp` | 同上可能在 `Main.cpp` include | 若已在 vcxproj，删除 `Main.cpp` 重复 include |
| `WaveformCompareHub.cpp` | 同上 | 同上 |
| `WaveformRenderer.cpp` / `WaveformPainter.cpp` | 已存在 | 确认在 vcxproj；检查 Panel 是否仍内联大量绘制 |

---

### 2.5 小修复

- **文件对话框**：`OpenTraceFileDialog` 等仍写 `*.fzt`，应为 `*.fst`，与 `SetProjectDir` 扩展名一致（`MainFrame.hpp` 内搜索 `fzt`）
- **文档**：E5-1 完成后将 `docs/LARGE_FILE_AND_ENGINEERING_TODO.md` 中 E5-1 标为 `[x]`

---

## 三、E5-2 — `WaveformPanel` 拆分

| 模块 | 路径 | 状态 |
|------|------|------|
| 渲染/缓存 | `TEST1/panels/WaveformRenderer.h`, `WaveformRenderer.cpp` | 部分已抽出 |
| 绘制 | `TEST1/panels/WaveformPainter.cpp` | 已有 |
| 主面板 | `TEST1/panels/WaveformPanel.hpp` | **约 3580 行**，待继续拆 |

**任务**：缓存构建、输入事件、后台 trace 加载再下沉；Panel 仅保留 wx 绑定与协调。

**验收**：`WaveformPanel.hpp` < 1500 行；行为不变；Debug x64 编译通过。

---

## 四、E5-3 / E5-4 — 后续架构

| ID | 任务 |
|----|------|
| E5-3 | 静态库 `bear2wave_trace`：`trace_loader` + `vcd` + `vcd_lazy` + FST/VZT/LXT/GHW loader |
| E5-4 | 头文件依赖收敛：loader 不再反向 include `MainFrame.hpp` / 巨型 `WaveformPanel.hpp` |
| E5-5 | `.editorconfig` / `clang-format` |
| E5-6 | `/W4` 或 CI 静态分析 job |
| E5-7 | `third_party/README.md`：来源、版本、许可证 |

---

## 五、大文件性能（P4）

### P4-3 大 VCD 懒加载 `[~]` — 详见 [VCD_LARGE_FILE_TASK.md](VCD_LARGE_FILE_TASK.md)

**已有**：

- `TEST1/vcd_lazy.h`, `vcd_lazy.cpp`
- `VCD_TRACE_BACKEND_VCD_LAZY`（`vcd.h`）
- `trace_loader.cpp` 分发、`BEAR2WAVE_VCD_LAZY` / `BEAR2WAVE_VCD_LAZY_MB`（`core/waveform_perf.h`）

**未完成 — 阶段 C（性能关键）**：

| ID | 内容 |
|----|------|
| C1 | 信号 ID → 文件 offset；可选侧车 `.bwvcdidx`（参考 `core/trace_sidecar_idx.cpp`） |
| C2 | 时间块粗索引，按 `[t0,t1]` seek |
| C3 | 大块 `fread` / `setvbuf`，避免全文件逐字节读 |
| C4 | 增量 gap 加载（对齐 `core/trace_load_gaps.h`、FST 逻辑） |

**阶段 E（文档/工具）**：`TraceTools bench` 支持 VCD；大 VCD 状态栏进度；P4-3 标 `[x]`。

---

### 其它 P4 未闭合项

| ID | 状态 | 任务 |
|----|------|------|
| P4-6 | `[ ]` | 模块树万级虚拟化、延迟展开 |
| P4-7 | `[~]` | 绘制 LOD、`BEAR2WAVE_MAX_SEGMENTS`、row-pressure |
| P4-9 | `[ ]` | VZT 块失败重试、单块超时 |
| P4-11 | `[ ]` | FST/VZT 并行块解码 |
| P4-12 | `[ ]` | mmap 只读映射 |

---

## 六、工程 / CI / 体验（按需）

| 优先级 | ID | 任务 |
|--------|-----|------|
| 中 | E0-2 | `TEST1/CMakeLists.txt` 与 vcxproj 对齐为 CI 单一真相 |
| 中 | E0-4 | `tools/fetch_gtkwave_libs.ps1` 幂等、hash、离线提示 |
| 中 | E0-5 | CMake 可选特性：`WITH_VZT` / `WITH_LXT2` / `WITH_GHW` / `WITH_TCL` |
| 中 | E1-4 ~ E1-8 | 大文件样例、BuildContext 快照、泄漏/取消加载竞态测试 |
| 低 | E2-2 | CI 缓存 vcpkg / wxWidgets |
| 低 | E2-7 | Authenticode 代码签名 |
| 低 | E3-1 ~ E3-5 | 分级日志、`BEAR2WAVE_LOG_LEVEL`、minidump、FST/VZT 诊断包 |
| 低 | E4-2 / E4-4 / E4-6 | 便携 `config.ini`、快速入门截图/GIF、Issue 模板 |

---

## 七、AI / 高级功能

| 项 | 文档 | 状态 |
|----|------|------|
| AI MVP 手测 | [AI_ANALYSIS_TODO.md](AI_ANALYSIS_TODO.md) A10 | `[~]` |
| P5-1 ~ P5-4 | [LARGE_FILE_AND_ENGINEERING_TODO.md](LARGE_FILE_AND_ENGINEERING_TODO.md) | Transaction / FSM / GPU / Compare 对齐 — 未做 |
| P5-5 会话文件 | 同上 | `[x]`（由 `WaveformSessionController` 承接 UI） |

---

## 八、建议执行顺序

```text
1. E5-1 收尾：接入 MainFrameMenus + vcxproj + 编译通过
2. E5-1：MainFrame 继续拆 File / Time / Markers + m_vcdData 访问审计
3. E5-2：WaveformPanel.hpp 瘦身（Renderer / Painter 已存在）
4. P4-3 阶段 C：VCD 索引与 seek（按 VCD_LARGE_FILE_TASK.md）
5. P4-7 / P4-6：绘制 LOD 与树虚拟化
6. E0 / E1 / E3：工程化与 CI（可并行）
```

---

## 九、关键源文件速查

| 用途 | 路径 |
|------|------|
| 主窗口 | `TEST1/ui/MainFrame.hpp` |
| 菜单（未接线） | `TEST1/ui/MainFrameMenus.hpp`, `MainFrameMenus.cpp` |
| 菜单 ID | `TEST1/ui/MenuIds.hpp` |
| 波形面板 | `TEST1/panels/WaveformPanel.hpp` |
| 加载入口 | `TEST1/trace_loader.cpp` |
| VCD 懒加载 | `TEST1/vcd_lazy.cpp`, `vcd_lazy.h` |
| 会话序列化 | `TEST1/core/WaveformSession.cpp`（当前由 `Main.cpp` include） |
| 侧车索引（FST） | `TEST1/core/trace_sidecar_idx.cpp` |
| 内存预算 | `TEST1/core/trace_memory_budget.cpp` |
| 工程文件 | `TEST1/TEST1.vcxproj` |
| 应用入口 | `TEST1/Main.cpp` |

---

## 十、验收通则

1. **编译**：MSBuild Debug x64 零错误。
2. **手工冒烟**：
   - 打开 `tests/traces/test2.vcd` 或 `tests/traces/bear2wave_sample.fst`
   - 添加信号、拖时间轴、缩放
   - File → Read/Write Session（`.bwv`）
3. **大 VCD**（P4-3）：`BEAR2WAVE_VCD_LAZY=1` 打开大文件，对比 RSS 与打开时间（见 `VCD_LARGE_FILE_TASK.md` §4.3）。
4. **完成后**：更新 `LARGE_FILE_AND_ENGINEERING_TODO.md` 对应项 `[ ]` → `[x]`，必要时改 `LARGE_TRACE_PERFORMANCE.md` 一行摘要。

---

## 十一、明确不做（防范围膨胀）

- 完整复刻 GTKWave 全菜单 / 全部 Tcl 命令  
- 内建仿真或 RTL 编辑  
- 云端波形托管（除非单独立项）  
- 默认自动加载芯片全部信号  

---

*维护：完成子任务后回写 `LARGE_FILE_AND_ENGINEERING_TODO.md` 状态，并在本文件顶部注明日期。*
