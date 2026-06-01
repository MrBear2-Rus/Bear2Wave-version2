# 阶段 B / C / E3 — TODO

| ID | 任务 | 状态 |
|----|------|------|
| B1 | `WaveformRenderer::BuildCacheAsync` 启用；`WaveformPanel` 委托 | [x] |
| C1 | VCD 时间索引侧车 `.bwvcdidx` + 打开时加载 | [x] |
| C1b | VCD 增量 gap 加载（C4） | [x] |
| E3-1 | `core/bear2wave_log` + `BEAR2WAVE_LOG_LEVEL` | [x] |
| E3-2 | 状态栏加载进度 + `bear2wave_minidump` | [x] |

## 环境变量

| 变量 | 默认 | 说明 |
|------|------|------|
| `BEAR2WAVE_LOG_LEVEL` | `warn` | `error`/`warn`/`info`/`debug`/`trace` |
| `BEAR2WAVE_LOG_FILE` | `0` | `1` → `%APPDATA%/Bear2Wave/logs/bear2wave.log` |
| `BEAR2WAVE_VCD_IDX_CACHE` | 继承 `BEAR2WAVE_IDX_CACHE` | `0` 关闭 VCD `.bwvcdidx` |
| `BEAR2WAVE_MINIDUMP` | `1` | `0` 关闭崩溃 minidump（`%TEMP%`） |
