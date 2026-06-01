# 第三周 — TODO

> **收尾**：见 [WEEK3_CLOSEOUT.md](WEEK3_CLOSEOUT.md)  
> 第二周 W2-1～W2-7 完成后进入「发布准备 + 大文件深化 + 架构收尾」。  
> 状态：`[ ]` 未做 · `[~]` 部分 · `[x]` 完成 · `[→]` 延后

---

## 目标

| 里程碑 | 验收 |
|--------|------|
| **M1 可发布** | Release 包、CI 绿、文档齐 |
| **M2 大 FST 稳** | 1GB / 20 路平移可接受（已手工冒烟 large_test） |
| **M3 大 VCD** | 500MB 懒加载 + 侧车（已具备，需可选 nightly） |
| **M4 多信号可视** | 显示列表超出视口时可垂直滚动，全部信号可达 |

---

## P0 — 多信号垂直滚动

| ID | 任务 | 说明 | 状态 |
|----|------|------|------|
| W3-16 | 波形面板垂直滚动 | `m_signalScrollRow` + 仅绘制可见行 | [x] |
| W3-16b | 滚动条拖拽 | 右侧 track 点击/拖动跳转 | [x] |
| W3-16c | 可见行缓存（可选） | `BuildCacheAsync` 仅为可见行建 segment | [x] |

---

## P0 — 发布与 CI

| ID | 任务 | 说明 | 状态 |
|----|------|------|------|
| W3-1 | `package_release.ps1` 验收 | Release/Debug 包 + 样例 FST/VCD | [x] |
| W3-2 | CI 跑 `run_smoke.ps1` | `windows-ci.yml` + `-TraceToolsOnly` | [x] |
| W3-3 | 大文件 nightly（可选） | `windows-ci-nightly.yml` + `-IncludeLarge` | [x] |
| W3-4 | wx Release 库对齐 | 本机 Release 已验证；流程见 W4-1 | [x] |

---

## P1 — 架构收尾（E5）

| ID | 任务 | 说明 | 状态 |
|----|------|------|------|
| W3-5 | 启用 `SignalModuleTree` | 懒加载树 + 信号列表；MainFrame 委托 | [x] |
| W3-6 | `TraceDocument` 接管模块索引/CSV | `m_traceDocument` + `ModuleIndex()` | [x] |
| W3-7 | `WaveformPanel` 再瘦身 | `WaveformPanel_measure.cpp`（频率/占空比） | [x] |
| W3-8 | `MainFrameMenus` 全接线 | `MenuIds.hpp` + `MainFrameMenus.cpp` | [x] |

---

## P1 — 大文件（P4 剩余）

| ID | 任务 | 说明 | 状态 |
|----|------|------|------|
| W3-9 | P4-9 VZT 加固 | 块失败重试 / 超时（`BEAR2WAVE_VZT_BLOCK_*`） | [x] |
| W3-10 | P4-11 并行块读 | FST 多 block 线程池（需独立 reader，见 CLOSEOUT） | [→] |
| W3-11 | 模块树压测 | `tools/bench_module_tree.ps1` + 手工打开 | [x] |

---

## P2 — 体验与文档

| ID | 任务 | 说明 | 状态 |
|----|------|------|------|
| W3-12 | E3-4 环境变量帮助页 | `docs/ENVIRONMENT.md` + Help 目录 | [x] |
| W3-13 | E3-5 诊断包导出 | View → Export diagnostics bundle | [x] |
| W3-14 | E4-4 快速入门截图 | 搁置；占位见 QUICKSTART / images/README | [—] |
| W3-15 | README 发布说明 | Alpha 限制、大文件 env 表 | [x] |

---

## 建议节奏（5 天）

```text
Day 1   W3-16 / W3-1 / W3-2  垂直滚动 + 发布脚本 + CI        [done]
Day 2   W3-5 / W3-6  SignalModuleTree + TraceDocument         [done]
Day 3   W3-7 / W3-8  Panel 瘦身 + 菜单                       [done]
Day 4   W3-9 / W3-11  VZT + 模块树压测                        [done]
Day 5   W3-12～15 / 收尾  文档 + 包 + CLOSEOUT                 [done]
```

---

## 命令速查

```powershell
powershell -ExecutionPolicy Bypass -File tools\release_check.ps1
powershell -ExecutionPolicy Bypass -File tools\run_smoke.ps1 -TraceToolsOnly -SkipBuild
powershell -ExecutionPolicy Bypass -File tools\package_release.ps1
powershell -ExecutionPolicy Bypass -File tools\package_release.ps1 -Configuration Release

msbuild TEST1\TEST1.vcxproj /p:Configuration=Release /p:Platform=x64

tools\x64\Debug\TraceTools.exe context-snapshot tests\traces\bear2wave_sample.vcd tests\fixtures\context_snapshot.hash
tools\x64\Debug\TraceTools.exe cancel-smoke tests\traces\large_test.fst 20
powershell -ExecutionPolicy Bypass -File tools\bench_module_tree.ps1 -Modules 5000
```
