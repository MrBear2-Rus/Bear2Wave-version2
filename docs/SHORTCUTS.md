# Bear2Wave 快捷键参考

> 波形区快捷键需先 **点击波形显示区** 获得焦点。  
> 全局快捷键在 **非文本输入框** 焦点下生效。

---

## 全局

| 快捷键 | 功能 |
|--------|------|
| `Ctrl+O` | 打开波形文件（File → Open Trace…） |
| `Ctrl+N` | 打开新窗口 |
| `Ctrl+T` | 打开新 Tab（同 Open Trace 流程） |
| `Ctrl+S` | 保存会话（.bwv / 当前会话路径） |
| `Shift+Ctrl+S` | 另存会话 |
| `Ctrl+W` | 关闭窗口 |
| `Ctrl+Q` | 退出 |
| `Ctrl+Shift+C` | Compare：打开第二路轨迹（勿用 `Ctrl+Shift+O`，中文 IME 常拦截） |
| `Ctrl+Shift+A` | 显示/隐藏 AI 分析面板 |
| `Ctrl+Shift+F` | 聚焦底部信号搜索框 |
| `Ctrl+Shift+L` | 显示/隐藏调试日志窗口 |
| `Ctrl+F1` | 打开帮助目录 |
| `Esc` | 取消 lazy 加载；清除 A/B 测量 |

---

## 波形区 — 时间与视口

| 快捷键 | 功能 |
|--------|------|
| `PgUp` | 视窗向左翻一页 |
| `PgDn` | 视窗向右翻一页 |
| `←` / `→` | 播放头微调（Shift：大步长） |
| `Ctrl+←` / `Ctrl+→` | 平移时间视窗（Shift：大步长） |
| `Home` | 播放头到 0 |
| `End` | 播放头到仿真末尾 |
| `Ctrl+Home` | 视窗到时间 0 |
| `Ctrl+End` | 视窗到末尾 |
| `F1` | Move To Time（菜单：Time → Move To Time） |

---

## 波形区 — 缩放

| 快捷键 | 功能 |
|--------|------|
| `+` / `=` | 放大 |
| `-` | 缩小 |
| `Ctrl+0` | Zoom Full（全时间范围） |
| `Ctrl++` / `Ctrl+-` | 菜单 Zoom In/Out（需菜单焦点或全局加速） |
| 滚轮 | 缩放（Alternate Wheel Mode 关闭时） |

---

## 波形区 — 边沿与行编辑

| 快捷键 | 功能 |
|--------|------|
| `,` | 跳转到上一跳变沿 |
| `.` | 跳转到下一跳变沿 |
| `Delete` / `Backspace` | 删除选中波形行 |
| `B` | 复制主标记到 B 标记 |

---

## 标记（Markers 菜单）

| 快捷键 | 功能 |
|--------|------|
| `Alt+H` | 在播放头放置命名标记 |
| `Alt+M` | Show-Change Marker Data |
| `Shift+Alt+M` | 删除主标记 |
| `Shift+点击` | 在点击处放置命名标记 |
| `Ctrl+拖动` | A/B 测量区间 |

---

## 编辑（Edit 菜单）

| 快捷键 | 功能 |
|--------|------|
| `Ctrl+B` | 插入空行 |
| `Ctrl+X` / `Ctrl+C` / `Ctrl+V` | 剪切 / 复制 / 粘贴 |
| `Ctrl+Delete` | Delete（菜单项） |
| `F3` / `F5` | Combine Down / Up |
| `Ctrl+A` | Highlight All |
| `Shift+Ctrl+A` | UnHighlight All |
| `Alt+A` | 别名高亮 trace |
| `T` | Toggle Group Open/Close |

---

## 鼠标操作

| 操作 | 功能 |
|------|------|
| 左键拖动波形区 | 移动播放头 |
| Ctrl+左键拖动 | A/B 测量 |
| Shift+点击 | 放置命名 Marker |
| 滚轮 | 缩放时间（或移动时间，视 Alternate Wheel Mode） |
| 双击模块树信号 | 添加到波形区 |
| 右键波形行 | 上下文菜单（含 AI Analyze） |

---

## 底栏

| 控件 | 功能 |
|------|------|
| 搜索框 | 过滤信号树/列表 |
| `+` / `-` / Reset | 缩放按钮 |
| 滑块 | 播放头位置 |
| From / To | 可见时间范围 |
| Play / Pause | 自动推进播放头 |

---

*完整功能说明见 docs/USER_GUIDE.md 或 Help → Help Contents。*
