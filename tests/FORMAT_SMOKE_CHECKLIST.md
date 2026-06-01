# 波形格式支持 — GUI 冒烟清单（Week 4）

**目的**：在 **已启用对应编译宏** 的前提下，验证每种格式能完成「打开 → 模块树 → 加信号 → 方波」。  
**可执行文件**：`out/x64/Release/Bear2Wave.exe`（或 Debug）  
**矩阵**：填完后同步到 [docs/FORMAT_SUPPORT_MATRIX.md](../docs/FORMAT_SUPPORT_MATRIX.md)

---

## 0. 编译前确认

- [ ] 已按 [docs/TRACE_FORMATS_BUILD.md](../docs/TRACE_FORMATS_BUILD.md) 配置 `Bear2WaveTraceFormats.props`
- [ ] 已知当前构建是否定义：`BEAR2WAVE_WITH_VZT` / `LXT2` / `GHW`

---

## 1. 始终启用格式

| # | 操作 | 样例 | 预期 |
|---|------|------|------|
| 1.1 | File → Open / 启动页选文件 | `tests/traces/bear2wave_sample.fst` | 模块树有层次，状态栏已加载 |
| 1.2 | 双击 1-bit 信号 | 同上 | 方波可见 |
| 1.3 | File → Open | `tests/traces/test2.vcd` | 同上 |
| 1.4 | File → Open | `tests/traces/bear2wave_sample.vcd` | 同上 |

---

## 2. 需宏：VZT / LXT2 / GHW

| # | 格式 | 样例 | 打开 | 加信号+方波 | 备注 |
|---|------|------|------|-------------|------|
| 2.1 | VZT | `bear2wave_sample.vzt` | PASS/FAIL | PASS/FAIL | 未宏应明确错误提示 |
| 2.2 | LXT (v1) | `bear2wave_sample.lxt` | PASS/FAIL | PASS/FAIL | 线性 legacy |
| 2.3 | LXT2 | `bear2wave_sample.lxt2` | PASS/FAIL | PASS/FAIL | |
| 2.3 | GHW | `bear2wave_sample.ghw`（若存在） | PASS/FAIL/SKIP | PASS/FAIL/SKIP | 无样例则 SKIP |

---

## 3. 扩展名与过滤器（W4-F5）

| # | 检查 | 预期 |
|---|------|------|
| 3.1 | 启动页 / File 打开过滤器含 `*.fst` 与 `*.fzt`（若保留容错） | 与 `trace_loader` 一致 |
| 3.2 | Compare 打开第二路过滤器含全部已支持扩展名 | 与主 Open 一致 |

---

## 4. 错误路径

| # | 操作 | 预期 |
|---|------|------|
| 4.1 | 打开损坏/空文件 | 对话框或状态栏有明确错误，不崩溃 |
| 4.2 | 未启用宏时打开 `.vzt` | 提示见 TRACE_FORMATS_BUILD，非无反应 |

---

## 5. CLI 对照（可选）

```powershell
powershell -File tools\run_trace_tests.ps1
powershell -File tools\run_smoke.ps1 -TraceToolsOnly -SkipBuild
```

---

## 7. 记录

| 日期 | 构建 | 宏 VZT/LXT2/GHW | 结果 | 备注 |
|------|------|-----------------|------|------|
| | Release 0.2.0-beta | / / | PASS/FAIL | §1～2 摘要 |
