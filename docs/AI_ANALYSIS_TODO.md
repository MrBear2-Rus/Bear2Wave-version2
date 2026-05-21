# Bear2Wave AI 分析面板 — TODO 清单

> 目标：从「只发信号名」升级为「基于当前视口波形的智能调试助手」。  
> 状态标记：`[ ]` 未做 · `[~]` 进行中 · `[x]` 完成

---

## 阶段 A — 核心闭环（MVP，优先）

与懒加载 / FST 大文件对齐，让 AI 真正「看到」波形。

| ID | 任务 | 说明 | 状态 |
|----|------|------|------|
| A1 | `WaveformAnalysisContext` | `core/waveform_analysis.{h,cpp}` | [x] |
| A2 | 分析前加载数据 | `PrepareSignals` + `trace_load_signals` | [x] |
| A3 | 信号列表 = 波形区 | `SetDisplayedSignals(m_displayedSignals2)` | [x] |
| A4 | 主窗口挂钩 | `SyncAiPanelFromWavePanel()` | [x] |
| A5 | 改写 `OnAnalyze` 上下文 | `BuildContext()` in prompt | [x] |
| A6 | 跳变抽样上限 | `BEAR2WAVE_AI_MAX_EDGES_PER_SIG` / `MAX_CONTEXT_CHARS` | [x] |
| A7 | 预设分析模板 | 模板下拉 4 项 | [x] |
| A8 | 移除面板内 Load VCD | 已移除 | [x] |
| A9 | 修复 API Key 校验 | `IsPlaceholderApiKey` | [x] |
| A10 | MVP 验收 | 需本地手测 + API key | [~] |

---

## 阶段 B — 体验与工程（稳定可演示）

| ID | 任务 | 说明 | 状态 |
|----|------|------|------|
| B1 | 异步 HTTP | `std::thread` + `CallAfter` 更新输出；Analyze 期间 UI 可响应 | [ ] |
| B2 | 取消分析 | 进行中可取消（`atomic` 标志 + 关闭请求） | [ ] |
| B3 | JSON 解析 | 替换 `Find("content:")`，用可靠 JSON 库或严格解析 | [ ] |
| B4 | API Key 持久化 | `%APPDATA%/Bear2Wave/config.json` 或注册表；启动加载 | [ ] |
| B5 | 导出安全 | `analysis.txt` / 报告**不写入** API Key；可选 Markdown | [ ] |
| B6 | 设置对话框 | 模型名、base URL、temperature、max tokens（可选） | [ ] |
| B7 | 错误提示中文化 | 401 / 超时 / 网络 / 空响应 | [ ] |
| B8 | 本地预分析报告 | 无 key 或「仅本地」模式：只显示 `WaveformLocalStats` 结果 | [ ] |
| B9 | `WaveformLocalStats` | 边沿数、占空比、周期估算、复位释放时刻、常值段检测 | [ ] |
| B10 | Marker 区间分析 | 勾选「仅 A–B 区间」；上下文带 marker 时间戳 | [ ] |

---

## 阶段 C — 调试向能力（差异化）

| ID | 任务 | 说明 | 状态 |
|----|------|------|------|
| C1 | 右键「AI 分析此信号」 | `SignalTraceContextMenu` → 勾选并聚焦该路 | [ ] |
| C2 | 总线/进制解码进上下文 | 复用 `WaveformRadix`，十六进制/ASCII 友好序列 | [ ] |
| C3 | 握手模板 | valid/ready 类信号名启发 + 成对检查摘要 | [ ] |
| C4 | 复位域模板 | `rst_n` 释放前后各信号变化顺序摘要 | [ ] |
| C5 | 时间戳跳转 | 从回复解析 `\d+\s*ns` → `SetCurrentTimestamp` / 平移视口 | [ ] |
| C6 | 多轮对话 | 保留最近 K 轮 messages（注意 token 上限） | [ ] |
| C7 | Compare 联动 | 双窗口：差异摘要（左有右无的边沿）再送 AI | [ ] |
| C8 | 自然语言搜信号 | 「找 uart tx」→ 过滤树/列表（可接搜索框） | [ ] |

---

## 阶段 D — 架构与质量

| ID | 任务 | 说明 | 状态 |
|----|------|------|------|
| D1 | 拆分 `LlmClient` | `core/LlmClient.{h,cpp}`：DeepSeek + 可配置 OpenAI 兼容 URL | [ ] |
| D2 | UI 与逻辑分离 | `AIAnalysisPanel` 仅 UI；业务进 context + client | [ ] |
| D3 | 单元测试 / 冒烟 | 对 `BuildWaveformContext` 用 sample.fst 固定输出快照 | [ ] |
| D4 | 文档 | 在 `README.md` 增加 AI 使用说明与环境变量 | [ ] |
| D5 | 可选：Ollama 本地模型 | 无云 API 演示 | [ ] |

---

## 依赖关系（建议顺序）

```text
A1 → A2 → A5 → A10
A3 → A4
A7、A9 可并行
B1、B3 在 A5 之后
B9 → B8、B10
C* 在 B 稳定后
D* 贯穿重构时做
```

---

## 环境变量（规划）

| 变量 | 默认 | 作用 |
|------|------|------|
| `BEAR2WAVE_AI_MAX_EDGES_PER_SIG` | 200 | 每信号送入 AI 的最大跳变数 |
| `BEAR2WAVE_AI_MAX_CONTEXT_CHARS` | 30000 | 上下文总字符上限 |
| `BEAR2WAVE_AI_API_BASE` | deepseek | 可改为兼容 endpoint |

---

## 不在此清单（避免范围膨胀）

- 全文件 VCD 塞进 prompt  
- 代替形式验证 / 精确 setup-hold 签核  
- 自动修改 RTL  
- GPU 推理本地大模型（除非单独立项）

---

*最后更新：与 P0–P3 波形性能路线独立，可并行开发。*
