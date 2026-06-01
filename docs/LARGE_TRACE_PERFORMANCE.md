# Bear2Wave 大波形文件性能指南

## 推荐格式

| 格式 | 大文件适用性 | 说明 |
|------|-------------|------|
| **FST** | 最佳 | 二进制、块索引；默认**懒加载**（先层次、按信号+时间窗读数据） |
| **VZT / LXT2** | 较好 | 需 `BEAR2WAVE_WITH_VZT` / `BEAR2WAVE_WITH_LXT2`；同样支持懒加载 |
| **GHW** | 较好 | 需 `BEAR2WAVE_WITH_GHW`；层次先加载，加入波形后按时间窗扫描 |
| VCD | 不推荐 | 全文解析进内存 |

仿真时尽量使用 `$dumpfile("waves.fst")` 或 `vcd2fst`。

## 使用习惯

1. 打开文件后，在左侧树**只双击需要的信号**加入波形区。
2. 平移/缩放时间轴时，会自动按**当前视口 ±20%** 扩展加载窗口（懒加载格式）。
3. 加载对话框可点「取消」中止 FST/VZT/LXT2 的块迭代。

## 内存粗算

```
约 ≈ 已加载信号数 × 视口内变化条数 × ~72 字节
```

## 环境变量

| 变量 | 默认值 | 作用 |
|------|--------|------|
| `BEAR2WAVE_LOAD_MARGIN` | `0.2` | 视口外预加载边距比例 |
| `BEAR2WAVE_CACHE_DEBOUNCE_MS` | `75` | 绘制缓存重建防抖（ms） |
| `BEAR2WAVE_TRACE_LOAD_DEBOUNCE_MS` | `120` | 波形数据后台加载防抖（ms） |
| `BEAR2WAVE_MAX_SEGMENTS` | `5000` | 单路信号绘制段数上限 |
| `BEAR2WAVE_CACHE_THREADS` | `0`（=CPU 核数，最多 8） | 绘制缓存并行线程数 |
| `BEAR2WAVE_VCD_WARN_MB` | `50` | 超过该大小的 VCD 打开时提示转 FST（`0` 关闭） |
| `BEAR2WAVE_MAX_SIGNAL_LIST` | `8000` | 模块信号列表最多显示行数 |
| `BEAR2WAVE_FST_DEBUG=1` | — | FST 加载详细日志 |
| `BEAR2WAVE_WAVE_DEBUG=1` | — | 波形逐帧调试（量很大） |

## 已实现

- 推迟全局排序；绘制缓存防抖（可配置）
- VZT 块内存上限 64MB
- **FST / VZT / LXT2 / GHW** 懒加载 + `trace_load_signals()` 统一 API
- 视口时间窗 + 可配置边距；**后台线程**加载，不阻塞 UI
- 已加载窗口覆盖视口时跳过重复加载
- **增量时间窗扩展**：平移/放大视口时只读缺口区间，不清空已有 `value_changes`
- 加载后 `vcd_signal_shrink_to_fit` 收紧缓冲区容量
- GHW 会话保持 `ghw_handler` 打开，按次 `ghw_read_dump` 扫描
- 绘制缓存：**平移时增量平移段坐标**；多线程按行构建
- 模块树仅按**唯一模块路径**建树；信号列表超限时截断并提示
- 大 VCD（默认 >50MB）打开时提示使用 `vcd2fst` 转 FST

取消加载：关闭文件或切换视口时会调用 `trace_loader_request_cancel`（FST/VZT/LXT2）。

## 性能基准

```text
TraceTools.exe bench <path.fst> [signals_to_load]
```

输出：打开耗时、加载 N 路信号耗时、信号数。

## 后续

- 侧车 `.idx` 索引、标量紧凑存储（位打包）、wxDataView 真虚拟列表、VCD 流式解析
