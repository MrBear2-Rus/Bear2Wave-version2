# 波形格式测试样例

本目录存放各格式的测试文件，供 `trace_tools` 与 Bear2Wave 手动打开验证。

## 生成与运行

在仓库根目录执行：

```powershell
powershell -ExecutionPolicy Bypass -File tools\run_trace_tests.ps1
```

或分步：

```powershell
msbuild tools\TraceTools.vcxproj /p:Configuration=Debug /p:Platform=x64
tools\x64\Debug\TraceTools.exe gen-all tests\traces
tools\x64\Debug\TraceTools.exe test-all tests\traces
```

## 文件说明

| 文件 | 格式 | 来源 |
|------|------|------|
| `bear2wave_sample.vcd` | VCD | `trace_tools gen-vcd` |
| `bear2wave_sample.fst` | FST | `trace_tools gen-fst` |
| `bear2wave_sample.vzt` | VZT | `trace_tools gen-vzt`（需 `BEAR2WAVE_WITH_VZT`） |
| `bear2wave_sample.lxt2` | LXT2 | `trace_tools gen-lxt2`（需 `BEAR2WAVE_WITH_LXT2`） |
| `bear2wave_sample.ghw` | GHW | 自 GTKWave 测试集复制（**加载**需 `BEAR2WAVE_WITH_GHW`，当前为占位） |
| `bear2wave_gtkwave_basic.vcd` | VCD | GTKWave `basic.vcd` |
| `bear2wave_gtkwave_basic.fst` | FST | GTKWave `basic.fst` |
| `bear2wave_test2.vcd` | VCD | 仓库根目录 `test2.vcd` |

## 单文件测试

```powershell
tools\x64\Debug\TraceTools.exe test tests\traces\bear2wave_sample.fst
```

在 Bear2Wave 中：**文件 → 打开**，选择对应扩展名文件，检查信号树与波形。

## 期望结果

- **VCD / FST / VZT / LXT2**：`test` 输出 `PASS`，`signals > 0`
- **GHW**：在未实现 `ghw_loader` 前，`test` 可能 `FAIL`（预期）；可用 `basic.fst` 代替验证 FST 路径
