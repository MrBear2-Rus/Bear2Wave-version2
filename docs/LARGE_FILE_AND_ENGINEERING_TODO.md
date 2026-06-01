# Bear2Wave 大文件性能 & 工程化 — TODO 清单

> **说明**：本清单覆盖「GB 级波形可调试」与「可维护、可发布、可 CI」两条线。  
> 状态：`[ ]` 未做 · `[~]` 部分 · `[x]` 完成  
> 与 [AI_ANALYSIS_TODO.md](AI_ANALYSIS_TODO.md)、[LOCAL_ANALYSIS_TODO.md](LOCAL_ANALYSIS_TODO.md) 并行，互不阻塞。

**已实现摘要**见 [LARGE_TRACE_PERFORMANCE.md](LARGE_TRACE_PERFORMANCE.md)。

---

## 一、大文件性能 — 已完成基线（P0–P3，勿重复造轮）

| ID | 任务 | 说明 | 状态 |
|----|------|------|------|
| P0-1 | FST 懒加载 | `fst_open_lazy` + 层次先开 | [x] |
| P0-2 | 统一 `trace_load_signals` | FST/VZT/LXT2/GHW 同一 API | [x] |
| P0-3 | 视口时间窗 + margin | `BEAR2WAVE_LOAD_MARGIN` | [x] |
| P0-4 | 后台加载 + 可取消 | `trace_loader` + UI 不阻塞 | [x] |
| P1-1 | 增量时间窗扩展 | 平移/缩放只补缺口，不清 `value_changes` | [x] |
| P1-2 | 加载后 `shrink_to_fit` | 收紧每路缓冲区 | [x] |
| P1-3 | 绘制缓存防抖 | `BEAR2WAVE_CACHE_DEBOUNCE_MS` | [x] |
| P1-4 | 绘制缓存平移复用 | 视口平移时偏移段坐标 | [x] |
| P2-1 | 多线程绘制缓存构建 | `BEAR2WAVE_CACHE_THREADS` | [x] |
| P2-2 | 模块树按路径建树 | 避免重复节点 | [x] |
| P2-3 | 信号列表行数上限 | `BEAR2WAVE_MAX_SIGNAL_LIST` | [x] |
| P2-4 | 大 VCD 打开警告 | `BEAR2WAVE_VCD_WARN_MB` + 提示 `vcd2fst` | [x] |
| P3-1 | VZT 块内存上限 | 64MB 级保护 | [x] |
| P3-2 | GHW 会话保持 | `ghw_handler` 复用按窗扫描 | [x] |
| P3-3 | AI/分析前按需加载 | `WaveformAnalysis::PrepareSignals` | [x] |

---

## 二、阶段 P4 — 大文件深化（优先）

目标：十亿级跳变、千路层次仍可用；内存与 UI 可预测。

| ID | 任务 | 说明 | 优先级 | 状态 |
|----|------|------|--------|------|
| P4-1 | 侧车索引 `.idx` | FST `.bwidx` + `fstReader` 块表 + 按窗迭代（`BEAR2WAVE_IDX_CACHE`） | P0 | [x] |
| P4-2 | `TraceBackend` 抽象 | `core/trace_backend.h`：`LoadWindow` / `Cancel` / `Kind` | P0 | [x] |
| P4-3 | VCD 流式解析 | A+B+C 完成：时间块粗索引 + `setvbuf` 缓冲 I/O + 索引 seek | P0 | [x] |
| P4-4 | 标量紧凑存储 | 1-bit 位打包 / 4-state 编码，降 `value_change_t` 开销 | P1 | [x] |
| P4-5 | wxDataView 真虚拟列表 | 模块下信号列表虚拟行，替代 `wxListCtrl` 大批量插入 | P1 | [x] |
| P4-6 | 层次树虚拟化 | `ModuleTreeLazyCtrl` 展开时填充；`gen_module_tree_vcd.py` 压测 | P2 | [x] |
| P4-7 | 绘制 LOD | `RowSegmentBudget` + step 降采样 + pan-shift；`WaveformRenderer` | P1 | [x] |
| P4-8 | 加载进度与 ETA | 状态栏块进度 + ETA；`fstReaderSetIterProgressCallback` | P2 | [x] |
| P4-9 | VZT 超大文件加固 | 块失败重试、单块超时（W3-9）；错误信号隔离 → W4-12 | P2 | [~] |
| P4-10 | 内存预算与驱逐 | `BEAR2WAVE_MAX_LOADED_CHANGES` + LRU（`trace_memory_budget`） | P1 | [x] |
| P4-11 | 并行块读 | FST 多 block 线程池（W4-9～11）；**VZT 块预解压 POC（E2-3）** | P2 | [~] |
| P4-12 | mmap 只读映射 | 大文件只读路径减少拷贝（视 libfst API） | P3 | [ ] |

