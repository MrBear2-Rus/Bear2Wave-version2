# Bear2Wave 快速入门截图（W3-14）

本目录存放发布文档用的界面截图。当前为 **占位说明**；发版前请用本机 `Bear2Wave.exe` 按下列步骤截取真实 PNG。

## 建议文件名

| 文件 | 内容 |
|------|------|
| `01-open-trace.png` | 启动后主窗口 + File → Open Trace |
| `02-module-tree-signals.png` | 左侧模块树展开、右侧信号列表 |
| `03-waveform-rows.png` | 已添加 2～3 路信号（含 1-bit 方波） |
| `04-scroll-many-signals.png` | 15+ 路信号 + 右侧垂直滚动条 |
| `05-session-save.png` | File → Write Session As |
| `06-help-diagnostics.png` | Help → Contents；View → Export diagnostics |

## 截取步骤（Windows）

1. 构建并运行：`out\x64\Debug\Bear2Wave.exe`（或 Release）。
2. 打开样例：`tests\traces\bear2wave_sample.fst` 或 `test2.vcd`。
3. **Win+Shift+S** 区域截图，保存为本表文件名。
4. 在 `docs/QUICKSTART.md` 中取消对应 `![...](images/...)` 注释。

## 可选 GIF

若需动图，可用 ScreenToGif 录制「双击加信号 → 拖时间轴 → 滚轮缩放」约 10～15 秒，保存为 `quickstart-demo.gif`。

## 引用

- 文字版步骤：`docs/QUICKSTART.md`、`docs/USER_GUIDE.md`
- 手工验收：`tests/SMOKE_CHECKLIST.md`
