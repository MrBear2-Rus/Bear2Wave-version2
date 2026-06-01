# Contributing to Bear2Wave

## Build (Windows x64)

1. Install Visual Studio 2022 with **Desktop development with C++**.
2. `vcpkg install zlib:x64-windows bzip2:x64-windows liblzma:x64-windows` (or repo `vcpkg.json`).
3. Install wxWidgets 3.2+ MSVC x64; set **`WXWIN`** to its root (or edit `Bear2WaveWx.props`).
4. Set **`VCPKG_ROOT`** (or edit `Bear2WaveTraceFormats.props` → `VcpkgRoot`).
5. `powershell -File tools\setup_local_props.ps1`
6. `msbuild TEST1\TEST1.vcxproj /p:Configuration=Release /p:Platform=x64`  
   (version string comes from root `VERSION.txt` via `Bear2Wave.Build.props`)
7. Output: `out/x64/Release/Bear2Wave.exe`
8. Beta package: `powershell -File tools\package_release.ps1 -Configuration Release` → `dist/Bear2Wave-<version>-win64.zip`
9. Optional: `tools/fetch_gtkwave_libs.ps1` for VZT/LXT2/GHW readers.

## Loader tests (no GUI)

```powershell
powershell -ExecutionPolicy Bypass -File tools\run_trace_tests.ps1
```

## Before a PR

- [ ] `TraceTools` / `test-all` passes locally or rely on CI `windows-ci.yml`
- [ ] No API keys or machine-specific paths committed
- [ ] Update `docs/LARGE_FILE_AND_ENGINEERING_TODO.md` or `docs/AI_ANALYSIS_TODO.md` if you complete a listed task

See [docs/LARGE_FILE_AND_ENGINEERING_TODO.md](docs/LARGE_FILE_AND_ENGINEERING_TODO.md) for the engineering roadmap.