### P4 建议顺序

```text
P4-2（接口）→ P4-1（索引）→ P4-3（VCD）→ P4-4/P4-10（内存）
→ P4-5/P4-6（UI 虚拟化）→ P4-7/P4-11（绘制与 IO）
```

---

## 三、阶段 P5 — 高级波形能力（大文件场景延伸）

| ID | 任务 | 说明 | 状态 |
|----|------|------|------|
| P5-1 | Transaction 视图 | `BEAR2WAVE_VT_TRANSACTION` 专用行渲染 | [x] |
| P5-2 | FSM / 状态向量摘要 | 长区间状态名而非逐跳变 | [ ] |
| P5-3 | GPU 加速绘制 | OpenGL 批绘制阶梯（可选编译） | [ ] |
| P5-4 | 双轨迹对齐加载 | Compare 两路共享时间索引 | [ ] |
| P5-5 | 会话文件 `.bwv` | 保存显示列表 + 视口 + radix + 标记/别名/注释行 | [x] |

---

## 四、阶段 E0 — 构建与依赖（工程化基础）

| ID | 任务 | 说明 | 状态 |
|----|------|------|------|
| E0-1 | `vcpkg.json` manifest | zlib/bzip2/lzma 版本锁定 | [x] |
| E0-2 | CMake 与 vcxproj 对齐 | `TEST1/CMakeLists.txt` 为 CI 单一真相；VS 可导入 | [~] |
| E0-3 | 无绝对路径 | `Bear2WaveWx.props` + `Bear2Wave.Build.props` + `WXWIN`/`VCPKG_ROOT` | [x] |
| E0-4 | `fetch_gtkwave_libs.ps1` 幂等 | 校验 hash / 浅克隆标签；离线失败提示 | [ ] |
| E0-5 | 可选特性 CMake 选项 | `WITH_VZT` / `WITH_LXT2` / `WITH_GHW` / `WITH_TCL` | [ ] |
| E0-6 | `.gitignore` 收紧 | 保留 `*.vcxproj`；忽略 `TEST1/build/`、产物、本地 props | [x] |
| E0-7 | 提交 `vcxproj` 或模板 | `TEST1.vcxproj`、`tools/*.vcxproj` 纳入版本库 | [x] |
| E0-8 | 统一输出目录 | `out/x64/{Debug,Release}/Bear2Wave.exe` 文档化 | [x] |

---

## 五、阶段 E1 — 测试与基准

