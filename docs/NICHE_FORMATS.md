# 小众 / 延后格式（E5）

Bear2Wave 主路径已覆盖 GTKWave 原生格式（E0–E2）与 Synopsys/Mentor/Verdi 外部转换（E4）。  
**阶段 5** 处理其余厂商或用途不同的格式：能转则转，不能转则给出明确说明。

## 总览

| ID | 扩展名 | 策略 | Bear2Wave 行为 |
|----|--------|------|----------------|
| E5-1 | `.aet` / `.aet2` / `.ae2` | **外部转换** | `aet2vcd` 包装器 → VCD（同 E4 管线） |
| E5-2 | FSDB 原生直读 | **延后** | 继续用 E4 `fsdb2vcd` |
| E5-3 | `.saif` | **拒绝** | 功耗格式，非时域波形 |
| E5-4 | `.json` 波形 | **延后** | 定义 schema；会话用 `.bwv`/`.b2w` |
| E5-5 | `.shm` / `.trn` | **外部转换** | Cadence Xcelium → VCD（同 E4 管线） |

## E5-5 Cadence SHM / TRN

### 推荐工具

| 工具 | 来源 | 说明 |
|------|------|------|
| `simvisdbutil` | Xcelium (`CDS_INST_DIR/tools/bin`) | Cadence 官方数据库导出 |
| `shm2vcd` | 用户脚本 / `tools/` mock | 包装器，接口：`shm2vcd <in> <out.vcd>` |

### 配置

**Edit → External Tool Paths…** → `shm2vcd:`  
或环境变量 `BEAR2WAVE_SHM2VCD`，或 `external_tools.cfg`：

```ini
shm2vcd=C:\path\to\simvisdbutil.exe
```

自动探测顺序：`CDS_INST_DIR`、`XCELIUM_HOME`、`CADENCE_HOME`、`INCA_HOME` → `tools/` → PATH。

### 开发 / CI

```powershell
.\tools\x64\Release\TraceTools.exe test-e5 .\tests\traces
```

使用 `tools\mock_shm2vcd.cmd` 模拟转换（与 `mock_vpd2vcd.cmd` 相同思路）。

## E5-3 SAIF

SAIF 记录开关活动用于功耗分析，**不是**逐周期波形。Bear2Wave 打开 `.saif` 会返回明确错误，并指向本文档。

## E5-1 AET / AET2 / AE2

IBM **All Events Trace**（AET2）为 IBM EDA 工具链专用二进制格式。Bear2Wave **不内置** `libae2rw` 直读（与新版 GTKWave 一致），而是通过 **外部转换器** 打开：

| 扩展名 | 说明 |
|--------|------|
| `.aet2` | AET Version 2（常用） |
| `.aet` | 旧版 AET |
| `.ae2` | 部分工具链别名 |

### 推荐工具

| 工具 | 来源 | 说明 |
|------|------|------|
| `aet2vcd` | 用户脚本 / `tools/` mock | 包装器，接口：`aet2vcd <in.aet2> <out.vcd>` |
| `ae2vcd` / `ae2export` | IBM SIMARAMA 安装目录 | 若站点提供，可指向 Bear2Wave 的 `aet2vcd:` 配置项 |
| IBM 工具内导出 | ChipBench / 仿真流程 | 直接导出 **VCD** 或 **FST** 后打开（无需转换器） |

GTKWave 用户：旧版通过 `SIMARAMA_BASE` 指向含 `libae2rw` 的目录以启用直读；Bear2Wave 采用与 VPD/SHM 相同的外部转换路径，便于在无 IBM 库的机器上使用。

### 配置

**Edit → External Tool Paths…** → `aet2vcd:`  
或环境变量 `BEAR2WAVE_AET2VCD`，或 `external_tools.cfg`：

```ini
aet2vcd=C:\path\to\aet2vcd.exe
```

自动探测顺序：`aet2vcd` 配置 → `$SIMARAMA_BASE/tools/bin`（及 `ae2vcd` / `ae2export`）→ `tools/mock_aet2vcd.cmd`（开发）→ PATH。

### 开发 / CI

```powershell
.\tools\x64\Release\TraceTools.exe test-e5 .\tests\traces
```

使用 `tools\mock_aet2vcd.cmd` 模拟转换（与 `mock_shm2vcd.cmd` 相同思路）。

### 未来：内置直读（可选）

若需链接 IBM `libae2rw` 原生读取，可另开 `BEAR2WAVE_WITH_AET2` 编译项（工作量大、授权绑定）。当前阶段以外部转换为主。

## E5-2 FSDB 原生

当前通过 **fsdb2vcd**（E4）导入。若未来需要 Verdi FSDB API 直读，再单独立项（工作量大、授权绑定）。

## E5-4 JSON 波形

- **会话文件**：使用 `.bwv` / `.b2w` / `.gtkw`（File → Read/Write Session）
- **未来 JSON trace**：见 [B2W_TRACE_JSON_SCHEMA.md](B2W_TRACE_JSON_SCHEMA.md)（尚未实现加载器）

启动窗口已移除误导性的「打开任意 .json」过滤器，改为 `.bwv`/`.b2w` 会话扩展名。

## 相关文档

- [EXTERNAL_CONVERTERS.md](EXTERNAL_CONVERTERS.md) — VPD/WLF/FSDB/SHM 外部工具
- [FORMAT_EXTENSION_ROADMAP.md](FORMAT_EXTENSION_ROADMAP.md) — 全路线图
- [../tests/FORMAT_PHASE5_ACCEPTANCE.md](../tests/FORMAT_PHASE5_ACCEPTANCE.md) — E5 验收清单
