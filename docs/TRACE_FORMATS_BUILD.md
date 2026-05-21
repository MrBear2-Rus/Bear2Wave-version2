# Bear2Wave 波形格式构建说明

所有格式（VCD / FST / VZT / LXT / LXT2 / GHW）最终都加载到统一的 `vcd_t` 结构，由 `trace_load_from_path()` 按扩展名分发。

## 快速对照

| 格式 | 实现 | 预处理器宏 | 第三方源码 |
|------|------|------------|------------|
| VCD | 内置 `vcd.cpp` | — | — |
| FST | `fst_loader.cpp` + libfst | — | `third_party/libfst`（已有） |
| **VZT** | `vzt_loader.cpp` + **libvzt** | `BEAR2WAVE_WITH_VZT` | GTKWave `lib/libvzt` |
| **LXT / LXT2** | `lxt2_loader.cpp` + **liblxt** | `BEAR2WAVE_WITH_LXT2` | GTKWave `lib/liblxt`（`lxt2_read.c`） |
| **GHW** | `ghw_loader.cpp` + **libghw** | `BEAR2WAVE_WITH_GHW` | GHDL `libghw` |
| Transaction | FST/VZT 事件类型 → `BEAR2WAVE_VT_TRANSACTION` | — | 无单独文件格式 |

未启用对应宏时，程序仍可编译运行；打开 `.vzt` / `.lxt` / `.ghw` 会在日志中提示如何启用。

---

## 1. 获取 GTKWave 读取库（VZT + LXT2）

在仓库根目录（`TEST1 - 1/TEST1`）执行：

```powershell
powershell -ExecutionPolicy Bypass -File tools\fetch_gtkwave_libs.ps1
```

脚本会把 GTKWave 的 `libvzt`、`liblxt`（内含 LXT2 读取器）复制到：

```
third_party/gtkwave/libvzt/
third_party/gtkwave/liblxt/
```

---

## 2. 用 vcpkg 安装压缩依赖（Windows x64 推荐）

在 **x64** 配置下链接（与现有 `TEST1.vcxproj` 中 vcpkg 路径一致）：

```powershell
vcpkg install zlib:x64-windows
vcpkg install bzip2:x64-windows
vcpkg install liblzma:x64-windows
```

| 包 | 用途 |
|----|------|
| **zlib** | VZT/LXT2 块解压（**必需**） |
| **bzip2** | `.vzt` / `.lxt2` 的 bzip2 压缩块（推荐） |
| **liblzma** | lzma 压缩块（可选） |

MSVC 上 libvzt 对 pthread 使用内置桩（见 `vzt_read.h` 中 `_MSC_VER`），一般**不需要**单独安装 pthreads。

---

## 3. Visual Studio 启用 VZT / LXT2

### 3.1 导入属性表（推荐）

1. 复制 `TEST1/Bear2WaveTraceFormats.props.example` 为 `TEST1/Bear2WaveTraceFormats.props`
2. 按本机 vcpkg 路径修改 `VcpkgRoot`
3. 在 Visual Studio：**项目 → 属性 → 通用属性 → VC++ 目录 → 导入 …**，选择 `Bear2WaveTraceFormats.props`（仅 **x64 Debug/Release**）

或在 `TEST1.vcxproj` 的 `<ImportGroup Label="PropertySheets">` 中加入：

```xml
<Import Project="Bear2WaveTraceFormats.props" Condition="Exists('Bear2WaveTraceFormats.props')" />
```

### 3.2 手动配置（摘要）

**C/C++ → 预处理器 → 附加定义：**

```
BEAR2WAVE_WITH_VZT
BEAR2WAVE_WITH_LXT2
HAVE_ZLIB
HAVE_BZ2
HAVE_LZMA
_CRT_SECURE_NO_WARNINGS
```

**C/C++ → 附加包含目录：**

