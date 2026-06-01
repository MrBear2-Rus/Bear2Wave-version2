# 阶段 0 验收指南（E0-1～E0-6）

完成阶段 0 后，按下列步骤验收。全部 PASS 即可进入阶段 1。

## 环境准备

1. 已安装 Visual Studio x64、vcpkg（zlib、bz2）。
2. 已复制 `TEST1/Bear2WaveTraceFormats.props.example` → `Bear2WaveTraceFormats.props`（启用 VZT/LXT2/GHW）。
3. 在仓库根目录（`TEST1 - 1/TEST1`）打开 PowerShell。

---

## 1. CLI 自动化（E0-4）

```powershell
powershell -ExecutionPolicy Bypass -File tools\run_trace_tests.ps1
```

**期望：**

- TraceTools 编译成功。
- `gen-all` 生成 `tests\traces\bear2wave_sample.*`。
- 以下文件 `test` 为 **PASS**（若某文件不存在则 SKIP，不算失败）：
  - `bear2wave_sample.vcd`
  - `bear2wave_sample.fst`
  - `bear2wave_sample.vzt`（需宏）
  - `bear2wave_sample.lxt2`（需宏）
  - `bear2wave_gtkwave_basic.vcd` / `.fst`（若已复制）
- `bear2wave_sample.lxt`：Windows 上可能 SKIP（`gen-lxt` 未生成）。
- `bear2wave_sample.ghw`：无样例时 SKIP；有样例且宏开启时应 PASS。
- 脚本退出码 **0**。

**能力探测：**

```powershell
tools\x64\Debug\TraceTools.exe caps
```

应打印 `BEAR2WAVE_WITH_VZT/LXT2/GHW` 为 `yes` 或 `no`。

---

## 2. Release Bear2Wave + DLL（E0-1）

```powershell
msbuild TEST1\TEST1.vcxproj /p:Configuration=Release /p:Platform=x64 /v:minimal
```

检查 `out\x64\Release\` 下存在：

- `Bear2Wave.exe`
- `zlib1.dll`
- `bz2.dll`

---

## 3. GUI 打开（E0-1、E0-5、E0-6）

用 **Release** `Bear2Wave.exe`：

| 步骤 | 操作 | 期望 |
|------|------|------|
| 3.1 | File → Open → 选 `tests\traces\bear2wave_sample.lxt2` | 打开成功；模块树有 TOP；加 `clk` 有方波 |
| 3.2 | 打开 `bear2wave_sample.fst` | 正常 |
| 3.3 | 打开 `bear2wave_sample.vzt`（宏开） | 正常 |
| 3.4 | 文件对话框过滤器含 `.fzt`、`.csv` | 可见 |
| 3.5 | 故意打开损坏/空文件或改扩展名的垃圾文件 | **对话框**说明失败，不静默闪退 |
| 3.6 | 用未启用宏的构建打开 `.vzt` | 提示启用宏 + 指向 `TRACE_FORMATS_BUILD.md` |

---

## 4. 填写矩阵（E0-4）

编辑 `docs/FORMAT_SUPPORT_MATRIX.md`：

- 编译宏开/关
- 各扩展名 CLI/GUI 填 `PASS` / `SKIP` / `FAIL`
- 粘贴 `run_trace_tests.ps1` 摘要到「CLI 记录」

---

## 5. 快速检查表

| ID | 检查项 | PASS |
|----|--------|------|
| E0-1 | LXT2 GUI 不卡退 | ☐ |
| E0-2 | GHW test PASS 或 documented SKIP | ☐ |
| E0-3 | VZT test PASS | ☐ |
| E0-4 | `run_trace_tests.ps1` 退出 0 | ☐ |
| E0-5 | 过滤器含 fzt/csv | ☐ |
| E0-6 | 打开失败有对话框 | ☐ |

全部勾选 → **阶段 0 完成**，可开始 `docs/FORMAT_EXTENSION_ROADMAP.md` 阶段 1。
