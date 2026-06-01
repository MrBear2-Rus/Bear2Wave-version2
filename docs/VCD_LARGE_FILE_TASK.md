# Bear2Wave — VCD 大文件优化任务单

> **用途**：交给 Claude / 其他 agent 按阶段实现。  
> **仓库**：`e:\EDA_Race\TEST1 - 1\TEST1`（主工程 `TEST1/`）  
> **里程碑**：对应 `LARGE_FILE_AND_ENGINEERING_TODO.md` 的 **M3 大 VCD 可用**（P4-3 + 可选 P4-4/P4-5）

---

## 1. 背景与目标

### 1.1 现状（已实现）

| 能力 | 位置 | 说明 |
|------|------|------|
| FST/VZT/LXT2/GHW 懒加载 | `fst_loader.cpp` 等 + `trace_loader.cpp` | 打开时只建层次；`trace_load_signals()` 按信号 + `[t0,t1]` 加载 |
| 视口时间窗 + margin | `trace_compute_padded_range` | `BEAR2WAVE_LOAD_MARGIN` |
| 后台加载 / 取消 | `WaveformPanel.hpp` + `trace_loader_request_cancel` | 懒格式有效 |
| 内存预算 LRU | `core/trace_memory_budget.cpp` | `BEAR2WAVE_MAX_LOADED_CHANGES` |
| 大 VCD 警告 | `trace_loader.cpp` | `BEAR2WAVE_VCD_WARN_MB`（默认 50MB），提示 `vcd2fst` |
| TraceBackend 抽象 | `core/trace_backend.h` | `Kind::VcdFull` 已预留，**无 VCD 懒实现** |

### 1.2 痛点（待解决）

- **`.vcd` 打开**：`trace_load_from_path` → `vcd_read_from_path()` **单次扫完全文件**（`vcd.cpp` 约 115–161 行），所有信号的 `value_changes` 一次性进内存。
- **`trace_load_signals`**：对 VCD 走 `default: rc = 0`（无操作），因数据已在打开时全加载。
- **大文件**：数百 MB～GB 级 VCD → 打开慢、内存爆、UI 假死；与 FST 懒加载体验不一致。

### 1.3 目标（验收口径）

| 指标 | 目标 |
|------|------|
| 打开 500MB VCD | **<30s** 完成层次解析；进程 RSS **不随跳变总数线性暴涨**（未选信号几乎无 VC 内存） |
| 用户选 20 路信号 | 仅加载视口 ±margin 时间窗内跳变；平移/缩放 **增量补窗**，不重复全文件扫描 |
| 内存 | 已加载跳变受 `BEAR2WAVE_MAX_LOADED_CHANGES` 约束；单条 `value_change_t` 可选紧凑存储（P4-4） |
| 兼容 | 小 VCD（<阈值）仍可走全文加载；`TraceTools` / 现有 `.vcd` 测试样例行为不变 |

**明确不做**：完整 VCD 写回、默认自动加载全部信号。（FST 写出见 `TraceTools vcd2fst` / `core/vcd_fst_writer.cpp`。）

---

## 2. 参考实现（必须对齐的模式）

实现 VCD 懒加载时，**照抄 FST 懒加载的分层**，不要另起一套 UI 协议。

```text
打开文件:
  fst_open_lazy()     → vcd 层次 + trace_session + trace_backend=FST_LAZY
  【目标】vcd_open_lazy() → 同上，backend=VCD_LAZY

按需加载:
  fst_load_signals(vcd, sigs[], count, t0, t1)
  【目标】vcd_load_signals(...) 或在 trace_load_signals 的 switch 中分发

释放:
  vcd_free() → fst_trace_session_destroy(trace_session)
  【目标】VcdTraceSession 关闭 FILE* / mmap、释放索引
```

**关键调用链（改动的入口）**：

