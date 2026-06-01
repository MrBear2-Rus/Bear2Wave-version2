# 测试与样例数据

本目录集中存放波形、CSV、会话等**非源码**测试资源，避免堆在仓库根目录。

## 目录

| 子目录 | 内容 |
|--------|------|
| [`traces/`](traces/) | VCD / FST / VZT / LXT2 / GHW 等波形样例；`run_trace_tests.ps1` 生成 `bear2wave_*` |
| [`csv/`](csv/) | CSV 导入测试用例 |
| [`sessions/`](sessions/) | GTKWave `.gtkw`、Bear2Wave `.bwv` 会话样例 |
| [`output/`](output/) | 本地运行产物（分析报告、bench JSON 等，**不提交**） |

## 冒烟测试（发版前）

手工清单：**[SMOKE_CHECKLIST.md](SMOKE_CHECKLIST.md)**（约 15 分钟，含 1-bit 波形、会话、Compare）。

## 常用命令

```powershell
# CLI 冒烟（构建 Bear2Wave + TraceTools，测样例 trace）
powershell -ExecutionPolicy Bypass -File tools\run_smoke.ps1

# 生成 + 跑通各格式样例
powershell -ExecutionPolicy Bypass -File tools\run_trace_tests.ps1

# 单文件
tools\x64\Debug\TraceTools.exe test tests\traces\bear2wave_sample.fst
```

## 根目录请勿再放

- `*.vcd` / `*.fst` / `*.csv` → 放进 `tests/traces` 或 `tests/csv`
- `analysis.txt`、`bench_results.json` → `tests/output/`
