# 第四周 — TODO（波形格式检查与扩展）

> **拍板（修订）**  
> - 版本：**`0.2.0-beta`**  
> - **本周重心**：各波形格式的 **支持矩阵盘点 → CLI/GUI 验收 → 缺口修复与扩展**  
> - Compare 双窗、对齐加载等 → **[→ 第五周](PHASE_WEEK5_BACKLOG.md)**（原 W4-10～19）  
> - 截图 W3-14 **搁置**  

> 前置：[WEEK3_CLOSEOUT.md](WEEK3_CLOSEOUT.md) · 格式构建：[TRACE_FORMATS_BUILD.md](TRACE_FORMATS_BUILD.md) · 工程总清单：[LARGE_FILE_AND_ENGINEERING_TODO.md](LARGE_FILE_AND_ENGINEERING_TODO.md)  
> 状态：`[ ]` 未做 · `[~]` 部分 · `[x]` 完成 · `[→]` 延后 · `[—]` 搁置

---

## 本周目标（4 个里程碑）

| 里程碑 | 含义 | 验收 |
|--------|------|------|
| **M1 支持矩阵清晰** | 每种扩展名在「编译开/关 × CLI × GUI」下的状态一张表 | `docs/FORMAT_SUPPORT_MATRIX.md` 填完 |
| **M2 CLI 全格式绿** | TraceTools `gen-all` + `test` 对样例扩展名 PASS | `run_trace_tests.ps1` + `run_smoke.ps1` 绿 |
| **M3 GUI 可打开可显示** | 每种已启用格式：打开 → 模块树 → 加信号 → 有方波 | `tests/FORMAT_SMOKE_CHECKLIST.md` 核心行 PASS |
| **M4 扩展与一致** | 过滤器/扩展名/错误提示与 `trace_loader` 一致；修已知缺口 | README + Help 与矩阵一致 |

---

## 格式支持基线（盘点起点）

| 扩展名 | Loader | 编译宏 | 懒加载 | 样例（仓库） | 备注 |
|--------|--------|--------|--------|--------------|------|
| `.vcd` | `vcd.cpp` | 始终 | 按大小 | `bear2wave_sample.vcd`, `test2.vcd` | 大文件 lazy + 侧车 |
| `.fst` / `.fzt` | `fst_loader.cpp` | 始终 | 是 | `bear2wave_sample.fst`, `large_test.fst` | **`.fzt` 为 FST 拼写容错** |
| `.vzt` | `vzt_loader.cpp` | `BEAR2WAVE_WITH_VZT` | 是 | `bear2wave_sample.vzt` | W3-9 块重试已合入 |
| `.lxt` | `lxt1_loader.cpp` | `BEAR2WAVE_WITH_LXT2` | 全量 | `bear2wave_sample.lxt` | 线性 LXT v1（`gen-lxt` / `vcd2lxt -linear`） |
| `.lxt2` | `lxt2_loader.cpp` | `BEAR2WAVE_WITH_LXT2` | 是 | `bear2wave_sample.lxt2` | 按文件头 0x1380 自动识别 |
| `.ghw` | `ghw_loader.cpp` | `BEAR2WAVE_WITH_GHW` | 是 | `gen-all` 可生成 | `run_trace_tests` 曾标 GHW 待完成 |
| Transaction | FST/VZT 事件 | — | — | — | 非独立文件；P5-1 延后 |

未定义宏时：仍可编译；打开对应扩展名应 **明确报错**（非静默失败）。详见 [TRACE_FORMATS_BUILD.md](TRACE_FORMATS_BUILD.md)。

---

## P0 — 盘点与自动化检查（Day 1～2）

| ID | 任务 | 说明 | 状态 |
|----|------|------|------|
| W4-F1 | **支持矩阵文档** | 新建 `docs/FORMAT_SUPPORT_MATRIX.md`：扩展名 × CLI/GUI × 宏 × 样例路径 × PASS/FAIL | [ ] |
| W4-F2 | **编译能力探测** | 脚本或 TraceTools 子命令输出：当前 exe 是否含 VZT/LXT2/GHW（`#ifdef` 或试开样例） | [ ] |
| W4-F3 | **run_trace_tests 强化** | 覆盖 `tests/traces` 下全部样例；GHW 期望从「必 FAIL」改为按宏判定 | [ ] |
| W4-F4 | **CI 接入格式测试** | `windows-ci.yml` 在 TraceTools 构建后跑 `run_trace_tests.ps1`（或 `test` 子集） | [ ] |
| W4-F5 | **扩展名一致性审计** | 统一 `trace_loader`、`MainFrame` 对话框、`ProjectStartWindow`（`*.fzt` vs `*.fst`） | [ ] |

### W4-F5 已知不一致（待修）

| 位置 | 问题 |
|------|------|
| `ProjectStartWindow.cpp` | 过滤器含 `*.fzt`，`MainFrame` 部分对话框仅 `*.fst` |
| `trace_loader.cpp` | 接受 `fzt` 扩展名；UI 未统一展示 |
| `CLAUDE_HANDOFF.md` | 曾记录 `fzt` 笔误，纳入 F5 一并关闭 |

---

## P0 — GUI 格式冒烟（Day 2～3）

