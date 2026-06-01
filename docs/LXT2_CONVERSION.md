# LXT2 转换与样例生成

> 阶段 2（E2-4）用户文档。构建细节见 [TRACE_FORMATS_BUILD.md](TRACE_FORMATS_BUILD.md)。

LXT2 是 GTKWave 的块压缩波形格式，比 VCD 更小、读取更快。Bear2Wave 对 `.lxt2` 使用**懒加载**（打开时只读层次，加入信号后再按时间窗加载跳变）。

---

## 1. 用 Bear2Wave 生成样例（Windows）

```powershell
# 编译 TraceTools 后
tools\x64\Release\TraceTools.exe gen-vcd tests\traces\sample.vcd
tools\x64\Release\TraceTools.exe gen-lxt2 tests\traces\sample.lxt2
tools\x64\Release\TraceTools.exe test tests\traces\sample.lxt2
```

`gen-all` 会一并生成 `bear2wave_sample.lxt2`。

---

## 2. 从 VCD 转换（GTKWave / Icarus 工具链）

### Linux / WSL / MSYS2

GTKWave 自带 `vcd2lxt2`（或部分发行版名为 `vcd2lxt` 的 LXT2 模式）：

```bash
vcd2lxt2 sim.vcd sim.lxt2
# 或
vcd2lxt -lxt2 sim.vcd sim.lxt2
```

常用选项（视 gtkwave 版本而定）：

| 选项 | 含义 |
|------|------|
| `-lxt2` | 输出 LXT2（非 legacy LXT v1） |
| `-dictpack` | 字典压缩（更小文件） |
| `-chgpack` | 变化块压缩 |

### 推荐工作流

1. 仿真写 VCD：`sim.vcd`
2. 转 LXT2：`vcd2lxt2 sim.vcd sim.lxt2`
3. Bear2Wave 打开 `sim.lxt2`，按需加入信号

大仿真更推荐 **FST**（`vcd2fst`），LXT2 适合与 GTKWave 工具链互通。

---

## 3. 在 Bear2Wave 中打开

1. **文件 → 打开**，过滤器含 `*.lxt2`
2. 左侧模块树选信号，**加入波形**
3. 首次加载走 `trace_load_signals()`，支持视口时间窗与取消

环境变量（可选）：

| 变量 | 默认 | 说明 |
|------|------|------|
| `BEAR2WAVE_LOAD_MARGIN` | 见 ENVIRONMENT.md | 视口外预加载边距 |
| `BEAR2WAVE_LXT2_SAFE` | — | 保守 zlib 路径（旧版 DLL 兼容） |

---

## 4. LXT v1（`.lxt`）与 LXT2 区别

| | LXT v1 | LXT2 |
|---|--------|------|
| 扩展名 | `.lxt` | `.lxt2` |
| Bear2Wave | 交织读全量载入 | 懒加载 |
| Windows 样例 | `TraceTools gen-lxt`（交织） | `TraceTools gen-lxt2` |
| 推荐 | 遗留/互通 | 日常查看 |

魔数：LXT2 为 `0x1380`；legacy LXT v1 为 `0x0138`。扩展名写错时仍可按魔数识别。

---

## 5. 验收命令

```powershell
powershell -ExecutionPolicy Bypass -File tools\run_trace_tests.ps1 -Configuration Release
```

期望：`bear2wave_sample.lxt2` PASS，`changes>0`。

CLI 单测：

```powershell
TraceTools.exe test tests\traces\bear2wave_sample.lxt2
```

---

## 6. 故障排除

| 现象 | 处理 |
|------|------|
| 打开即退出 | 确认 exe 同目录有 `zlib1.dll`（及可能的 `bz2.dll`） |
| `Not an LXT2 file` | 文件可能是 LXT v1 或损坏；用 `TraceTools test` 看 ext |
| 无波形 | 确认已**加入信号**；LXT2 不会自动加载全部跳变 |
| 编译无 LXT2 | 启用 `BEAR2WAVE_WITH_LXT2`，见 TRACE_FORMATS_BUILD.md |