| ID | 任务 | 说明 | 状态 |
|----|------|------|------|
| E1-1 | `TraceTools bench` 纳入文档 | 已有 `tools/trace_tools.cpp` | [x] |
| E1-2 | CI 跑 `test-all` | `.github/workflows/windows-ci.yml` + `run_trace_tests.ps1` | [x] |
| E1-3 | 基准回归 JSON | `TraceTools bench --json out.json` | [x] |
| E1-4 | 大文件样例（可选 LFS） | 100MB+ FST 仅 CI nightly，仓库放链接 | [ ] |
| E1-5 | `LlmClient::SelfTestJsonParser` CI | `tools/LlmJsonSmoke.exe` | [x] |
| E1-6 | `BuildContext` 快照测试 | `TraceTools context-snapshot` + `tests/fixtures/context_snapshot.hash` | [x] |
| E1-7 | 内存泄漏抽检 | VS Diagnostic Tools / Application Verifier 清单 | [ ] |
| E1-8 | 取消加载竞态测试 | `TraceTools cancel-smoke`（后台 load + cancel） | [x] |

---

## 六、阶段 E2 — CI/CD 与发布

| ID | 任务 | 说明 | 状态 |
|----|------|------|------|
| E2-1 | GitHub Actions `windows-x64` | `windows-ci.yml`：TraceTools + test-all | [x] |
| E2-2 | 缓存 vcpkg / wxWidgets | 缩短 CI 时间 | [ ] |
| E2-3 | Release 产物 | `tools/package_release.ps1` → `dist/*.zip` | [x] |
| E2-4 | 版本号与 `VERSION.txt` | `BEAR2WAVE_VERSION` 宏 + 根目录 `VERSION.txt` | [x] |
| E2-5 | CHANGELOG.md | Keep a Changelog 格式 | [x] |
| E2-6 | 预发布检查脚本 | `tools/release_check.ps1` | [x] |
| E2-7 | 代码签名（可选） | Authenticode 减 SmartScreen 警告 | [ ] |

---

## 七、阶段 E3 — 可观测性与诊断

| ID | 任务 | 说明 | 状态 |
|----|------|------|------|
| E3-1 | 分级日志 | `core/bear2wave_log` + `BEAR2WAVE_LOG_LEVEL` / `BEAR2WAVE_LOG_FILE` | [x] |
| E3-2 | 加载指标面板 | 状态栏：块进度/ETA/已加载跳变数（FST/VCD 懒加载） | [x] |
| E3-3 | 崩溃 minidump | `core/bear2wave_minidump` → `%TEMP%`（`BEAR2WAVE_MINIDUMP`） | [x] |
| E3-4 | 环境变量一览 | 帮助菜单或 `docs` 自动生成 env 表 | [~] |
| E3-5 | FST/VZT 诊断包 | 一键导出：路径、backend、最后错误、env | [ ] |

---

## 八、阶段 E4 — 配置、文档与体验

| ID | 任务 | 说明 | 状态 |
|----|------|------|------|
| E4-1 | 用户配置目录规范 | 已有 `%APPDATA%/Bear2Wave/`（AI）；扩展最近文件列表 | [~] |
| E4-2 | 便携模式 | 同目录 `config.ini` 优先 | [ ] |
| E4-3 | 更新 README 已知限制 | AI 异步、懒加载、内存预算 env | [x] |
| E4-4 | 截图与快速入门 GIF | `docs/images/` + `docs/QUICKSTART.md`（占位，见 images/README） | [~] |
| E4-5 | 贡献指南 CONTRIBUTING.md | 分支、构建、PR 检查项 | [x] |
| E4-6 | Issue 模板 | bug / 性能 / 功能请求 | [ ] |

---

## 九、阶段 E5 — 架构与代码质量

| ID | 任务 | 说明 | 状态 |
|----|------|------|------|
| E5-1 | 拆分 `MainFrame.hpp` | `TraceDocument` + `WaveformController` + `WaveformSessionController` + `MainFrameMenus` 接入 + 构建卫生 + `m_vcdData` 审计 | [x] |
| E5-2 | `WaveformPanel` 绘制与输入分离 | Renderer + `WaveformPanel_trace_load.cpp`；Panel 仍 ~2k 行 | [~] |
| E5-3 | 加载层独立库 | `bear2wave_trace` 静态库（loader + vcd） | [ ] |
| E5-4 | 头文件依赖收敛 | 减少 `MainFrame.hpp` 在 loader 中的反向 include | [ ] |
| E5-5 | clang-format / `.editorconfig` | 统一风格 | [ ] |
| E5-6 | 静态分析 | `/W4` 或 PVS-Studio 可选 job | [ ] |
| E5-7 | 第三方边界文档 | `third_party/README.md`：来源、版本、许可证 | [ ] |

