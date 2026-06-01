# Bear2Wave 波形格式支持矩阵

> 由 `run_trace_tests.ps1` + GUI 手工填写。  
> 路线图：[FORMAT_EXTENSION_ROADMAP.md](FORMAT_EXTENSION_ROADMAP.md) · 阶段 0 验收：[../tests/FORMAT_PHASE0_ACCEPTANCE.md](../tests/FORMAT_PHASE0_ACCEPTANCE.md)

## 编译配置

| 项 | 值 |
|----|-----|
| `BEAR2WAVE_WITH_VZT` | 是（`Bear2WaveTraceFormats.props`） |
| `BEAR2WAVE_WITH_LXT2` | 是 |
| `BEAR2WAVE_WITH_GHW` | 是 |
| Bear2Wave 版本 | `VERSION.txt` |

## 矩阵

| 扩展名 | trace_loader | TraceTools `test` | GUI 打开 | GUI 加信号+方波 | 样例路径 | 备注 |
|--------|--------------|-------------------|----------|-----------------|----------|------|
| `.vcd` | PASS | PASS | 待测 | 待测 | `tests/traces/bear2wave_sample.vcd` | |
| `.fst` | PASS | PASS | 待测 | 待测 | `tests/traces/bear2wave_sample.fst` | |
| `.fzt` | PASS | PASS | 待测 | 待测 | FST 容错扩展名 | |
| `.vzt` | PASS | PASS* | 待测 | 待测 | `tests/traces/bear2wave_sample.vzt` | 懒加载 + 并行块预解压（`BEAR2WAVE_VZT_THREADS`） |
| `.lxt2` | PASS | PASS* | 待测 | 待测 | `tests/traces/bear2wave_sample.lxt2` | 需宏 + zlib DLL |
| `.lxt` | PASS | PASS | 待测 | 待测 | `tests/traces/bear2wave_sample.lxt` | 交织 LXT v1；Windows `gen-lxt` |
| `.idx` | — | PASS* | 待测 | 待测 | `<vcd>.idx` / `.bwvcdidx` | VCD fastload 侧车（非独立波形） |
| `.ghw` | PASS | PASS | 待测 | 待测 | `tests/traces/bear2wave_sample.ghw` | lazy load；`basic.ghw` 来自 gtkwave-src |
| `.csv` | PASS | PASS | 待测 | 待测 | `tests/traces/bear2wave_sample.csv` | 经 `trace_loader` |
| `.vpd` `.wlf` `.fsdb` | PASS* | PASS* | 待测 | 待测 | E4 mock / 真实工具 | 外部转换 |
| `.shm` `.trn` | PASS* | PASS* | 待测 | 待测 | E5 mock / simvisdbutil | 外部转换 |
| `.saif` | REJECT | PASS* | 待测 | — | 任意 `.saif` | 明确拒绝（E5-3） |
| `.aet2` / `.aet` / `.ae2` | EXT | PASS* | 待测 | 待测 | mock + IBM aet2vcd | 外部转换（E5-1） |

**图例**：`PASS` / `FAIL` / `SKIP`（未编译宏或无样例）/ `待测`（需 GUI 手工）

\* 需 Release TraceTools + DLL 旁路更稳

## CLI 记录

```text
=== Format capabilities ===
  ghw             : yes (BEAR2WAVE_WITH_GHW)

=== Summary ===
bear2wave_sample.ghw        PASS  (signals=1 max_ts=1 changes=11)
```

## GUI 记录

见 `tests/FORMAT_SMOKE_CHECKLIST.md` §7。
