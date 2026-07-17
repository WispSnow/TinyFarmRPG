# CMake 构建脚本模块

本目录包含由 `cmake/BuildHelpers.cmake` 调用的独立 CMake 脚本。

## SyncDirectory.cmake

`SyncDirectory.cmake` 将 source 目录增量同步到可执行文件的运行目录，统一处理 `assets/`、`ui/`、`scripts/` 和 `config/`。

必需参数：

- `SOURCE_DIR`：源目录
- `TARGET_DIR`：目标目录

可选参数：

- `EXTRA_SOURCE_FILE`：额外复制的单个文件
- `EXTRA_TARGET_FILE`：额外文件的目标路径，必须与 `EXTRA_SOURCE_FILE` 一起传入

调用示例：

```bash
cmake -DSOURCE_DIR=/path/to/assets \
      -DTARGET_DIR=/path/to/build/assets \
      -DEXTRA_SOURCE_FILE=/path/to/imgui.ini \
      -DEXTRA_TARGET_FILE=/path/to/build/imgui.ini \
      -P cmake/scripts/SyncDirectory.cmake
```

同步规则：

- 使用 `cmake -E copy_if_different`，内容未变化的文件不会重写。
- 在目标目录写入 `.tf_source_manifest`，只记录 source 中的相对路径。
- source 中删除的文件会从目标目录删除。
- 未被清单记录的运行时生成文件会保留，例如 `config/user_settings.json`。
- 同一输出目录上的并行同步通过进程锁串行化。

`BuildHelpers.cmake` 将同步任务设为可执行目标的依赖。因此再次构建目标即可同步内容文件，无需触发 C++ 重链接。

## CopyDLLs.cmake

`CopyDLLs.cmake` 在 Windows 上复制目标的运行时 DLL，以及存在的 PDB 调试符号。脚本通过配置阶段生成的 wrapper 使用 `include()` 调用，因为 `$<TARGET_RUNTIME_DLLS:...>` 需要在生成阶段展开。

wrapper 需要设置：

- `DLL_LIST`：DLL 文件列表
- `TARGET_DIR`：可执行文件目录

示例：

```cmake
set(DLL_LIST "C:/path/SDL3.dll;C:/path/other.dll")
set(TARGET_DIR "C:/path/to/build")
include("cmake/scripts/CopyDLLs.cmake")
```

脚本逐个比较 DLL 的 MD5，仅复制发生变化的文件。游戏、`engine_tests` 和 `game_tests` 都通过 `setup_windows_dll_copy` 接入该流程。

## 故障排查

资源未同步时：

1. 检查 `SOURCE_DIR` 与 `TARGET_DIR` 是否正确。
2. 直接按上面的示例运行 `SyncDirectory.cmake`，查看脚本报错。
3. 确认正在构建实际依赖同步任务的可执行目标。

Windows DLL 未复制时：

1. 确认目标使用动态库，并且 CMake 版本支持 `$<TARGET_RUNTIME_DLLS>`。
2. 查看 `build/<preset>/copy_dlls_wrapper_<target>_<config>.cmake`。
3. 确认依赖以 CMake target 的形式链接到该可执行目标。

## 添加新的脚本

脚本应先验证必需参数，再由 `BuildHelpers.cmake` 通过 `add_custom_target` 或 `add_custom_command` 接入，并为命令设置 `VERBATIM`。内容同步类任务应优先复用 `SyncDirectory.cmake`，避免重复实现复制和清理逻辑。
