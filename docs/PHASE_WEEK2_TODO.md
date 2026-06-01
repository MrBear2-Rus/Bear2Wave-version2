# 第二周 — TODO（冒烟通过后）

> 第一周：阶段 A + B/C/E3 已完成；手工冒烟已 PASS。  
> 状态：`[ ]` 未做 · `[~]` 部分 · `[x]` 完成

---

## 验收（已完成）

| 项 | 状态 |
|----|------|
| `tests/SMOKE_CHECKLIST.md` 手工冒烟 | [x] 用户确认 |
| `tests/traces/large_test.vcd` / `large_test.fst` | [x] ~100MB 样例 |

---

## 第二周任务

| ID | 任务 | 状态 |
|----|------|------|
| W2-1 | `tools/run_smoke.ps1`（TraceTools + 样例路径） | [x] |
| W2-2 | E5-2：`WaveformPanel_trace_load.cpp` 拆分懒加载 | [x] |
| W2-3 | P4-7：`RowSegmentBudget` 接入 `WaveformRenderer` | [x] |
| W2-4 | A3-4：Release x64 编译 | [x] |
| W2-5 | P4-6：`ModuleTreeLazyCtrl` 展开时填充子模块 | [x] |
| W2-6 | E1-6：`TraceTools context-snapshot` + golden hash | [x] |
| W2-7 | E1-8：`TraceTools cancel-smoke` | [x] |

---

## 命令速查

```powershell
# CLI 冒烟（不打开 UI）
powershell -ExecutionPolicy Bypass -File tools\run_smoke.ps1

# 含大文件 loader 测试（较慢）
powershell -ExecutionPolicy Bypass -File tools\run_smoke.ps1 -IncludeLarge

# Release 构建
msbuild TEST1\TEST1.vcxproj /p:Configuration=Release /p:Platform=x64
```
