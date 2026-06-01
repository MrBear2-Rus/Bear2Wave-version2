# Bear2Wave 冒烟测试清单（手工）

**目的**：每次发版或大块改动后，约 **15 分钟**验证核心路径。  
**可执行文件**：`out/x64/Debug/Bear2Wave.exe` 或 **`out/x64/Release/Bear2Wave.exe`**（Beta `0.2.1-beta` 发版用 Release）

---

## 0. 环境

- [ ] Windows x64，已按 `README.md` 配置 `WXWIN`、`VCPKG_ROOT`
- [ ] 能启动：`Bear2Wave.exe` 无立即崩溃

---

## 1. 打开波形

| # | 操作 | 样例 | 预期 |
|---|------|------|------|
| 1.1 | 启动后选项目 / File → Open | `tests/traces/bear2wave_sample.fst` | 左侧模块树有层次，状态栏显示已加载 |
| 1.2 | 打开 VCD | `tests/traces/test2.vcd` | 同上 |
| 1.3 | 大文件警告（可选） | 超大 `.vcd` | 出现「Large VCD」提示或懒加载，不长时间假死 |

---

## 2. 显示信号（含 1-bit）

| # | 操作 | 预期 |
|---|------|------|
| 2.1 | 展开模块树，双击 **单 bit** 信号（如 clk） | 右侧出现 **方波阶梯线**，非空白 |
| 2.2 | 再添加 **多 bit 总线** 信号 | 总线以方框/十六进制标注显示 |
| 2.3 | 拖时间轴 / 滚轮缩放 | 波形随动，无卡死；游标数值随时间变化 |
| 2.4 | Zoom Full / Zoom In / Out | 视口合理 |
| 2.5 | 连续添加 **15 路以上** 信号 | 左侧信号名列 **滚轮** 或 **↑/↓** 可滚到最底行；新加信号自动滚入视口；右侧出现细滚动条 |

**滚动操作提示**：信号名列上滚轮、**Ctrl+滚轮**（任意位置）、↑↓、Shift+PgUp/PgDn；菜单可开启「整区滚轮滚信号」。

---

## 3. 会话

| # | 操作 | 预期 |
|---|------|------|
| 3.1 | File → Write Session As → `tests/output/manual.bwv` | 保存成功 |
| 3.2 | 关闭显示列表或重开文件后 File → Read Session | 显示列表、视口、radix 大致恢复 |
| 3.3 | 打开 GTKWave 样例（若有） | `tests/sessions/test2.gtkw` 能读或给出明确错误 |

---

## 3b. 菜单 / 帮助（W3-8）

| # | 操作 | 预期 |
|---|------|------|
| 3b.1 | Help → Contents（或 **Ctrl+F1**） | 帮助对话框打开 |
| 3b.2 | Markers → Add Marker | 不崩溃；与 Edit 菜单无 ID 冲突 |
| 3b.3 | View → Export diagnostics bundle | 生成 zip/文件夹，含版本与环境摘要 |
| 3b.4 | File → Close（**Ctrl+W**） | 当前窗口/标签关闭行为符合预期 |

---

## 4. Compare（第五周 — 非当前 Week 4 主线）

> Week 4 主攻格式支持 → [FORMAT_SMOKE_CHECKLIST.md](FORMAT_SMOKE_CHECKLIST.md)。Compare 见 [docs/PHASE_WEEK5_BACKLOG.md](../docs/PHASE_WEEK5_BACKLOG.md)。

| # | 操作 | 预期 |
|---|------|------|
| 4.1 | 主窗打开 `bear2wave_sample.fst`，加 2 路信号 | 主窗波形正常 |
| 4.2 | **Compare → Open Second Trace**（`Ctrl+Shift+C` 或菜单），打开 `test2.vcd` 或第二份 FST | 第二窗打开；可选 **Tile Horizontally** |
| 4.3 | 主窗拖播放头（Link Playheads 开） | 次窗红线同步 |
| 4.4 | 主窗缩放时间轴（Link Time View 开） | 次窗 From/To 同步 |
| 4.5 | 关闭 Link Time View，各窗独立缩放 | 互不干扰 |
| 4.6 | 两窗各加信号；本地分析 / AI「Compare 差异」 | 有摘要或明确「未打开次轨」提示 |
| 4.7 | 大文件主窗平移时间（W4-10 后） | 次窗同窗数据补齐、无长时间假死 |

---

## 5. AI / 本地分析（可选）

| # | 操作 | 预期 |
|---|------|------|
| 5.1 | 显示 2+ 路信号后打开 AI 面板 | 信号列表与波形区一致 |
| 5.2 | 「本地报告」/ 统计刷新 | 有边沿/占空等文字，非全空 |

---

## 6. CLI（自动化补充）

```powershell
# 推荐：构建 + TraceTools 样例自检（对应本清单 1.x / 2.x 的数据层）
powershell -ExecutionPolicy Bypass -File tools\run_smoke.ps1

# Release 构建后同上
powershell -ExecutionPolicy Bypass -File tools\run_smoke.ps1 -Configuration Release

# 含 large_test.vcd / large_test.fst（较慢，仅 loader 测试）
powershell -ExecutionPolicy Bypass -File tools\run_smoke.ps1 -IncludeLarge

# 全格式生成 + 测试
powershell -ExecutionPolicy Bypass -File tools\run_trace_tests.ps1
```

---

## 7. 记录（在这里填 PASS / FAIL）

测完后在**下面表格同一行**填写（可提交到 Git，也可只留本地）：

| 日期 | 构建 | 测试人 | 结果 | 备注 |
|------|------|--------|------|------|
| 2026-05-19 | Release `0.2.1-beta` | 维护者 | **PASS** | §1～3b PASS；DirectWrite GL 文字回归 PASS |
| 2026-06-01 | Release `0.2.1-beta` | 维护者 | **PASS** | 全流程 §A～G PASS（见 [FULL_FLOW_TEST.md](FULL_FLOW_TEST.md)） |

- **结果列**：整轮冒烟写 `PASS` / `FAIL`；也可写 `§4 FAIL 4.3` 指明章节。  
- 各节 §1～§4 的表格**不必**逐格写 PASS，按「预期」列对照即可。  
- 快捷键无效时先用菜单：**Compare → Open Second Trace for Compare…**（`Ctrl+Shift+C`，避免中文输入法占用 `Ctrl+Shift+O`）。

**Release 路径**：`out\x64\Release\Bear2Wave.exe` · 需先重新编译后再测 Compare 快捷键。

完整 Beta 验收记录见 [docs/RELEASE_SIGNOFF.md](../docs/RELEASE_SIGNOFF.md)。

---

*维护：阶段 A 完成后在 `docs/PHASE_A_TODO.md` 将 A1 标为 `[x]`。*
