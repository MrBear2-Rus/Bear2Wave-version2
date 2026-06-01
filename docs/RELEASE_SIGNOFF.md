# Bear2Wave Beta 发布签字记录

> **版本**：见 [VERSION.txt](../VERSION.txt)  
> **目标平台**：Windows x64  
> **发布档位**：内测 / 公开 **Beta**（非 1.0 生产保证）

---

## 1. 静态发布检查

运行（仓库根目录）：

```powershell
powershell -ExecutionPolicy Bypass -File tools\release_check.ps1
```

预期：除 wx `mswu` 可选 WARN 外，全部 `[OK]`。

打包：

```powershell
powershell -ExecutionPolicy Bypass -File tools\package_release.ps1 -Configuration Release
```

产物：`dist/Bear2Wave-<version>-win64.zip`

---

## 2. 手工冒烟（GUI）

清单：[tests/SMOKE_CHECKLIST.md](../tests/SMOKE_CHECKLIST.md)

| 日期 | 构建 | 结果 | 覆盖范围 |
|------|------|------|----------|
| 2026-05-19 | Release `0.2.1-beta` | **PASS** | §1 打开波形 · §2 显示/缩放/滚动 · §3 会话 · §3b 帮助/诊断 · DirectWrite GL 文字回归 |
| 2026-06-01 | Release `0.2.1-beta` | **PASS** | 全流程 [FULL_FLOW_TEST.md](../tests/FULL_FLOW_TEST.md) §A～G；large FST CLI+GUI；CLI 自动化 `-IncludeLarge` PASS |

自动化补充：

```powershell
powershell -ExecutionPolicy Bypass -File tools\run_smoke.ps1 -Configuration Release
```

---

## 3. 0.2.1-beta 相对 0.2.0-beta 的功能文档对齐

| 能力 | 文档 | 代码入口 |
|------|------|----------|
| FST 流式写入 | [TRACE_FORMATS_BUILD.md](TRACE_FORMATS_BUILD.md) · `TraceTools vcd2fst` | `core/vcd_fst_writer.cpp` |
| DirectWrite GL 文字 | 本文 §4 | `panels/DirectWriteTextCanvas.cpp` |
| AI + Transaction Filter | [AI_USAGE.md](AI_USAGE.md) · [TRANSACTION_FILTER_DECODER.md](TRANSACTION_FILTER_DECODER.md) | `waveform_analysis.cpp` · `ai_analysis_service.cpp` |
| Tcl 扩展命令 | [TCL_BUILD.md](TCL_BUILD.md) | `script/TclScriptEngine.cpp` |
| 暗色主题 | [USER_GUIDE.md](USER_GUIDE.md) | `core/ui_theme.cpp` |

---

## 4. DirectWrite 文字回归（GL 模式）

| 检查项 | 预期 |
|--------|------|
| 时间轴刻度、信号名、总线标签 | OpenGL 渲染下可见 |
| 暗色主题切换 | 文字颜色随主题更新 |
| 回退路径 | `BEAR2WAVE_DIRECTWRITE=0` 时 wx 白底抠图仍可用 |

**签字**：2026-05-19 **PASS**（DirectWrite 回归） · 2026-06-01 **PASS**（全流程 §A～G）

---

## 5. Beta 已知限制（发布说明摘要）

完整列表见 [README.md](../README.md#已知限制beta)。

- Windows x64 为主验证平台
- Tcl 需 `BEAR2WAVE_WITH_TCL` 编译；与 GTKWave 脚本**部分兼容**
- Transaction Filter 依赖外部 `transaction_proc`（与 GTKWave 相同模型）
- AI 需 API Key 或本地 Ollama；波形上下文有字符/跳变上限
- 外部转换器 / Filter 进程：用户配置的可执行文件，**信任本地环境**
- Release 若缺少 wx `mswu` 库，可能链 `mswud`（见 [WX_RELEASE_BUILD.md](WX_RELEASE_BUILD.md)）

---

## 6. 下一档发布（公开 Beta）可选增强

非阻塞当前内测签字，建议后续迭代：

- Authenticode 代码签名（E2-7）
- `docs/SECURITY.md` 威胁模型与外部工具白名单
- 更新 `docs/help/*.html` 与截图（`docs/images/`）
- GitHub Releases 附 SHA256 与样例 FST

---

*维护：每次发版更新 §2 表格一行；重大功能变更同步 [CHANGELOG.md](../CHANGELOG.md)。*