| ID | 任务 | 说明 | 状态 |
|----|------|------|------|
| W4-F6 | **格式冒烟清单** | `tests/FORMAT_SMOKE_CHECKLIST.md`：每格式 打开→加信号→方波 | [ ] |
| W4-F7 | **VCD / FST** | 回归 §1；含 `test2.vcd`、`bear2wave_sample.fst` | [~] |
| W4-F8 | **VZT / LXT2** | 宏开启构建下打开 `bear2wave_sample.vzt` / `.lxt2` | [ ] |
| W4-F9 | **GHW** | 若有样例：打开 + 层次 + 至少 1 路波形；无样例则 gen-all + 文档说明 | [ ] |
| W4-F10 | **打开失败 UX** | 未启用宏 / 坏文件：状态栏 + 对话框 + 指向 `TRACE_FORMATS_BUILD.md` | [ ] |

---

## P1 — 扩展与加固（Day 3～5）

| ID | 任务 | 说明 | 状态 |
|----|------|------|------|
| W4-F11 | **P4-9 VZT 余量** | 单信号失败隔离、错误日志（在 W3-9 重试/超时之上） | [ ] |
| W4-F12 | **GHW  loader 收尾** | 若 `ghw_loader` 未达 CLI test PASS：补齐至与 FST 同级懒加载 | [ ] |
| W4-F13 | **LXT1 线性读入** | `lxt1_loader` + `gen-lxt` 样例；交织 LXT v1 仍提示转 LXT2/FST | [x] |
| W4-F0 | **阶段 0 收尾** | 见 [FORMAT_EXTENSION_ROADMAP.md](FORMAT_EXTENSION_ROADMAP.md) E0-1～E0-6 | [x] |
| W4-F14 | **诊断包格式信息** | Export diagnostics：backend 种类、宏开关、最后 `trace_loader` 错误 | [ ] |
| W4-F15 | **Help / README** | `USER_GUIDE`、`docs/help/`、README 格式表与矩阵同步 | [ ] |
| W4-F16 | **package 样例** | `package_release.ps1` 打包各格式样例（体积允许时） | [ ] |

---

## P1 — 发版底座（保留，不抢主线）

| ID | 任务 | 说明 | 状态 |
|----|------|------|------|
| W4-1 | Release 打包流程 | CONTRIBUTING + `package_release.ps1 -Configuration Release` | [x] |
| W4-2 | 版本 `0.2.0-beta` | `VERSION.txt` + CHANGELOG | [x] |
| W4-3 | CI Release job | `windows-ci-release.yml` | [x] |
| W4-4 | 冒烟 §7 记录 | `SMOKE_CHECKLIST.md` 总结果行 | [~] |

---

## 延后 — 原 Compare 主线（第五周）

> 详见 [PHASE_WEEK5_BACKLOG.md](PHASE_WEEK5_BACKLOG.md)（Compare、W4-10 对齐加载、差异摘要等）。

| 原 ID | 摘要 | 状态 |
|-------|------|------|
| W4-10～19 | Compare 对齐加载 / 差异 / 冒烟 §4 | [→] |
| W4-9 | FST 并行块读 | [→] |
| W4-B1 | Compare 打开第二路（CallAfter 对话框） | [x] 已修复，待 Compare 周回归 |

---

## 建议节奏（5 天）

```text
Day 1   W4-F1～F3              矩阵 + run_trace_tests + 修 GHW 期望
Day 2   W4-F4～F5              CI + 扩展名/过滤器统一（fzt/fst）
Day 3   W4-F6～F8              格式冒烟清单 + VCD/FST/VZT/LXT2 GUI
Day 4   W4-F9～F10             GHW + 打开失败 UX
Day 5   W4-F11～F16            VZT/GHW 加固 + 文档 + 发布样例
```

---

## 命令速查

```powershell
# 格式 CLI
powershell -File tools\run_trace_tests.ps1
powershell -File tools\run_smoke.ps1 -TraceToolsOnly -SkipBuild

# 查看宏（属性表）
# TEST1/Bear2WaveTraceFormats.props — BEAR2WAVE_WITH_VZT / LXT2 / GHW

# GTKWave 读库
powershell -File tools\fetch_gtkwave_libs.ps1

# GUI 手工
# tests/FORMAT_SMOKE_CHECKLIST.md
```

---

## 第四周结束定义（Definition of Done）

- [ ] `docs/FORMAT_SUPPORT_MATRIX.md` 存在且与实测一致  
- [ ] `run_trace_tests.ps1` 对仓库样例 **0 FAIL**（或矩阵中注明 SKIP 原因）  
- [ ] `tests/FORMAT_SMOKE_CHECKLIST.md` 中 **已启用格式** 核心项 PASS  
- [ ] 文件对话框 / `trace_loader` / `ProjectStartWindow` 扩展名一致  
- [ ] README「支持的格式」与矩阵一致  

---

## 附录：W4-B1 Compare 打开第二路（已修复，非本周主线）

**状态**：`[x]`（2026-05-28）— 根因：`CallAfter` + `wxFileSelector` 在 Windows 上模态框不显示；改直接 `wxFileDialog::ShowModal`。  
**回归**：放在第五周 Compare 专项（§4.2）。

---

## 第五周预告

| 方向 | 候选 |
|------|------|
| Compare | 对齐加载、§4 冒烟、差异摘要 |
| 性能 | FST 并行块读、大文件 bench |
| 发布 | `0.2.0` 候选、截图 |
