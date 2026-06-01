# 阶段 3（E3）验收 — 显示语义

> 路线图：[FORMAT_EXTENSION_ROADMAP.md](../docs/FORMAT_EXTENSION_ROADMAP.md)

**阶段 3 状态：已完成（E3-1～E3-4）**

## E3-1 Transaction 行（P5-1）

| 检查项 | 命令 / 操作 | 期望 |
|--------|-------------|------|
| 类型分类 | `TraceTools.exe test-e3` | PASS；`FST_VT_VCD_EVENT` / `BEAR2WAVE_VT_TRANSACTION` → `TransactionEvent` |
| GUI | FST/VZT 中带 event 类型信号加入波形 | 紫色事件条（区别于普通 TextString 米色） |

实现：`core/trace_display.cpp` 新增 `Bear2waveTraceKind::TransactionEvent`；`WaveformPainter` 专用配色。

## E3-2 GHW 9 态字符映射

| 检查项 | 命令 / 操作 | 期望 |
|--------|-------------|------|
| 波形电平 | `TraceTools.exe test-e3` | `L→0`、`H→1`、`U/W/D→x` |
| 光标标签 | GUI 打开 `bear2wave_sample.ghw`，scalar 信号 | 显示原始 `U`/`H` 等（非强制折叠为 `x`） |
| Loader 标记 | GHW `std_logic` (e8) | `fst_var_type = BEAR2WAVE_VT_GHW_LOGIC` |

实现：`core/ghw_state.cpp`；`ParseVcdValue` / `trace_vc` compact 统一映射。

## E3-3 Alias / 总线展开

| 检查项 | 命令 / 操作 | 期望 |
|--------|-------------|------|
| Alias 委托 | `TraceTools.exe test-e3` | alias 行 `trace_vc_count` 与 root 一致 |
| 层次树 | LXT/LXT2/VZT 含 alias fac 的文件 | 树中可见 alias 名称；波形数据来自 root |

实现：`signal_t::trace_alias_source`；LXT1/LXT2/VZT hierarchy 第二遍追加 alias 节点。

## E3-4 Dumpoff / blackout 段

| 检查项 | 命令 / 操作 | 期望 |
|--------|-------------|------|
| 解析 | `TraceTools.exe test-e3` | 生成 `bear2wave_e3_blackout.vcd`；span `[100,300]` |
| 全量 + Lazy | 同上 | 全量 `vcd_read_from_path` 与 `vcd_open_lazy` 均检测到 blackout |
| GUI | 打开含 `$dumpoff`/`$dumpon` 的 VCD | 波形区对应时段灰色半透明遮罩 |

实现：`core/trace_blackout.cpp`；`vcd.cpp` / `vcd_lazy.cpp` 解析；`WaveformPainter` 绘制。

## 一键 CLI

```powershell
cd "E:\EDA_Race\TEST1 - 1\TEST1"
powershell -ExecutionPolicy Bypass -File tools\run_trace_tests.ps1 -Configuration Release
```

应包含 **test-e3 PASS**（与 E2 项并列）。

单独运行：

```powershell
.\tools\x64\Release\TraceTools.exe test-e3 .\tests\traces
```

## GUI 冒烟（L3）

| 步骤 | 期望 |
|------|------|
| 打开 `bear2wave_sample.vcd` | 正常方波 |
| 打开 `bear2wave_e3_blackout.vcd`（test-e3 生成） | t=100～300 区间灰色 blackout 带 |
| 打开 `bear2wave_sample.ghw`（若已编译 GHW） | scalar 光标显示 9 态字符 |
| FST 中带 event 的信号（若有） | 紫色 Transaction 行 |