```
$(ProjectDir)..\third_party\gtkwave          ← config.h 所在目录
$(ProjectDir)..\third_party\gtkwave\libvzt
$(ProjectDir)..\third_party\gtkwave\libvzt\lzma
$(ProjectDir)..\third_party\gtkwave\liblxt
```

仓库已提供 `third_party/gtkwave/config.h`（MSVC 用精简版）。若提示找不到 `config.h`，确认上述第一项在包含路径中。

**链接器 → 附加依赖项：**

```
zlib.lib
bz2.lib
lzma.lib
```

**添加到项目的 .c 源文件（编译为 C，非 C++）：**

- `third_party/gtkwave/libvzt/vzt_read.c`
- `third_party/gtkwave/liblxt/lxt2_read.c`

（若链接报错，再按 gtkwave 工程补全同目录下被 `vzt_read.c` / `lxt2_read.c` 引用的辅助 `.c` 文件。）

**本项目已包含的 C++ 源：**

- `trace_loader.cpp`
- `vzt_loader.cpp`
- `lxt2_loader.cpp`
- `ghw_loader.cpp`

---

## 4. GHW（GHDL 波形）

GHW 来自 [GHDL](https://github.com/ghdl/ghdl) 的 **libghw**。

1. 安装或编译 GHDL，取得 `libghw` 头文件与库（或源码目录 `src/libghw`）
2. 定义 `BEAR2WAVE_WITH_GHW`，包含 `ghw.h`，链接 `libghw`
3. 在 `ghw_loader.cpp` 中实现与 `vzt_loader.cpp` 相同的「读入 → `vcd_signal_append_change` → `vcd_sort_all_value_changes`」流程

**临时替代：** 仿真时直接写 FST，或在命令行转换：

```bash
ghdl run ... --wave=wave.ghw
ghdl ... -fst wave.fst
```

然后在 Bear2Wave 中打开 `wave.fst`。

---

## 5. Transaction / 字符串波形

不是独立磁盘格式。在 FST/VZT/LXT2 中识别事件、字符串、实型变量，写入 `signal_t::fst_var_type`（见 `core/trace_var_types.h`），`WaveformPanel` 用 `ClassifyTraceKind()` 映射为 `TextString` / `RealAnalog`。

---

## 6. 运行时 bz2.dll

vcpkg 的 `x64-windows` 三元组中 **bzip2 为动态库**，链接 `bz2.lib` 后运行 `TEST1.exe` 需要同目录有 **bz2.dll**。

- 工程已在生成后自动从 `$(VcpkgRoot)\bin\bz2.dll` 复制到 `x64\Debug`（或 Release 输出目录）。
- 若仍提示缺少 DLL，可手动复制：

  ```
  E:\download\vcpkg-master\installed\x64-windows\bin\bz2.dll
  → TEST1\x64\Debug\bz2.dll
  ```

- 若希望**不依赖 DLL**，可安装静态三元组并改链接路径：

  ```powershell
  vcpkg install bzip2:x64-windows-static
  ```

  然后将 `VcpkgRoot` / 库目录改为 `installed\x64-windows-static`（需自行调整 `TEST1.vcxproj` 中的路径）。

---

## 7. 验证

1. 编译 x64 Debug，确认无链接错误
2. **文件 → 打开**，过滤器应包含 `*.vzt;*.lxt;*.lxt2;*.ghw`
3. 打开样例 `.vzt` / `.lxt2`，状态栏应显示 `Loaded VZT trace: ...` 且信号树有节点
4. 若失败，查看输出窗口中的 `[VZT]` / `[LXT2]` 行及 `trace_load failed` 消息

---

## 8. Linux / CMake（可选）

```bash
sudo apt install zlib1g-dev libbz2-dev liblzma-dev
# 运行 fetch 脚本或手动 clone gtkwave
cmake -DBEAR2WAVE_WITH_VZT=ON -DBEAR2WAVE_WITH_LXT2=ON ..
```

宏与源文件列表与 Windows 相同。