1. `trace_loader.cpp`：`strcmp(ext,"vcd")==0` 分支 — 按文件大小 / env 选择 `vcd_open_lazy` vs `vcd_read_from_path`
2. `trace_load_signals()`：`case VCD_TRACE_BACKEND_VCD_LAZY:` → `vcd_load_signals(...)`
3. `trace_uses_lazy_backend()`：包含 VCD_LAZY
4. `core/trace_backend.h`：`Kind::VcdLazy`（或复用命名约定）
5. `WaveformPanel::EnsureTraceSignalLoadedSync` — 已对懒 backend 调 `trace_load_signals`，**无需改协议**，只要 VCD 后端生效即可

---

## 3. 任务拆分（按推荐顺序）

### 阶段 A — 设计与骨架（P4-3a）

| ID | 任务 | 文件 | 完成标准 |
|----|------|------|----------|
| A1 | 定义 `VCD_TRACE_BACKEND_VCD_LAZY = 5` | `vcd.h` | 与 FST 枚举并列；`trace_backend` 文档注释更新 |
| A2 | 新增 `VcdTraceSession`（C++ 实现，C API 用 `void* trace_session`） | `vcd_lazy.h` / `vcd_lazy.cpp`（新建） | 持有：UTF-8 路径、`FILE*` 或 mmap 句柄、timescale、`id→signal_t*` 映射、文件大小、可选取消标志 |
| A3 | `vcd_open_lazy(const char* path)` | 同上 | 只解析 `$scope`/`$var`/`$enddefinitions` 等 **DEFS 段**；**不**解析 `#时间` 后的赋值；返回 `vcd_t*` 且 `trace_backend=VCD_LAZY` |
| A4 | `vcd_load_signals(vcd, sigs, count, t0, t1)` | 同上 | 对选中信号扫描 DATA 段，仅 `[t0,t1]` 内赋值 append 到 `value_changes`；返回 0/-1/1（取消） |
| A5 | `vcd_trace_session_destroy` | 同上 | `vcd_free` 中按 backend 调用 |

**实现提示**：

- 现有 `parse_instruction` / `parse_assignment`（`vcd.cpp`）可拆为 **defs 模式** 与 **data 模式**，避免重复造 parser。
- 打开时建立 `signal_id`（VCD token）→ `signal_t*` 的 `unordered_map`（全文加载已有 `id_to_sig`）。
- `trace_max_timestamp`：懒打开时可先 **扫尾** 或 **二次轻量扫描** 只找最大 `#` 时间（不必加载 VC）。

---

### 阶段 B — 接入 trace_loader（P4-3b）

| ID | 任务 | 文件 | 完成标准 |
|----|------|------|----------|
| B1 | `trace_load_from_path` 分发逻辑 | `trace_loader.cpp` | 见下方环境变量；超阈值用 `vcd_open_lazy`，否则 `vcd_read_from_path` |
| B2 | `trace_load_signals` 增加 `VCD_LAZY` 分支 | `trace_loader.cpp` | 调用 `vcd_load_signals`；加载后 `vcd_signal_shrink_to_fit` + `trace_memory_budget_enforce`（与 FST 一致） |
| B3 | `trace_uses_lazy_backend` | `trace_loader.cpp` | VCD_LAZY 返回 true |
| B4 | `trace_loader_request_cancel` | `vcd_lazy.cpp` + `trace_loader.cpp` | 长时间扫描时置位 atomic，下一段检查并返回 1 |
| B5 | `TraceBackend::Kind` | `core/trace_backend.h` | 增加 `VcdLazy`；`FromVcd` / `UsesLazyIO` 更新 |

**环境变量（新增，写入 `LARGE_TRACE_PERFORMANCE.md`）**：

| 变量 | 默认 | 行为 |
|------|------|------|
| `BEAR2WAVE_VCD_LAZY` | `-1` | `-1`=按大小自动；`1`=强制懒加载；`0`=强制全文 |
| `BEAR2WAVE_VCD_LAZY_MB` | `10` | 自动模式下 ≥该 MB 用懒加载 |
| `BEAR2WAVE_VCD_SCAN_PROGRESS` | `0` | `1` 时状态栏报告扫描进度（可选，P4-8 扩展） |

