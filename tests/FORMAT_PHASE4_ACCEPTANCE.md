# 阶段 4（E4）验收 — 外部转换器接入

> 路线图：[FORMAT_EXTENSION_ROADMAP.md](../docs/FORMAT_EXTENSION_ROADMAP.md)  
> 用户文档：[EXTERNAL_CONVERTERS.md](../docs/EXTERNAL_CONVERTERS.md)

**阶段 4 状态：已完成（E4-1～E4-6）**

## E4-1 `.vpd`（vpd2vcd）

| 检查项 | 命令 / 操作 | 期望 |
|--------|-------------|------|
| trace_loader 识别 | `TraceTools.exe caps` | 输出 `vpd wlf fsdb : external converter` |
| Mock 转换 | `TraceTools.exe test-e4` | PASS（`mock_vpd2vcd.cmd`） |
| 真实工具（可选） | 配置 vpd2vcd 后 GUI 打开 `.vpd` | 转为 VCD 后正常显示 |

## E4-2 `.wlf`（wlf2vcd）

| 检查项 | 期望 |
|--------|------|
| 扩展名过滤 | 打开对话框含 `*.wlf` |
| 配置项 | External Tool Paths 含 wlf2vcd |

## E4-3 `.fsdb`（fsdb2vcd）

| 检查项 | 期望 |
|--------|------|
| 扩展名过滤 | 打开对话框含 `*.fsdb` |
| 无工具时 | 明确错误 + 文档指向 `EXTERNAL_CONVERTERS.md` |

## E4-4 转换结果缓存

| 检查项 | 命令 | 期望 |
|--------|------|------|
| 首次转换 | `test-e4` | 生成 `{cache_dir}/*.vcd` + `.meta` |
| 二次打开 | `test-e4` | stderr 含 `using cached conversion` |
| 禁用缓存 | `BEAR2WAVE_EXT_CACHE=0` | 每次重新调用 converter |

## E4-5 设置页「外部工具路径」

| 检查项 | 操作 | 期望 |
|--------|------|------|
| 菜单 | Edit → External Tool Paths… | 对话框可编辑三路工具 + 缓存目录 |
| 持久化 | 保存后重启 Bear2Wave | `%LOCALAPPDATA%\Bear2Wave\external_tools.cfg` 生效 |

## 一键 CLI

```powershell
cd "E:\EDA_Race\TEST1 - 1\TEST1"
powershell -ExecutionPolicy Bypass -File tools\run_trace_tests.ps1 -Configuration Release
```

应包含 **test-e4 PASS** 与 **test-e4-convert PASS**（与 E2/E3 并列）。

单独运行：

```powershell
.\tools\x64\Release\TraceTools.exe test-e4 .\tests\traces
.\tools\x64\Release\TraceTools.exe test-e4-convert .\tests\traces
.\tools\x64\Release\TraceTools.exe convert .\tests\traces\bear2wave_sample.vcd .\tests\traces\out.fst
```

## E4-6 格式转换（File → Convert Trace / GTKWave 写库）

| 检查项 | 操作 | 期望 |
|--------|------|------|
| 菜单 | File → Convert Trace… | 选源 trace + 目标 FST/VCD/LXT/LXT2/VZT |
| VCD→FST | CLI `convert sample.vcd out.fst` | 成功，可再打开 out.fst |
| VPD 源 | mock vpd2vcd 配置后 convert mock.vpd out.fst | 先 E4 转 VCD 再写 FST |
| 不可写目标 | convert x.vcd out.vpd | 明确错误（VPD/WLF/FSDB 仅 import） |

## GUI 冒烟（L3，可选）

| 步骤 | 期望 |
|------|------|
| Edit → External Tool Paths → 指向 `tools\mock_vpd2vcd.cmd` 作为 vpd2vcd | 保存成功 |
| 打开任意 `.vpd`（可为空占位文件） | 加载样例 VCD 波形 |
| 再次打开同一 `.vpd` | 更快（缓存命中） |
