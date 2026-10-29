# ZIP Linux 内嵌 TCC 阶段超时（门禁环境已收口）

2026-10-27 后续定位：确认当时没有其他编译进程后，同一
`text_transfer.xl` 在 WSL 的 `/mnt/d` 上完成需 130.93 秒
（用户态 11.85 秒、系统态 14.05 秒），将 `xl`、`libxlc.so` 和
`zip`/`_libzip`/`_zlib` 原样复制到 WSL `/tmp` 后仅需 9.01 秒
（用户态 7.81 秒、系统态 1.09 秒）；保持 `release/` 层级的
原生副本需 9.50 秒。证据支持挂载盘 I/O 是此前阶段超时的主因，
不支持将其判定为 ZIP 算法错误或编译器死锁。包级 Linux 门禁现仅
将 XLang JIT/AOT 阶段搬到一次性原生文件系统副本；同一测试清单
完整退出 0，包括 C ASan/UBSan、Python 格式互通、双语 AOT 和
符号链接边界。此项门禁超时已解除；ZIP 私有构造器与并发提取的
独立安全阻断仍未解决。

2026-10-27 在 WSL 中从 `/mnt/d/GIT/x-lang/demo6` 运行现有 Linux
`release/xl`。`zip/test/boundaries.xl` 的 `xl check` 约 1.9 秒通过，
`xl ir --emit` 约 2.0 秒完成，`xl cgen --backend xlir --emit` 约 5.7 秒
完成，生成 C 约 1.2 MiB。相同源的 `xl run` 在 210 秒后由 `timeout`
终止，累计用户态约 13 秒、系统态约 9 秒；`xl build` 也在 180 秒后
超时。较小的 `zip/test/text_transfer.xl` 的 `xl run` 在 100 秒后超时。
这些命令均未输出包级错误，也未证明 ZIP 操作本身已开始执行。

复现命令（在 `demo6` 根目录，单独执行）：

```sh
timeout 80s ./release/xl ir release/extlibs/zip/test/boundaries.xl --emit build/tmp/zip_boundary.xlir
timeout 100s ./release/xl cgen release/extlibs/zip/test/boundaries.xl --backend xlir --emit build/tmp/zip_boundary.c
timeout 210s ./release/xl run release/extlibs/zip/test/boundaries.xl
timeout 180s ./release/xl build release/extlibs/zip/test/boundaries.xl -o build/tmp/zip_boundary
```

`zip` 依赖 `_libzip` 的大量 C 源文件。当前运行位于 WSL 的 Windows
挂载盘上，复现时还有其他 Linux 编译任务同时运行。上述证据只把慢路径
限定在 IR/cgen 之后的编译、链接或资源加载阶段；尚不能区分 TCC
处理源码、挂载盘 I/O、资源竞争和其他原因。Windows 新发布版的 ZIP
默认包门禁同日退出 0。不要直接放宽 Linux 超时并宣称问题修复。

下一步应在无并行构建的环境重跑，分别记录内嵌 TCC 读入各源码、编译、
链接和运行入口的耗时，并比较 WSL 原生文件系统与挂载盘。若确定是
共享编译器路径，再由主线修复；若确定是包清单引入了不需要的 libzip
单元，再在保持格式和安全测试通过的前提下收紧清单。本任务不在此处
修改 xlang 主线。
