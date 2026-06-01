# Bear2Wave 快速入门

5 分钟上手；完整说明见 [USER_GUIDE.md](USER_GUIDE.md)。

## 1. 打开波形

启动 `Bear2Wave.exe` → **File → Open Trace…**（`Ctrl+O`）→ 选择 `tests/traces/bear2wave_sample.fst` 或任意 `.vcd` / `.fst`。

<!-- 截图占位：发版前将 docs/images/01-open-trace.png 放入仓库后取消下行注释 -->
<!-- ![打开波形](images/01-open-trace.png) -->

## 2. 添加信号

1. 左侧 **模块树** 展开层次。  
2. 点击模块，右侧 **信号列表** 列出信号。  
3. **双击** 信号加入波形区（可重复添加同一路）。

<!-- ![模块树与信号列表](images/02-module-tree-signals.png) -->

## 3. 浏览时间

- 拖动波形区：移动播放头（红线）。  
- **滚轮**：缩放时间轴（Markers 菜单可切换为滚轮平移时间）。  
- 底部 **From / To**：精确设置可见时间范围。

<!-- ![波形显示](images/03-waveform-rows.png) -->

## 4. 多路信号与滚动

连续添加多路信号后，左侧信号名列 **滚轮** 或 **↑↓** 可滚动；**Ctrl+滚轮** 在任意位置滚动信号列表。  
默认仅为**可见行**构建绘制缓存（`BEAR2WAVE_CACHE_VISIBLE_ROWS=1`），适合 15 路以上。

<!-- ![多路信号滚动](images/04-scroll-many-signals.png) -->

## 5. 保存会话

**File → Write Session**（`Ctrl+S`）保存 `.bwv`；下次 **Read Session** 恢复显示列表与视口。

<!-- ![保存会话](images/05-session-save.png) -->

## 6. 帮助与排障

- **Help → Contents**（`Ctrl+F1`）：环境变量与快捷键。  
- **View → Export diagnostics bundle**：导出路径、backend、环境摘要。

<!-- ![帮助与诊断](images/06-help-diagnostics.png) -->

## 下一步

| 主题 | 文档 |
|------|------|
| 环境变量 | [ENVIRONMENT.md](ENVIRONMENT.md) |
| 大文件性能 | [LARGE_TRACE_PERFORMANCE.md](LARGE_TRACE_PERFORMANCE.md) |
| Beta 限制 | [README.md](../README.md) |
| 发版前检查 | `tests/SMOKE_CHECKLIST.md` |
