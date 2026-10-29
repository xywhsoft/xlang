# graph: xl.dll 宿主缺少集合长度头文件

状态：主线发布物阻断，graph 包未绕过；2026-10-19 复现。

2026-10-20 复验：`tools/check_extlib_graph.ps1 -SkipAot` 中 C 边界、
128 组独立参照图、分配故障、线程、XLang JIT、生成原型和 C 范例
均通过，仍在 `xl-host-reuse` 第 0 轮缺少同一内嵌头文件。
当前 `release/xl.dll` 的文件时间为 2026-10-06 21:36:32 UTC；
本任务不重建或修改主线发布物，待主线更新后复跑以下最小复现和包门禁。

2026-10-21 复验：C 测试改为在 `-DNDEBUG` 下仍执行断言后，
`tools/check_extlib_graph.ps1 -SkipAot` 的三组 C、XLang JIT、九项 ABI 原型
与 C 范例继续通过，仍在 `xl-host-reuse` 第 0 轮报
`__xl_runtime/xl6_collection_count.h` 缺失。测试修复没有改变该主线阻断。

## 最小复现

脚本：`dev/extlib_blockers/graph_dll_vfs_min.xl`。在 demo6 根目录运行：

```powershell
$env:XLANG_PACKAGE_PATH = 'D:\GIT\x-lang\demo6\release\extlibs'
gcc -std=c11 -O2 -Wall -Wextra -Werror -Irelease/include test/support/extlib_runtime_host.c release/xl.dll -o build/test/graph_guard/xl-host.exe
./build/test/graph_guard/xl-host.exe 'D:\GIT\x-lang\demo6' 'D:\GIT\x-lang\demo6\dev\extlib_blockers\graph_dll_vfs_min.xl'
```

预期退出码 0；当前 `xl.dll` 报：

```text
tcc: __xlvfs__/module.c:144: error: include file '__xl_runtime/xl6_collection_count.h' not found
```

同样的宿主源码链接 `release/xlc.dll` 后运行同一脚本，退出码为 0。
`graph.isDag([])` 不触发，`graph.nodes([]).length` 触发；graph 的普通
`xl run`、C 范例以及 Linux 包门禁均通过。

## 定位与建议

`src/cgen/xl6_ir_cgen_emit_xvalue_core.c` 在集合长度路径输出该 include；
`src/runtime/xl6_collection_count.h` 存在，而且当前 Windows runtime/xlc
两份 VFS 清单均声明 `src/runtime/*.h`。当前 `release/xl.dll` 早于 runtime
清单更新，`release/xlc.dll` 的对应路径可用，因此首先重新生成内嵌资源、
重建并发布 `xl.dll`，再复跑该最小复现与 `tools/check_extlib_graph.ps1 -SkipAot`。
若重建后仍缺失，应审查运行版资源清单展开及 TCC include/VFS 查找路径。
长期门禁应让发布的两个 DLL 分别运行集合返回值 `.length` 的宿主用例。
