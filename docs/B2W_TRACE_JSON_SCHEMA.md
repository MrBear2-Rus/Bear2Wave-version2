# Bear2Wave JSON Trace Schema（E5-4，预留）

> **状态：未实现加载器** — 仅定义格式，供后续 `json_loader` 或导出功能使用。  
> 当前请使用 VCD/FST/LXT 等已支持格式，或 `.bwv`/`.b2w` 会话文件。

## 文件识别

- 建议扩展名：`.b2wtrace.json`
- 根对象必须含 `"bear2wave_trace": "1.0"`

## 最小示例

```json
{
  "bear2wave_trace": "1.0",
  "timescale": "1ns",
  "signals": [
    {
      "name": "TOP.clk",
      "width": 1,
      "changes": [[0, "0"], [10, "1"], [20, "0"]]
    },
    {
      "name": "TOP.data",
      "width": 8,
      "changes": [[0, "00"], [15, "ff"]]
    }
  ]
}
```

## 字段说明

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `bear2wave_trace` | string | 是 | 版本，当前 `"1.0"` |
| `timescale` | string | 否 | 同 VCD `$timescale`，默认 `"1ns"` |
| `signals` | array | 是 | 信号列表 |
| `signals[].name` | string | 是 | 全名，如 `TOP.clk` |
| `signals[].width` | number | 否 | 位宽，默认 1 |
| `signals[].changes` | array | 是 | `[[time, value], ...]`，`value` 为二进制字符串 |

## 与 CSV 的区别

- CSV：表格式，首列为时间（现有 `csv_loader`）
- JSON schema：结构化、适合程序生成，尚未接入 `trace_loader`

## 验收（实现后）

```powershell
TraceTools.exe test bear2wave_sample.b2wtrace.json
```