---

## 十、环境变量规划（大文件 + 工程）

| 变量 | 默认 | 作用 | 状态 |
|------|------|------|------|
| `BEAR2WAVE_LOAD_MARGIN` | 0.2 | 视口外预加载比例 | [x] |
| `BEAR2WAVE_TRACE_LOAD_DEBOUNCE_MS` | 120 | 后台加载防抖 | [x] |
| `BEAR2WAVE_CACHE_DEBOUNCE_MS` | 75 | 绘制缓存防抖 | [x] |
| `BEAR2WAVE_MAX_SEGMENTS` | 5000 | 单路最大绘制段 | [x] |
| `BEAR2WAVE_CACHE_THREADS` | CPU≤8 | 缓存构建线程 | [x] |
| `BEAR2WAVE_MAX_SIGNAL_LIST` | 8000 | 列表行上限 | [x] |
| `BEAR2WAVE_VCD_WARN_MB` | 50 | VCD 警告阈值 | [x] |
| `BEAR2WAVE_MAX_LOADED_CHANGES` | 0（关） | 全局已加载跳变上限；LRU 驱逐（P4-10） | [x] |
| `BEAR2WAVE_IDX_CACHE` | 1 | FST 侧车 `.bwidx` 写/校验（P4-1） | [x] |
| `BEAR2WAVE_LOG_LEVEL` | warn | 日志级别（E3-1 `bear2wave_log`） | [x] |
| `BEAR2WAVE_VCD_IDX_CACHE` | 继承 `IDX_CACHE` | VCD 侧车 `.bwvcdidx` | [x] |

---

## 十一、里程碑建议

| 里程碑 | 包含 | 验收 |
|--------|------|------|
| **M1 可发布** | E0-3/6/7/8、E2-1/3/4/5/6、E4-3/5、E1-2 | 克隆+props 可编；`package_release.ps1`；CI TraceTools 绿 |
| **M2 大 FST 稳** | P4-1/2/10、P4-7、E1-3 | 1GB FST、20 路信号平移无明显卡顿（P4-1/E1-3 已实现，待大文件验收） |
| **M3 大 VCD 可用** | P4-3、P4-5 | 500MB VCD 可选信号打开不 OOM |
| **M4 工程成熟** | E3/E5 大部分、P5 按需 | 文档齐、日志/崩溃可诊断 |

---

## 十二、明确不做（防范围膨胀）

- 完整复刻 GTKWave 全菜单 / 全部 Tcl 命令  
- 内建仿真或 RTL 编辑  
- 云端波形托管与协作（除非单独立项）  
- 默认全芯片自动加载所有信号  

---

## 相关文档

| 文档 | 内容 |
|------|------|
| [LARGE_TRACE_PERFORMANCE.md](LARGE_TRACE_PERFORMANCE.md) | 已实现能力与环境变量 |
| [TRACE_FORMATS_BUILD.md](TRACE_FORMATS_BUILD.md) | VZT/LXT2/GHW 编译 |
| [AI_ANALYSIS_TODO.md](AI_ANALYSIS_TODO.md) | AI 功能路线（A–D 已完成） |
| [LOCAL_ANALYSIS_TODO.md](LOCAL_ANALYSIS_TODO.md) | 本地统计路线 |

---

*维护：完成某项后将 `[ ]` 改为 `[x]`，并在 `LARGE_TRACE_PERFORMANCE.md` 的「已实现/后续」中同步一行摘要。*
