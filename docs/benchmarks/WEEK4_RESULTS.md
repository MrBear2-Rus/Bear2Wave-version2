# Week 4 基准与验收记录

> **修订**：第四周主线为 **波形格式**（W4-F1～F16）。Compare 见第五周。  
> 矩阵：[FORMAT_SUPPORT_MATRIX.md](../FORMAT_SUPPORT_MATRIX.md) · CLI：`run_trace_tests.ps1`

## 环境

| 项 | 值 |
|----|-----|
| 日期 | |
| OS | Windows x64 |
| 构建 | Release / Debug |
| CPU / RAM | |
| Bear2Wave 版本 | |
| 样例路径 | |

## 格式 CLI（`run_trace_tests.ps1`）

| 扩展名 | 样例 | test 结果 | 备注 |
|--------|------|-----------|------|
| .vcd | bear2wave_sample.vcd | | |
| .fst | bear2wave_sample.fst | | |
| .vzt | bear2wave_sample.vzt | | 需宏 |
| .lxt2 | bear2wave_sample.lxt2 | | 需宏 |
| .ghw | bear2wave_sample.ghw | | 需宏 |

## 格式 GUI（`tests/FORMAT_SMOKE_CHECKLIST.md`）

| 格式 | § 结果 | 测试人 | 备注 |
|------|--------|--------|------|
| VCD / FST | | | |
| VZT / LXT2 / GHW | | | |

## Compare（第五周 — 可选预跑）

| 场景 | 主轨 | 次轨 | 操作 | 结果 | 备注 |
|------|------|------|------|------|------|
| 双窗联动 | sample.fst | test2.vcd | SMOKE §4 | | |
| 对齐加载 | 大 FST | 大 FST / VCD | 主窗平移 30s | | W5 |

## FST（轻量签字）

| 场景 | 信号数 | 操作 | 耗时 | 备注 |
|------|--------|------|------|------|
| 打开 `large_test.fst` | — | 冷启动 | | |
| 视口平移 | 20 | 连续拖 30s | | 主观卡顿 Y/N |

## 手工冒烟（Release）

| 章节 | 结果 PASS/FAIL | 测试人 | 备注 |
|------|----------------|--------|------|
| SMOKE §1～3b | | | |
| FORMAT_SMOKE §1～2 | | | |
| SMOKE §4 Compare（W5） | | | |
