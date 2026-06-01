# Tcl scripting (official libtcl)

Bear2Wave embeds the **Tcl** interpreter ([tcl-lang.org](https://www.tcl-lang.org/)) when built with `BEAR2WAVE_WITH_TCL`.

## Install Tcl (Windows / vcpkg)

```powershell
vcpkg install tcl:x64-windows
vcpkg integrate install
```

## Visual Studio (TEST1.vcxproj)

1. **C/C++ → Preprocessor**: add `BEAR2WAVE_WITH_TCL`
2. **C/C++ → Additional Include Directories**: vcpkg `installed\x64-windows\include`
3. **Linker → Additional Library Directories**: `installed\x64-windows\lib`
4. **Linker → Additional Dependencies**: `tcl.lib` (or versioned name e.g. `tcl90.lib`)
5. `script/TclScriptEngine.cpp` is already in the project.

Copy `tcl90.dll` (name may vary) next to `Bear2Wave.exe`, or add vcpkg `bin` to `PATH`.

## Without Tcl

The app builds normally. **File → Read Tcl Script File** shows enable instructions.

## Running scripts

| Method | Description |
|--------|-------------|
| **File → Read Tcl Script File** | Pick a `.tcl` file after startup |
| **`BEAR2WAVE_TCL_SCRIPT=path.tcl`** | Auto-run once after the main window opens |
| **`TclScriptEngine::RunString`** | API for a future REPL / console |

## Bear2Wave commands

| Command | Description |
|---------|-------------|
| `bear2wave_help` | List commands |
| `bear2wave_load path` | Open trace (VCD/FST/…) |
| `bear2wave_reload` | Re-open current trace |
| `bear2wave_add sig ?sig…?` | Add signals (returns count) |
| `bear2wave_add_list {a b}` | Add from Tcl list |
| `bear2wave_add_glob pattern` | Add all signals matching glob |
| `bear2wave_remove sig ?sig…?` | Remove from wave list |
| `bear2wave_clear` | Clear wave list |
| `bear2wave_zoom full\|in\|out` | Zoom |
| `bear2wave_page left\|right` | Pan one page |
| `bear2wave_set_time t` | Playhead |
| `bear2wave_set_range start end` | Visible window |
| `bear2wave_get_time` | Current playhead |
| `bear2wave_get_max_time` | Trace end time |
| `bear2wave_get_range` | `{start end}` list |
| `bear2wave_get_num_signals` | Signals in dump |
| `bear2wave_get_signal_name idx` | 0-based full name |
| `bear2wave_get_display_count` | Rows in wave list |
| `bear2wave_get_display_name idx` | Displayed signal name |
| `bear2wave_get_dump_path` | Current trace path |
| `bear2wave_load_session path` | Load `.b2w`/`.gtkw`/`.bwv` |
| `bear2wave_save_session path` | Save session |
| `bear2wave_filter keyword` | Highlight/search filter (empty clears) |
| `bear2wave_theme light\|dark` | UI theme |
| `bear2wave_radix hex\|bin\|dec\|oct\|ascii\|signed\|real` | Radix on all displayed |
| `bear2wave_marker add t ?label?` | Named marker |
| `bear2wave_marker clear` | Remove all markers |
| `bear2wave_marker list` | `{{t label} …}` |
| `bear2wave_baseline t` / `clear` | Baseline marker |
| `bear2wave_export csv path` | Export visible window to CSV |
| `bear2wave_export png path` | Grab waveform PNG |
| `bear2wave_echo msg` | Status bar + log |
| `bear2wave_nop` | No-op |

### Query & selection

| Command | Description |
|---------|-------------|
| `bear2wave_get_value signal time` | Formatted value at sim time |
| `bear2wave_find pattern` | Glob-match signal full names → Tcl list |
| `bear2wave_select row index` | Select display row |
| `bear2wave_select signal name` | Select row by signal name |
| `bear2wave_get_selection` | `{rowIndex signalName}` |
| `bear2wave_get_timescale` | `{scale unit}` e.g. `{1 ns}` |
| `bear2wave_get_measure` | `{A B}` measure markers, or `{}` |

### View & rows

| Command | Description |
|---------|-------------|
| `bear2wave_measure seta\|setb time` | Set measure marker A/B |
| `bear2wave_measure clear` | Clear measure markers |
| `bear2wave_zoom_factor` | Get current zoom factor |
| `bear2wave_zoom_factor 2.5` | Set zoom factor (1.0 = full trace) |
| `bear2wave_cursor_value on\|off\|toggle` | Cursor value labels |
| `bear2wave_scroll row index` | Scroll waveform list to row |
| `bear2wave_insert blank` | Insert blank row at selection |
| `bear2wave_insert comment text` | Insert comment row |
| `bear2wave_radix hex ?signal?` | Radix for all rows, or one signal |

### Conversion & protocol decode

| Command | Description |
|---------|-------------|
| `bear2wave_convert src dst` | Format convert (e.g. VCD→FST) |
| `bear2wave_transaction run ?visible\|full? ?all\|selected?` | Run transaction_proc synchronously |

## GTKWave-compatible procs (subset)

| Proc | Maps to |
|------|---------|
| `gtkwave::loadFile` | `bear2wave_load` |
| `gtkwave::addSignals` / `addSignalsFromList` | add |
| `gtkwave::getNumFacs` / `getFacName` | signal query |
| `gtkwave::getDumpFile` | current trace path |
| `gtkwave::setZoomRangeTimes` | `bear2wave_set_range` |
| `gtkwave::setMarker` / `getMarkerList` / `deleteAllMarker` | markers |
| `gtkwave::setBaselineMarker` | baseline |
| `gtkwave::zoom_*` / `page_*` | zoom / page |
| `gtkwave::getCursorTime` | playhead |
| `gtkwave::getTimeRange` / `getZoomRangeTimes` | visible range |
| `gtkwave::getFacValue fac time` | signal value |
| `gtkwave::search pattern` | find signals |
| `gtkwave::selectMarker` | add named marker |

## Example

See `TEST1/examples/scripts/demo_wave.tcl`.
