# 阶段 2（E2）验收 — GTKWave 原生格式补齐

> 路线图：[FORMAT_EXTENSION_ROADMAP.md](../docs/FORMAT_EXTENSION_ROADMAP.md)

**阶段 2 状态：已完成（E2-1～E2-4）**

## E2-1 LXT v1 交织读

| 检查项 | 命令 / 操作 | 期望 |
|--------|-------------|------|
| 生成交织样例 | `TraceTools.exe gen-lxt-interlaced tests/traces/bear2wave_sample_interlaced.lxt` | 文件存在，`sync` 段非零 |
| CLI 加载 | `TraceTools.exe test tests/traces/bear2wave_sample.lxt` | PASS，`changes>=4` |
| CLI 加载（交织） | `TraceTools.exe test tests/traces/bear2wave_sample_interlaced.lxt` | PASS |
| GUI | 打开 `bear2wave_sample.lxt` → 加 `TOP.clk` | 有方波 |

**已知限制**

- 交织 + `-chgpack`（压缩 change 块）尚未支持
- 线性 bzip2 LXT（`BZh` 魔数）需 bzlib；Windows 样例使用交织 LXT v1

## E2-2 `.idx` fastload

| 检查项 | 命令 / 操作 | 期望 |
|--------|-------------|------|
| 侧车生成 | `TraceTools.exe test-vcd-idx tests/traces/bear2wave_sample.vcd` | PASS；写出 `.bwvcdidx` 与 `.idx` |
| 二次打开 | 同上命令第二次运行（侧车已存在） | PASS；`changes>0` |
| 环境变量 | `BEAR2WAVE_VCD_IDX_CACHE=0` | 不写侧车（可选） |

侧车格式：`BWVCDIDX1` + 源文件 size/mtime + data section offset + `(timestamp, file_offset)*`。

Bear2Wave 原生名为 `<vcd>.bwvcdidx`；GTKWave 风格别名为 `<vcd>.idx`（相同 payload）。

**说明**：不支持读取 GTKWave 3.x 原生 VList spill `.idx`；Bear2Wave 使用自研时间索引侧车实现等效的 fastload 加速。

## E2-3 VZT 并行读 POC

| 检查项 | 命令 / 操作 | 期望 |
|--------|-------------|------|
| 正确性 | `TraceTools.exe test-vzt-parallel tests/traces/bear2wave_sample.vzt` | PASS；单线程与 4 线程 `changes` 一致 |
| 日志 | 打开时 stderr | `[VZT] parallel block decompress enabled`（threads>1） |
| 环境变量 | `BEAR2WAVE_VZT_THREADS=1` | 强制单线程 |

实现：`vzt_rd_init_smp()` + MSVC 上 `BEAR2WAVE_VZT_PTHREAD_WIN32`（Win32 pthread 桩）。

## E2-4 LXT2 写/转工具文档

| 检查项 | 文档 / 命令 | 期望 |
|--------|-------------|------|
| 用户文档 | [docs/LXT2_CONVERSION.md](../docs/LXT2_CONVERSION.md) | 含 gen-lxt2、vcd2lxt2、故障排除 |
| CLI | `TraceTools.exe test tests/traces/bear2wave_sample.lxt2` | PASS |

## 一键 CLI

```powershell
powershell -ExecutionPolicy Bypass -File tools\run_trace_tests.ps1 -Configuration Release
```

应包含：

- `bear2wave_sample.lxt` PASS
- `bear2wave_sample.vcd.idx`（test-vcd-idx）PASS
- `bear2wave_sample.vzt`（test-vzt-parallel）PASS
