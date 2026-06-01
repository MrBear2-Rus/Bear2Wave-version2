# Bear2Wave 环境变量参考

所有变量均为可选；未设置时使用默认值。修改后需 **重启 Bear2Wave** 生效（除非另有说明）。

---

## 大文件与加载

| 变量 | 默认值 | 说明 |
|------|--------|------|
| `BEAR2WAVE_LOAD_MARGIN` | `0.2` | 视口外预加载边距（占可见范围比例） |
| `BEAR2WAVE_TRACE_LOAD_DEBOUNCE_MS` | `120` | 后台波形加载防抖（毫秒） |
| `BEAR2WAVE_CACHE_DEBOUNCE_MS` | `75` | 绘制缓存重建防抖（毫秒） |
| `BEAR2WAVE_MAX_LOADED_CHANGES` | `0`（关） | 全局已加载跳变上限；超出 LRU 驱逐 |
| `BEAR2WAVE_IDX_CACHE` | `1` | FST 侧车 `.bwidx`；`0` 关闭 |
| `BEAR2WAVE_COMPACT_VC` | `1` | 1-bit 标量紧凑存储；`0` 关闭 |

---

## VZT

| 变量 | 默认值 | 说明 |
|------|--------|------|
| `BEAR2WAVE_VZT_BLOCK_RETRIES` | `2` | granule 块读失败后的重试次数（每次减半块内存上限） |
| `BEAR2WAVE_VZT_BLOCK_TIMEOUT_MS` | `0`（关） | 单次块迭代墙钟超时（毫秒）；超时后取消并返回 |
| `BEAR2WAVE_VZT_THREADS` | `0`（auto） | VZT 块预解压线程数（1=单线程，2–8=并行 POC；MSVC 需 `BEAR2WAVE_VZT_PTHREAD_WIN32`） |

---

## VCD

| 变量 | 默认值 | 说明 |
|------|--------|------|
| `BEAR2WAVE_VCD_WARN_MB` | `50` | 超过该 MB 打开 VCD 时警告；`0` 关闭 |
| `BEAR2WAVE_VCD_LAZY` | `-1` | `-1` 按大小自动；`1` 强制 lazy；`0` 全文加载 |
| `BEAR2WAVE_VCD_LAZY_MB` | `10` | 自动 lazy 阈值（MB） |

---

## 绘制与 UI

| 变量 | 默认值 | 说明 |
|------|--------|------|
| `BEAR2WAVE_MAX_SEGMENTS` | `5000` | 单路信号最大绘制段数（LOD） |
| `BEAR2WAVE_CACHE_THREADS` | `0`（=CPU 核数，最多 8） | 绘制缓存并行线程数 |
| `BEAR2WAVE_CACHE_VISIBLE_ROWS` | `1` | 仅为垂直可见行构建绘制缓存；`0` 恢复全列表 |
| `BEAR2WAVE_CACHE_VISIBLE_ROW_PAD` | `1` | 可见行上下各多缓存的行数 |
| `BEAR2WAVE_MAX_SIGNAL_LIST` | `8000` | 模块信号列表软上限提示 |
| `BEAR2WAVE_DIRECTWRITE` | `1`（Windows） | OpenGL 文字用 DirectWrite；`0` 强制 wx 白底抠图回退 |
| `BEAR2WAVE_FST_HIER` | `0` | `1` 尝试解码 FST embedded `.hier`（Windows 默认 GEOM-only，与 GUI 一致） |
| `BEAR2WAVE_FST_GEOM_ONLY` | Win 默认开 | `0` 强制 embedded `.hier`；`1` 显式 GEOM 占位信号 |

---

## AI 分析

| 变量 | 默认值 | 说明 |
|------|--------|------|
| `BEAR2WAVE_AI_MAX_EDGES_PER_SIG` | `200` | 每信号送入 AI 的最大跳变采样 |
| `BEAR2WAVE_AI_MAX_CONTEXT_CHARS` | `30000` | AI 上下文总字符上限 |
| `BEAR2WAVE_AI_MAX_CHAT_TURNS` | `4` | 多轮对话保留轮数 |

---

## 本地分析

| 变量 | 默认值 | 说明 |
|------|--------|------|
| `BEAR2WAVE_GLITCH_MAX_WIDTH` | `3` | 毛刺检测最大脉宽（时间单位） |

---

## 调试

| 变量 | 默认值 | 说明 |
|------|--------|------|
| `BEAR2WAVE_FST_DEBUG` | — | 设为 `1` 输出 FST 加载详细日志 |
| `BEAR2WAVE_WAVE_DEBUG` | — | 设为 `1` 波形逐帧调试（量极大） |

---

## Windows 设置示例

```powershell
# 当前会话
$env:BEAR2WAVE_VCD_LAZY = "1"
$env:BEAR2WAVE_MAX_LOADED_CHANGES = "50000000"
& ".\out\x64\Release\Bear2Wave.exe"

# 永久（用户环境）
[System.Environment]::SetEnvironmentVariable("BEAR2WAVE_LOAD_MARGIN", "0.3", "User")
```

---

## 内存粗算

```
已加载内存 ≈ 已加载信号数 × 视口内跳变数 × 每跳变字节数
```

- 普通 `value_change_t`：约 **72 B/跳变**
- 1-bit 紧凑模式（`BEAR2WAVE_COMPACT_VC=1`）：约 **9 B/跳变**

---

*性能详解见 docs/LARGE_TRACE_PERFORMANCE.md。*
