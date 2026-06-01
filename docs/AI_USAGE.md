# Bear2Wave AI 分析使用说明

## 快速开始

1. 在右侧 **AI 分析** 面板填写 DeepSeek API 密钥，点击 **保存密钥**（写入 `%APPDATA%\Bear2Wave\config.ini`）。
2. 在波形区添加要分析的信号，点击 **从波形刷新**。
3. 选择模板或编辑请求，点击 **AI 分析**（异步，可 **取消**）。
4. 无密钥时可点 **本地报告** 查看统计。

## 阶段 C 功能

| 功能 | 操作 |
|------|------|
| 分析单路信号 | 波形 trace 名右键 → **AI Analyze This Signal** |
| 总线十六进制 | 多比特信号上下文自动带 hex 解码 |
| 握手 / 复位 | 选模板「总线/握手」或「复位与时钟」会附加启发式摘要 |
| 跳转时间 | 分析完成后点 **跳转时间**，从结果中选时间戳 |
| 多轮对话 | 勾选 **多轮对话**（保留最近 K 轮） |
| Compare | 先 **Compare → Open Second Trace**，勾选 **附加 Compare 差异摘要** |
| 搜信号 | AI 面板输入如 `uart tx` → **搜信号**（同步主窗口搜索高亮） |
| 协议解码 + AI | 选模板 **协议解码 (Transaction Filter)** 或提问含 I2C/SPI/UART 等关键词；先运行 Transaction Filter，再 **从波形刷新** |
| `[TXN]` 前缀行 | Transaction 虚拟信号自动进入 AI 信号列表 |
| 时间精化 | 问题中含 `1.2ms` 等物理时间时，分析窗口自动收窄到该时刻附近 |

## 阶段 D：架构

- `core/LlmClient` — OpenAI 兼容 HTTP + JSON 解析 + 多轮 `messages`
- `core/ai_analysis_service` — 拼 prompt、对话历史、时间戳解析、信号匹配
- `waveform_analysis` — 波形上下文 + 握手/复位/Compare 附录

## 环境变量

| 变量 | 默认 | 说明 |
|------|------|------|
| `BEAR2WAVE_AI_MAX_EDGES_PER_SIG` | 200 | 每信号最大跳变采样数 |
| `BEAR2WAVE_AI_MAX_CONTEXT_CHARS` | 30000 | 上下文总字符上限 |
| `BEAR2WAVE_AI_MAX_CHAT_TURNS` | 4 | 多轮对话保留轮数 |

## Ollama 本地（无云 API）

1. 本机运行 Ollama 并拉取模型（如 `llama3.2`）。
2. AI 面板 **设置** → 勾选 **Ollama 本地** → 确定（主机 `127.0.0.1:11434`，路径 `/v1/chat/completions`）。
3. API 密钥可填任意非空占位（Ollama 通常不校验）。

## 冒烟自测

`LlmClient::SelfTestJsonParser()` 可在调试时调用，验证 JSON 解析无需网络。
