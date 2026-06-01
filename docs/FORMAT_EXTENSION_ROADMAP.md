# Bear2Wave 波形格式扩展路线图

> 在现有格式做稳（**阶段 0**）之后，按阶段逐步扩展。  
> 矩阵验收：`docs/FORMAT_SUPPORT_MATRIX.md` · 构建：`docs/TRACE_FORMATS_BUILD.md`  
> 阶段 0 验收：`tests/FORMAT_PHASE0_ACCEPTANCE.md`

## 图例

| 符号 | 含义 |
|------|------|
| 状态 | `[x]` 完成 · `[~]` 部分 · `[ ]` 未做 |
| 工作量 | S（1–2 天）/ M（3–5 天）/ L（1–2 周+） |

---

## 阶段 0 — 现有格式做稳（当前）

| ID | 项 | 状态 | 工作量 | 验收 |
|----|----|------|--------|------|
| E0-1 | LXT2 稳定性（zlib DLL、`BEAR2WAVE_LXT2_SAFE`） | [x] | S | `test bear2wave_sample.lxt2` + GUI 打开不闪退 |
| E0-2 | GHW loader 收尾 | [x] | M | `test bear2wave_sample.ghw` PASS（有样例时） |
| E0-3 | VZT 块读加固（重试日志） | [x] | S | VZT 样例 PASS；坏块有 stderr 提示 |
| E0-4 | 格式矩阵 + `run_trace_tests` + CI | [x] | S | 脚本绿；`TraceTools caps` 输出宏状态 |
| E0-5 | UI/过滤器扩展名一致（含 `fzt`、`csv`） | [x] | S | 各对话框过滤器一致 |
| E0-6 | 打开失败 UX（明确错误 + 文档指引） | [x] | S | 坏文件/未启用宏有对话框 |

---

## 阶段 1 — 低成本扩展

| ID | 格式/能力 | 状态 | 工作量 | 说明 |
|----|-----------|------|--------|------|
| E1-1 | `.evcd` | [x] | S | `trace_loader` 与 `.vcd` 同路径 |
| E1-2 | CSV 统一 `trace_loader` | [x] | S | `csv_loader.cpp`；`test bear2wave_sample.csv` |
| E1-3 | LXT v1 线性 + Windows 样例 | [x] | M | `gen-lxt` 交织样例；`test bear2wave_sample.lxt` PASS |
| E1-4 | 实型/字符串变量显示 | [x] | M | `core/trace_display.cpp` 统一 `ClassifyTraceKind` |
| E1-5 | `.fzt` / `.evcd` 文档与过滤器 | [x] | S | `trace_file_filters.h`、README/help |

验收：[FORMAT_PHASE1_ACCEPTANCE.md](../tests/FORMAT_PHASE1_ACCEPTANCE.md)

---

## 阶段 2 — GTKWave 原生格式补齐

| ID | 格式 | GTKWave | 状态 | 工作量 | 说明 |
|----|------|---------|------|--------|------|
| E2-1 | LXT v1 交织读 | ✅ | [x] | L | sync 表 + heap；`bear2wave_sample.lxt` PASS |
| E2-2 | `.idx` fastload | ✅ | [x] | M | VCD 侧车 `.bwvcdidx` + `.idx`；`test-vcd-idx` |
| E2-3 | VZT 并行读 POC | ✅ | [x] | M | `vzt_rd_init_smp` + `test-vzt-parallel` |
| E2-4 | LXT2 写/转工具文档 | ✅ | [x] | S | [LXT2_CONVERSION.md](LXT2_CONVERSION.md) |

---

## 阶段 3 — 显示语义（非新文件）

| ID | 能力 | 状态 | 工作量 |
|----|------|------|--------|
| E3-1 | Transaction 行（P5-1） | [x] | M |
| E3-2 | GHW 9 态字符映射 | [x] | S |
| E3-3 | Alias / 总线展开 | [x] | M |
| E3-4 | Dumpoff / blackout 段 | [x] | S |

验收：[FORMAT_PHASE3_ACCEPTANCE.md](../tests/FORMAT_PHASE3_ACCEPTANCE.md)

---

## 阶段 4 — 外部转换器接入

| ID | 格式 | 状态 | 工作量 | 外部工具 |
|----|------|------|--------|----------|
| E4-1 | `.vpd` | [x] | M | `vpd2vcd` |
| E4-2 | `.wlf` | [x] | M | `wlf2vcd` |
| E4-3 | `.fsdb` | [x] | L | `fsdb2vcd` / Verdi 库 |
| E4-4 | 转换结果缓存 | [x] | S | — |
| E4-5 | 设置页「外部工具路径」 | [x] | S | — |
| E4-6 | File → Convert Trace（GTKWave 写库） | [x] | M | libfst / liblxt / libvzt |

验收：[FORMAT_PHASE4_ACCEPTANCE.md](../tests/FORMAT_PHASE4_ACCEPTANCE.md) · 文档：[EXTERNAL_CONVERTERS.md](EXTERNAL_CONVERTERS.md)

---

## 阶段 5 — 小众 / 延后

| ID | 格式 | 状态 | 工作量 | 说明 |
|----|------|------|--------|------|
| E5-1 | `.aet2` | [x] | L | 外部 `aet2vcd` + SIMARAMA 探测 |
| E5-2 | FSDB 原生直读 | [ ] | L | 继续 E4 fsdb2vcd |
| E5-3 | `.saif` | [x] | S | 拒绝打开 + 说明 |
| E5-4 | `.json` 波形 | [~] | M | schema 文档；加载器未做 |
| E5-5 | Cadence `.shm`/`.trn` | [x] | M | 外部转换（simvisdbutil / shm2vcd） |

验收：[FORMAT_PHASE5_ACCEPTANCE.md](../tests/FORMAT_PHASE5_ACCEPTANCE.md) · 文档：[NICHE_FORMATS.md](NICHE_FORMATS.md)

---

## 推荐实施顺序

1. **E0**（本阶段）→ 2. **E1-1/E1-2** → 3. **E1-4** → 4. **E2-1** → 5. **E3-1** → 6. **E4-x**

## 里程碑

| 里程碑 | 范围 |
|--------|------|
| M-α | 阶段 0 完成：矩阵 + CLI 绿 + GUI 冒烟 |
| M-β | 阶段 1 + 交织 LXT + Transaction |
| M-γ | IDX + VPD/WLF 转换管线 |
| M-δ | E4 外部转换 + Convert Trace；E5 SHM/SAIF 策略 |

## 每项完成时更新

- `trace_loader.cpp` / `*_loader.cpp`
- `tools/run_trace_tests.ps1`、`tools/trace_tools.cpp`
- `docs/FORMAT_SUPPORT_MATRIX.md`
- `README.md`、`docs/help/formats.html`
