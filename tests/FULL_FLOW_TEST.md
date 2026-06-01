# Bear2Wave 全流程测试

**版本**：0.2.1-beta  
**构建**：`out/x64/Release/Bear2Wave.exe`  
**自动化入口**：`powershell -ExecutionPolicy Bypass -File tools\run_full_flow.ps1 -Configuration Release`

---

## 测试分层

| 层 | 内容 | 执行方式 |
|----|------|----------|
| **0** | 发布静态检查 | `tools\release_check.ps1` |
| **1** | 加载器冒烟 | `tools\run_smoke.ps1` |
| **2** | 全格式 CLI | `tools\run_trace_tests.ps1` |
| **3** | Filter / 搜索 / 转换 | TraceTools 扩展命令 |
| **4** | GUI 全流程 | 本文 §A～§G（手工） |

报告输出：`tests/output/full_flow_report.txt`

---

## 自动化结果（2026-06-01 · 真 Release /MD 复测）

> 构建：`out/x64/Release/Bear2Wave.exe`（`MSVCP140.dll`，无 `*140D`）  
> 发布包：`dist/Bear2Wave-0.2.1-beta-win64.zip`

### Phase 0 — 静态检查

| 项 | 结果 |
|----|------|
| release_check | **PASS**（wx **mswu OK**） |

### Phase 1 — run_smoke（含 `-IncludeLarge`）

| 项 | 结果 | 备注 |
|----|------|------|
| sample FST / VCD / test2 | **PASS** | |
| context-snapshot | **PASS** | |
| cancel-smoke (large_test.fst) | **PASS** | |
| large FST (~100MB) | **PASS** | 1024 信号 / 800008 跳变 |
| large VCD (~100MB) | **PASS** | 64216 跳变 |

### Phase 2 — run_trace_tests

| 项 | 结果 |
|----|------|
| 全格式小样例 | **PASS** |
| test-e3 / test-e4 / test-e5 | **PASS** |

### Phase 3 — TraceTools 扩展

| 项 | 结果 |
|----|------|
| test-fp0 / fp1 / fp2 | **PASS** |
| sst-filter / pattern-search | **PASS** |
| sim-log / pow10-snap | **PASS** |
| vcd2fst + roundtrip | **PASS** | 13 信号 / 43 跳变 |

---

## 自动化结果（2026-06-01 · 初测，伪 Release）

### Phase 0 — 静态检查

| 项 | 结果 |
|----|------|
| release_check | **PASS**（wx mswu WARN 可忽略） |

### Phase 1 — run_smoke（含 `-IncludeLarge`）

| 项 | 结果 | 备注 |
|----|------|------|
| sample FST / VCD / test2 | **PASS** | |
| context-snapshot | **PASS** | |
| cancel-smoke (large_test.fst) | **PASS** | |
| large FST (~100MB) | **PASS** | 1024 信号 / 800008 跳变 |
| large VCD (~100MB) | **PASS** | 64216 跳变 |

### Phase 2 — run_trace_tests

| 项 | 结果 |
|----|------|
| 全格式小样例 | **PASS** |
| test-e3 / test-e4 / test-e5 | **PASS** |
| test-e4-convert | **SKIP (Windows)** | TraceTools 写 FST teardown；GUI 不受影响 |

### 修复说明

**根因**：TraceTools 未默认走 FST **GEOM-only** 层次（GUI 在 Windows 上默认跳过 embedded `.hier` gzip），与 vcpkg zlib 冲突 → `0xC0000409`。

**修复**：`fst_loader.cpp` — 所有 `_WIN32` 默认 GEOM-only（`BEAR2WAVE_FST_HIER=1` 可尝试 embedded `.hier`）。

### 已知限制

- **TraceTools `vcd2fst`**：Windows CLI 写 FST 时 `fstWriterClose` 退出异常（与 GUI 无关；GUI 打开大 FST 已验证正常）

---

## §A — 启动与打开（GUI）

| # | 操作 | 样例 | 结果 | 备注 |
|---|------|------|------|------|
| A.1 | 启动 Release exe | — | ☐ | |
| A.2 | File → Open FST | `tests/traces/bear2wave_sample.fst` | ☐ | 模块树 + 状态栏 |
| A.3 | File → Open VCD | `tests/traces/test2.vcd` | ☐ | |
| A.4 | File → Open 大 VCD | `tests/traces/large_test.vcd` | ☐ | 懒加载 / 警告 |
| A.5 | File → Open 大 FST | `tests/traces/large_test.fst` | ☑ CLI | CLI + 你已验证 GUI |

---

## §B — 显示与交互

| # | 操作 | 结果 | 备注 |
|---|------|------|------|
| B.1 | 双击 1-bit 信号（clk） | ☐ | 方波可见，DirectWrite 文字 |
| B.2 | 添加多 bit 总线 | ☐ | hex/方框 |
| B.3 | 滚轮缩放 / 拖时间轴 | ☐ | |
| B.4 | 添加 15+ 信号，名列滚动 | ☐ | 滚轮 / ↑↓ |
| B.5 | View → Theme Dark/Light | ☐ | 文字颜色随主题 |
| B.6 | Markers + Ctrl 拖 A/B 测量 | ☐ | ΔT 显示 |

---

## §C — 会话

| # | 操作 | 结果 |
|---|------|------|
| C.1 | Write Session → `tests/output/full_flow.bwv` | ☐ |
| C.2 | 清空后 Read Session 恢复 | ☐ |
| C.3 | Read GTKWave session（若有） | ☐ |

---

## §D — Compare

| # | 操作 | 结果 |
|---|------|------|
| D.1 | Compare → Open Second Trace | ☐ |
| D.2 | Link Playheads 同步 | ☐ |
| D.3 | Link Time View 同步 | ☐ |
| D.4 | Tile Horizontally | ☐ |

---

## §E — 分析与 AI

| # | 操作 | 结果 |
|---|------|------|
| E.1 | AI 面板 → 从波形刷新 | ☐ |
| E.2 | 本地报告 | ☐ |
| E.3 | Transaction Filter + AI 协议模板 | ☐ |
| E.4 | Search → Pattern Search | ☐ |

---

## §F — 菜单 / 诊断

| # | 操作 | 结果 |
|---|------|------|
| F.1 | Help → Contents (Ctrl+F1) | ☐ |
| F.2 | View → Export diagnostics bundle | ☐ |
| F.3 | File → Print / Grab PNG | ☐ |

---

## §G — Tcl（若编译 BEAR2WAVE_WITH_TCL）

| # | 操作 | 结果 |
|---|------|------|
| G.1 | Script → Run Script | `TEST1/examples/scripts/demo_wave.tcl` | ☐ |
| G.2 | `bear2wave_help` 输出 40+ 命令 | ☐ |

---

## 签字

| 日期 | 构建 | 测试人 | CLI | GUI | 备注 |
|------|------|--------|-----|-----|------|
| 2026-06-01 | Release 0.2.1-beta | 维护者 | **PASS** | **待复测** | 真 Release /MD；CLI 全绿；GUI 用 dist zip 复测 §A～G |

---

*更新：每轮全流程测试后刷新「自动化结果」表与签字行。*
