# Tcl scripting (official libtcl)

Bear2Wave embeds the **Tcl** interpreter ([tcl-lang.org](https://www.tcl-lang.org/)) when built with `BEAR2WAVE_WITH_TCL`.

## Install Tcl (Windows / vcpkg)

```powershell
vcpkg install tcl:x64-windows
vcpkg integrate install
```

## Visual Studio (TEST1.vcxproj)

1. **C/C++ → Preprocessor**: add `BEAR2WAVE_WITH_TCL`
2. **C/C++ → Additional Include Directories**: add vcpkg `installed\x64-windows\include` (contains `tcl.h`)
3. **Linker → Additional Library Directories**: `installed\x64-windows\lib`
4. **Linker → Additional Dependencies**: `tcl.lib` (or `tcl9.0.lib` depending on version)
5. Add to the project **二选一**:
   - **推荐**：在 `TEST1.vcxproj` 里加入 `script/TclScriptEngine.cpp`，并删掉 `Main.cpp` 里对它的 `#include`
   - **或**：保持 `Main.cpp` 末尾的 `#include "script/TclScriptEngine.cpp"`（默认已启用，免改 vcxproj）

Copy `tcl90.dll` (name may vary) next to your `.exe`, or ensure vcpkg `installed\x64-windows\bin` is on `PATH`.

## Without Tcl

The app still builds. **File → Read Tcl Script File** shows instructions to enable Tcl.

## Commands (Bear2Wave)

| Tcl command | Description |
|-------------|-------------|
| `bear2wave_load path` | Open `.vcd` / `.fst` / `.csv` |
| `bear2wave_add sig ?sig...?` | Add signals to wave list |
| `bear2wave_add_list {a b c}` | Add from Tcl list |
| `bear2wave_zoom full\|in\|out` | Zoom |
| `bear2wave_page left\|right` | Page view |
| `bear2wave_set_time t` | Playhead time |
| `bear2wave_set_range start end` | Visible window |
| `bear2wave_echo msg` | Status bar + log |
| `bear2wave_nop` | No-op |

## GTKWave-compatible procs (subset)

| Proc | Maps to |
|------|---------|
| `gtkwave::loadFile path` | `bear2wave_load` |
| `gtkwave::addSignalsFromList {..}` | `bear2wave_add_list` |
| `gtkwave::nop` | `bear2wave_nop` |
| `gtkwave::zoom_full` / `zoom_in` / `zoom_out` | zoom |
| `gtkwave::page_left` / `page_right` | page |

## Example

See `examples/scripts/demo_wave.tcl`.
