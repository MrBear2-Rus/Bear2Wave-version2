# Changelog

All notable changes to Bear2Wave are documented here.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [0.2.1-beta] - 2026-05-19

### Added

- **FST 流式写入**：`vcd_stream_write_fst()` + `TraceTools vcd2fst` + `trace_convert_path` VCD/EVCD→FST
- **DirectWrite GL 文字层**：Windows 上 OpenGL 模式 Grayscale 抗锯齿 + wx 回退（`BEAR2WAVE_DIRECTWRITE=0`）
- **AI + 协议解码联动**：Transaction Filter 虚拟行进入 AI 上下文；自然语言时间（如 `1.2ms`）；模板「协议解码」
- **Tcl 扩展**：`get_value`、`find`、`select`、`measure`、`convert`、`transaction run`、`zoom_factor` 等 + gtkwave 兼容 proc
- **发布文档**：`docs/RELEASE_SIGNOFF.md`；冒烟 / DirectWrite 回归签字

### Changed

- 模拟信号默认 **线性插值** 段（`AnalogRenderStyle::Linear`）
- `docs/BEAR2WAVE_VS_GTKWAVE.md`、`README` 与 FST 写入 / Transaction / Tcl 现状对齐

### Fixed

- DirectWrite ClearType 在透明纹理上导致 GL 文字全不可见

## [0.2.0-beta] - 2026-05-19

### Added (Week 4 Day 2)

- Compare: hub broadcast reentrancy guard; `SyncLinkMenuChecks` on all windows
- Compare: open second trace loads before show; failed load closes peer window
- Zoom in/out emits time-view change for linked compare windows

### Added

- Waveform panel vertical scroll (wheel, keys, scrollbar) for many displayed signals
- Visible-row draw cache (`BEAR2WAVE_CACHE_VISIBLE_ROWS`) to reduce CPU with 15+ traces
- `TraceDocument` + `SignalModuleTree` architecture; `MainFrameMenus` unified via `MenuIds.hpp`
- Help menu (Ctrl+F1), diagnostics bundle export (View menu)
- VZT block read retry / timeout (`BEAR2WAVE_VZT_BLOCK_*`)
- CI: `run_smoke.ps1 -TraceToolsOnly`, optional nightly `-IncludeLarge`
- Docs: `QUICKSTART.md`, `WEEK3_CLOSEOUT.md`, `WX_RELEASE_BUILD.md`

### Changed

- File → Close uses dedicated menu id (1015); Markers menu ids 8501+
- Release build path documented; `package_release.ps1` wx `mswu` detection

## [0.1.0-alpha] - 2026-05-19

### Added

- FST/VZT/LXT2/GHW lazy loading with viewport windowing
- AI analysis panel (DeepSeek / Ollama), local statistics notebook
- Compare dual-trace mode, Tcl scripting (partial)
- TraceTools CI smoke tests, `vcpkg.json` manifest
- Memory budget (`BEAR2WAVE_MAX_LOADED_CHANGES`) and `TraceBackend` API header

### Changed

- M1 release layout: `out/x64/Release/Bear2Wave.exe`, portable build props via `WXWIN` / `VCPKG_ROOT`
