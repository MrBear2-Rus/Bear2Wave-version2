# 阶段 A — 稳态 TODO

> 架构 E5-1 收口后的质量线。完整路线见 [NEXT_ROADMAP.md](NEXT_ROADMAP.md)（若存在）或 [CLAUDE_HANDOFF.md](CLAUDE_HANDOFF.md)。

**状态**：`[ ]` 未做 · `[~]` 进行中 · `[x]` 完成

---

## A1 — 固定冒烟清单

| ID | 任务 | 状态 |
|----|------|------|
| A1-1 | 编写 `tests/SMOKE_CHECKLIST.md`（手工步骤 + 样例路径） | [x] |
| A1-2 | `tests/README.md` 链接冒烟清单 | [x] |
| A1-3 | 可选：`tools/run_smoke.ps1` 调用 TraceTools + 样例路径 | [x] |

**验收**：新人按清单 15 分钟内可验证「能开、能画、能存会话」。

---

## A2 — `trace_vc` 全路径审计

| ID | 模块 | 状态 |
|----|------|------|
| A2-1 | `panels/WaveformPanel.hpp`（边沿/频率/颜色/导出相关） | [x] |
| A2-2 | `core/TraceDocument.cpp`（CSV max 时间戳） | [x] |
| A2-3 | `ui/MainFrame.hpp`（CSV 导出/加载） | [x] |
| A2-4 | `waveform_analysis.cpp` / `waveform_local_stats.cpp` | [x] |
| A2-5 | `AIAnalysisPanel.cpp` | [x] |
| A2-6 | `panels/WaveformRenderer.cpp` `TraceVcValueAt` | [x] |

**规则**：禁止用 `!sig->value_changes` 判断「无数据」；用 `trace_vc_count(sig)==0`。读时间/值用 `trace_vc_timestamp` / `trace_vc_format_value`。

---

## A3 — 构建卫生

| ID | 任务 | 状态 |
|----|------|------|
| A3-1 | `Main.cpp` 不 `#include` 其它 `.cpp` | [x] |
| A3-2 | `TEST1.vcxproj` 收录 Session/Tcl/Compare/Menus 等 | [x] |
| A3-3 | Debug x64 全量编译通过 | [x] |
| A3-4 | Release x64 编译通过（wx 仅 debug 库时链 `/MDd`） | [x] |

---

## 执行顺序（当前）

1. A1-1 / A1-2  
2. A2-1 → A2-3  
3. A3-3  
