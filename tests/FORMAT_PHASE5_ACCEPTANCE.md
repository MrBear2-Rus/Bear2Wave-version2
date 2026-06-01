# 阶段 5（E5）验收 — 小众 / 延后格式

> 路线图：[FORMAT_EXTENSION_ROADMAP.md](../docs/FORMAT_EXTENSION_ROADMAP.md)  
> 用户文档：[NICHE_FORMATS.md](../docs/NICHE_FORMATS.md)

**阶段 5 状态：进行中（E5-1～E5-5 策略已落地；E5-2/E5-4 加载器仍延后）**

## 验收思路（三层）

| 层级 | 目的 | 命令 / 操作 |
|------|------|-------------|
| **L1 CLI 自动** | 分类、错误文案、mock 管线 | `run_trace_tests.ps1` → `test-e5 PASS` |
| **L2 caps / 配置** | 能力声明与工具探测 | `TraceTools.exe caps`；External Tool Paths |
| **L3 GUI 手工** | 真实用户路径 | 打开 `.saif` / `.shm` / 启动窗过滤器 |

推荐顺序：**先跑 L1 全绿 → L2 对照文档 → L3 有环境再测**。

---

## E5-5 `.shm` / `.trn`（Cadence 外部转换）

| 检查项 | 命令 / 操作 | 期望 |
|--------|-------------|------|
| 扩展名 | `caps` | 含 `shm trn : external converter` |
| Mock 管线 | `TraceTools.exe test-e5` | PASS（`mock_shm2vcd.cmd`） |
| 过滤器 | File → Open Trace | 含 `*.shm;*.trn` |
| 设置页 | Edit → External Tool Paths | 有 `shm2vcd` 行 + Auto-detect |
| 真实工具（可选） | 配置 `simvisdbutil` 后打开 `.shm` | 转 VCD 后正常显示 |

## E5-3 `.saif`（拒绝）

| 检查项 | 操作 | 期望 |
|--------|------|------|
| CLI | `TraceTools.exe test path.saif` | FAIL，错误含 `SAIF` / power |
| GUI | 打开 `.saif` | 对话框说明非波形格式 |

## E5-1 `.aet2` / `.aet` / `.ae2`（外部转换）

| 检查项 | 操作 | 期望 |
|--------|------|------|
| CLI | `TraceTools.exe test-e5` | PASS；含 aet2 mock 转换 |
| CLI | `TraceTools.exe caps` | `aet aet2 ae2 : external converter` |
| CLI | 无 `aet2vcd` 时打开 `.aet2` | 明确「external converter not configured」 |
| GUI | External Tool Paths → `aet2vcd` | 可配置 + Auto-detect |
| GUI | 配置 mock 后打开 `.aet2` | 加载转换后的 VCD |
| 文档 | `docs/NICHE_FORMATS.md` E5-1 | SIMARAMA / aet2vcd 指引 |

## E5-2 FSDB 原生直读（延后）

| 检查项 | 操作 | 期望 |
|--------|------|------|
| caps | `TraceTools.exe caps` | `fsdb native : deferred` |
| 打开 FSDB | E4 `fsdb2vcd` 仍可用 | 与 E4 验收一致，不重复造轮子 |

## E5-4 JSON 波形（schema 预留）

| 检查项 | 操作 | 期望 |
|--------|------|------|
| 启动窗过滤器 | ProjectStart | **无** 误导性 `*.json`；有 `.bwv`/`.b2w` |
| Schema 文档 | `docs/B2W_TRACE_JSON_SCHEMA.md` | 存在且描述 `bear2wave_trace` |
| 误开 `.json` | 从命令行/旧快捷方式打开 `.json` | 提示用 VCD/FST 或会话文件 |

---

## 一键 CLI

```powershell
cd "E:\EDA_Race\TEST1 - 1\TEST1"
powershell -ExecutionPolicy Bypass -File tools\run_trace_tests.ps1 -Configuration Release
```

应包含 **test-e5 PASS**（与 test-e4 并列）。

单独运行：

```powershell
.\tools\x64\Release\TraceTools.exe test-e5 .\tests\traces
.\tools\x64\Release\TraceTools.exe caps
```

## GUI 冒烟（L3，可选）

| 步骤 | 期望 |
|------|------|
| Edit → External Tool Paths → `shm2vcd` 指向 `tools\mock_shm2vcd.cmd` | 保存成功 |
| 同上 → `aet2vcd` 指向 `tools\mock_aet2vcd.cmd` | 保存成功 |
| 打开 `tests\traces\bear2wave_mock.shm`（test-e5 生成） | 加载样例波形 |
| 打开 `tests\traces\bear2wave_mock.aet2`（test-e5 生成） | 加载样例波形 |
| 尝试打开 `.saif` | 友好拒绝，不崩溃 |
| File → Convert Trace 后 | 主界面不退出，弹「已成功转换」（E4-6） |

## 通过标准（建议）

- **必过**：`test-e5 PASS` + `caps` 含 E5 能力行 + SAIF 拒绝文案正确
- **可选**：真实 Cadence `simvisdbutil` / IBM AET 导出链（依赖实验室环境）
- **明确不做**：SAIF 波形化、FSDB 原生库、JSON 加载器（留待后续里程碑）
