# wxWidgets Release 构建（W3-4）

Bear2Wave 的 **Release|x64** 配置会优先链接 wx **非 debug** 静态库（`mswu`）。若本机只安装了 debug 库（`mswud`），工程会自动回退，Release 仍会生成 exe，但运行时库为 **MultiThreadedDebugDLL**（见 `TEST1/Bear2Wave.Build.props`）。

## 检查当前环境

```powershell
$wx = (Select-String -Path TEST1\Bear2WaveWx.props -Pattern '<WxWidgetsRoot>([^<]+)').Matches[0].Groups[1].Value
Test-Path "$wx\lib\vc_x64_lib\mswu\wx\setup.h"   # True = 真 Release
Test-Path "$wx\lib\vc_x64_lib\mswud\wx\setup.h"  # 常见开发机仅有此项
```

## 构建 wx `mswu`（Windows x64，与 VS 工具集一致）

在 wx 源码目录（示例 `wxWidgets-3.2.x`）：

```powershell
mkdir build\msw-x64-release -Force
cd build\msw-x64-release
cmake ..\..\ -G "Visual Studio 17 2022" -A x64 `
  -DwxBUILD_SHARED=OFF `
  -DwxBUILD_USE_STATIC_RUNTIME=OFF `
  -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release --parallel
```

或使用官方 `build\msw` 解决方案打开 `wx_vc17.sln`，对 **Release|x64** 构建 `base`、`core` 等目标，确认生成 `lib\vc_x64_lib\mswu\*.lib`。

## 构建 Bear2Wave Release

```powershell
msbuild TEST1\TEST1.vcxproj /p:Configuration=Release /p:Platform=x64
powershell -File tools\package_release.ps1 -Configuration Release
powershell -File tools\run_smoke.ps1 -Configuration Release -TraceToolsOnly -SkipBuild
```

## 发布包说明

- `tools\package_release.ps1` 默认 **Debug**（多数 CI/开发机仅有 `mswud`）。  
- 对外 **Beta**（`0.2.0-beta`）若需真 Release，必须先有 `mswu` 再 `-Configuration Release`。  
- 脚本结束时会打印 wx 库后缀检测结果。
