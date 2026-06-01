# 第三周收尾说明

**状态**：Week 3 核心目标已达成；实验项与截图素材按下列方式收口。

## 里程碑

| 里程碑 | 状态 | 说明 |
|--------|------|------|
| M1 可发布 | ✅ | CI + `run_smoke` + `package_release` + `release_check` |
| M2 大 FST | ✅ | large_test 手工冒烟 + cancel-smoke |
| M3 大 VCD | ✅ | lazy + 侧车；nightly `-IncludeLarge` |
| M4 多信号可视 | ✅ | W3-16/16b 垂直滚动 + W3-16c 可见行缓存 |

## 任务清单

| ID | 状态 | 备注 |
|----|------|------|
| W3-1～3, 5～9, 11～13, 15～16b | ✅ | 见 `PHASE_WEEK3_TODO.md` |
| W3-16c | ✅ | `BEAR2WAVE_CACHE_VISIBLE_ROWS`（默认开） |
| W3-4 | ✅（本机） | Release 已手动编译运行；流程固化见第四周 W4-1～3 |
| W3-14 | — 搁置 | [QUICKSTART.md](QUICKSTART.md) 占位保留，截图不做 |
| W3-10 | 🔜 W5 | FST 并行块读 → [PHASE_WEEK5_BACKLOG.md](PHASE_WEEK5_BACKLOG.md) |

## 发版前命令

```powershell
powershell -File tools\release_check.ps1
powershell -File tools\run_smoke.ps1 -TraceToolsOnly
msbuild TEST1\TEST1.vcxproj /p:Configuration=Debug /p:Platform=x64
powershell -File tools\package_release.ps1
# 有 mswu 时：
powershell -File tools\package_release.ps1 -Configuration Release
```

## 手工验收

`tests/SMOKE_CHECKLIST.md` §1～3b（含菜单/滚动）。

## 第四周

详见 **[PHASE_WEEK4_TODO.md](PHASE_WEEK4_TODO.md)**（**修订**：波形格式检查与扩展）。Compare 等见 [PHASE_WEEK5_BACKLOG.md](PHASE_WEEK5_BACKLOG.md)。