---

### 阶段 C — 扫描性能（P4-3c，大文件关键）

| ID | 任务 | 说明 | 优先级 |
|----|------|------|--------|
| C1 | **按信号 ID 索引**（侧车 `.vcdidx` 可选） | 首次全扫建立 `(signal_id, file_offset)` 或按时间块索引；二次起按 offset seek，避免每次 O(文件) 扫描 | P1 |
| C2 | 时间块粗索引 | 每 N MB 或每 M 个 `#timestamp` 记录文件 offset，加载 `[t0,t1]` 时只读相关区间 | P1 |
| C3 | 缓冲 I/O | `setvbuf` 或大缓冲区 `fread` 块解析；避免逐字节 `fgetc` 在全文件上 | P0 |
| C4 | 增量加载 | 信号已有 `trace_loaded_t0/t1`（`signal_t` 已有字段）时，只读窗口 **外侧** 缺口（与 FST gap 逻辑对齐，可参考 `core/trace_load_gaps.h`） | P1 |

**侧车格式（若做 C1）**：建议 `\<vcdpath\>.bwvcdidx`，版本号 + 源文件 mtime/size；与 FST `.bwidx`（`core/trace_sidecar_idx.cpp`）风格一致。

---

### 阶段 D — 内存优化（P4-4，可与 B 并行）

| ID | 任务 | 文件 | 说明 |
|----|------|------|------|
| D1 | `trace_compact_vc` | `core/trace_compact_vc.h/.cpp` | 1-bit 标量：紧凑 `timestamp + 2bit value`；`BEAR2WAVE_COMPACT_VC=1` 默认开 |
| D2 | 统一访问层 | `trace_vc_at()` / `trace_vc_count()` | `WaveformPanel::RawValueAtOrBefore`、`waveform_local_stats.cpp` 禁止直接假设 `value_changes[]` |
| D3 | 与懒加载协同 | `vcd_load_signals` | append 时可写紧凑数组；`vcd_signal_shrink_to_fit` 兼容 |

**注意**：若 D 改动面大，可 **先做 P4-3**，P4-4 作为第二 PR。

---

### 阶段 E — UI / 进度 / 文档（P4-3d + E1）

| ID | 任务 | 完成标准 |
|----|------|----------|
| E1 | 打开大 VCD 时状态栏 | 「正在建立层次…」/「正在扫描跳变…」非模态提示 |
| E2 | 复用 `TraceLoadMetricsPanel` | 显示已加载跳变数、耗时（与 FST 一致） |
| E3 | 更新文档 | `LARGE_TRACE_PERFORMANCE.md`、`LARGE_FILE_AND_ENGINEERING_TODO.md` 将 P4-3 标 `[x]` |
| E4 | `TraceTools bench` 支持 `.vcd` | 输出：open 层次耗时、load N 路 [t0,t1] 耗时、RSS（可选） |
| E5 | 样例与脚本 | `tests/traces/` 下 medium VCD 或 `tools/gen_large_vcd.ps1`（若不存在则新建） |

---

## 4. 测试计划

### 4.1 样例文件

| 样例 | 用途 |
|------|------|
| `tests/traces/bear2wave_sample.vcd` | 回归：全文 vs 懒加载结果一致 |
| 自建 ~50MB / ~200MB VCD | 性能：打开内存、选 10 路加载耗时 |
| `tests/traces/test2.vcd` | 手工：层次树 + 波形显示 |

### 4.2 自动化

```powershell
# 构建
MSBuild TEST1\TEST1.vcxproj /p:Configuration=Debug /p:Platform=x64

# 基准（实现 E4 后）
$env:BEAR2WAVE_VCD_LAZY='1'
.\out\x64\Debug\TraceTools.exe bench tests\traces\large_test.vcd 20

# 一致性（建议加 tools/vcd_lazy_smoke.cpp）
# 同一 vcd：vcd_read_from_path 与 vcd_open_lazy + load full window 的 hash 对比
```

