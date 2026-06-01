# 阶段 1 验收指南（E1-1～E1-5）

前置：阶段 0 已通过（见 [FORMAT_PHASE0_ACCEPTANCE.md](FORMAT_PHASE0_ACCEPTANCE.md)）。

## CLI

```powershell
powershell -ExecutionPolicy Bypass -File tools\run_trace_tests.ps1 -Configuration Release
tools\x64\Release\TraceTools.exe caps
tools\x64\Release\TraceTools.exe test tests\traces\bear2wave_sample.csv
```

**期望：**

- `caps` 含 `vcd evcd fst fzt csv : always`
- `bear2wave_sample.csv` → `[test] PASS`，`signals≥1`，`changes>0`
- 脚本退出码 0

## GUI

| ID | 操作 | 期望 |
|----|------|------|
| E1-1 | 若有 `.evcd` 样例，File → Open | 与 VCD 相同，能看方波 |
| E1-2 | 打开 `tests\traces\bear2wave_sample.csv` | 模块 CSV；加 Signal1 有方波 |
| E1-5 | 过滤器含 `.evcd`、`.fzt` | 可见 |

## 矩阵更新

在 [FORMAT_SUPPORT_MATRIX.md](../docs/FORMAT_SUPPORT_MATRIX.md) 增加行：

| `.evcd` | PASS（同 vcd） | SKIP* | … | |
| `.csv` | PASS | PASS | … | `bear2wave_sample.csv` |

\* 无 evcd 样例时 CLI/GUI 标 SKIP

## E1-3 / E1-4

- **E1-3**（LXT v1 Windows 样例）：仍为 SKIP，不阻塞 E1 收尾
- **E1-4**（实型/字符串）：`core/trace_display.cpp` 已集中分类；有 FST real/string 文件时 GUI 应显示折线/文本行
