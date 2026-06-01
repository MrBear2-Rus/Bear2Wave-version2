# 外部波形转换器（E4）

Bear2Wave 不内置 VPD/WLF/FSDB 解析，而是通过 **外部 CLI 工具** 转为 VCD，再经现有 `trace_loader` 打开。

## 支持格式

| 扩展名 | 推荐工具 | 说明 |
|--------|----------|------|
| `.vpd` | `vpd2vcd` | Synopsys VCS / Verdi 生态 |
| `.wlf` | `wlf2vcd` | ModelSim / Questa |
| `.fsdb` | `fsdb2vcd` | Verdi / Novas（需授权环境） |
| `.shm` / `.trn` | `simvisdbutil` 或 `shm2vcd` 包装 | Cadence Xcelium（见 [NICHE_FORMATS.md](NICHE_FORMATS.md)） |
| `.aet` / `.aet2` / `.ae2` | `aet2vcd` 包装 | IBM SIMARAMA / AET2（见 [NICHE_FORMATS.md](NICHE_FORMATS.md) E5-1） |

## 配置方式

### 自动探测（推荐）

留空配置项即可。Bear2Wave 按以下顺序搜索转换器：

1. **Edit → External Tool Paths** 或 `external_tools.cfg` 中的显式路径
2. **环境变量**：`BEAR2WAVE_VPD2VCD` / `WLF2VCD` / `FSDB2VCD` / `SHM2VCD` / `AET2VCD`（完整 exe 路径）
3. **EDA 安装目录**：
   - VPD → `$VCS_HOME/bin/vpd2vcd`
   - WLF → `$MODELTECH` / `$MTI_HOME` / `$QUESTA_HOME` 下的 `wlf2vcd`
   - FSDB → `$VERDI_HOME/bin/fsdb2vcd`（或 `$NOVAS_HOME`）
   - SHM/TRN → `$CDS_INST_DIR/tools/bin/simvisdbutil`（或 `XCELIUM_HOME`）
   - AET/AET2 → `$SIMARAMA_BASE/tools/bin/aet2vcd`（或 `ae2vcd` / `ae2export`）
4. **Bear2Wave 旁** `tools/` 目录（开发/mock）
5. **系统 PATH**
6. 额外目录：`BEAR2WAVE_EXT_SEARCH_DIRS`（`;` 分隔）

GUI 对话框打开时会显示 **Auto-detect** 结果；也可点 **Auto-detect all** 一键填入。

CLI 查看探测结果：

```powershell
.\tools\x64\Release\TraceTools.exe caps
```

### 与 GTKWave 内置格式的区别

| 类型 | Bear2Wave 处理方式 |
|------|-------------------|
| FST / LXT / LXT2 / VZT / GHW | **内置读取**（`third_party/gtkwave` 库，无需外部工具） |
| VCD / EVCD / CSV | **内置读取** |
| VPD / WLF / FSDB | **外部 EDA 工具** 转 VCD（GTKWave 本身也不内置这三种） |

GTKWave 自带的 `vcd2fst`、`vcd2lxt`、`vcd2vzt` 等也可用于写出压缩格式；Bear2Wave **内置** `TraceTools vcd2fst`（VCD/EVCD→FST 流式），并已直接读 FST/VZT/LXT，一般不需要再调 GTKWave 的转换器。

### GUI 手动指定

**Edit → External Tool Paths…**（导入 VPD/WLF/FSDB 时）

### File → Convert Trace…（导出 GTKWave 格式）

将任意已支持源（含经 E4 转换后的 VPD/WLF/FSDB）写成：

| 目标 | 实现 |
|------|------|
| `.vcd` / `.evcd` | 内置 VCD writer |
| `.fst` / `.fzt` | libfst `fstWriter` |
| `.lxt` / `.lxt2` | GTKWave `liblxt`（需 `BEAR2WAVE_WITH_LXT2`） |
| `.vzt` | GTKWave `libvzt`（需 `BEAR2WAVE_WITH_VZT`） |

**不能写出** VPD/WLF/FSDB（需各自 EDA 工具；Bear2Wave 仅 import）。

CLI：

```powershell
.\tools\x64\Release\TraceTools.exe convert in.vcd out.fst
.\tools\x64\Release\TraceTools.exe test-e4-convert .\tests\traces
```

### GUI 手动路径（Edit 菜单）

- 填写各转换器可执行文件路径（留空则在 `PATH` 中搜索 `vpd2vcd.exe` 等）
- 启用/禁用转换缓存及缓存目录

### 配置文件

`%LOCALAPPDATA%\Bear2Wave\external_tools.cfg`（UTF-8）：

```ini
vpd2vcd=C:\tools\vpd2vcd.exe
wlf2vcd=
fsdb2vcd=
shm2vcd=
aet2vcd=
cache_enabled=1
cache_dir=C:\Users\you\AppData\Local\Bear2Wave\convert_cache
```

### 环境变量（覆盖配置文件）

| 变量 | 含义 |
|------|------|
| `BEAR2WAVE_VPD2VCD` | vpd2vcd 路径 |
| `BEAR2WAVE_WLF2VCD` | wlf2vcd 路径 |
| `BEAR2WAVE_FSDB2VCD` | fsdb2vcd 路径 |
| `BEAR2WAVE_SHM2VCD` | shm2vcd / simvisdbutil 包装路径 |
| `BEAR2WAVE_AET2VCD` | aet2vcd 包装路径 |
| `BEAR2WAVE_EXT_SEARCH_DIRS` | 额外搜索目录（`;` 分隔） |
| `BEAR2WAVE_EXT_CACHE` | `0` 禁用缓存 |
| `BEAR2WAVE_EXT_CACHE_DIR` | 缓存目录 |

## 转换与缓存（E4-4）

1. 打开 `.vpd` / `.wlf` / `.fsdb`
2. `trace_external_convert_to_vcd()` 调用：`tool input output.vcd`
3. 输出写入缓存：`{cache_dir}/{hash}_{basename}.vcd` + `.meta`（源文件 path/size/mtime）
4. 源文件未变时二次打开直接复用缓存 VCD

## CLI 验证

```powershell
cd "E:\EDA_Race\TEST1 - 1\TEST1"
.\tools\x64\Release\TraceTools.exe test-e4 .\tests\traces
```

使用 `tools\mock_vpd2vcd.cmd` 模拟转换（复制 `bear2wave_sample.vcd`），无需真实 EDA 工具。

## 故障排除

| 现象 | 处理 |
|------|------|
| `external converter not configured` | 设置 Edit → External Tool Paths 或环境变量 |
| 转换 exit≠0 | 在命令行手动运行 `vpd2vcd in out.vcd` 查看 stderr |
| FSDB 无工具 | 需在 Verdi 环境安装 `fsdb2vcd`；或先用 Verdi 导出 FST/VCD |

详见验收：[FORMAT_PHASE4_ACCEPTANCE.md](../tests/FORMAT_PHASE4_ACCEPTANCE.md)