### 4.3 手工验收（M3）

1. `BEAR2WAVE_VCD_LAZY=1` 打开 500MB 级 VCD → 任务管理器 RSS 明显低于全文模式。
2. 只添加 5–20 路信号 → 波形正常；拖时间轴 → 后台补载，无卡死。
3. `BEAR2WAVE_MAX_LOADED_CHANGES=5000000` → 多路后 LRU 驱逐，不 OOM。
4. 小文件（<10MB）默认行为与改前一致（或显式 `BEAR2WAVE_VCD_LAZY=0`）。

---

## 5. 工程约束（Claude 必须遵守）

1. **编译**：`TEST1.vcxproj` 已启用 `/utf-8` 与 `/FS`；新增 `.cpp` 须加入 vcxproj 与 `CMakeLists.txt`（若存在）。
2. **编码**：`vcd.h` 含中文注释，源文件 UTF-8；`SignalGroup` 成员名为 `signal_ptrs`（勿改回 `signals`）。
3. **范围**：不改 wx 主框架大结构（E5-1）；P4-5 虚拟列表可只做 TODO 留坑。
4. **API**：对外仍用 `vcd_t` / `signal_t` / `trace_load_from_path`；新函数加在 `vcd_lazy.h` 或 `vcd.h` 的 `#ifdef __cplusplus` 区。
5. **提交**：按阶段 A→B→C 分 commit；每阶段后 **Debug x64 全量编译通过**。

---

## 6. 建议 PR / 对话节奏

| PR | 内容 | 预估 |
|----|------|------|
| PR1 | A + B（懒加载骨架 + trace_loader 接入） | 核心，先合 |
| PR2 | C1–C3（索引 + 缓冲 I/O） | 大文件性能 |
| PR3 | D（紧凑 VC，可选） | 内存减半 |
| PR4 | E（进度、bench、文档） | 可观测性 |

---

## 7. 给 Claude 的启动 Prompt（可直接复制）

```text
你在 Bear2Wave 仓库实现 VCD 大文件懒加载（任务单 docs/VCD_LARGE_FILE_TASK.md）。

约束：
- 对齐 fst_open_lazy / fst_load_signals / trace_load_signals 模式；
- 阶段 A+B 优先，完成 Debug x64 编译与 bear2wave_sample.vcd 回归；
- 遵守 /utf-8、SignalGroup::signal_ptrs、不扩大范围到 MainFrame 重构。

开始前请阅读：vcd.cpp（vcd_read_from_path）、trace_loader.cpp（vcd 分支）、fst_loader.h、core/trace_backend.h、panels/WaveformPanel.hpp（EnsureTraceSignalLoadedSync）。

输出：每阶段简要说明设计、改动文件列表、如何运行 bench/smoke 验证。
```

---

## 8. 相关路径速查

| 路径 | 作用 |
|------|------|
| `TEST1/vcd.cpp`, `TEST1/vcd.h` | 现有全文 VCD 解析 |
| `TEST1/trace_loader.cpp` | 格式分发与 `trace_load_signals` |
| `TEST1/fst_loader.cpp` | 懒加载参考实现 |
| `TEST1/panels/WaveformPanel.hpp` | 视口加载、后台线程 |
| `TEST1/core/trace_memory_budget.cpp` | LRU |
| `TEST1/core/trace_load_gaps.h` | 增量时间窗缺口 |
| `TEST1/core/trace_sidecar_idx.cpp` | FST `.bwidx` 侧车（VCD 索引可参考） |
| `docs/LARGE_FILE_AND_ENGINEERING_TODO.md` | P4-3/P4-4 总表 |
| `docs/LARGE_TRACE_PERFORMANCE.md` | 用户向 env 文档 |

---

*任务单版本：2026-05-19 · 维护者：完成子任务后回写 TODO 清单 `[x]` 状态。*
